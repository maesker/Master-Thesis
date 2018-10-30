/**
 * @file DaoHandler.cpp
 *
 * @date 4 Sept 2011
 * @author Denis Dridger
 *
 *
 * @brief Listener which observes the DistibutedAtomicOperation queue and executes found commands
 * 
 */


#ifndef DAOHANDLER_H_
#define DAOHANDLER_H_

#include "../communication/CommunicationHandler.h"
#include "LoadManager.h"
#include "../coordination/MltHandler.h"
#include "coco/loadbalancing/LBLogger.h"	
#include "coco/loadbalancing/CommonLBTypes.h"	
#include "coco/dao/CommonDAOTypes.h"               
#include "coco/dao/DAOAdapter.h"           
#include "coco/dao/DistributedAtomicOperations.h"


class DaoHandler: public DAOAdapter
{
public:
	DaoHandler();

	virtual ~DaoHandler();	
		
	static bool is_coordinator(DAOperation* uncomplete_operation);
	
	static int set_sending_addresses(DAOperation* uncomplete_operation);

	static int set_subtree_entry_point(DAOperation* uncomplete_operation);
	
private:
	LoadManager *load_manager;

	MltHandler *mlt_handler;

	void  handle_operation_result(uint64_t  id,  bool  success);

	bool  handle_operation_request(uint64_t  id,  void*  operation_description,  uint32_t  operation_length); 	

	bool  my_handle_operation_request(uint64_t  id,  void*  operation_description,  uint32_t  operation_length); 	

	bool handle_operation_rerequest(uint64_t id, void* operation_description, uint32_t operation_length);

	bool handle_operation_undo_request(uint64_t id, void* operation_description, uint32_t operation_length);

	bool handle_operation_reundo_request(uint64_t id, void* operation_description, uint32_t operation_length);

	bool my_handle_operation_undo_request(uint64_t id, void* operation_description, uint32_t operation_length);

	//not needed right now
	Subtree get_next_participant(void* operation, uint32_t operation_length) {};	
	
	bool is_initiator();

	//need to remember tree move properties globaly
	InodeNumber inode;
	SubtreeChange subtree_change;
	Server target_server;
	string path;
	
};

#endif /* DAOHANDLER */
