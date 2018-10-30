#include <string>
#include <sstream>
#include <stdio.h>
#include "gtest/gtest.h"
#include "coco/loadbalancing/Balancer.h"
#include "coco/loadbalancing/LoadManager.h"
#include "coco/loadbalancing/MDistributionManager.h"
#include "coco/loadbalancing/CommonLBTypes.h"
#include "coco/loadbalancing/LBCommunicationHandler.h"
#include "coco/coordination/MltHandler.h"
#include <string.h>
#include "coco/loadbalancing/LBConf.h"
#include "coco/loadbalancing/LBLogger.h"
#include "coco/loadbalancing/Sensors.h"
#include "coco/loadbalancing/DaoHandler.h"
#include "mm/journal/JournalManager.h"
#include "mm/storage/storage.h"

#define OBJECT_NAME "LBTest"
#define CONFIGFILE "../conf/loadbalancing.conf"

namespace
{


class LBTest: public ::testing::Test
{
protected:
	LBTest()
	{

		//fill mlt
		mlt_handler = &(MltHandler::get_instance());
		mlt_handler->init_new_mlt("127.0.0.1", 49152, 2, 1);
		Server myAddress;
		myAddress.address = "127.0.0.1";
		myAddress.port = 49152;
		mlt_handler->set_my_address(myAddress);

		pthread_t thread1;
		SendCommunicationServer::get_instance().set_up_external_sockets();
		//pthread_create(&thread1, NULL, LBTest::run_receive_server, NULL);
		//set loadbalancing configuration values

		init_storage();
		DistributedAtomicOperations::get_instance().do_recovery();

		LBConf::init(CONFIGFILE);

		//create and init loadbalancing logger
		LBLogger::create_instance();
		LBLogger::get_instance()->init();

		LoadManager::create_instance();
		this->load_manager = LoadManager::get_instance();

		HotSpotManager::create_instance();
		this->hot_spot_manager = HotSpotManager::get_instance();


		LBCommunicationHandler::create_instance();
		this->com_handler = LBCommunicationHandler::get_instance();
		com_handler->init(COM_LoadBalancing);
		com_handler->start();



		 dao = new DaoHandler();
		 dao->set_module(DistributedAtomicOperationModule::DAO_LoadBalancing);


	}

	~LBTest()
	{
	}

	virtual void SetUp()
	{

	}

	virtual void TearDown()
	{
	}

	void init_storage()
	{
		 JournalManager *jm = JournalManager::get_instance();



		// Create and initialize Storage device
		AbstractStorageDevice *device = new FileStorageDevice();
		char *device_identifier;

		device_identifier = strdup("/tmp/storage/");
		struct stat st;
		if (!(stat(device_identifier, &st) == 0))
			mkdir(device_identifier, 700);

		device->set_identifier(device_identifier);
		if (!device)
		{
			cout << "Error while creating FileStorageDevice object" << endl;
			return;
		}

		std::list<AbstractStorageDevice*> *devices = new std::list<
				AbstractStorageDevice*>();
		devices->push_back(device);

		AbstractDataDistributionFunction *ddf =
				new SimpleDataDistributionFunction();
		ddf->change_metadata_storage_devices(NULL, devices);
		delete devices;

		// create and initialize data distribution manager
		DataDistributionManager *ddm = new DataDistributionManager(ddf);
		if (!ddm)
		{
			cout << "Error while creating DataDistributionManager object" << endl;;
			return;
		}

		// create and initialize storage abstraction layer
		StorageAbstractionLayer *storage_abstraction_layer =
				new StorageAbstractionLayer(ddm);
		if (!storage_abstraction_layer)
		{
			cout <<  "Error while creating StorageAbstractionLayer object" << endl;;
			return;
		}
		// register storage abstraction layer at journal manager
		if (jm->get_instance()->set_storage_abstraction_layer(
				storage_abstraction_layer))
		{
			cout << "Unable to register sal at journal manager" << endl;
		}

		 jm->create_journal(1);
	}

	static void* run_receive_server(void* pntr)
	{
		RecCommunicationServer::get_instance().set_up_receive_socket();
	    RecCommunicationServer::get_instance().receive();
	    return NULL;
	}


	int test_get_cluster_load()
	{
		log("begin test_get_cluster_load()");

		int result;
		vector <Server> server_list;
		mlt_handler->get_server_list(server_list);
		vector <PossibleTargets> cluster_load;

		log("Requesting load of other servers...");
		result = load_manager->get_cluster_load(server_list, cluster_load);

		//TODO test results here somehow, but it should work

		log("end test_get_cluster_load()");
		return result;
	}

public:
	LoadManager *load_manager;
	HotSpotManager *hot_spot_manager;
	MltHandler* mlt_handler;
	LBCommunicationHandler *com_handler;
	DaoHandler* dao;
};



TEST_F (LBTest, get_clusterload)
{

	int result;
//	vector <Server> server_list;
//	mlt_handler->get_server_list(server_list);
//	vector <PossibleTargets> cluster_load;
//	load_manager->get_own_load();
//	result = load_manager->get_cluster_load(server_list, cluster_load);
//	cout << cluster_load.at(1).server_load.cpu_load << endl;
//	cout << cluster_load.at(1).server_load.threads << endl;

	InodeNumber inode = 1;
	SubtreeChange subtree_change = FULL_SUBTREE_MOVE;
	Server target_server;
	target_server.address = "127.0.0.1";


	LoadManager::get_instance()->start_dao(inode, subtree_change, target_server);
	sleep(10);
}

} // namespace

