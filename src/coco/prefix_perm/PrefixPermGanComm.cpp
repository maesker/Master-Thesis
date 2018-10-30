/*
 * PrefixPermGanComm.cpp
 *
 *  Created on: Sep 19, 2011
 *      Author: sandeep
 *
 * @file PrefixPermGanComm.cpp
 * @class PrefixPermGanComm
 * @author Sandeep Korrapati, sandy.4k@gmail.com
 * @date 19.09.11
 *
 * @brief PrefixPermGanComm is utilized by PermCommunicationHandler to forward the messages received from other MDS to the
 * 			prefix-permissions module implemented in the internal Ganesha.
 */

#include "coco/prefix_perm/PrefixPermGanComm.h"

/**
 * @brief Singleton implementation of PrefixPermGanComm.
 */
PrefixPermGanComm *PrefixPermGanComm::__instance = NULL;


PrefixPermGanComm::PrefixPermGanComm()
{
	//get access to MLT functions
	this->mlt_handler = &(MltHandler::get_instance());
}

PrefixPermGanComm::~PrefixPermGanComm()
{
	// TODO Auto-generated destructor stub
}

void PrefixPermGanComm::create_instance()
{
	if ( __instance == NULL )
		__instance = new PrefixPermGanComm;
}

PrefixPermGanComm *PrefixPermGanComm::get_instance()
{
		assert(__instance);
		return __instance;
}

/**
 * @brief handles messages sent to Prefix-Permissions module
 * @param message [IN] data retrieved by the communication handler.
 */
void PrefixPermGanComm::handle_request(ExtractedMessage& message)
{
	switch(message.sending_module)
	{
		case COM_PrefixPerm:
		{
			break;
		}
		default:
		{
			break;
		}
	}

    free(message.data);
}
