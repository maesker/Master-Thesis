/**
 * @file Balancer.h
 * @date 5 May 2011
 * @author Denis Dridger
 *
 * @brief Balancer is the super class of LoadManager and HotSpotManager
 * it provides common functionalities needed in both managers.
 *
 * Main functionalities are:
 * - determining own workload
 * - requesting workload of other servers 
 * - determining  best suitable servers to share load 
 * - updating metadata lookup table (MLT)
 *
 */

#ifndef BALANCER_H_
#define BALANCER_H_

#include <cassert>
#include "CommonLBTypes.h"
#include "LBCommunicationHandler.h"
#include "coco/coordination/MltHandler.h"
#include "global_types.h"



class Balancer
{
public:
	Balancer();
	virtual ~Balancer();

	//methods used by all load balancing managers (inherited)
	ServerLoadMessage get_own_load();
	
	//void get_target_mds(vector<Server>& server_list, vector<string>& target_servers, float w_cpu, float w_swaps, float w_threads);

	int get_cluster_load(vector<Server>& server_list, vector<PossibleTargets>& server_load);

	int get_best_target_server(vector<PossibleTargets>& cluster_load, Server& asyn_tcp_server);

	//in Sensors now
	//float normalize_load(float cpu_load, int swaps, int threads);	
	
	MltHandler *get_mlt_handler();

	LBCommunicationHandler *get_com_handler();	

	int move_subtree();

	int broadcast_mlt(InodeNumber inode, string path, Server asyn_tcp_server, SubtreeChange subtree_change);
	
	int update_local_mlt(InodeNumber inode, string path, Server asyn_tcp_server, SubtreeChange subtree_change);

	InodeNumber get_root_of_partition(InodeNumber some_inode);

	int get_path(InodeNumber some_inode, string& path);
	
		
private:

//	LBCommunicationHandler *com_handler;		
	

	//MltHandler *mlt_handler;

	
	

		
};

#endif /* BALANCER_H_ */
