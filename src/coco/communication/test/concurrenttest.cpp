#include <pthread.h>
#include <zmq.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <malloc.h>
#include <iostream>
#include "coco/communication/ConcurrentQueue.h"
#include "gtest/gtest.h"

using namespace std;

void *test_push_queue(void *ptr)
{
    ConcurrentQueue<int> *testqueue;
    testqueue = (ConcurrentQueue<int>*) ptr;
    sleep(1);
    testqueue->push(2);
}

TEST(TestSuite, ConcurrentQueue)
{
    try
    {
        ConcurrentQueue<int> testqueue;
        int x, alloc_size;
        pthread_t thread1;

        //Check whether the queue was initialized the right way
        ASSERT_TRUE(testqueue.empty());

        testqueue.push(1);
        // check the method empty()
        ASSERT_FALSE(testqueue.empty());

        testqueue.push(2);
        ASSERT_FALSE(testqueue.empty());

        //test if wait_and_pop returns the correct values
        testqueue.wait_and_pop(x);
        ASSERT_EQ(1,x);
        testqueue.wait_and_pop(x);
        ASSERT_EQ(2,x);
        //test if try_pop returns false if the queue is empty and empty returns true
        ASSERT_FALSE(testqueue.try_pop(x));
        ASSERT_TRUE(testqueue.empty());

        testqueue.push(1);
        testqueue.push(2);
        //test if try_pop returns the correct values
        ASSERT_TRUE(testqueue.try_pop(x));
        ASSERT_EQ(1,x);
        ASSERT_TRUE(testqueue.try_pop(x));
        ASSERT_EQ(2,x);

        pthread_create(&thread1, NULL, test_push_queue, (void*) &testqueue);
        ////test if wait_and_pop returns the correct values after blocking
        testqueue.wait_and_pop(x);
        pthread_join(thread1, NULL);
        ASSERT_EQ(2,x);
    }
    catch (int e)
    {
        cout<<"Exeption occured: "<<e<<endl;
    }
}
TEST(TestSuite, ConcurrentQueue_timeout_test)
{
    try
    {
        ConcurrentQueue<int> testqueue;
        int x;
        pthread_t thread1;

        // set timeout = 1 second
        int timeout = 1;
        EXPECT_TRUE(testqueue.empty());

        EXPECT_FALSE(testqueue.try_wait_and_pop(x, timeout));

    }
    catch (int e)
    {
        cout<<"Exeption occured: "<<e<<endl;
    }
}

