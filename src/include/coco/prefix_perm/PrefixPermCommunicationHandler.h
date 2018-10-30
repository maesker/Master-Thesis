/*
 * @file PrefixPermCommunicationHandler.h
 * @author Sandeep Korrapati, sandy.4k@gmail.com
 * @date 19.09.11
 */

#ifndef PERMCOMMUNICATIONHANDLER_H_
#define PERMCOMMUNICATIONHANDLER_H_

#include "PrefixPermGanComm.h"
#include "message_types.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"


struct PermMessage
{
	ganesha_request_types_t message_type;
	char * data;
};

class PermCommunicationHandler: public CommunicationHandler
{
	public :

	PermCommunicationHandler();
	virtual ~PermCommunicationHandler();

	void handle_request(ExtractedMessage& message);

	static void create_instance();

	static PermCommunicationHandler* get_instance();

	PrefixPermGanComm* get_com_handler();

	Logger *log;

	private:

	static PermCommunicationHandler* __instance;

	Pc2fsProfiler* ps_profiler;
};


#endif /* PERMCOMMUNICATIONHANDLER_H_ */
