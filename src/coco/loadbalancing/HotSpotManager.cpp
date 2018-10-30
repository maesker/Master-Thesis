/*
 * HotSpotManager.cpp //NOT IMPLEMENTED CLASS
 *
 *  Created on: 15 July 2011
 *      Author: Denis Dridger
 *
 *
 * @brief HotSpotManager is responsible for creating, maintaining and reclaiming 
 * directories, which are replicated to other servers due to very high access numbers
 */

#include "coco/loadbalancing/HotSpotManager.h"

#include <cstddef>
#include "coco/loadbalancing/LBLogger.h"
#include "coco/loadbalancing/LBConf.h"
#include "coco/loadbalancing/LBErrorCodes.h"


HotSpotManager* HotSpotManager::__instance = NULL;

HotSpotManager::HotSpotManager() 
{
		
}

HotSpotManager::~HotSpotManager() 
{
	
}

void HotSpotManager::create_instance()
{
	if ( __instance == NULL )
	{
		__instance = new HotSpotManager();
	}
}

HotSpotManager* HotSpotManager::get_instance()
{
		assert(__instance);
		return __instance;
}

int HotSpotManager::manage_hotspots()
{
	vector <string> target_servers;

	vector <Server> server_list;
	get_mlt_handler()->get_server_list(server_list);
	
	//determine server(s) where to copy highly loaded directories / subtrees	
	//TODO get cluster load here	
	//this->get_target_mds(server_list, target_servers, W_CPU, W_SWAPS, W_THREADS); 

	//if at least one server exist that can take the load then copy the directory / subtrees to it
	if (target_servers.size() > 0)
	{
		log("Target servers do exist!");
		
		//TODO implement this
		this->try_create_replica();		
		
		//TODO update mlt
		/*
		MltChangeKind mlt_change = this->update_mlt(inode, target_servers.at(0));

		if (mlt_change == MLT_CHANGE_ERROR)
		{
			log("Error updating MLT during rebalance attempt. Aborting rebalancing procedure.");
			return MLT_UPDATE_FAILED;
		}
		else
		{
			//tell other servers about the change
			this->broadcast_mlt(mlt_change);
		}

		*/
		return 0;

	}


	//it can't be helped, no server found to hold the replica
	log("Bad luck, no suitable target server found");
	return -1;//LB_ERROR_TARGET_SERVER_NOT_FOUND;

}

int HotSpotManager::try_create_replica()
{
	//TODO all
}

int HotSpotManager::try_reclaim_replica()
{
	//TODO all
}

	


