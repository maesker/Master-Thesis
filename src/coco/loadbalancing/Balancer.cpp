/**
 * @file Balancer.cpp
 * @class Balancer
 *
 * @date 5 May 2011
 * @author Denis Dridger
 *
 *
 * @brief Balancer is the super class of LoadManager and HotSpotManager
 * it provides common functionalities needed in both managers.
 *
 * Main functionalities are:
 * - determining own workload
 * - requesting workload of other servers 
 * - determining  best suitable servers to share load 
 * - updating metadata lookup table (MLT)
 */


#include <stdio.h>
#include <iostream>
#include "coco/loadbalancing/Balancer.h"
#include "coco/loadbalancing/Sensors.h"
#include "coco/loadbalancing/LBErrorCodes.h"
#include "coco/loadbalancing/LBLogger.h"
#include "coco/coordination/MltHandler.h"
#include "coco/loadbalancing/LBConf.h"
#include "mm/journal/JournalManager.h"
#include "mm/journal/Journal.h"
#include <string.h>

Balancer::Balancer()
{
	//LBCommunicationHandler::create_instance();
	//this->com_handler = LBCommunicationHandler::get_instance();
}

Balancer::~Balancer()
{
	// TODO Auto-generated destructor stub
}


/**
 * @brief Determines own load
 * @return struct containing load metrics 
 */
ServerLoadMessage Balancer::get_own_load()
{
	prof_start();

	ServerLoadMessage load;	
	load.cpu_load = Sensors::get_average_load(); 
	load.swaps = Sensors::get_ganesha_swapping(this->get_com_handler()); 
	load.threads = Sensors::get_no_of_ganesha_threads();
	load.normalized_load = Sensors::normalize_load(load.cpu_load, load.swaps, load.threads);

	prof_end();

	return load;
}

	

/**
 * @brief Updates MLT due to a subtree movement
 * @param[in] inode: number of inode to move
 * @param[in] path: absolute path to inode that shall be moved
 * @param[in] server: target server
 * @param[in] subtree_change: info whether whole subtree is moved or only a part of it 
 * @return 0 on success some error code else 
 */
int Balancer::update_local_mlt(InodeNumber inode, string path, Server asyn_tcp_server, SubtreeChange subtree_change)
{
	prof_start();

	//get access to MLT functions
	MltHandler *mlt_handler = this->get_mlt_handler();
	
	//if highly accessed inode is partition root then it's an easy case: just change the server, which shall maintain this inode from now on
	//if (mlt_handler->is_partition_root(inode))
	if (subtree_change == FULL_SUBTREE_MOVE)
	{
		int update_success;

		//change inode owner
		try
		{					
			update_success = mlt_handler->update_entry(inode, asyn_tcp_server); 
			mlt_handler->write_to_file(MLT_PATH);

		}
		catch (MltHandlerException e)
		{
			prof_end();
			log("Error updating MLT, given server is unknown by MLT. Aborting rebalancing procedure...");
			return MLT_ERROR_CANNOT_UPDATE_MLT_UNKNOWN_SERVER;
		}	
		

		if (update_success == 0) 
		{
			
			if (mlt_handler->reload_mlt(MLT_PATH) != 0)
			{
				log("Error reloading MLT after update. MDS needs to be restarted!");
			}
			
			prof_end();

			return 0;
		}
		else 
		{
			prof_end();
			return MLT_ERROR_CANNOT_UPDATE_MLT;
		}
	}
	//not the root partition itself but a partition child node shall be maintained by an other server 
	//i.e. truncate partition and transfer the new part (a subtree again) to the other server
	else
	{
		int entry_add_success;								
		
		try
		{
			//create new partition with given inode and path to this inode
			log3("Creating new MLT entry with path %s and inode %li ", path.c_str(), inode);						
			entry_add_success = mlt_handler->handle_add_new_entry(asyn_tcp_server, 0, inode, path.c_str());
			mlt_handler->write_to_file(MLT_PATH);
		}
		catch (MltHandlerException e)
		{
			log("Error updating MLT, given server is unknown by MLT. Aborting rebalancing procedure...");
			prof_end();
			return MLT_ERROR_CANNOT_CREATE_NEW_ENTRY_INVALID_PATH_GIVEN;

		}
		
		if (entry_add_success == 0) 
		{
			if (mlt_handler->reload_mlt(MLT_PATH) != 0)
			{
				log("Error reloading MLT after update. MDS needs to be restarted!");
			}
				
			prof_end();

			return 0;
		}
		else  
		{
			prof_end();
			return MLT_ERROR_CANNOT_UPDATE_MLT;
		}
	}
}



/**
 * @brief Informs other servers about MLT changes
 * Note that broadcasting MLT changes is only necessary if no shared storage for MLT is used, i.e. each server has its own MLT.
 * @param[in] inode: number inode to move
 * @param[in] path: absolute path to inode that shall be moved
 * @param[in] server: target server
 * @param[in] subtree_change: info whether whole subtree is moved or only a part of it 
 * @return 0 on success some error else
 */
int Balancer::broadcast_mlt(InodeNumber inode, string path, Server asyn_tcp_server, SubtreeChange subtree_change)
{
	prof_start();
		
	LBMessage msg;

	MLTUpdate mlt_update;
	memcpy(mlt_update.server_address, asyn_tcp_server.address.c_str(), 32);		
	mlt_update.server_port = asyn_tcp_server.port;
	mlt_update.inode = inode;	
	memcpy(mlt_update.path, path.c_str(), 1024);
	
	
	if (subtree_change == FULL_SUBTREE_MOVE)
	{
		mlt_update.message_type = UPDATE_PARTITION_OWNER;
	}

	if (subtree_change == PARTIAL_SUBTREE_MOVE)
	{
		mlt_update.message_type = CREATE_NEW_PARTITION;
	}


	uint64_t message_id;
	vector<ExtractedMessage> message_list;

	vector <Server> server_list;
	get_mlt_handler()->get_other_servers(server_list); 

	vector <string> server_list_only_ips;

	//grab only the ips from server_list, since request_reply() expects a string list of ips
	for (int j = 0; j < server_list.size(); j++)
	{
		server_list_only_ips.push_back(server_list[j].address);		
	}
	
	
	LBCommunicationHandler *com_handler = this->get_com_handler();
	

	//send info about overloaded inode and new server to all cluster memebers so that they can update their MLTs
	int ret = com_handler->request_reply(&message_id, (void*) &mlt_update, sizeof(mlt_update), COM_LoadBalancing, server_list_only_ips, message_list);

	if(ret != 0)
	{
		log("request_reply FAIL");	
		prof_end();
		return COM_ERROR_MLT_BROADCAST_FAILED_CANNOT_SEND_REQUEST;
	}
	
	if ( (com_handler->receive_reply(message_id, 1000) != 0))
	{			
		log("No reply received... damn");
		prof_end();
		return COM_ERROR_MLT_BROADCAST_FAILED_NO_REPLY_RECEIVED;
	}

	prof_end();		
	return 0;
	
}


/**
* @brief asks other servers to send their load
* @param[in] server_list: list of servers to be asked for load
* @param[out] server_load: list where collected server loads shall be stored 
* @return 0 on success some error code else
* */
int Balancer::get_cluster_load(vector<Server>& server_list, vector<PossibleTargets>& server_load)
{
	prof_start();

	int servers = server_list.size();

	LBMessage msg;
	msg.message_type = LOAD_REQUEST;

	uint64_t message_id;
	vector<ExtractedMessage> message_list;

	vector <string> server_list_only_ips;

	//grab only the ips from server_list, since request_reply() expects a string list of ips
	log("Requesting cluster load from:");
	for (int j = 0; j < server_list.size(); j++)
	{
		server_list_only_ips.push_back(server_list[j].address);
		log2("%s", server_list[j].address.c_str());
	}


	LBCommunicationHandler *com_handler = this->get_com_handler();


	//request list of loads from all cluster members
	int ret = com_handler->request_reply(&message_id, (void*) &msg, sizeof(msg), COM_LoadBalancing, server_list_only_ips, message_list);

	if(ret != 0)
	{
		log("request_reply FAILED");	
		prof_end();
		return COM_ERROR_LOAD_REQUEST_FAILED_CANNOT_SEND_REQUEST;
	}

	if ( (com_handler->receive_reply(message_id, 5) == 0))
	{			
		log("Reply received!!!");

		for(unsigned int i = 0; i<message_list.size(); i++)
		{
			ServerLoadMessage s_load;
			s_load = *((ServerLoadMessage*) message_list.at(i).data);
			PossibleTargets target;
			target.server_load = s_load;
			target.server.address = message_list.at(i).sending_server;

			//recover port, since list of server loads contains only the ip
			 for (int k = 0; k < server_list.size(); k++)
                        {
                                if (strcmp(server_list[k].address.c_str() , message_list.at(i).sending_server.c_str()) == 0)
                                {                                        
                                        target.server.port = server_list[k].port;  
                                        break;
                                }
                        }
			
			server_load.push_back(target);
			log2("Load of: %s",  message_list.at(i).sending_server.c_str());
			log2("Cpu load: %f", s_load.cpu_load);
			log2("No of threads: %i", s_load.threads);
			log2("No of swaps: %i", s_load.swaps);
			log2("Normalized load: %f", s_load.normalized_load);
		}
	}
	else
	{	
		log("No reply received... damn");
		prof_end();
		return COM_ERROR_LOAD_REQUEST_FAILED_NO_REPLY_RECEIVED;
	}

	prof_end();	
	return 0;	
}

	

/**
* @brief determines best suitable MDS to move the subtree to
* @param[in] cluster_load: list with server loads 
* @param[out] server: best server 
* @return 0 if a server with lower load could be found, error code "NO_BETTER_SERVER_FOUND" else
* */
int Balancer::get_best_target_server(vector<PossibleTargets>& cluster_load, Server& asyn_tcp_server)
{
	prof_start();

	//get own load to be compared with cluster_load
	ServerLoadMessage own_load = this->get_own_load();
	
	log2("Own load normalized: %f", own_load.normalized_load);

	//min load is the own load initially ...	
	float min_load = own_load.normalized_load;

	//best target server initially undefined 
	int best_target = -1;

	//find server with lowest load, especially lower than the own load
	for(int k = 0; k < cluster_load.size(); k++)
	{		
		float serverk_load = cluster_load[k].server_load.normalized_load;

		if ( serverk_load < min_load )
		{
			min_load = serverk_load;
			best_target = k;
		}
	}
	
	//return server-not-found-error if no better server exists or if a better server exists but can't take the load (almost overloaded)
	if ((best_target != -1) || ((cluster_load[best_target].server_load.normalized_load * ALMOST_OVERLOADED_THRESHOLD) > own_load.normalized_load))
	{
		prof_end();
		asyn_tcp_server = cluster_load[best_target].server;
		return 0;
	}
	else 
	{
		prof_end();
		return LB_ERROR_NO_BETTER_SERVER_FOUND;
	}
	
}

/**
* @brief get access to MLT
* @return pointer to the MLT handler
* */
MltHandler *Balancer::get_mlt_handler()
{
	prof_start();
	MltHandler *mlt_handler = &(MltHandler::get_instance());	
	prof_end();
	return mlt_handler;
}

/**
* @brief get access to loadbalancing communication handler
* @return pointer to the LBCommunicationHandler
* */
LBCommunicationHandler *Balancer::get_com_handler()
{
	prof_start();
	LBCommunicationHandler::create_instance();
	LBCommunicationHandler *com_handler = LBCommunicationHandler::get_instance();
	prof_end();
	return com_handler;
}


/**
* @brief determines the number of the root inode given some inode nr of the partition 
* @param some_inode[in] some inode for that the root inode nr shall be determined
* @return inodenumber or root partition to which the inode belongs
* */
InodeNumber Balancer::get_root_of_partition(InodeNumber some_inode)
{
	prof_start();

	InodeNumber partition_root;

	if (JournalManager::get_instance()->inode_exists(some_inode, partition_root) == -1 )
	{
		prof_end();
		return 0;			
	}
	prof_end();
	return partition_root;
}


/**
* @brief determines the relative path for a given inode nr 
* @param some_inode[in] some inode for that the path shall be determined 
* @param path[out] the path to be returned 
* @return 0 on success, some error code else
* */
int Balancer::get_path(InodeNumber some_inode, string& path)
{
	prof_start();

	log("Getting root of this partition...");
	InodeNumber partition_root = this->get_root_of_partition(some_inode);
	if (partition_root == 0) 
	{
		prof_end();
		return JOURNAL_ERROR_UNKNOWN_INODE;		
	}
	log2("Got it. It's: %li",partition_root);

	
	//get journal that manages partition with root inode "partition_root"
	log("Getting journal using this root inode...");
	Journal *journal = JournalManager::get_instance()->get_journal(partition_root);	
	if (journal == NULL)
	{
		prof_end();
		return JOURNAL_ERROR_GETTING_JOURNAL_FOR_INODE_FAILED;
	}
	log("Got the journal.")
		
	///char path1[1024];
	string path1;
	string path2;
		
	log2("Getting relative path to inode %li", some_inode);
	string rel_path_to_given_inode;
	int result = journal->get_path(some_inode, rel_path_to_given_inode);
	if(result != 0)
	{	
		prof_end();
		log("Error getting path!");
		return JOURNAL_ERROR_GETTING_PATH_FOR_GIVEN_INODE_FAILED;
	}

	log2("Got the path. It's: %s", rel_path_to_given_inode.c_str());	

	log2("Getting absolute path to partition root %li", partition_root);
	string abspath;
	try
	{
		abspath = this->get_mlt_handler()->get_path(partition_root);
	}
	catch(MltHandlerException e)
	{
		log("Error getting path!");
		return MLT_ERROR_GETTING_PATH_FOR_GIVEN_INODE_FAILED;
	}

	log2("Got the path. It's: %s", abspath.c_str());	

	string resultpath;

	if(strcmp(abspath.c_str(),"/") == 0) 
		resultpath = "/"+rel_path_to_given_inode;
	else
		resultpath = "/"+abspath + rel_path_to_given_inode;	

		
	log3("Constructed full path to inode %li is: %s", some_inode, resultpath.c_str());
		
		
	prof_end();
	return 0;
}


