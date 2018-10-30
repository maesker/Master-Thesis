/**
 * @file AdminCommunicationHandler.cpp
 * @class AdminCommunicationHandler
 * @date 17 Jun 2011
 * @author Denis Dridger
 * @brief AdminCommunicationHandler broadcasts the MLT to all servers if such command was invoked by the adminTool
 */

#include "coco/administration/AdminCommunicationHandler.h"
#include "coco/loadbalancing/LoadManager.h"
#include "coco/loadbalancing/LBLogger.h"
#include <cstddef>
#include <cassert>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mm/mds/MetadataServer.h"

//paths must be the same as in AdminOperations::add_mds()
#define TMP_MLT_PATH "/tmp/temp_mlt"
#define TMP_MLT_PATH2 "/tmp/temp_mlt2"


/**
 * @brief Checks for new servers in MLT
 * @param[in] pntr unused void pointer
 * */
void* run_mlt_listener(void* com_handler)
{    
   AdminCommunicationHandler::get_instance()->check_for_mlt_updates();
}

AdminCommunicationHandler* AdminCommunicationHandler::__instance = NULL;

/**
 * @brief init listener thread for Administration Communication Handler
 * */
AdminCommunicationHandler::AdminCommunicationHandler()
{
	remove(TMP_MLT_PATH);	
	pthread_t thread;
        pthread_create(&thread, NULL, run_mlt_listener, NULL);        
	this->mlt_handler = &(MltHandler::get_instance());
	
}

AdminCommunicationHandler::~AdminCommunicationHandler()
{
	// TODO Auto-generated destructor stub
}


void AdminCommunicationHandler::create_instance()
{
	if ( __instance == NULL )
	{
		__instance = new AdminCommunicationHandler;
	}
}

AdminCommunicationHandler* AdminCommunicationHandler::get_instance()
{
		assert(__instance);
		return __instance;
}



/**
* @brief checks whether a new temp MLT file exists, if so MLT is broadcasted to all servers found in it
* */
void AdminCommunicationHandler::check_for_mlt_updates()
{
	while(1)
	{		
		sleep(2);

		if (FILE * fp = fopen(TMP_MLT_PATH, "rb")) 
		{
			cout << "adminTool invoked MLT update procedure. Sending MLT update to other servers..." << endl;	

			//determine MLT's size and copy its contents to buffer
    			int prev = ftell(fp);
    			fseek(fp, 0L, SEEK_END);
    			int mlt_size = ftell(fp);
    			fseek(fp,prev,SEEK_SET);	
			char *file_stream;  
			file_stream = (char*) malloc(mlt_size); 
			fread(file_stream, 1, mlt_size, fp);
			fclose(fp);
			
			vector <string> server_list_only_ips;
			vector <Server> server_list;
			
			//get servers to send the update to			
			if (this->mlt_handler->reload_mlt(TMP_MLT_PATH) != 0)
			{
				cout << "MLT update broadcast error: cannot read temp MLT." << endl;
			}
			this->mlt_handler->get_other_servers(server_list);
			
			//grab only the ips from server_list, since request_reply() expects a string list of ips
			cout << "MLT update sent to:" << endl;	
			for (int j = 0; j < server_list.size(); j++)
			{				
				//re-register all servers at communication module
				SendCommunicationServer::get_instance().add_server_send_socket(server_list[j].address);

				//add only ip addresses of all other servers to list
				server_list_only_ips.push_back(server_list[j].address);		
				cout << server_list[j].address << endl;	
			}	

			//some message id to be used internal by the communication module
			uint64_t message_id;

			//message list that shall store the message sent by the other server
			vector<ExtractedMessage> message_list;
		
			//send copy of local MLT to all cluster members
			int ret = this->request_reply(&message_id, (void*) file_stream, mlt_size, COM_AdminOp, server_list_only_ips, message_list);						

			//sleep(5);		

			//if ( (this->receive_reply(message_id, 5000) != 0))
			//{			
			//	//cout << "Warning: at least one server didn't reply to MLT update request, " << endl;
			//	sleep(5);
				
			//}

										
			sleep(3);
			cout << "MLT update finished. Removing temp file" << endl;	
			remove(TMP_MLT_PATH);

						

		}		
	}
}


/**
* @brief receives incoming messages from other modules and reacts corresponding to message type 
* @param[in] incoming message from AdminCommunicationHandler of an other server 
* */
void AdminCommunicationHandler::handle_request(ExtractedMessage& message)
{
	
	cout << "Received MLT update request from server " << message.sending_server << "."<<  endl;						

	switch(message.sending_module)
	{
		//message comes from AdminOp module
		case COM_AdminOp:
		{
			int32_t success = 0;

			char *file_stream;
			file_stream = (char*) malloc(message.data_length);	
			memcpy(file_stream, (char*) message.data, message.data_length);
	
			//create temp MLT file on disk
			FILE *mlt_file;	
			mlt_file = fopen(TMP_MLT_PATH2,"wb");

			if (mlt_file == NULL)
			{				
				cout << "MLT update broadcast error: cannot create temp MLT. " << endl;						
				success = -1;
			}
			else
			{	
				fwrite (file_stream, 1, message.data_length, mlt_file);			
			}

			fclose(mlt_file);
			
			//read temp MLT and store it to real MLT
			//this->mlt_handler->destroy_mlt();
			//if (this->mlt_handler->read_from_file(TMP_MLT_PATH2) != 0) 
			if (this->mlt_handler->reload_mlt(TMP_MLT_PATH2) != 0)
			{
				cout << "MLT update broadcast error: cannot read temp MLT." << endl;
				success = -2;

			}
			
			vector <Server> server_list;
			this->mlt_handler->get_server_list(server_list);
			
			//re-register all servers at communication module			
			for (int j = 0; j < server_list.size(); j++)	
			{						
				SendCommunicationServer::get_instance().add_server_send_socket(server_list[j].address);				
			}

			if (this->mlt_handler->write_to_file(MLT_PATH) != 0) 
			{
				cout << "MLT update broadcast error: cannot write MLT." << endl;
				success = -3;
			}
			
			remove(TMP_MLT_PATH2);			
			//remove(TMP_MLT_PATH);	
			free(file_stream);
			
			if (LoadManager::get_instance()->migration_done)
			{		
				log("Updating ganesha configuration...");
				int ganesha_update_result = MetadataServer::get_instance()->handle_ganesha_configuration_update();				
				if (ganesha_update_result != 0)
				{
					log2("Error updating Ganesha configuration (error code:  %i) ", ganesha_update_result);			
				}
				else
				{
					log("Done.");
				}
			}
	
			//notify sender that its done sending empty message
			reply(message.message_id, &success, sizeof(int32_t), message.sending_module, message.sending_server);

			break;

		}		


		default:
		{
			break;
		}
	}

    free(message.data);
}



