/**
 * @file MDistributionManager.h
 *
 * @date 5 May 2011
 * @author Denis Dridger
 *
 *
 * @brief The main module for metadata distribution.
 * Invokes rebalancing procedures and communicates with external modules.
 *
 * The Metadata Distribution Manager monitors the load of an MDS,
 * and invokes rebalancing procedures if necessary.
 * Therefore it creates the corresponding manager (LoadManager, HotspotManager, LargeDirManager)
 * and invokes its rebalance() procedure.
 */


#ifndef MDISTRIBUTIONMANAGER_H_
#define MDISTRIBUTIONMANAGER_H_


#include "LoadManager.h"
#include "HotSpotManager.h"
#include "LBCommunicationHandler.h"
#include "mm/storage/StorageAbstractionLayer.h"
//#include "LoadMetricProvider.h"


class MDistributionManager
{
public:
	MDistributionManager(StorageAbstractionLayer *storage_abstraction_layer);
	virtual ~MDistributionManager();

	static void create_instance(StorageAbstractionLayer *storage_abstraction_layer);

	static MDistributionManager *get_instance();

	void run();

	//used ONLY for test cases
	LoadManager * testLoadManager();
	HotSpotManager * testHotSpotManager();

	void static_tree_move_listener(); //public only to be started from thread
private:
	
	StorageAbstractionLayer *storage_abstraction_layer;

	LBCommunicationHandler *com_handler;

	LoadManager *load_manager;

	HotSpotManager *hot_spot_manager; 	

	//individual rebalancing procedures
	void manage_workload();
	void manage_hotspots();
	void manage_large_directories();
	
	static MDistributionManager* __instance;

	bool mds_overloaded();

};

#endif /* MDISTRIBUTIONMANAGER_H_ */
