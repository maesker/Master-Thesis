#include <pthread.h>
#include <zmq.hpp>
#include "coco/communication/IPCGanesha.h"
#include "zhelpers.hpp"
#include "gtest/gtest.h"
#include <cstdio>

void *test_gan(void *ptr)
{

    zmq::context_t context (1);
    zmq::socket_t subscriber(context, ZMQ_REQ);
    subscriber.connect("ipc://InterProcessGanesha.ipc");

    sleep(1);
    s_send(subscriber, "reply");
    std::string message = s_recv (subscriber);

    *((std::string*)ptr) = message;
}

RequestResult message_treatment (void* request, int lenght)
{
    RequestResult result;
    if ( *((char*)request)=='r'&& *((char*)request+1)=='e'&& *((char*)request+2)=='p'&& *((char*)request+3)=='l'&& *((char*)request+4)=='y')
    {
        result.length = 4;
        char* intermediate = "test";
        result.data = malloc(result.length);
        memcpy(result.data, intermediate, result.length);
        return result;
    }
    result.length=0;
    result.data = NULL;
    return result;
}

void* test_rec_gan(void* ptr)
{
    receive_ipc_request(&message_treatment);
}

TEST(TestSuite, IPCGanesha)
{
    try
    {
        std::string s1("failed");

        pthread_t thread1, thread2;

        pthread_create(&thread1, NULL, test_gan, (void*) &s1);

        pthread_create(&thread2, NULL, test_rec_gan, NULL);

        pthread_join(thread1, NULL);

        ASSERT_TRUE(s1.compare("test")==0);

    }
    catch (int e)
    {
        std::cout<<"Exeption occured: "<<e<<std::endl;
    }
}



