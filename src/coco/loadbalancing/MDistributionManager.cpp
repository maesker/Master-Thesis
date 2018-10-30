/**
 * @file MDistributionManager.cpp
 * @class MDistributionManager
 * @date 5 May 2011
 * @author Denis Dridger 
 *
 * @brief The main module for metadata distribution.
 * Invokes rebalancing procedures and communicates with external modules.
 *
 * The Metadata Distribution Manager monitors the load of an MDS,
 * and invokes rebalancing procedures if necessary.
 * Therefore it creates the corresponding manager (LoadManager, HotspotManager, LargeDirManager)
 * and invokes its rebalance() procedure.
 */

#include <stdio.h>
#include <cstddef>
#include <unistd.h> //for sleep()
#include <cassert>
#include <iostream>
#include <stdint.h>

#include "coco/loadbalancing/Sensors.h"
#include "coco/loadbalancing/MDistributionManager.h"
#include "coco/loadbalancing/LoadManager.h"
#include "coco/loadbalancing/HotSpotManager.h"
#include "coco/loadbalancing/LBLogger.h"
#include "coco/communication/CommonCommunicationTypes.h"
#include "coco/loadbalancing/LBConf.h"
#include "coco/loadbalancing/DaoHandler.h"
#include "coco/loadbalancing/LoadBalancingTest.h"
#include "coco/loadbalancing/PartitionManagerAccess.h"
#include "mm/einodeio/EmbeddedInodeLookUp.h"
#include "coco/coordination/MltHandler.h"
#include "mm/journal/JournalManager.h"



#define CONFIGFILE "conf/loadbalancing.conf"
#define STATIC_MOVE_PARAMS "/tmp/static_move_params"
#define STATIC_MOVE_FEEDBACK "/tmp/static_move_feedback"

using namespace std;

MDistributionManager *MDistributionManager::__instance = NULL;


MDistributionManager::MDistributionManager(StorageAbstractionLayer *storage_abstraction_layer)
{		
	this->storage_abstraction_layer = storage_abstraction_layer; 
}

MDistributionManager::~MDistributionManager()
{
	// TODO Auto-generated destructor stub
}

void MDistributionManager::create_instance(StorageAbstractionLayer *storage_abstraction_layer)
{
	if ( __instance == NULL )
		__instance = new MDistributionManager(storage_abstraction_layer);
}

MDistributionManager *MDistributionManager::get_instance()
{
		assert(__instance);
		return __instance;
}

/**
 * @brief listens to static tree move requests from adminTool
 * @param[in] pntr unused void pointer
 * */
void* start_static_tree_move_listener(void* pntr)
{
	MDistributionManager::get_instance()->static_tree_move_listener();
}


/**
* @brief MDistributionManager core:
* Inits basic objects needed for monitoring and managing work load,
* and runs the balancing managers repeatedly 
* */
void MDistributionManager::run()
{
	//set loadbalancing configuration values
	LBConf::init(CONFIGFILE);
	
	//create and init loadbalancing logger
	LBLogger::create_instance();
	LBLogger::get_instance()->init();
	
	pthread_t thread;	
    	pthread_create(&thread, NULL, start_static_tree_move_listener, NULL);	
	
	//if load balancer is disabled then take a nap (~60 years)
	if (!LOAD_BALANCER_ENABLED)  while(1) sleep(2000000000);
	
	//create and init loadbalancing communication handler
	LBCommunicationHandler::create_instance();
	this->com_handler = LBCommunicationHandler::get_instance();
	com_handler->init(COM_LoadBalancing);
	com_handler->start();
	
	//provides easy access to storage module's PartitionManager		
	PartitionManagerAccess::create_instance(this->storage_abstraction_layer);	
	     
	//create load manager
	LoadManager::create_instance();
	this->load_manager = LoadManager::get_instance();
	
	//create hot-spot manager	
	//HotSpotManager::create_instance();
	//this->hot_spot_manager = HotSpotManager::get_instance();
	
	//create and init distributed atomic operation part of loadbalancing
	DaoHandler *dao_handler = new DaoHandler();
	dao_handler->set_module(DistributedAtomicOperationModule::DAO_LoadBalancing);

	log("Load balancer setup done!")

	//if dynamic load balancer is disabled then take a nap (~60 years)
	//the created objects and threads can still be uses for static tree move
	if (!AUTO_LOAD_BALANCER_ENABLED) 
	{
		log("NOTE: the dynamic load balancer is disabled. You can enable it in 'loadbalancing.conf'");
		while(1) sleep(2000000000);
	}

	//prevent loadbalancing log messages from interfering metadataServer's startup log messages
	sleep(1);
	
	/*
	- check for load, if MDS overloaded then invoke subtree movement
	- sleep specified amount of time then repeat */
	while(1)
	{				
		//if MDS overloaded then start rebalancing procedure
		//NOTE receiving subtree on the other server is done automatically by LoadManager		

		if (this->mds_overloaded()) 
		{
			log("Starting rebalance procedure...\n");

			int result = this->load_manager->send_subtree_dynamic();		

			if(result != 0) 
			{
				log("Error moving tree: load balancter returned following error code:");
				log(this->load_manager->error_to_str(result).c_str());
				this->load_manager->idle = true;							
				log("Load balancer is idle...");																	
			}
									
		}

		/* HOTSPOT STUFF NOT SUPPORTED YET

		//if MDS overloaded AND accesses to a directory are too high to be moved to an other MDS
		//then replicate the directory
		if (this->mds_overloaded(com_handler) && this->is_hotspot()) 
		{
			log("Starting hot-spot management procedure.....\n");
			this->hot_spot_manager->manage_hotspots();
		}
		//if MDS is not overloaded anymore then try to reclaim earlier created replica
		else if (!this->mds_overloaded(com_handler))
		{
			log("Trying to reclaim existing hot-spot replica.....\n");
			this->hot_spot_manager->try_reclaim_replica();
		}		

		//TODO all...
		//this->manage_large_directories();
	
		*/

		log("");
		log2("*** Next rebalancing attempt in %i seconds ***", SECONDS_UNTIL_NEXT_REBALANCE);		
		log("");
		sleep(SECONDS_UNTIL_NEXT_REBALANCE);		
		 
	}
}



/**
* @brief Checks whether the own server is overloaded or not
* TODO think of a more elaborate algorithm here (?)
* (in the moment all load metrics are treated with same priority)
* @return true if overloaded
* */
bool MDistributionManager::mds_overloaded()
{	
	prof_start();
	if(Sensors::cpu_overloaded() || Sensors::swap_limit_reached(this->com_handler) || Sensors::thread_limit_reached())
	{
		log("MDS load: OVERLOADED!");
		prof_end();
		return true;
	}
	else
	{
		log("MDS load: OK!");
		prof_end();
		return false;
	}
}

/**
* @brief Listens to static subtree move request from adminTool and inits the move
* */
void MDistributionManager::static_tree_move_listener()
{
	//make sure there is no such files initially
	remove(STATIC_MOVE_PARAMS);
	remove(STATIC_MOVE_FEEDBACK);

	while(1)
	{		
		sleep(3);		
		
		//begin moving specified subtree statically if user performed the corresponding function using adminTool (i.e. file with move parameters exists)
		if(FILE * fp = fopen(STATIC_MOVE_PARAMS, "r")) 
		{
			log("Static subtree move invoked by adminTool");
		
			//read parameters from file
			char *ip = new char[32]; fgets(ip, 32, fp);			
			char *cport = new char[16]; fgets(cport, 16, fp);
			char *path = new char[512]; fgets(path, 512, fp);			
			fclose(fp);				
			unsigned short iport = atoi(cport);					
		
			//get rid of annoying '\n' produced by fgets()
			for (int i = 0; i < strlen(ip); i++) if ( ip[i] == '\n' ) ip[i] = '\0';        

			//create server from parameters
			Server target_server; 			
			target_server.address = string(ip);
			target_server.port = iport;	
			
			string spath = string(path);

			//get server list from MLT and check whether provided server actually exists
			MltHandler* mlt_handler = &(MltHandler::get_instance());			
			vector <Server> server_list;
			mlt_handler->get_other_servers(server_list);

			bool valid_server = false;
			bool inode_lookup_ok = true;

			char *file_stream;
						
			FILE *afile;	
			afile = fopen(STATIC_MOVE_FEEDBACK,"w");
			string error;

/*
log("getting path...");
string s;
InodeNumber inodee = 2;
JournalManager::get_instance()->get_journal(2)->handle_mds_readdir_request(&inodee, NULL, NULL);
this->load_manager->get_path(3, s);
log("got path");
*/

			if (!LOAD_BALANCER_ENABLED) 
			{
				log("");
				log("Load balancer is disabled! You can enable it in 'loadbalancing.conf'.");	
				log("");				
				error = string("LOAD_BALANCER_DISABLED");
			}
			else
				if (  (strcmp(target_server.address.c_str(), mlt_handler->get_my_address().address.c_str()) == 0 ) && (mlt_handler->get_my_address().port == target_server.port) )
				{								
					error = string("TARGET_SERVER_IS_SOURCE_SERVER");								
					log("Error moving tree: source server and target server are the same.");
				}			
				else
				{		
					for (int j = 0; j < server_list.size(); j++)				
						if ((strcmp(server_list[j].address.c_str(), target_server.address.c_str()) == 0) &&	(server_list[j].port == target_server.port))	
							valid_server = true;				

					if (!valid_server)
					{
						error = string("UNKNOWN_TARGET_SERVER");								
						log3("Error moving tree: unknown server: %s:%i",target_server.address.c_str(),target_server.port);
					}
					else
					{						
						log("Provided target address for static tree is valid, proceeding...");
						log("Warning: there is no check (yet) whether provided path is actually present on this MDS.");

						InodeNumber inode;
			
						//get inode by path
						if (strcmp(spath.c_str(),"/")==0) inode = FS_ROOT_INODE_NUMBER;
						else
						{	
							InodeNumber root_inode;
							string relative_path;
							bool is_root = false;

							try
							{
								is_root = mlt_handler->path_to_root(spath, root_inode, relative_path);
							}											
							catch (MltHandlerException e) 
							{
								log2("Error getting inode for path %s. Root inode is unknown by MLT." , spath.c_str());								
								error = string("ERROR_RESOLVING_PATH_TO_INODE");
								goto end;
							}							

							if (is_root)
							{
								log2("Resolved inode: %li is the root of given path." , root_inode);
								inode = root_inode;
							}
							else
							{
								log2("Resolved inode: %li is not the root of the given path. Looking for root in subpath..." , root_inode);

								Journal *journal = JournalManager::get_instance()->get_journal(root_inode);

								if(journal == NULL)
								{
									log2("Error getting inode for path %s. Corresponding journal object is NULL", spath.c_str());								
									error = string("ERROR_RESOLVING_PATH_TO_INODE");
									goto end;
								}
								else
								{
									if (journal->handle_resolve_path(spath, inode) != 0)
									{
										log2("Error getting inode for path %s. Corresponding journal failed to resolve path.", spath.c_str());								
										error = string("ERROR_RESOLVING_PATH_TO_INODE");
										goto end;
									}
									else
									{
										log2("Path resolved. The desired inode number is %li", inode);
									}
									
								}// else: Journal != NULL					
							}// else: inode not root of given path
						}// else: path != "/"
	
					
						//invoke tree move
						this->load_manager->path = spath;
						int result = this->load_manager->send_subtree_static(inode, target_server);

						if(result != 0)
						{					
							error = this->load_manager->error_to_str(result);												
							log("Error moving tree: load balancter returned following error code:");
							log(error.c_str());
							this->load_manager->idle = true;															
						}		
						else error = string("MOVING_TREE_SUCCESSFUL");		
						
					} //else: valid server == true
			} //else: source server != target server

			end: 


			if (strcmp(error.c_str(), "MOVING_TREE_SUCCESSFUL") != 0)
			{
				log("Load balancer is idle...");
			}

			//write error/success message to file in order to inform adminTool about tree move result
			file_stream = (char*) malloc(error.length());				
			memcpy(file_stream, error.c_str(), error.length());				
			fwrite (file_stream, 1, error.length(), afile);						
			fclose(afile);						
															
			delete [] ip;
			delete [] cport;
			delete [] path;	
			
			remove(STATIC_MOVE_PARAMS);	

			

		}//file exists			
	} //while
}




