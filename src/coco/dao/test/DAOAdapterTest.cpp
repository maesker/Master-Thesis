#include <cstring>

#include "gtest/gtest.h"
#include "DAOTestEmulator.h"
#include "coco/dao/DistributedAtomicOperations.h"


TEST(AdapterTestSuite, RequestTestSuccess)
{
    // test set up
    DAOAdapterTestRealization adapter;
    adapter.set_test_behavior(true, true);
    adapter.set_module(DAO_MetaData);
    char* operation = (char*)malloc(6);
    operation[0] = (char) DAORequest;
    operation[1] = 't';
    operation[2] = 'e';
    operation[3] = 's';
    operation[4] = 't';
    operation[5] = '\0';
    OutQueue input;
    input.ID = 0;
    input.operation = operation;
    input.operation_length = 6;
    input.protocol = OrderedOperationExecution;

    ConcurrentQueue<OutQueue>* input_queue = DistributedAtomicOperations::get_instance(). get_queue(DAO_MetaData);
    ASSERT_TRUE(input_queue->empty());
    // put request into queue
    input_queue->push(input);
    // get response
    InQueue output;
    adapter.test_output.wait_and_pop(output);
    // check answer
    ASSERT_EQ(output.ID, 0);
    ASSERT_EQ(output.success, ExecutionSuccessful);
    ASSERT_EQ(output.next_participant.subtree_entry, 0);
    ASSERT_EQ(output.next_participant.server.size(),9);
    ASSERT_TRUE(strcmp(output.next_participant.server.c_str(), "127.0.0.1")==0);

    // check queues empty
    ASSERT_TRUE(input_queue->empty());
    ASSERT_TRUE(adapter.test_output.empty());
}


TEST(AdapterTestSuite, RequestTestFailure)
{
    // test set up
    DAOAdapterTestRealization adapter;
    adapter.set_test_behavior(false, true);
    adapter.set_module(DAO_MetaData);
    char* operation = (char*)malloc(6);
    operation[0] = (char) DAORequest;
    operation[1] = 't';
    operation[2] = 'e';
    operation[3] = 's';
    operation[4] = 't';
    operation[5] = '\0';
    OutQueue input;
    input.ID = 0;
    input.operation = operation;
    input.operation_length = 6;
    input.protocol = TwoPhaseCommit;

    ConcurrentQueue<OutQueue>* input_queue = DistributedAtomicOperations::get_instance(). get_queue(DAO_MetaData);
    ASSERT_TRUE(input_queue->empty());
    // put request into queue
    input_queue->push(input);
    // get response
    InQueue output;
    adapter.test_output.wait_and_pop(output);
    // check answer
    ASSERT_EQ(output.ID, 0);
    ASSERT_EQ(output.success, ExecutionUnsuccessful);
    ASSERT_EQ(output.next_participant.server.size(), 0);

    // check queues empty
    ASSERT_TRUE(input_queue->empty());
    ASSERT_TRUE(adapter.test_output.empty());
}

TEST(AdapterTestSuite, RedoRequestTestSuccess)
{
    // test set up
    DAOAdapterTestRealization adapter;
    adapter.set_test_behavior(true, true);
    adapter.set_module(DAO_MetaData);
    char* operation = (char*)malloc(6);
    operation[0] = (char) DAORedoRequest;
    operation[1] = 't';
    operation[2] = 'e';
    operation[3] = 's';
    operation[4] = 't';
    operation[5] = '\0';
    OutQueue input;
    input.ID = 0;
    input.operation = operation;
    input.operation_length = 6;
    input.protocol = OrderedOperationExecution;

    ConcurrentQueue<OutQueue>* input_queue = DistributedAtomicOperations::get_instance(). get_queue(DAO_MetaData);
    ASSERT_TRUE(input_queue->empty());
    // put request into queue
    input_queue->push(input);
    // get response
    InQueue output;
    adapter.test_output.wait_and_pop(output);
    // check answer
    ASSERT_EQ(output.ID, 0);
    ASSERT_EQ(output.success, ExecutionSuccessful);
    ASSERT_EQ(output.next_participant.subtree_entry, 0);
    ASSERT_EQ(output.next_participant.server.size(),9);
    ASSERT_TRUE(strcmp(output.next_participant.server.c_str(), "127.0.0.1")==0);

    // check queues empty
    ASSERT_TRUE(input_queue->empty());
    ASSERT_TRUE(adapter.test_output.empty());
}

TEST(AdapterTestSuite, RedoRequestTestFailure)
{
    // test set up
    DAOAdapterTestRealization adapter;
    adapter.set_test_behavior(false, true);
    adapter.set_module(DAO_MetaData);
    char* operation = (char*)malloc(6);
    operation[0] = (char) DAORedoRequest;
    operation[1] = 't';
    operation[2] = 'e';
    operation[3] = 's';
    operation[4] = 't';
    operation[5] = '\0';
    OutQueue input;
    input.ID = 0;
    input.operation = operation;
    input.operation_length = 6;
    input.protocol = TwoPhaseCommit;

    ConcurrentQueue<OutQueue>* input_queue = DistributedAtomicOperations::get_instance(). get_queue(DAO_MetaData);
    ASSERT_TRUE(input_queue->empty());
    // put request into queue
    input_queue->push(input);
    // get response
    InQueue output;
    adapter.test_output.wait_and_pop(output);
    // check answer
    ASSERT_EQ(output.ID, 0);
    ASSERT_EQ(output.success, ExecutionUnsuccessful);
    ASSERT_EQ(output.next_participant.server.size(), 0);

    // check queues empty
    ASSERT_TRUE(input_queue->empty());
    ASSERT_TRUE(adapter.test_output.empty());
}

TEST(AdapterTestSuite, UndoRequestTestSuccess)
{
    // test set up
    DAOAdapterTestRealization adapter;
    adapter.set_test_behavior(true, true);
    adapter.set_module(DAO_MetaData);
    char* operation = (char*)malloc(6);
    operation[0] = (char) DAOUndoRequest;
    operation[1] = 't';
    operation[2] = 'e';
    operation[3] = 's';
    operation[4] = 't';
    operation[5] = '\0';
    OutQueue input;
    input.ID = 0;
    input.operation = operation;
    input.operation_length = 6;
    input.protocol = TwoPhaseCommit;

    ConcurrentQueue<OutQueue>* input_queue = DistributedAtomicOperations::get_instance(). get_queue(DAO_MetaData);
    ASSERT_TRUE(input_queue->empty());
    // put request into queue
    input_queue->push(input);
    // get response
    InQueue output;
    adapter.test_output.wait_and_pop(output);
    // check answer
    ASSERT_EQ(output.ID, 0);
    ASSERT_EQ(output.success, UndoSuccessful);
    ASSERT_EQ(output.next_participant.server.size(), 0);

    // check queues empty
    ASSERT_TRUE(input_queue->empty());
    ASSERT_TRUE(adapter.test_output.empty());
}

TEST(AdapterTestSuite, UndoRequestTestFailure)
{
    // test set up
    DAOAdapterTestRealization adapter;
    adapter.set_test_behavior(false, true);
    adapter.set_module(DAO_MetaData);
    char* operation = (char*)malloc(6);
    operation[0] = (char) DAOUndoRequest;
    operation[1] = 't';
    operation[2] = 'e';
    operation[3] = 's';
    operation[4] = 't';
    operation[5] = '\0';
    OutQueue input;
    input.ID = 0;
    input.operation = operation;
    input.operation_length = 6;
    input.protocol = TwoPhaseCommit;

    ConcurrentQueue<OutQueue>* input_queue = DistributedAtomicOperations::get_instance(). get_queue(DAO_MetaData);
    ASSERT_TRUE(input_queue->empty());
    // put request into queue
    input_queue->push(input);
    // get response
    InQueue output;
    adapter.test_output.wait_and_pop(output);
    // check answer
    ASSERT_EQ(output.ID, 0);
    ASSERT_EQ(output.success, UndoUnsuccessful);
    ASSERT_EQ(output.next_participant.server.size(), 0);

    // check queues empty
    ASSERT_TRUE(input_queue->empty());
    ASSERT_TRUE(adapter.test_output.empty());
}

TEST(AdapterTestSuite, ReUndoRequestTestSuccess)
{
    // test set up
    DAOAdapterTestRealization adapter;
    adapter.set_test_behavior(true, true);
    adapter.set_module(DAO_MetaData);
    char* operation = (char*)malloc(6);
    operation[0] = (char) DAOReundoRequest;
    operation[1] = 't';
    operation[2] = 'e';
    operation[3] = 's';
    operation[4] = 't';
    operation[5] = '\0';
    OutQueue input;
    input.ID = 0;
    input.operation = operation;
    input.operation_length = 6;
    input.protocol = TwoPhaseCommit;

    ConcurrentQueue<OutQueue>* input_queue = DistributedAtomicOperations::get_instance(). get_queue(DAO_MetaData);
    ASSERT_TRUE(input_queue->empty());
    // put request into queue
    input_queue->push(input);
    // get response
    InQueue output;
    adapter.test_output.wait_and_pop(output);
    // check answer
    ASSERT_EQ(output.ID, 0);
    ASSERT_EQ(output.success, UndoSuccessful);
    ASSERT_EQ(output.next_participant.server.size(), 0);

    // check queues empty
    ASSERT_TRUE(input_queue->empty());
    ASSERT_TRUE(adapter.test_output.empty());
}

TEST(AdapterTestSuite, ReUndoRequestTestFailure)
{
    // test set up
    DAOAdapterTestRealization adapter;
    adapter.set_test_behavior(false, true);
    adapter.set_module(DAO_MetaData);
    char* operation = (char*)malloc(6);
    operation[0] = (char) DAOReundoRequest;
    operation[1] = 't';
    operation[2] = 'e';
    operation[3] = 's';
    operation[4] = 't';
    operation[5] = '\0';
    OutQueue input;
    input.ID = 0;
    input.operation = operation;
    input.operation_length = 6;
    input.protocol = TwoPhaseCommit;

    ConcurrentQueue<OutQueue>* input_queue = DistributedAtomicOperations::get_instance(). get_queue(DAO_MetaData);
    ASSERT_TRUE(input_queue->empty());
    // put request into queue
    input_queue->push(input);
    // get response
    InQueue output;
    adapter.test_output.wait_and_pop(output);
    // check answer
    ASSERT_EQ(output.ID, 0);
    ASSERT_EQ(output.success, UndoUnsuccessful);
    ASSERT_EQ(output.next_participant.server.size(), 0);

    // check queues empty
    ASSERT_TRUE(input_queue->empty());
    ASSERT_TRUE(adapter.test_output.empty());
}

TEST(AdapterTestSuite, AcknowledgeTest)
{
    // test set up
    DAOAdapterTestRealization adapter;
    adapter.set_test_behavior(true, true);
    adapter.set_module(DAO_MetaData);
    char* operation = (char*)malloc(1);
    operation[0] = 1;
    OutQueue input;
    input.ID = 0;
    input.operation = operation;
    input.operation_length = 0;
    input.protocol = TwoPhaseCommit;

    ConcurrentQueue<OutQueue>* input_queue = DistributedAtomicOperations::get_instance(). get_queue(DAO_MetaData);
    ASSERT_TRUE(input_queue->empty());
    // put request into queue
    input_queue->push(input);

    // wait until acknowledge treated
    do
    {
    }
    while (!adapter.operation_result_handled());

    // check queue emtpy
    ASSERT_TRUE(input_queue->empty());
}