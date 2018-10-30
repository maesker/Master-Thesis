/*
 * HotSpotManager.cpp
 *
 *  Created on: 15 July 2011
 *      Author: Denis Dridger
 *
 *
 * @brief HotSpotManager is responsible for creating, maintaining and reclaiming 
 * directories, which are replicated to other servers due to very high access numbers
 *
 */


#ifndef HOTSPOTMANAGER_H_
#define HOTSPOTMANAGER_H_

#include "Balancer.h"



class HotSpotManager: public Balancer
{
public:
	HotSpotManager();
	virtual ~HotSpotManager();

	static void create_instance();

	static HotSpotManager* get_instance();

	int manage_hotspots();

	int try_reclaim_replica();	

	//LBCommunicationHandler *com_handler;

private:
	static HotSpotManager* __instance;

	int try_create_replica();
	
	
};

#endif /* HOTSPOTMANAGER_H_ */

