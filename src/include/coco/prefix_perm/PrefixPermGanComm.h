/*
 * @file PrefixPermGanComm.h
 * @author Sandeep Korrapati, sandy.4k@gmail.com
 * @date 19.09.11
 */

#ifndef PREFIXPERMGANCOMM_H_
#define PREFIXPERMGANCOMM_H_

#include "../communication/CommunicationHandler.h"
#include "../coordination/MltHandler.h"

class PrefixPermGanComm: public CommunicationHandler
{
public:

	PrefixPermGanComm();
	virtual ~PrefixPermGanComm();

	static void create_instance();
	static PrefixPermGanComm *get_instance();

	void handle_request(ExtractedMessage& message);

private:
	MltHandler *mlt_handler;

	static PrefixPermGanComm* __instance;

};


#endif /* PREFIXPERMGANCOMM_H_ */
