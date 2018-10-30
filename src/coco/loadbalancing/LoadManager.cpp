/**
 * @file LoadManager.cpp
 * @date 5 May 2011
 * @author Denis Dridger
 * @brief LoadManager is responsible for moving highly loaded subtrees to other MDS
 *
 * Load manager's procedure is:
 * - determining own workload
 * - requesting workload of all other servers
 * - determining best suitable server(s) where to move the subtree
 * - determining the subtree to move
 * - moving subtree and informing other servers about it
 */

#include <cstddef>
#include <stdlib.h>
#include <stdio.h>
#include <cassert>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include "coco/loadbalancing/Sensors.h"
#include "coco/loadbalancing/CommonLBTypes.h"
#include "coco/loadbalancing/LBErrorCodes.h"
#include "coco/loadbalancing/LBLogger.h"
#include "coco/loadbalancing/LBConf.h"
#include "coco/loadbalancing/LoadManager.h"
#include "coco/dao/CommonDAOTypes.h"               
#include "coco/dao/DistributedAtomicOperations.h"
#include "mm/storage/Partition.h"
#include "mm/storage/PartitionManager.h"
#include "coco/loadbalancing/PartitionManagerAccess.h"
#include "mm/mds/MetadataServer.h"

#define MLT_RECOVERY_PATH "/tmp/mlt_backup"

/**
 * @brief copies files to target server and deletes files from overloaded server
 * @param[in] pntr unused void pointer
 * */
void* physical_data_migrator(void* pntr)
{
	LoadManager::get_instance()->run_physical_data_migrator();
}


LoadManager* LoadManager::__instance = NULL;

/**
 * @brief inits LoadManager
 * */
LoadManager::LoadManager()
{
	this->mlt_handler = &(MltHandler::get_instance());
	this->idle = true;	
	this->migration_done = false;
	this->start_migration = false;

	pthread_t thread;	
    	pthread_create(&thread, NULL, physical_data_migrator, NULL);
	
}

LoadManager::~LoadManager(){}

void LoadManager::create_instance()
{
	if ( __instance == NULL )
	{
		__instance = new LoadManager();
	}
}

LoadManager* LoadManager::get_instance()
{
		assert(__instance);
		return __instance;
}




/**
* @brief requests the overloaded tree node from Ganesha process
* @return struct containing the corresponding inode number and info whether the inode is root of the partition
* */
access_counter_return_t *LoadManager::determine_subtree_to_move()
{
	prof_start();

	log("Requesting information on overloaded subtree from Ganesha...");

	//message to be sent to Ganesha
	int message = REQ_ACCESS_COUNTER;

	//request reply from Ganesha
	RequestResult reply = this->get_com_handler()->ganesha_send((void*) &message, sizeof(ganesha_request_types_t));

	access_counter_return_t *bad_node = (access_counter_return_t*) (reply.data);

	log2("Determined inode: %li", bad_node->inode_number);
	

	prof_end();
	
	return bad_node;
}

/**
* @brief starts distributed atomic operation
* @param inode[in] number of inode to move
* @param subtree_change[in] indicates whether to move whole partition or not
* @param server[in] target server for migration
* */
int LoadManager::start_dao(InodeNumber inode, SubtreeChange subtree_change, Server& target_server)
{
	prof_start();

	this->errorcode = -1; //error that DaoHandler may return
	
	//get distributed atomic operation handler
	DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();

	//define the distributed operation
	DAOSubtreeMove *operation = new DAOSubtreeMove;
	operation->inode = inode;	
	operation->subtree_change = subtree_change;
	memcpy(operation->receiver_address, target_server.address.c_str(), 32);	 
	memcpy(operation->sender_address, mlt_handler->get_my_address().address.c_str(), 32);
	operation->receiver_port = target_server.port;	

	//define participant (the subtree-receiving server)
	vector<Subtree> participants;
   	Subtree tree;
   	tree.server = string(target_server.address);
   	tree.subtree_entry = 0; //probably not needed at all
   	participants.push_back(tree);

	log("*** Following operations will be executed atomic. ***");
	//log2("Target for dao operation: %s", target_server.address.c_str());	
	//start atomic operation
  	dao.start_da_operation( (void*) operation, sizeof(DAOSubtreeMove), MoveSubtree, participants, inode);
	
	//TODO delete operation after dao has finished

	int seconds = 0;
	while (errorcode == -1)
	{
		sleep(1);	
		seconds++;
		if(seconds == 10) 
		{
			this->errorcode = LB_ERROR_DAO_STUFF_TIMED_OUT;
			break;
		}
	}
	
	prof_end();
		
	return this->errorcode; //errorcode is set in DaoHandler.cpp;
}

/**
* @brief move overloaded subtree to an other server
*
* the approach in detail:
*
* 1) check whether there are other servers at all
* 2) request workload of the other servers
* 3) determine subtree to move communicating with Ganesha process
* 4) try to select neighbour servers for moving the overloaded subtree to (need result from 3) here)
* 5) if neighbour servers can't take the load then try to find some other server
* 6) begin atomic operations (all following steps are performed atomic)
* 7) update the local MLT and write it to disk
* 8) tell the "MDS" module to: finish all running operations, to block all new operations, to clear the journal, to clear Ganesha cache
* 9) update partition changes on storage device
* 10) end atomic operations
* NOTE: some of these steps may return an error, which aborts the whole rebalancing process
*
* @return 0 if send_subtree was successful, return an error code else
* */
int LoadManager::send_subtree_dynamic()
{
	prof_start();

	//abort if static tree movement is in progress
	if(!this->idle) 
	{
		log("Error moving tree: another tree move is in progress. Aborting the rebalancing procedure...");
		prof_end();	
		return LB_ERROR_ANOTHER_SUBTREE_MOVE_IN_PROGRESS;
	}
	this->idle = false;
	this->migration_done = false;

	//retun value to be used for error checks
	int result;

	//list holding servers that can take over the overloaded subtree
	vector <string> target_servers;

	//get list of servers to send the request to
	vector <Server> server_list;
	this->mlt_handler->get_other_servers(server_list);

	//list of workloads of all servers
	vector <PossibleTargets> cluster_load;

	//no othe servers defined in MLT, can't rebalance
	if (server_list.size() < 1) 
	{
		log("This is bad, there are no other servers at all. Aborting the rebalancing procedure...");
		prof_end();
		return LB_ERROR_NO_SERVERS_TO_CONTACT;
	}

	//there are other servers => ask them for threir load and store it to the "cluster_load" list
	else
	{
		log("Requesting load of other servers...");
		result = this->get_cluster_load(server_list, cluster_load);

		if (result != 0)
		{
			log("Error requesting cluster load during rebalancing attempt. Aborting rebalancing procedure.");
			prof_end();
			return result;
		}
	}


	//determine which subtree shall be moved
	log("Determining subtree that shall be moved to an other server...");
	access_counter_return_t *hot_entry = this->determine_subtree_to_move();
  	InodeNumber inode = hot_entry->inode_number;
	SubtreeChange subtree_change;
	if (hot_entry->flag == 0) subtree_change = FULL_SUBTREE_MOVE;
	else subtree_change = PARTIAL_SUBTREE_MOVE;
	
	//can't move inode, got an unknown inode
	if (inode == 0) 
	{
		log2("Invalid inode: %li. Aborting rebalancing procedure...",inode);
		prof_end();
		return FS_ERROR_INVALID_INODE;
	}

	//can't move inode, it's the file system root
	//if (inode == 1) return FS_ROOT;

	//get path to inode, needed for MLT update later
	//however, path is not needed when moving the entire partition	
	if (subtree_change == FULL_SUBTREE_MOVE)
	{
		this->path = "dontcare";	
	}
	else
	{
		result = this->get_path(inode, this->path);
		if (result != 0)
		{
			prof_end();
			return result;
		}
	}

	//server that is supposed to receive the moved subtree
	Server target_server;

	//try to find suitalbe neighbour servers first (i.e. servers that hold a partition that overlaps with the partition containing the subtree to move)
	log("Trying to find 'neighbour' server that can take the load..."); 
	result = this->get_suitable_neighbour_server(cluster_load, target_server, inode);

	if (result != 0)
	{
		log("No suitable neighbour servers found. Trying to find some other server that can take the load...");

		//last resort, trying to find any server, that can take the load			
		result = this->get_best_target_server(cluster_load, target_server);

		if (result != 0)
		{
			log("No better server could be found during rebalancing attempt. Aborting rebalancing procedure...");
			prof_end();
			return result;
		}		
		log2("Target server for rebalaning: %s", target_server.address.c_str());
	}

	//TODO check whether the server can take the load (?)
	
	log("");
	log("Preperations for sending subtree done. Beginning tree move...");
	log("");
	
	prof_end();

	return this->start_dao(inode, subtree_change, target_server);
		
}

/**
* @brief performs atomic steps needed to move the subtree to an other server
* @param inode[in] number of affected inode
* @param subtree_change[in] flag that specifies whether the whole partition has to be moved or only a part of it
* @param target_server[in] address of server that is supposed to receive the sent subtree
* @return 0 if send_subtree was successful, return an error code else
* */
int LoadManager::dao_send_subtree(InodeNumber inode, SubtreeChange subtree_change, Server target_server)
{
	prof_start();

	this->atomic_steps_done = 0;	

	//log("Beginning dao...");
	log2("Path: %s",this->path.c_str());
	log2("Inode: %li", inode);
	log2("Server: %s", target_server.address.c_str());

	//holds 0 or error codes
	int result; 
	
	//create temp MLT just in case	
	remove(MLT_RECOVERY_PATH);
	if (this->mlt_handler->write_to_file(MLT_RECOVERY_PATH) != 0)
	{
		log2("Warning: could not create temporary MLT file: %s, will not be able to recover MLT on failure!",MLT_RECOVERY_PATH)
	}

	//update local MLT: transfer subtree responsibility to the server with lowest loaded
	log("Updating MLT...");

	//log2("Target server: %s", target_server.address.c_str());
	//log2("Server port %i", target_server.port);

	result = this->update_local_mlt(inode, this->path, target_server, subtree_change);

	if (result != 0)
	{
		log("Error updating MLT during rebalance attempt. Aborting the rebalancing procedure...");
		prof_end();
		return result;
	}
	
	//this->atomic_steps_done++;

	//write MLT to disk because Ganesha process needs to read the updated MLT now
	log("Writing MLT to disk...")
	result = this->mlt_handler->write_to_file(MLT_PATH);

	if (result != 0)
	{
		log("Error writing MLT to disk during rebalance attempt. Aborting the rebalancing procedure...");
		prof_end();
		return MLT_ERROR_CANNOT_WRITE_MLT_TO_DISK;
	}

	this->atomic_steps_done++;
	
	log("Handling partition migration by MDS module...");

	InodeNumber partition_root = inode;

	//set partition_root to real root if its not yet
	if (subtree_change == PARTIAL_SUBTREE_MOVE)
	{
		partition_root = this->get_root_of_partition(inode);
		if (partition_root == 0) 
		{
			prof_end();
			return JOURNAL_ERROR_UNKNOWN_INODE;
		}
	}		


	try
	{				
		result = MetadataServer::get_instance()->handle_migration_quit_partition(&partition_root);		
	}
	catch (MDSException e)
	{
		log2("Error handling migration by MDS module (quit partition)!   Following exception occured: %s", e.what());
		log("Aborting the rebalancing procedure...");
		prof_end();
		return MDS_ERROR_HANDLING_MIGRATION_FAILED;
	}

	if (result != 0)
	{
		log2("Error handling migration by MDS module (quit partition error code:  %i). Aborting the rebalancing procedure...", result);
		prof_end();
		return MDS_ERROR_HANDLING_MIGRATION_FAILED;
	}


	if (subtree_change == PARTIAL_SUBTREE_MOVE)
	{
		try
		{				
			result = MetadataServer::get_instance()->handle_migration_create_partition(&partition_root);		
		}
		catch (MDSException e)
		{
			log2("Error handling migration by MDS module (create partition)!  Following exception occured: %s", e.what());
			log("Aborting the rebalancing procedure...");
			prof_end();
			return MDS_ERROR_HANDLING_MIGRATION_FAILED;
		}

		if (result != 0)
		{
			log2("Error handling migration by MDS module (create partition error code:  %i). Aborting the rebalancing procedure...", result);
			prof_end();
			return MDS_ERROR_HANDLING_MIGRATION_FAILED;
		}

		this->atomic_steps_done++;

		log("Updating partitions on storage...");
		result = this->remove_subtree_from_storage_logical(inode, subtree_change);

		if (result != 0)
		{
			log("Error updating storage partitions during rebalance attempt. Aborting the rebalancing procedure...");
			prof_end();
			return result;
		}
	}

	this->atomic_steps_done++;

	prof_end();
		
	return 0;

}

/**
* @brief receive overloaded subtree from an other server
*
* the approach in detail:
*
* 1) initial situation: the overloaded server has put information concerning subtree move to distributed operations queue, DAOListener has finds this information and calls receive_subtree()
* 2) update storage (i.e. mount received subtree)
* 3) create corresponding journal
* 4) update local MLT
* 5) end atomic operation
*
* @param inde[in] new root inode
* @param subtree_change[in] info whether the whole partition has been moved or only a part of it
* @return 0 if receive_subtree was successful, return an error code else
* */
int LoadManager::dao_receive_subtree(InodeNumber inode, SubtreeChange subtree_change)
{
	prof_start();

	//abort if dynamic tree movement is in progress
	if(!this->idle) 
	{
		log("Error receiving tree: another tree move is in progress. Aborting the rebalancing procedure...");
		prof_end();
		return LB_ERROR_ANOTHER_SUBTREE_MOVE_IN_PROGRESS;
	}
	this->idle = false;
	this->atomic_steps_done = 0;
	
	log("Beginning to integrate deligated subtree...");

	int result;

	/*
	TODO check whether share storage is used
	
	//update local MLT (using the same procedure with the same parameters as on the overloaded server) 		
	log("Updating local MLT...");
	result = this->update_local_mlt(inode, path, this->mlt_handler->get_my_address(), subtree_change);
	if (result != 0)
	{
		log("Error updating local MLT while receiving new subtree!  Aborting the rebalancing procedure...");
		prof_end();
		return result;
	}
	
	this->atomic_steps_done++;
	*/

	//inform MDS module
	log("Handling partition migration by MDS module...");
	
	
	try
	{	
		result = MetadataServer::get_instance()->handle_migration_create_partition(&inode);
	}
	catch (MDSException e)
	{
		log2("Error handling migration by MDS module!  Following exception occured: %s", e.what());
		log("Aborting the rebalancing procedure...");
		prof_end();
		return MDS_ERROR_HANDLING_MIGRATION_FAILED;
	}

	if (result != 0)
	{
		log2("Error handling migration by MDS module (error code:  %i). Aborting the rebalancing procedure...", result);
		prof_end();
		return MDS_ERROR_HANDLING_MIGRATION_FAILED;
	}

	this->atomic_steps_done++;

	//update partition state on storage
	log("Updating partitions on storage...");
	result = this->create_subtree_on_storage(inode, subtree_change);
	if (result != 0)
	{
		log("Error updating storage while receiving new subtree...");
		prof_end();
		return result;
	}	

	this->atomic_steps_done++;

	prof_end();
	
	return 0;

}


/**
* @brief determines best suitable neighbour server to move the subtree to
* @param cluster_load[in] list with server loads
* @param server[out] best suitable neighbour server that could take the load 
* @param inode[in] number of overloaded node, parameter shouldn't be 0 or 1 at this point
* @return 0 if a suitable server (low load && neighbour) could be found, error code "NO_SUITABLE_NEIGHBOUR_SERVER_FOUND" else
* */
int LoadManager::get_suitable_neighbour_server(vector<PossibleTargets>& cluster_load, Server& asyn_tcp_server, InodeNumber inode)
{
	prof_start();

	InodeNumber tmp_inode = 0;
	
	ServerLoadMessage own_load = this->get_own_load();
	//collect inodes of neighbour partitions into this list
	vector <InodeNumber> inodes;

	// get parent inode number
	try 
	{
		tmp_inode = this->mlt_handler->get_parent(inode);

		if(tmp_inode == INVALID_INODE_ID)	return MLT_ERROR_INCONSISTENT_MLT;
		else if( tmp_inode != inode) inodes.push_back(tmp_inode);

	} 
	catch (MltHandlerException e) 
	{
		// inode is the root of filesystem, nothing happens here
	}

	this->mlt_handler->get_children(inode, inodes);
	//add inodes of child partitions
	// TODO
//	if ( !inodes.empty() )
//	{
//
//		float min_load = own_load.normalized_load;
//
//		int best_server = -1;
//
//		//check load of each server holding the corresponding inode and pick the one with the lowest load
//		for (unsigned int i = 0; i < cluster_load.size(); i++)
//			for (unsigned int k = 0; k < inodes.size(); k++)
//			{
//				if ((strcmp(this->mlt_handler->get_mds(inodes.at(k)).address.c_str(),
//						cluster_load.at(i).server.address.c_str()) == 0)
//						&& (cluster_load.at(i).normalized_load < min_load))
//				{
//					min_load = cluster_load.at(i).normalized_load;
//					best_server = i;
//				}
//			}
//
//		//if there is a neighbour server that is better than the own server then store it to OUT param server else return error code
//		if (best_server == -1)
//			return NO_SUITABLE_NEIGHBOUR_SERVER_FOUND;
//		else
//		{
//			server = cluster_load.at(best_server).server;
//			return 0;
//		}
//	}
	prof_end();

	return LB_ERROR_NO_SUITABLE_NEIGHBOUR_SERVER_FOUND;
}

/**
 * @brief Updates storage state due to subtree move (invoked on subtree sending server)
 * note: if partition manager is NULL then a simple storage is used and there is nothing to do
 *
 * @param inode[in] number of inode to move
 * @param subtree_change[in] info whether the whole subtree to move or only a part of it
 * @return 0 on success some error code else
 */
int LoadManager::remove_subtree_from_storage_logical(InodeNumber inode, SubtreeChange subtree_change)
{
	prof_start();

	PartitionManager *pm = PartitionManagerAccess::get_instance()->get_partition_manager();

        if (pm == NULL) 
        {
                log("No need to update the storage, since a simple storage is used");
		this->idle = true;		
		//log("Load balancer is idle...");
		prof_end();
                return 0;
        }
        else
        {
		Partition *partition;

		//simplest case: the whole partition has to be removed from this server
		//get affected partition and set it to read-only the other server will mount the partitiion then in read-write mode
		if (subtree_change == FULL_SUBTREE_MOVE)
		{			
			log("Delegating subtree to other server (full subtree move)...");
			log2("Getting partition object with root inode %li ...",inode);		
			try 
			{
				partition = pm->get_partition(inode);
			} 
			catch (StorageException e) 
			{
				log("Error getting needed storage partition. Aborting the rebalancing procedure...");
				prof_end();
				return STORAGE_ERROR_PARTITION_NOT_FOUND;
			}
			
			log("Mounting partition as ready only...");	
			try 
			{
				partition->mount_ro();	
			}
			catch (StorageException e)
			{
				log("Error mounting partition as read only!. Aborting the rebalancing procedure...");
				prof_end();
				return STORAGE_ERROR_MOUNTING_PARTITION_RO_FAILED;
			}		
			this->idle = true;				
			//log("Load balancer is idle...");		
		}
		else
		{
			
		 	log("Delegating subtree to other server (partial subtree move)...");

			log2("Getting root inode of partition that manages the inode %li ...", inode);
			InodeNumber root_inode = this->get_root_of_partition(inode);
			if (root_inode == 0) 
			{
				prof_end();
				return JOURNAL_ERROR_UNKNOWN_INODE;		
			}	
			
			log2("Getting partition object with root inode %li ...",root_inode);	
			try
			{
				partition = pm->get_partition(root_inode);
			}
			catch (StorageException e)
			{
				log("Error getting needed storage partition. Aborting the rebalancing procedure...");
				prof_end();
				return STORAGE_ERROR_PARTITION_NOT_FOUND;
			}

			//store tree removal info global to be used later by LBCommunicationHandler upon message arrival which confirms that removal may begin			
			this->phys_tree_rem.partition = partition;
			this->phys_tree_rem.inode = root_inode;

			log("Ready to remove data from disk. Waiting for confirmation from other server...");
			
		}
        }

	prof_end();

        return 0;
}



/**
 * @brief Updates storage state due to subtree move (invoked on subtree receiving server)
 * note: if partition manager is NULL then a simple storage is used and there is nothing to do
 *
 * @param inode[in] number of inode to move
 * @param subtree_change[in] info whether the whole subtree to move or only a part of it
 * @return 0 on success some error code else
 */
int LoadManager::create_subtree_on_storage(InodeNumber inode, SubtreeChange subtree_change)
{
	prof_start();

	PartitionManager *pm = PartitionManagerAccess::get_instance()->get_partition_manager();

        if (pm == NULL)
        {
                log("No need to update the storage, since a simple storage is used");
		this->idle = true;		
		//log("Load balancer is idle...");				
		prof_end();
                return 0;
        }
        else
        {
		Partition *partition;
		
		//simplest case: the whole partition has to be transferred to this server
		if (subtree_change == FULL_SUBTREE_MOVE)
		{
			log("Integrating new subtree (full subtree move)...");

			//get pointer to affected partition
			
			log2("Getting partition object with root inode %li ...",inode);
			try
			{
				partition = pm->get_partition(inode); 
			}
			catch (StorageException e)
			{
				log("Error getting needed storage partition. Aborting the rebalancing procedure...");
				prof_end();
				return STORAGE_ERROR_PARTITION_NOT_FOUND;
			}

			log2("Setting owner of inode %li to this server...",inode);
			try
			{			
				partition->set_owner((char*)this->mlt_handler->get_my_address().address.c_str());
			}
			catch (StorageException e)
			{
				log("Error changing owner of partition. Aborting the rebalancing procedure...")
				prof_end();
				return STORAGE_ERROR_CHANGING_INODE_OWNER_FAILED;
			}
		
			log("Mounting partition as read-write...");
			try
			{
				partition->mount_rw(); 
			}
			catch (StorageException e)
			{
				log("Error mounting partition as read write!. Aborting the rebalancing procedure...");
				prof_end();
				return STORAGE_ERROR_MOUNTING_PARTITION_RW_FAILED;
			}
			this->idle = true;			
			//log("Load balancer is idle...");				
		}
		else
		{
			log("Integrating new subtree (partial subtree move)...");

				
			log("Getting a free partition...");
			try
			{
				partition = pm->get_free_partition(); 
			}
			catch (StorageException e)
			{
				log("Error getting a free partition!. Aborting the rebalancing procedure...");
				prof_end();
				return STORAGE_ERROR_NO_FREE_PARTITION_AVAILABLE;
			}
		
			log("Mounting partition as read-write...");
			try
			{
				partition->mount_rw(); 
			}
			catch (StorageException e)
			{
				log("Error mounting partition as read write!. Aborting the rebalancing procedure...");
				prof_end();
				return STORAGE_ERROR_MOUNTING_PARTITION_RW_FAILED;
			}

			log2("Setting owner of inode %li to this server...",inode);
			try
			{					
				partition->set_owner((char*)this->mlt_handler->get_my_address().address.c_str()); 
			}
			catch (StorageException e)
			{
				log("Error changing owner of partition. Aborting the rebalancing procedure...")
				prof_end();
				return STORAGE_ERROR_CHANGING_INODE_OWNER_FAILED;
			}

			log2("Setting inode %li as root of the new partition...", inode);
			try
			{			
				partition->set_root_inode(inode); 
			}
			catch (StorageException e)
			{
				log("Error setting inode as root. Aborting the rebalancing procedure...")
				prof_end();
				return STORAGE_ERROR_SETTING_ROOT_INODE_FAILED;
			}	

			//migrate truncated subtree to determined partition
			this->start_migration = true;						
			this->phys_tree_copy.partition = partition;
			this->phys_tree_copy.inode = inode;			
		}
        }

	prof_end();
	
        return 0;
}

/**
 * @brief moves subtree specified by user to a server also specified by user  *
 * @param inode[in] number of inode to move
 * @param target_server[in] server that shall maintain the subtree in future
 * @return 0 on success some error code else
 */
int LoadManager::send_subtree_static(InodeNumber inode, Server target_server)
{
	prof_start();

	//abort if dynamic tree movement is in progress
	if(!this->idle) 
	{	log("Error moving tree: another tree move is in progress. Aborting the rebalancing procedure...");
		prof_end();
		return LB_ERROR_ANOTHER_SUBTREE_MOVE_IN_PROGRESS;
	}
	this->idle = false;	
	this->migration_done = false;

	log3("Trying to move inode: %li to server %s",inode, target_server.address.c_str());

	//determine whether to move the whole partition or just a part of it
	SubtreeChange subtree_change;
	if (this->mlt_handler->is_partition_root(inode)) 
	{
		subtree_change = FULL_SUBTREE_MOVE; 
		log("Inode is root of the partition, the whole partitiion is being moved...");
	}
	else 
	{
		subtree_change = PARTIAL_SUBTREE_MOVE; //most likely only this will occur while static move
		log("Inode is not root of the partition, only a part of the partitiion is being moved...");
	}
	
	prof_end();
	log("Starting distributed atomic operation 'tree move'...")		
	return this->start_dao(inode, subtree_change, target_server);
}


/**
 * @brief tries to undo changes which were performed on initiator MDS during unsuccessful atomic operation
 * @param inode[in] number of inode to move
 * @param subtree_change[in] info whether the whole subtree to move or only a part of it
 * @param target_server[in] server that is supposed to receive the overloaded tree
 * @return 0 on success some error code else
 */
int LoadManager::undo_changes_on_initiator_mds(InodeNumber inode, SubtreeChange subtree_change, Server target_server)
{
	prof_start();

	int result = 0;


	if (this->atomic_steps_done > 0)
	{
		//undo local MLT update
		log("Undoing MLT changes...")
		result = this->undo_mlt_changes();
		if (result != 0)
		{
			log("Error undoing MLT changes!  Aborting the rollback procedure...");
			prof_end();
			return result;
		}
	}	

	//undo changes in MDS module
	log("Undoing partition migration by MDS module...");
	if (this->atomic_steps_done > 1)
	{			
		result = MetadataServer::get_instance()->handle_migration_create_partition(&inode);
		if (result != 0)
		{
			log("Error undoing migration by MDS module!  Aborting the rollback procedure...");
			prof_end();
			return MDS_ERROR_CANNOT_UNDO_MIGRATION;
		}
	}

	log("Undoing storage changes...");
	if (this->atomic_steps_done > 2)
	{
		//TODO all
	}

	prof_end();

	return result;

}

/**
 * @brief tries to undo changes which were performed on participant MDS during unsuccessful atomic operation
 * @param inode[in] number of inode to move
 * @param subtree_change[in] info whether the whole subtree to move or only a part of it
 * @param target_server[in] server that is supposed to receive the overloaded tree
 * @return 0 on success some error code else
 */
int LoadManager::undo_changes_on_participant_mds(InodeNumber inode, SubtreeChange subtree_change, Server target_server)
{
	prof_start();

	int result = 0;


	//NOTE this was needed when no shared storage for MLT was available
	/*
	//undo local MLT update
	log("Undoing local MLT changes...")
	result = this->undo_mlt_changes();
	if (result != 0)
	{
		log("Error undoing MLT changes!  Aborting the rollback procedure...");
		prof_end();
		return result;
	}
	*/

	//undo changes in MDS module
	log("Undoing partition migration by MDS module...");
	if (this->atomic_steps_done > 0)
	{		
		string path = this->mlt_handler->get_path(inode);
		result = MetadataServer::get_instance()->handle_migration_quit_partition(&inode);
		if (result != 0)
		{
			log("Error undoing migration by MDS module!  Aborting the rollback procedure...");
			prof_end();
			return MDS_ERROR_CANNOT_UNDO_MIGRATION;
		}
	}

	log("Undoing storage changes...");
	if (this->atomic_steps_done > 1)
	{
		//TODO all
	}

	prof_end();

	return result;

}



/**
 * @brief tries to undo MLT changes which were performed during unsuccessful atomic operation
 * @return 0 on success some error code else
 */
int LoadManager::undo_mlt_changes()
{
	prof_start();

	
		if(FILE * fp = fopen(MLT_RECOVERY_PATH, "rb"))
		{
			//reload MLT from temp file
			int result = (this->mlt_handler->reload_mlt(MLT_RECOVERY_PATH));
			fclose(fp);
			if (result != 0) return MLT_ERROR_CANNOT_RELOAD_MLT_TO_UNDO_CHANGES;			

			//write recovered MLT to original path
			result = (this->mlt_handler->write_to_file(MLT_PATH));	
			if (result != 0) return MLT_ERROR_CANNOT_WRITE_TO_MLT_FILE_TO_UNDO_CHANGES;
		}
		else return MLT_ERROR_CANNOT_UNDO_MLT_CHANGES_BACKUP_DOESNT_EXIST;					
	

	prof_end();
	return 0;
}

/**
 * @brief copies actual metadata to the target server 
 */
void LoadManager::run_physical_data_migrator()
{
	while(1)
	{		
		sleep(2);
		
		if (this->start_migration)
		{						
			log("Copying data to target storage...");		
			
			try
			{
				this->phys_tree_copy.partition->start_migration(this->phys_tree_copy.partition, this->phys_tree_copy.inode);	
				log("Data copied successfully!");

				log2("Informing server %s that its data can be removed,,,", this->sender_address);

				LBCommunicationHandler *com_handler = LBCommunicationHandler::get_instance();
			
				LBMessage msg;
				msg.message_type = PHYS_TREE_REMOVAL_REQUEST;

				uint64_t message_id;
				vector<ExtractedMessage> message_list;

				vector <string> server_list_only_ips;						
				server_list_only_ips.push_back(string(this->sender_address));			

				com_handler->request_reply(&message_id, (void*) &msg, sizeof(msg), COM_LoadBalancing, server_list_only_ips, message_list);
			
			}
			catch (StorageException e)
			{
				log("Error copying data to target!!!")
				//TODO do something here
			}	
			
			this->start_migration = false;
			//delete this->phys_tree_copy;						
			this->idle = true;			
			//log("Load balancer is idle...");		
		}
		
		if (this->migration_done)
		{
			log("Removing data from storage...");
				
			try
			{
				this->phys_tree_rem.partition->remove_subtree(this->phys_tree_rem.inode);
				log("Data removed successfully!");
			}
			catch (StorageException e)
			{
				log("Error removing data from storage!");
				//TODO do something here
			}				
	
			this->migration_done = false;
			//delete this->phys_tree_rem;
			this->idle = true;			
			//log("Load balancer is idle...");		
		}	
	}		
}


/**
 * @brief helper function, returns human readable error message specified by numeric error code
 * @param errorcode[in] some integer error code from LBErrorCodes.h
 */
string LoadManager::error_to_str(int errorcode)
{
	prof_start();

	switch(errorcode)
	{				
		case LB_ERROR_ANOTHER_SUBTREE_MOVE_IN_PROGRESS:
		{
			return string("LB_ERROR_ANOTHER_SUBTREE_MOVE_IN_PROGRESS");
			break;
		}
		case MLT_ERROR_INCONSISTENT_MLT:
		{
			return string("MLT_ERROR_INCONSISTENT_MLT");
			break;
		}
		case MLT_ERROR_CANNOT_UPDATE_MLT_UNKNOWN_SERVER:
		{
			return string("MLT_ERROR_CANNOT_UPDATE_MLT_UNKNOWN_SERVER");
			break;
		}
		case MLT_ERROR_CANNOT_UPDATE_MLT:
		{
			return string("MLT_ERROR_CANNOT_UPDATE_MLT");
			break;
		}
		case COM_ERROR_MLT_BROADCAST_FAILED_CANNOT_SEND_REQUEST:
		{
			return string("COM_ERROR_MLT_BROADCAST_FAILED_CANNOT_SEND_REQUEST");
			break;
		}
		case COM_ERROR_MLT_BROADCAST_FAILED_NO_REPLY_RECEIVED:
		{
			return string("COM_ERROR_MLT_BROADCAST_FAILED_NO_REPLY_RECEIVED");
			break;
		}
		case LB_ERROR_NO_SERVERS_TO_CONTACT:
		{
			return string("LB_ERROR_NO_SERVERS_TO_CONTACT");
			break;
		}
		case LB_ERROR_NO_BETTER_SERVER_FOUND:
		{
			return string("LB_ERROR_NO_BETTER_SERVER_FOUND");
			break;
		}
		case COM_ERROR_LOAD_REQUEST_FAILED_CANNOT_SEND_REQUEST:
		{
			return string("COM_ERROR_LOAD_REQUEST_FAILED_CANNOT_SEND_REQUEST");
			break;
		}
		case COM_ERROR_LOAD_REQUEST_FAILED_NO_REPLY_RECEIVED:
		{
			return string("COM_ERROR_LOAD_REQUEST_FAILED_NO_REPLY_RECEIVED");
			break;
		}
		case LB_ERROR_NO_SUITABLE_NEIGHBOUR_SERVER_FOUND:
		{
			return string("LB_ERROR_NO_SUITABLE_NEIGHBOUR_SERVER_FOUND");
			break;
		}
		
		case FS_ERROR_INVALID_INODE:
		{
			return string("FS_ERROR_INVALID_INODE");
			break;
		}
		
		case MLT_ERROR_CANNOT_WRITE_MLT_TO_DISK:
		{
			return string("MLT_ERROR_CANNOT_WRITE_MLT_TO_DISK");
			break;
		}
		case MDS_ERROR_HANDLING_MIGRATION_FAILED:
		{
			return string("MDS_ERROR_HANDLING_MIGRATION_FAILED");
			break;
		}
			
		case MLT_ERROR_CANNOT_RELOAD_MLT_TO_UNDO_CHANGES:
		{
			return string("MLT_ERROR_CANNOT_RELOAD_MLT_TO_UNDO_CHANGES");
			break;
		}		
		case MLT_ERROR_CANNOT_WRITE_TO_MLT_FILE_TO_UNDO_CHANGES:
		{
			return string("MLT_ERROR_CANNOT_WRITE_TO_MLT_FILE_TO_UNDO_CHANGES");
			break;
		}
		case MLT_ERROR_CANNOT_UNDO_MLT_CHANGES_BACKUP_DOESNT_EXIST:
		{
			return string("MLT_ERROR_CANNOT_UNDO_MLT_CHANGES_BACKUP_DOESNT_EXIST");
			break;
		}
		case MDS_ERROR_CANNOT_UNDO_MIGRATION:
		{
			return string("MDS_ERROR_CANNOT_UNDO_MIGRATION");
			break;
		}
		case LB_ERROR_PARTICIPANT_FAILED_TO_INTEGRATE_TREE:
		{
			return string("LB_ERROR_PARTICIPANT_FAILED_TO_INTEGRATE_TREE");
			break;
		}
		case STORAGE_ERROR_PARTITION_NOT_FOUND:
		{
			return string("STORAGE_ERROR_PARTITION_NOT_FOUND");
			break;
		}
		case STORAGE_ERROR_MOUNTING_PARTITION_RO_FAILED:
		{
			return string("STORAGE_ERROR_MOUNTING_PARTITION_RO_FAILED");
			break;
		}
		case STORAGE_ERROR_MOUNTING_PARTITION_RW_FAILED:
		{
			return string("STORAGE_ERROR_MOUNTING_PARTITION_RW_FAILED");
			break;
		}
		case STORAGE_ERROR_CHANGING_INODE_OWNER_FAILED:
		{
			return string("STORAGE_ERROR_CHANGING_INODE_OWNER_FAILED");
			break;
		}
		case STORAGE_ERROR_NO_FREE_PARTITION_AVAILABLE:
		{
			return string("STORAGE_ERROR_NO_FREE_PARTITION_AVAILABLE");
			break;
		}
		case STORAGE_ERROR_SETTING_ROOT_INODE_FAILED:
		{
			return string("STORAGE_ERROR_SETTING_ROOT_INODE_FAILED");
			break;
		}
		case LB_ERROR_DAO_STUFF_TIMED_OUT:
		{
			return string("LB_ERROR_DAO_STUFF_TIMED_OUT");
			break;
		}
		case JOURNAL_ERROR_UNKNOWN_INODE:
		{
			return string("JOURNAL_ERROR_UNKNOWN_INODE");
			break;
		}
		case JOURNAL_ERROR_GETTING_JOURNAL_FOR_INODE_FAILED:
		{
			return string("JOURNAL_ERROR_GETTING_JOURNAL_FOR_INODE_FAILED");
			break;
		}
		case MLT_ERROR_CANNOT_CREATE_NEW_ENTRY_INVALID_PATH_GIVEN:
		{
			return string("MLT_ERROR_CANNOT_CREATE_NEW_ENTRY_INVALID_PATH_GIVEN");
			break;
		}
		case JOURNAL_ERROR_GETTING_PATH_FOR_GIVEN_INODE_FAILED:
		{
			return string("JOURNAL_ERROR_GETTING_PATH_FOR_GIVEN_INODE_FAILED");
			break;
		}
		case MLT_ERROR_GETTING_PATH_FOR_GIVEN_INODE_FAILED:
		{
			return string("MLT_ERROR_GETTING_PATH_FOR_GIVEN_INODE_FAILED");
			break;
		}	
		case MDS_ERROR_GANESHA_CONFIG_UPDATE_FAILED:
		{
			return string("MDS_ERROR_GANESHA_CONFIG_UPDATE_FAILED");
			break;
		}	


		default:
		{
			return string("UNKNOWN_ERROR");
			break;
		}
	
	}//end switch
	
	prof_end();
}

