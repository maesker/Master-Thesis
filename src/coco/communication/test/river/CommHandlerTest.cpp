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
#define CITRUSSSUMMER_ADDRESS "192.168.42.103"

namespace
{

void* start_receive(void* pntr)
{
	cout << "RecCommunicationServer::get_instance().set_up_receive_socket();" << endl;
	RecCommunicationServer::get_instance().set_up_receive_socket();

	cout << "RecCommunicationServer::get_instance().receive();" << endl;
	RecCommunicationServer::get_instance().receive();
	cout << "Receive server started.. " << endl;
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
		mlt_handler->add_server(CITRUSSSUMMER_ADDRESS, 49153);
		Server myAddress;
		myAddress.address = RIVER_ADDRESS;
		myAddress.port = 49152;
		mlt_handler->set_my_address(myAddress);
		cout << "my address: " << mlt_handler->get_my_address().address << endl;	
		cout << "INIT Sendserver..." << endl;

		SendCommunicationServer::get_instance().set_up_external_sockets();
		commHandler_1.init(COM_LoadBalancing);
		pthread_t thread1;

		cout << "INIT ReceiveServer.." << endl;
		pthread_create(&thread1, NULL, start_receive, NULL);

		server_list.push_back(CITRUSSSUMMER_ADDRESS);
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
			cout << "A request arrived" << endl;
			// echo server
			char* msg = (char*) malloc(message.data_length + 1);
			memcpy(msg, (char*) message.data, message.data_length);
			msg[message.data_length] = '\0';
			
			cout << "Request from: " << message.sending_server << endl;
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

TEST_F(CommHandlerTest, test_requests)
{
    init();
    sleep(1);
    commHandler_1.start();
    

    
   ExtractedMessage incoming_message;
   uint64_t message_id;

        string msg_str = "";

        char* reply;
        int32_t ret;

        for (int i = 0; i < 10; i++)
        {
            ostringstream ost;
            vector<ExtractedMessage> message_list;
            ost << id_gen.get_next_id();

            msg_str = ost.str();
            cout << "Send request: " << msg_str << endl;
            commHandler_1.request_reply(&message_id, (void*) msg_str.data(),
                                        msg_str.size(), COM_LoadBalancing, server_list, message_list);
            ret = commHandler_1.receive_reply(message_id, 10);
            if (ret == 0)
            {

                incoming_message = message_list.front();
                message_list.pop_back();

                reply = (char*) malloc(incoming_message.data_length + 1);
                memcpy(reply, incoming_message.data,
                       incoming_message.data_length);
                reply[incoming_message.data_length] = '\0';
                cout << "ID: " << message_id << "Test reply received: " <<reply << " ret: " << ret << endl;
                EXPECT_TRUE( msg_str.compare(string(reply)) == 0);
                free(reply);
                free(incoming_message.data);

            }
            EXPECT_FALSE(ret == UnknownMessageId);
            EXPECT_FALSE(ret == Timeout);
        }
    
    commHandler_1.stop();
    cout << "finished" << endl;
    sleep(1);
}


/* TEST_F(CommHandlerTest, test_request)
{
	vector<ExtractedMessage> message_list;
	
	init();
	sleep(1);
	commHandler_1.start();

	string msg_str = "test";
	ExtractedMessage incoming_message;
	uint64_t message_id;
	int32_t ret;

	cout << "send message to citrussummer: " << endl;
	commHandler_1.request_reply(&message_id, (void*) msg_str.data(),
			msg_str.size(), COM_LoadBalancing, server_list, message_list);
	ret = commHandler_1.receive_reply(message_id, 10);

	if(!message_list.empty())
		cout << "reply received: " << endl;


} */
}
