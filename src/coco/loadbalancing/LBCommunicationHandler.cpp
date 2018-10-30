/**
 * @file LBCommunicationHandler.cpp
 * @class LBCommunicationHandler
 * @date 25 May 2011
 * @author Denis Dridger
 *
 *
 * @brief Load-Balancing-Communication-Handler is responsible for handling incoming messages
 * to the Loadbalancing module.
 */

#include <stdio.h>
#include <stdint.h>
#include <cstdlib>
#include <sstream>
#include <cstddef>
#include <cassert>
#include <string.h>

#include "coco/loadbalancing/LBCommunicationHandler.h"
#include "coco/loadbalancing/CommonLBTypes.h"
#include "coco/loadbalancing/Sensors.h"
#include "coco/loadbalancing/LBLogger.h"
#include "coco/loadbalancing/LoadManager.h"
#include "coco/loadbalancing/LBConf.h"


LBCommunicationHandler *LBCommunicationHandler::__instance = NULL;


LBCommunicationHandler::LBCommunicationHandler()
{
	//get access to MLT functions
	this->mlt_handler = &(MltHandler::get_instance());
	//this->load_manager = LoadManager::get_instance();
}

LBCommunicationHandler::~LBCommunicationHandler()
{
	// TODO Auto-generated destructor stub
}

void LBCommunicationHandler::create_instance()
{
	if ( __instance == NULL )
		__instance = new LBCommunicationHandler;
}

LBCommunicationHandler *LBCommunicationHandler::get_instance()
{
		assert(__instance);
		return __instance;
}

/**
 * @brief handles messages sent to Loadbalancing module
 * @param message[in] the message send from an other module
 */
void LBCommunicationHandler::handle_request(ExtractedMessage& message)
{	
	prof_start();

	switch(message.sending_module)
	{
		//message comes from Loadbalancing module
		case COM_LoadBalancing:
		{			
			log2("Message arrived from Loadbalancing module of server: %s", message.sending_server.c_str());

			//convert data of incoming message to a LBMessage
			LBMessage *incoming_message = (LBMessage*) message.data;
			
			//handle load request message
			if (incoming_message->message_type == LOAD_REQUEST)
			{
				log("Message type: LOAD_REQUEST");
				log("Returning load of this server...");
				ServerLoadMessage *load = new ServerLoadMessage; //need to store it on heap
				load->cpu_load = Sensors::get_average_load(); 
				load->swaps = Sensors::get_ganesha_swapping(this);
				load->threads = Sensors::get_no_of_ganesha_threads();
				load->normalized_load = Sensors::normalize_load(load->cpu_load, load->swaps, load->threads);				

				reply(message.message_id, (void*) load, sizeof(ServerLoadMessage), message.sending_module, message.sending_server);

				free(load);				
			}

			//remove subtree data (physical files) from storage
			if (incoming_message->message_type == PHYS_TREE_REMOVAL_REQUEST)
			{
				log("Message type: TREE_REMOVE_REQUEST");
				log("Other server accomplished copying data to its storage. Removing physical tree data from disk...");
				//LoadManager::get_instance()->remove_subtree_from_storage_physical();								
				LoadManager::get_instance()->migration_done = true;
			}

			//MLT update request (not used if shared storage for MLT is available)
			if ((incoming_message->message_type == UPDATE_PARTITION_OWNER) || (incoming_message->message_type == CREATE_NEW_PARTITION))
			{
				//contains information needed to update the MLT			
				MLTUpdate *mlt_update;		
				mlt_update = (MLTUpdate*) message.data;	
				
				//indicates whether MLT update worked properly, where 1 = failure, 0 = success
				int32_t mlt_update_success;

				//create server from address:port
				Server asyn_tcp_server;
				asyn_tcp_server.address = string(mlt_update->server_address);
				asyn_tcp_server.port = mlt_update->server_port;

				SubtreeChange subtree_change;

				//change inode owner
				if (incoming_message->message_type == UPDATE_PARTITION_OWNER)			
				{											
					log("Message type: 'UPDATE_PARTITION_OWNER'");							
					log3("Updating MLT: setting MDS %s as new owner of inode %li ...", mlt_update->server_address, mlt_update->inode);					
					subtree_change = FULL_SUBTREE_MOVE;
					
					//mlt_update_success = LoadManager::get_instance()->update_local_mlt(mlt_update->inode, server, subtree_change);			
										
					mlt_update_success = mlt_handler->update_entry(mlt_update->inode, asyn_tcp_server); 			 					
					mlt_handler->write_to_file(MLT_PATH);					
				}

				//create new partition on target server
				if (incoming_message->message_type == CREATE_NEW_PARTITION)
				{
					
					log("Message type: 'CREATE_NEW_PARTITION'");			
					log3("Updating MLT: creating new partition with root inode %li on MDS %s ...", mlt_update->inode, mlt_update->server_address);					
					subtree_change = PARTIAL_SUBTREE_MOVE;			
										
					mlt_update_success = mlt_handler->handle_add_new_entry(asyn_tcp_server, 0, mlt_update->inode, mlt_update->path);
					mlt_handler->write_to_file(MLT_PATH); 					
				}	

				
				//inform sender about success/failure
				reply(message.message_id, (void*) &mlt_update_success, sizeof(int32_t), message.sending_module, message.sending_server);

				if (mlt_update_success == 0)
				{
					log("MLT update done!");	
					log("Rebalancing process finished sucessfully. Load balancer is idle...");
				}
				else	
				{
					log("Error: MLT update failed!!!");
				}			
			}								

			break;
		}		

		default:
		{
			log("Message type: UNKNOWN");
			break;
		}
	}


	free(message.data);

	prof_end();
}

