/**
 * @file AdminCommunicationHandler.h
 * @date 17 Jun 2011
 * @author Denis Dridger
 * @brief AdminCommunicationHandler broadcasts the MLT to all servers if such command was invoked by the adminTool
 */


#include "coco/communication/CommunicationHandler.h"
#include "coco/coordination/MltHandler.h"
#include "coco/loadbalancing/LoadManager.h"
#include "coco/loadbalancing/LBLogger.h"

#ifndef ADMINCOMMUNICATIONHANDLER_H_
#define ADMINCOMMUNICATIONHANDLER_H_

#define ADMIN_MESSAGE_SIZE 128

enum AdminMessageType {
					SERVER_ADDITION_GET_MLT_SIZE, 
					SERVER_ADDITION_GET_MLT_DATA, 
					SERVER_ADDITION_BROADCAST, 
					SERVER_ADDITION_CONFIRM,
					SERVER_REMOVAL_INIT, 
					SERVER_REMOVAL_BROADCAST
				};
struct AdminMessage
{
	AdminMessageType message_type;
	char data[ADMIN_MESSAGE_SIZE];
};


class AdminCommunicationHandler: public CommunicationHandler
{
	public :

	AdminCommunicationHandler();
	virtual ~AdminCommunicationHandler();

	void handle_request(ExtractedMessage& message);

	static void create_instance();

	static AdminCommunicationHandler* get_instance();

	void check_for_mlt_updates(); //not for use from outside

	private:

	static AdminCommunicationHandler* __instance;

	MltHandler* mlt_handler;	

};




#endif /* ADMINCOMMUNICATIONHANDLER_H_ */
