#include <pthread.h>
#include <zmq.hpp>
#include "coco/communication/RecCommunicationServer.h"
#include "zhelpers.hpp"
#include "coco/communication/ConcurrentQueue.h"
#include "coco/communication/CommonCommunicationTypes.h"
#include "gtest/gtest.h"

void *test_xsend(void *ptr)
{

    zmq::context_t context (1);
    zmq::socket_t sender(context, ZMQ_XREQ);
    sender.setsockopt( ZMQ_IDENTITY, "127.0.0.1", 10);
    sender.connect("tcp://127.0.0.1:49152");

    sleep(1);
    zmq_msg_t message;
    void* data = malloc(7);
    *((char*)data)=COM_LoadBalancing;
    *(((char*)data)+1)=COM_LoadBalancing;
    *(((char*)data)+2)='t';
    *(((char*)data)+3)='e';
    *(((char*)data)+4)='s';
    *(((char*)data)+5)='t';
    *(((char*)data)+6)='1';
    zmq_msg_init_size(&message, 7);
    memcpy(zmq_msg_data(&message), data, 7);
    zmq_send(sender, &message,0);

    *(((char*)data)+6)='2';
    zmq_msg_init_size(&message, 7);
    memcpy(zmq_msg_data(&message), data, 7);
    zmq_send(sender, &message,ZMQ_NOBLOCK);

    *(((char*)data)+1)=COM_DistributedAtomicOp;
    zmq_msg_init_size(&message, 7);
    memcpy(zmq_msg_data(&message), data, 7);
    zmq_send(sender, &message,ZMQ_NOBLOCK);


    *((char*)data)=COM_DistributedAtomicOp;
    zmq_msg_init_size(&message,7);
    memcpy(zmq_msg_data(&message), data, 7);
    zmq_send(sender, &message,ZMQ_NOBLOCK);

    zmq_msg_close(&message);
    free(data);
}

void *test_recieve(void *ptr)
{

    RecCommunicationServer& server = RecCommunicationServer::get_instance();
    server.receive();

}


TEST(TestSuite, RecCommunicationServer)
{
    try
    {
        RecCommunicationServer& server = RecCommunicationServer::get_instance();
        ASSERT_EQ(0,server.set_up_receive_socket());
        ASSERT_EQ(0,server.set_up_receive_socket());

        ConcurrentQueue<ExternalMessage> test_queue;
        server.register_module(COM_LoadBalancing, &test_queue);
        ExternalMessage message;
        std::string s1;

        pthread_t thread1, thread2;

        pthread_create(&thread1, NULL, test_xsend, NULL);
        pthread_create(&thread2, NULL, test_recieve, NULL);


        test_queue.wait_and_pop(message);
        ASSERT_EQ("127.0.0.1", message.sending_server);
        ASSERT_EQ(COM_LoadBalancing, message.sending_module);
        ASSERT_EQ(5, message.data_length);
        s1 = std::string((char*)message.data,message.data_length);
        ASSERT_TRUE(s1.compare("test1")==0);

        test_queue.wait_and_pop(message);
        ASSERT_EQ("127.0.0.1", message.sending_server);
        ASSERT_EQ(COM_LoadBalancing, message.sending_module);
        ASSERT_EQ(5, message.data_length);
        s1 = std::string((char*)message.data,message.data_length);
        ASSERT_TRUE(s1.compare("test2")==0);

        test_queue.wait_and_pop(message);
        ASSERT_EQ("127.0.0.1", message.sending_server);
        ASSERT_EQ(COM_DistributedAtomicOp, message.sending_module);
        ASSERT_EQ(5, message.data_length);
        s1 = std::string((char*)message.data,message.data_length);
        ASSERT_TRUE(s1.compare("test2")==0);

        pthread_join(thread1, NULL);

    }
    catch (int e)
    {
        std::cout<<"Exeption occured: "<<e<<std::endl;
    }
}
