/**
 * @file DaoHandler.cpp 
 * @date: 4 Sept 2011
 * @author Denis Dridger
 * @class DaoHandler
 *
 * @brief This class handles incoming distributed atomic operations, 
 * at which each participant executes his own part of the operation.
 * Success or failure situations are commited to the DistributedAtomicOperation module, which pervorms the recovery if needed
 * Furthermore failure situations in the Loadbalancing module itself have to be unrolled as well
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <cstdlib>
#include <sstream>
#include <stdlib.h>

#include "coco/loadbalancing/DaoHandler.h"
#include "coco/loadbalancing/LBErrorCodes.h"
#include "coco/loadbalancing/LBConf.h"
#include "mm/mds/MetadataServer.h"


DaoHandler::DaoHandler()
{
	LoadManager::create_instance();
	this->load_manager = LoadManager::get_instance();
	this->mlt_handler = &(MltHandler::get_instance());	
}

//boring
DaoHandler::~DaoHandler()
{	
}

/**
* @brief handles the result of the distributed atomic operation
* A positive result is handled iff both, initiator and participant server succeeded 
* @param id[in] unique ID to identify the operation by the DAO module
* @param success[in] endresult of the distributed atomic operation
* */
void DaoHandler::handle_operation_result(uint64_t  id,  bool  success)
{
	prof_start();

	if(success)
	{			
		log("Participant's part moving tree went successfully !");				
				
		if (SHARED_MLT_USED == 0)
		{
			log("Informing other servers about MLT change...");
			system("./adminTool/adminTool --synch-mlt");
		}			

		log("Updating ganesha configuration...");
		int ganesha_update_result = MetadataServer::get_instance()->handle_ganesha_configuration_update();				
		if (ganesha_update_result != 0)
		{
			log2("Error updating Ganesha configuration (error code:  %i) ", ganesha_update_result);			
		}
		else
		{
			log("Rebalancing process has finished sucessfully !!!");		
			log("Load balancer is idle...");
		}

		/*


		int result = this->load_manager->broadcast_mlt(this->inode, this->path, this->target_server, this->subtree_change);

		if (result != 0)
		{		
			log("Error broadcasting MLT. Warning: this error is not handled yet.");				
		}
		else
		{
			log("All cluster members updated threir MLTs successfully !");			
			log("Rebalancing process has finished sucessfully !!!");
			log("Load balancer is idle...");
		}		
		*/
		

		this->load_manager->idle = true;			
	}	
	else if (this->load_manager->errorcode == 0)	
	{	
		log("Error: participant failed to ingegrate the tree. See it's logfile for details!");
		this->load_manager->errorcode = LB_ERROR_PARTICIPANT_FAILED_TO_INTEGRATE_TREE;
	}

	prof_end();
}


/**
* @brief handles the operation request of a distributed atomic operation
* This is the "entry point" when beginning a distributed atomic operation
* This method is used as a wrapper, inside the actual "handle operation request" method is called.
* Doing this it is possible to call the method several times from this class manually 
* @param id[in] unique ID to identify the operation by the DAO module
* @param operation_description[in] encoded description of the operation to perform
* @param operation_length[in] length of operation_description parameter in bytes
* @return true if handling operation request was successful
* */
bool DaoHandler::handle_operation_request(uint64_t id, void* operation_description, uint32_t operation_length)
{	
	log("Handling atomic operation request...");	
	return this->my_handle_operation_request(id, operation_description,  operation_length);
}


/**
* @brief called if something goes wrong during handle_operation_request, e.g. a timeout
* @param id[in] unique ID to identify the operation by the DAO module
* @param operation_description[in] encoded description of the operation to perform
* @param operation_length[in] length of operation_description parameter in bytes
* @return true if handling operation rerequest was successful
* */
bool DaoHandler::handle_operation_rerequest(uint64_t id, void* operation_description, uint32_t operation_length)
{
	prof_start();

	log("Handling atomic operation RErequest...");	

	log("Undoing already performed operations first...");	
	int result = my_handle_operation_undo_request(id, operation_description, operation_length);
	if(result != 0)
	{
		log("Error undoing already performed operations!");
		log("Aborting with the following error code:");
		log(this->load_manager->error_to_str(result).c_str());
		prof_end();
		return false;
	}	

	log("All operations undone!");
	log("Retrying to perfome the tree move...");
	prof_end();
	return this->my_handle_operation_request(id, operation_description,  operation_length);
}



/**
* @brief undo procedure
* @param id[in] unique ID to identify the operation by the DAO module
* @param operation_description[in] encoded description of the operation to perform
* @param operation_length[in] length of operation_description parameter in bytes
* @return true if handling undo request was successful
* */
bool DaoHandler::handle_operation_undo_request(uint64_t id, void* operation_description, uint32_t operation_length)
{
	prof_start();

	log("Distributed operation failed to execute. Undoing the changes...");

	int result = my_handle_operation_undo_request(id, operation_description, operation_length);
	if(result != 0)
	{
		log("Error undoing already performed operations!");
		log("Aborting with the following error code:");
		log(this->load_manager->error_to_str(result).c_str());
		prof_end();
		return false;
	}
		
	log("All operations undone!");	
	prof_end();
	return true;
} 


/**
* @brief Handles incoming request to perform an undo of the atomic operation.
* @param id[in] unique ID to identify the operation by the DAO module
* @param operation_description[in] encoded description of the operation to perform
* @param operation_length[in] length of operation_description parameter in bytes
* @return true if handling the undo request was successful
* */
bool DaoHandler::my_handle_operation_undo_request(uint64_t id, void* operation_description, uint32_t operation_length)
{
	prof_start();

	DAOSubtreeMove subtree_move = *((DAOSubtreeMove*) operation_description);

	//create Server server from ip and port	
	this->target_server.address = string(subtree_move.receiver_address);
	this->target_server.port = subtree_move.receiver_port;
	
	int result = 0;
	
	if (this->is_initiator())
	{		
		result = this->load_manager->undo_changes_on_initiator_mds(subtree_move.inode, subtree_move.subtree_change, target_server);		
	}
	else
	{
		result = this->load_manager->undo_changes_on_participant_mds(subtree_move.inode, subtree_move.subtree_change, target_server);
	}

	if(result != 0)
	{
		
		log(this->load_manager->error_to_str(result).c_str());
		prof_end();
		return result;			
	}
	prof_end();
	return 0;
}



/**
* @brief Core method of this class. Handles incoming request to perform an atomic operation.
* @param id[in] unique ID to identify the operation by the DAO module
* @param operation_description[in] encoded description of the operation to perform
* @param operation_length[in] length of operation_description parameter in bytes
* @return true if handling the request was successful
* */
bool  DaoHandler::my_handle_operation_request(uint64_t  id,  void*  operation_description,  uint32_t  operation_length)
{
	prof_start();

	DAOSubtreeMove subtree_move = *((DAOSubtreeMove*) operation_description);

	//create Server server from ip and port	
	this->target_server.address = string(subtree_move.receiver_address);
	this->target_server.port = subtree_move.receiver_port;
	
	//remember sender address to send some confirmation replies later
	memcpy(this->load_manager->sender_address, subtree_move.sender_address, 32);

	//needed for MLT broadcast later
	this->inode = subtree_move.inode;
	this->subtree_change = subtree_move.subtree_change;
	//this->path = string(subtree_move.path);
			
	//action of the receiving server (take overloaded tree)
	if (this->is_initiator())
	{
		log2("Going to move tree with inode number: %li", subtree_move.inode);
		if (subtree_move.subtree_change == FULL_SUBTREE_MOVE)
		{			
			log("This inode is the root of a partition => migrating entire partition...");
		}
		else    	
		{
			log("This inode is not the root of a partition => migrating only a part of the partition...");
		}
		
		int send = this->load_manager->dao_send_subtree(subtree_move.inode, subtree_move.subtree_change, this->target_server);

		this->load_manager->errorcode = send;

		if (send != 0)
		{
			//log("Error: Moving subtree failed with the following failure code:");
			//log(this->load_manager->error_to_str(send).c_str());												
			prof_end();
			return false;
		}
		else
		{				

			log("Coordinator's part moving tree went successfully !");
		}
		
	}
	else
	{
		log("This server is supposed to receive a subtree from another server...");
		log2("Going to take over inode: %li", subtree_move.inode);

		//string path;

		if (subtree_move.subtree_change == FULL_SUBTREE_MOVE)
		{			
			log("This inode is the root of a partition => migrating entire partition...");
			//this->path = this->mlt_handler->get_path(inode);
		}
		else    	
		{
			log("This inode is not the root of a partition => migrating only a part of the partition...");
			
			
			/*
			int resolve_path = this->load_manager->get_path(subtree_move.inode, this->path);

			if (resolve_path != 0)
			{
				prof_end();
				log("Error: Receiving subtree failed with the following failure code:")			
				log(this->load_manager->error_to_str(resolve_path).c_str());								
				return false;
			}
			*/
		}			
		

		int receive = this->load_manager->dao_receive_subtree(subtree_move.inode, subtree_move.subtree_change);
		
		if (receive != 0)
		{
			log("Error: Receiving subtree failed with the following failure code:")
			//check which error occured and try again or unroll
			log(this->load_manager->error_to_str(receive).c_str());					
			prof_end();
			return false;
		}
		else
		{

			if(SHARED_MLT_USED)
			{
				log("Updating ganesha configuration...");
				int ganesha_update_result = MetadataServer::get_instance()->handle_ganesha_configuration_update();				
				if (ganesha_update_result != 0)
				{
					log2("Error updating Ganesha configuration (error code:  %i). Aborting the rebalancing procedure...", ganesha_update_result);
					log("Ganesha and MDS need to be restarted");
					//prof_end();
					//return MDS_ERROR_GANESHA_CONFIG_UPDATE_FAILED;
				}
				else
				{
					log("Participant's part moving tree went successfully !");
					this->load_manager->migration_done = true;
				}
			}
			else
			{
				log("Participant's part moving tree went successfully !");			
				this->load_manager->migration_done = true;
			}
		}
		
	}
	prof_end();
	return true;
}



/**
* @brief Handles incoming request to perform an reundo of the atomic operation.
* @param id[in] unique ID to identify the operation by the DAO module
* @param operation_description[in] encoded description of the operation to perform
* @param operation_length[in] length of operation_description parameter in bytes
* @return true if handling the reundo request was successful
* */
bool DaoHandler::handle_operation_reundo_request(uint64_t id, void* operation_description, uint32_t operation_length)
{
	prof_start();

	log("Distributed operation failed to execute. Trying to undo the changes once more...");

	int result = my_handle_operation_undo_request(id, operation_description, operation_length);
	if(result != 0)
	{
		log("Error undoing already performed operations!");
		log("Aborting with the following error code:");
		log(this->load_manager->error_to_str(result).c_str());
		prof_end();
		return false;
	}
		
	log("All operations undone!");	
	prof_end();
	return true;
}


/**
* @brief Determines whether the this server is the initiator of the running distributed atomic operation
* @return true if this server is the initiator of the running distributed atomic operation
* */
bool DaoHandler::is_initiator()
{	
	MltHandler *mlt_handler = &(MltHandler::get_instance());

	if (strcmp(this->target_server.address.c_str(), mlt_handler->get_my_address().address.c_str()) == 0) 
		 return false;
	else return true;
}


/**
* @brief Determines whether the this server is the initiator of the running distributed atomic operation
* This method is similar to the previous one, with the difference that it cannot be called manually
* @param uncomplete_operation[in] encoded description of the operation to perform
* @return true if this server is the initiator of the running distributed atomic operation
* */
bool DaoHandler::is_coordinator(DAOperation* uncomplete_operation)
{
	prof_start();

	DAOSubtreeMove subtree_move = *((DAOSubtreeMove*) uncomplete_operation->operation);

	MltHandler *mlt_handler = &(MltHandler::get_instance());

	prof_end();

	if (strcmp(subtree_move.receiver_address, mlt_handler->get_my_address().address.c_str()) == 0) 
		 return false;
	else return true;
}


/**
* @brief sets address of sender
* Not really needed in the moment
* @param uncomplete_operation[in] encoded description of the operation to perform
* @return true 0 if operation was successful
* */
int DaoHandler::set_sending_addresses(DAOperation* uncomplete_operation)
{
	prof_start();

	DAOSubtreeMove subtree_move = *((DAOSubtreeMove*) uncomplete_operation->operation);
	vector<Subtree> participants;
   	Subtree tree;
   	tree.server = string(subtree_move.receiver_address);
   	tree.subtree_entry = 0; //not needed?
   	participants.push_back(tree);
	uncomplete_operation->participants = participants;
	prof_end();
	return 0;
}

/**
* @brief sets subtree entry point
* Not really needed in the moment
* @param uncomplete_operation[in] encoded description of the operation to perform
* @return true 0 if operation was successful
* */
int DaoHandler::set_subtree_entry_point(DAOperation* uncomplete_operation)
{	
	uncomplete_operation->subtree_entry = 0; //not needed?
	return 0;
}

