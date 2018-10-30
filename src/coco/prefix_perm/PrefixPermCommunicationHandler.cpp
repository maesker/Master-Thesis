/*
 * PrefixPermCommunicationHandler.cpp
 *
 *  Created on: Sep 19, 2011
 *      Author: sandeep
 *
 * @file PrefixPermCommunicationHandler.cpp
 * @class PermCommunicationHandler
 * @author Sandeep Korrapati, sandy.4k@gmail.com
 * @date 19.09.11
 *
 * @brief PermCommunicationHandler is utilized by the prefix-permissions module
 * 			(1) to forward messages received from the internal ganesha module to the target MDS.
 * 			(2) to handle the messages received from another MDS and inturn them forward them to the prefix-permissions module implemented in ganesha.
 *
 * PermCommunicationHandler class is used by the prefix-permissions module, to forward the message sent by it from the ganesha module to the
 * target MDS specified in the message-data. And at the target MDS it retrieves the message received from the communication handler and forwards
 * it to the Ganesha module to invoke the respective method.
 */

#include "coco/prefix_perm/PrefixPermCommunicationHandler.h"
#include "coco/prefix_perm/PrefixPermGanComm.h"


/**
 * @brief Singleton implementation of PermCommunicationHandler.
 */
PermCommunicationHandler* PermCommunicationHandler::__instance = NULL;

/**
 *
 * TODO: location of the log file has to be changed
 */
PermCommunicationHandler::PermCommunicationHandler() : CommunicationHandler()
{
	// TODO Auto-generated constructor stub
	log = new Logger();
    log->set_log_level( 0 );
    log->set_console_output( 0 );
//    TODO
    std::string log_location = "/tmp/pp_comm_log";
    log->set_log_location( log_location );

    ps_profiler = Pc2fsProfiler::get_instance();
}

PermCommunicationHandler::~PermCommunicationHandler()
{
	// TODO Auto-generated destructor stub
}


void PermCommunicationHandler::create_instance()
{
	if ( __instance == NULL )
	{
		__instance = new PermCommunicationHandler;
	}
}

PermCommunicationHandler* PermCommunicationHandler::get_instance()
{
		assert(__instance);
		return __instance;
}


/**
 * @brief Retrieves the  singleton instance of PrefixPermGanComm.
 * @return instance of PrefixPermGanComm
 */
PrefixPermGanComm* PermCommunicationHandler::get_com_handler()
{
	PrefixPermGanComm::create_instance();
	PrefixPermGanComm *com_handler = PrefixPermGanComm::get_instance();
	return com_handler;
}


/**
 * @brief: Handles the messages received from another MDS and forwards it to Ganesha.
 * @param message [IN] data retrieved by the communication handler.
 */
void PermCommunicationHandler::handle_request(ExtractedMessage& message)
{
	ps_profiler->function_start();
	switch(message.sending_module)
	{
		//message comes from AdminOp module
		case COM_AdminOp:
		{

			break;

		}

		case COM_LoadBalancing:
		{
			break;
		}

		case COM_DistributedAtomicOp:
		{
			break;
		}

		case COM_PrefixPerm:
		{
			log->debug_log("Prefix Permission Message arrived from server: %s", message.sending_server.c_str());
			PermMessage *incoming_message;
			incoming_message = (PermMessage*) message.data;

			if (incoming_message->message_type == POPULATE_PREFIX_PERM)
			{
				//Received message is of type Populate-Prefix perm, forward it to ganesha
				struct MDSPopulatePrefixPermission rcv_msg;
				memcpy(&rcv_msg, message.data, sizeof(struct MDSPopulatePrefixPermission));

				ps_profiler->function_sleep();
				RequestResult reply = this->get_com_handler()->ganesha_send((void*) &rcv_msg, sizeof(MDSPopulatePrefixPermission));
				ps_profiler->function_wakeup();

			}
			else if (incoming_message->message_type == UPDATE_PREFIX_PERM)
			{
				//Received message is of type Update-Prefix perm, forward it to ganesha
				struct MDSUpdatePrefixPermission rcv_msg;
				memcpy(&rcv_msg, message.data, sizeof(struct MDSUpdatePrefixPermission));

				ps_profiler->function_sleep();
				RequestResult reply = this->get_com_handler()->ganesha_send((void*) &rcv_msg, sizeof(MDSUpdatePrefixPermission));
				ps_profiler->function_wakeup();
			}

			break;
		}

		default:
		{
			log->debug_log("Invalid message type arrived from server: %s", message.sending_server.c_str());
			break;
		}
	}

	free(message.data);
	ps_profiler->function_end();
}
