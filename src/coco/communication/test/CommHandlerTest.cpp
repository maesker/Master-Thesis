#include "gtest/gtest.h"
#include <pthread.h>
#include <unistd.h>

#include "coco/communication/CommunicationHandler.h"
#include "coco/communication/RecCommunicationServer.h"
#include "coco/communication/UniqueIdGenerator.h"
#include <malloc.h>
#include <sstream>

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
        SendCommunicationServer::get_instance().set_up_external_sockets();
        commHandler_1.init(COM_LoadBalancing);
        commHandler_2.init(COM_DistributedAtomicOp);
        server_list.push_back("127.0.0.1");
        pthread_t thread1;

        pthread_create(&thread1, NULL, start_receive, NULL);
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

            //cout << "Repling on Request: " << message.message_id << endl;

            reply(message.message_id, (void*) msg, message.data_length,
                  message.sending_module, message.sending_server);
            free(msg);
            free(message.data);
        }
    };

    MyHandler commHandler_1;
    MyHandler commHandler_2;
    vector<string> server_list;
    UniqueIdGenerator id_gen;

};

//TEST_F(CommHandlerTest, test_mapping)
//{
//	struct mallinfo mi_init = mallinfo();
//	{
//		MessageMap map;
//
//		string my_data = "1some testdata";
//		ExtractedMessage extracted;
//		extracted.data = malloc(sizeof(char) * my_data.size());
//		memcpy(extracted.data, (void*) my_data.data(), my_data.size());
//		vector<ExtractedMessage> vec;
//		vec.push_back(extracted);
//		MessMapEntry* entry = new MessMapEntry();
//		entry->set_msg_list(&vec);
//		entry->set_garbage(true);
//		delete entry;
//	}
//
//	std::cout << "Init allocation status: " << mi_init.uordblks << std::endl;
//	std::cout << "finished tallocation status: " << mallinfo().uordblks
//			<< std::endl;
//}

TEST_F(CommHandlerTest, test_request)
{
    init();
    commHandler_1.start();
    commHandler_2.start();
    struct mallinfo mi_init = mallinfo();

    {
        ExtractedMessage incoming_message;
        uint64_t message_id;

        string msg_str = "";

        std::cout << "allocation status: " << mallinfo().uordblks << std::endl;
        char* reply;
        int32_t ret;

        for (int i = 0; i < 1; i++)
        {
            ostringstream ost;
            vector<ExtractedMessage> message_list;
            ost << id_gen.get_next_id();

            msg_str = ost.str();
            //cout << "Send request: " << msg_str << endl;
            commHandler_2.request_reply(&message_id, (void*) msg_str.data(),
                                        msg_str.size(), COM_LoadBalancing, server_list, message_list);
            ret = commHandler_2.receive_reply(message_id, 10);
            if (ret == 0)
            {

                incoming_message = message_list.front();
                message_list.pop_back();

                reply = (char*) malloc(incoming_message.data_length + 1);
                memcpy(reply, incoming_message.data,
                       incoming_message.data_length);
                reply[incoming_message.data_length] = '\0';
                //cout << "ID: " << message_id << "Test reply received: " <<reply << " ret: " << ret << endl;
                EXPECT_TRUE( msg_str.compare(string(reply)) == 0);
                free(reply);
                free(incoming_message.data);

            }
            EXPECT_FALSE(ret == UnknownMessageId);
            EXPECT_FALSE(ret == Timeout);
        }
    }
    commHandler_1.stop();
    commHandler_2.stop();
    std::cout << "Init allocation status: " << mi_init.uordblks << std::endl;
    std::cout << "finished tallocation status: " << mallinfo().uordblks << std::endl;
    cout << "finished" << endl;
    sleep(1);
}

}
