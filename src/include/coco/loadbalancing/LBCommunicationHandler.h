/**
 * @file LBCommunicationHandler.cpp
 *
 * @date 25 May 2011
 * @author Denis Dridger
 *
 *
 * @brief Load-Balancing-Communication-Handler is responsible for handling incoming messages
 * to the Loadbalancing module.
 */


#ifndef LBCOMMUNICATIONHANDLER_H_
#define LBCOMMUNICATIONHANDLER_H_

#include "../communication/CommunicationHandler.h"
#include "../coordination/MltHandler.h"

class LBCommunicationHandler: public CommunicationHandler
{
public:

	LBCommunicationHandler();
	virtual ~LBCommunicationHandler();

	static void create_instance();
	static LBCommunicationHandler *get_instance();	

	void handle_request(ExtractedMessage& message);

private:
	MltHandler *mlt_handler;
	
	static LBCommunicationHandler* __instance;

};

#endif /* LBCOMMUNICATIONHANDLER_H_ */
