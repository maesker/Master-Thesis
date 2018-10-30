#include <pthread.h>
#include <zmq.hpp>
#include <vector>
#include "coco/communication/SendCommunicationServer.h"
#include "coco/communication/CommunicationFailureCodes.h"
#include "zhelpers.hpp"
#include "gtest/gtest.h"

void *test_xrec(void *ptr)
{

    zmq::context_t context (1);
    zmq::socket_t subscriber(context, ZMQ_XREP);
    subscriber.setsockopt( ZMQ_IDENTITY, "A", 1);
    subscriber.bind("tcp://*:49152");
    std::string adress = s_recv (subscriber);

    std::string message = s_recv (subscriber);
    string s(message.begin()+2,message.end());
    *((string*)ptr) = s;
}

void *test_gan(void *ptr)
{

    zmq::context_t context (1);
    zmq::socket_t subscriber(context, ZMQ_REP);
    subscriber.bind("tcp://127.0.0.1:5551");
    std::string message = s_recv (subscriber);

    s_send(subscriber, "reply");

    *((string*)ptr) = message;
}



TEST(TestSuite, SendCommunicationServer)
{
    try
    {
        SendCommunicationServer& server = SendCommunicationServer::get_instance();
        ASSERT_EQ(0, server.set_up_external_sockets());
        ASSERT_EQ(0, server.set_up_external_sockets());

        std::vector<string> server_list;
        server_list.push_back("127.0.0.1");
        string s1("failed");
        string s2("failed");

        pthread_t thread1;
        pthread_create(&thread1, NULL, test_xrec, (void*) &s1);
        ASSERT_EQ(0,server.asynch_send((void*)"test", 4, COM_LoadBalancing, COM_LoadBalancing, server_list));
        pthread_join(thread1, NULL);
        ASSERT_TRUE(s1.compare("test")==0);
        s1 = "failed";

        pthread_create(&thread1, NULL, test_xrec, (void*) &s1);
        sleep(1);
        ASSERT_EQ(0,server.asynch_send((void*)"test", 0, COM_LoadBalancing, COM_LoadBalancing, server_list));
        pthread_join(thread1, NULL);
        ASSERT_TRUE(s1.compare("")==0);
        s1 = "failed";

        pthread_create(&thread1, NULL, test_xrec, (void*) &s1);
        sleep(1);
        server_list.clear();
        ASSERT_EQ(0,server.asynch_send((void*)"test", 4, COM_LoadBalancing, COM_LoadBalancing, server_list));
        pthread_join(thread1, NULL);
        ASSERT_TRUE(s1.compare("test")==0);
        s1 = "failed";

        string wrongserver ("0.0.0.0");
        server_list.push_back (wrongserver);
        ASSERT_EQ(SocketNotExisting, server.asynch_send((void*)"test", 4, COM_LoadBalancing, COM_LoadBalancing, server_list));


        pthread_create(&thread1, NULL, test_gan, (void*) &s2);
        sleep(1);
        RequestResult result = server.ganesha_send((void*)"test", 4);
        pthread_join(thread1, NULL);
        ASSERT_TRUE(s2.compare("test")==0);
        s1 = string((char*)result.data,result.length);
        ASSERT_TRUE(s1.compare("reply")==0);
        free(result.data);
        s1 = "failed";
        s2 = "failed";

        pthread_create(&thread1, NULL, test_gan, (void*) &s2);
        sleep(1);
        result = server.ganesha_send((void*)"test", 0);
        pthread_join(thread1, NULL);
        ASSERT_TRUE(s2.compare("")==0);
        s1 = string((char*)result.data,result.length);
        ASSERT_TRUE(s1.compare("reply")==0);
        free(result.data);

        ASSERT_EQ(server.add_server_send_socket(string("127.0.0.1")),SocketAlreadyExisting);
        ASSERT_EQ(server.add_server_send_socket(string("191.156.493.24")),0);

        ASSERT_EQ(server.delete_server_send_socket(string()),SocketNotExisting);
        ASSERT_EQ(server.delete_server_send_socket(string("191.156.493.24")),0);
    }
    catch (int e)
    {
        cout<<"Exeption occured: "<<e<<endl;
    }
}



