/*
 * Test.cpp
 *
 *  Created on: 09 August 2011
 *      Author: Denis Dridger
 *
 *
 * @brief because gtest+sconscript sucks
 * 
 */

#include "coco/loadbalancing/LoadBalancingTest.h"

//LoadBalancingTest::LoadBalancingTest()
//{
//	printf(" \n");
//	printf("**************************************************** \n");
//	printf("************** LOADBALANCER TESTRUN **************** \n");
//	printf("**************************************************** \n");
//	printf(" \n");
//
//
//	LoadManager::create_instance();
//	this->load_manager = LoadManager::get_instance();
//
//	HotSpotManager::create_instance();
//	this->hot_spot_manager = HotSpotManager::get_instance();
//
//
//	//fill mlt
//	mlt_handler = &(MltHandler::get_instance());
//	mlt_handler->add_server("127.0.0.1", 2222);
//	mlt_handler->add_server("127.0.0.1", 3333);
//
//}
//
//LoadBalancingTest::~LoadBalancingTest()
//{
//}
//
//
//void LoadBalancingTest::testLoadManager()
//{
//	if (this->test_get_cluster_load() == 0) printf("test_get_cluster_load() PASSED \n");
//	else printf("test_get_cluster_load() FAILED \n");
//
//	if (this->test_update_local_mlt() == 0) printf("test_update_local_mlt() PASSED \n");
//	else printf("test_update_local_mlt() FAILED \n");
//
//	if (this->test_broadcast_mlt() == 0) printf("test_broadcast_mlt() PASSED \n");
//	else printf("test_broadcast_mlt() FAILED \n");
//
//	//if (this->test_start_dao() == 0) printf("test_start_dao() PASSED \n");
//	//else printf("test_start_dao() FAILED \n");
//
//	if (this->test_update_storage() == 0) printf("test_update_storage() PASSED \n");
//	else printf("test_update_storage() FAILED \n");
//
//
//	sleep(1);
//	printf(" \n");
//	printf("**************************************************** \n");
//	printf("************ LOADBALANCER TESTRUN DONE ************* \n");
//	printf("**************************************************** \n");
//	printf(" \n");
//}
//
//int LoadBalancingTest::test_get_cluster_load()
//{
//	log("begin test_get_cluster_load()");
//
//	int result;
//	vector <Server> server_list;
//	mlt_handler->get_server_list(server_list);
//	vector <ServerLoad> cluster_load;
//
//	log("Requesting load of other servers...");
//	result = load_manager->get_cluster_load(server_list, cluster_load);
//
////	cout << cluster_load.at(1).server.port << endl;
//	//TODO test results here somehow, but it should work
//
//	log("end test_get_cluster_load()");
//	return result;
//}
//
//int LoadBalancingTest::test_update_local_mlt()
//{
//	log("begin test_update_local_mlt()");
//
//	int result;
//	InodeNumber inode = 1;
//	vector <Server> server_list;
//	mlt_handler->get_server_list(server_list);
//	result = load_manager->update_local_mlt(inode, server_list.at(0), FULL_SUBTREE_MOVE);
//	result = load_manager->update_local_mlt(inode, server_list.at(0), PARTIAL_SUBTREE_MOVE);
//	//TODO test whether subtree part is actually moved (?)
//
//	log("end test_update_local_mlt()");
//
//	return result;
//}
//
//int LoadBalancingTest::test_broadcast_mlt()
//{
//	log("begin test_broadcast_mlt()");
//
//	int result;
//
//	InodeNumber inode = 1;
//	vector <Server> server_list;
//	mlt_handler->get_server_list(server_list);
//
//	result = load_manager->broadcast_mlt(inode, server_list.at(0), FULL_SUBTREE_MOVE);
//	result = load_manager->broadcast_mlt(inode, server_list.at(0), PARTIAL_SUBTREE_MOVE);
//
//	//TODO look inside MLT and check whether everything is as it should be
//
//	log("end test_broadcast_mlt()");
//
//	return result;
//}
//
//int LoadBalancingTest::test_start_dao()
//{
//
//	vector <Server> server_list;
//	mlt_handler->get_server_list(server_list);
//
//	//need to store it on heap
//	DAOSubtreeMove* operation = new DAOSubtreeMove;
//
//	DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
//	DAOSubtreeMove subtree_move;
//	operation->inode = 7;
//	//operation->subtree_change = PARTIAL_SUBTREE_MOVE;
//	operation->subtree_change = PARTIAL_SUBTREE_MOVE;
//
//
//	//define participant (the subtree-receiving server)
//	vector<Subtree> participants;
//    	Subtree tree;
//    	tree.server = server_list.at(0).address;
//    	tree.subtree_entry = 0; //wat dat?
//    	participants.push_back(tree);
//
//	//start atomic operation
//    	dao.start_da_operation(operation, sizeof(operation), MoveSubtree, participants, 0);
//
//
//	//TODO check result
//
//	log("end test_start_dao()");
//
//
//	return 0;
//
//}
//
//int LoadBalancingTest::test_update_storage()
//{
//	//InodeNumber some_inode = 42;
//	//load_manager->remove_subtree_from_storage(42);
//
//	//TODO
//	return 0;
//
//}
//
//void LoadBalancingTest::testHotSpotManager()
//{
//
//}
