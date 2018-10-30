#include "gtest/gtest.h"
#include <pthread.h>
#include <unistd.h>

#include "coco/communication/CommunicationHandler.h"
#include "coco/communication/RecCommunicationServer.h"
#include "coco/communication/UniqueIdGenerator.h"
#include "coco/coordination/MltHandler.h"
#include <malloc.h>
#include <sstream>

#define RIVER_ADDRESS "192.168.42.21"
#define CITRUSSSUMMER_ADDRESS "192.168.42.86"

namespace
{

void* start_receive(void* pntr)
{
	RecCommunicationServer::get_instance().set_up_receive_socket();
	RecCommunicationServer::get_instance().receive();
	return NULL;
}

class CommHandlerTest: public ::testing::Test
{
protected:

	CommHandlerTest()
	{
		// You can do set-up work for each test here.
	}

	virtual ~CommHandlerTest()
	{
		// You can do clean-up work that doesn't throw exceptions here.
	}

	void init()
	{
		//fill mlt
		mlt_handler = &(MltHandler::get_instance());
		mlt_handler->init_new_mlt(RIVER_ADDRESS, 49152, 2, 1);
		mlt_handler->add_server("CITRUSSSUMMER_ADDRESS", 49153);
		Server myAddress;
		myAddress.address = CITRUSSSUMMER_ADDRESS;
		myAddress.port = 49152;
		mlt_handler->set_my_address(myAddress);

		SendCommunicationServer::get_instance().set_up_external_sockets();
		commHandler_1.init(COM_LoadBalancing);
		pthread_t thread1;

		pthread_create(&thread1, NULL, start_receive, NULL);

		server_list.push_back(RIVER_ADDRESS);
	}
	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp()
	{

	}

	virtual void TearDown()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
	}

	class MyHandler: public CommunicationHandler
	{
		void handle_request(ExtractedMessage& message)
		{
			// echo server
			char* msg = (char*) malloc(message.data_length + 1);
			memcpy(msg, (char*) message.data, message.data_length);
			msg[message.data_length] = '\0';

			cout << "Repling on Request: " << message.message_id << endl;

			reply(message.message_id, (void*) msg, message.data_length,
					message.sending_module, message.sending_server);
			free(msg);
			free(message.data);
		}
	};

	MyHandler commHandler_1;
	vector<string> server_list;
	UniqueIdGenerator id_gen;

	MltHandler* mlt_handler;

};

TEST_F(CommHandlerTest, test_request)
{
	vector<ExtractedMessage> message_list;
	sleep(5);
	init();
	commHandler_1.start();

	string msg_str = "test";
	ExtractedMessage incoming_message;
	uint64_t message_id;
	int32_t ret;

	cout << "send message to river: " << endl;
	commHandler_1.request_reply(&message_id, (void*) msg_str.data(),
			msg_str.size(), COM_LoadBalancing, server_list, message_list);
	ret = commHandler_1.receive_reply(message_id, 10);

	if(!message_list.empty())
		cout << "reply received: " << endl;
}

}
