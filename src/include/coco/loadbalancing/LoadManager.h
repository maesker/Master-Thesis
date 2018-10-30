/*
 * LoadManager.h
 *
 *  Created on: 5 May 2011
 *      Author: Denis Dridger
 *
 *
 * @brief LoadManager is responsible for moving highly loaded subtrees to other MDS
 *
 * Load manager's procedure is:
 * - determining own workload
 * - requesting workload of all other servers
 * - determining best suitable server(s) where to move the subtree
 * - determining the subtree to move
 * - moving subtree and informing other servers about it
 */


#ifndef LOADMANAGER_H_
#define LOADMANAGER_H_

#include "Balancer.h"
#include "LBCommunicationHandler.h"
#include "mm/storage/StorageAbstractionLayer.h"
#include "global_types.h"

class LoadManager: public Balancer
{
public:
	LoadManager();
	virtual ~LoadManager();

	static void create_instance();

	static LoadManager* get_instance();

	int try_rebalaning();

	int send_subtree_dynamic();

	int send_subtree_static(InodeNumber inode, Server target_server);

	int dao_receive_subtree(InodeNumber inode, SubtreeChange subtree_change);	

	int dao_send_subtree(InodeNumber inode, SubtreeChange subtree_change, Server target_server);

	access_counter_return_t *determine_subtree_to_move();

	int get_suitable_neighbour_server(vector<PossibleTargets>& cluster_load, Server& asyn_tcp_server, InodeNumber inode);

	int remove_subtree_from_storage_logical(InodeNumber inode, SubtreeChange subtree_change);

	int remove_subtree_from_storage_physical();

	int create_subtree_on_storage(InodeNumber inode, SubtreeChange subtree_change);

	bool idle;

	int start_dao(InodeNumber inode, SubtreeChange subtree_change, Server& target_server); //public only for test

	string error_to_str(int errorcode);

	int errorcode;
	
	int atomic_steps_done;

	int undo_mlt_changes();
	
	int undo_changes_on_initiator_mds(InodeNumber inode, SubtreeChange subtree_change, Server target_server);
	
	int undo_changes_on_participant_mds(InodeNumber inode, SubtreeChange subtree_change, Server target_server);

	PhysicalTreeMigration phys_tree_rem;	

	PhysicalTreeMigration phys_tree_copy;	

	void run_physical_data_migrator();

	bool migration_done;

	bool start_migration;

	char sender_address[32];

	string path;
	
private:
	static LoadManager* __instance;
		
	//LBCommunicationHandler *com_handler;

	StorageAbstractionLayer *storage_abstraction_layer;
	
	MltHandler* mlt_handler;	
	

};

#endif /*  LOADMANAGER_H_ */
