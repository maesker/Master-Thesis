#include <cstring>
#include <malloc.h>
#include "gtest/gtest.h"
#include "DAOTestEmulator.h"
#include "coco/dao/DistributedAtomicOperations.h"
#include <unordered_map>
#include <vector>
#include <set>

//using namespace std;
int before_allocation;
/* Testing  the modified two phase commit protocol*/

/* Coordinator tests*/

TEST(TestSuite, CoordinatorSuccessfulM2PC)
{
    // first test executed, call recovery method
    ASSERT_TRUE(DistributedAtomicOperations::do_recovery()== 0);

    struct mallinfo mi;
    mi = mallinfo();
    before_allocation = mi.uordblks;
    // set up test behaviour for emulation
    CommunicationHandler::set_test_behavior(true, false);
    // test if queues are empty

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry = 1;
    participants.push_back(tree);

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, Rename, participants, 0);
    ASSERT_TRUE(id!=0);

    // log begin
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==ModifiedTwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success =0;
    dao.provide_operation_execution_result(execution_result);

    // start participant log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_EQ(MTPCIStartP, journal_entry.status);

    ASSERT_TRUE(Journal::get_queue().empty());
    // message sent to participant
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(MTPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate successful participant execution
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.1");
    message.sending_module = COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=MTPCCommit;
    // let dao receive message
    dao.handle_request(message);

    // Commit log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_EQ(StatusCommit, journal_entry.status);

    // Acknowledge sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(MTPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // Client response
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE((*(char*)request.operation)==1);
    ASSERT_TRUE(request.protocol==ModifiedTwoPhaseCommit);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // No further activity TODO ?

    //ASSERT_EQ(before_allocation, mallinfo().uordblks);

}

TEST(TestSuite, CoordinatorUnsuccessfulM2PCParticipant)
{
    // set up test behaviour for emulation
    CommunicationHandler::set_test_behavior(false, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry = 1;
    participants.push_back(tree);

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, Rename, participants, 0);
    ASSERT_TRUE(id!=0);

    // log begin
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==ModifiedTwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success =0;
    dao.provide_operation_execution_result(execution_result);

    // start participant log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_EQ(MTPCIStartP, journal_entry.status);

    ASSERT_TRUE(Journal::get_queue().empty());
    // message sent to participant
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(MTPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate unsuccessful participant execution
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.1");
    message.sending_module = COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=MTPCAbort;
    // let dao receive message
    dao.handle_request(message);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_EQ(StatusAbort, journal_entry.status);

    // Acknowledge sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(MTPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // Client response
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE((*(char*)request.operation)==0);
    ASSERT_TRUE(request.protocol==ModifiedTwoPhaseCommit);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // No further activity TODO ?
}

TEST(TestSuite, CoordinatorUnsuccessfulOperationUndoneM2PCParticipant)
{
    // set up test behaviour for emulation
    CommunicationHandler::set_test_behavior(false, true);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry = 1;
    participants.push_back(tree);

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, MoveSubtree, participants, 0);
    ASSERT_TRUE(id!=0);

    // log begin
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==ModifiedTwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success =0;
    dao.provide_operation_execution_result(execution_result);

    // start participant log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_EQ(MTPCIStartP, journal_entry.status);

    ASSERT_TRUE(Journal::get_queue().empty());
    // message sent to participant
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(MTPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate unsuccessful participant execution
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.1");
    message.sending_module = COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=MTPCAbort;
    // let dao receive message
    dao.handle_request(message);

    // operation undo execution requested
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAOUndoRequest);
    ASSERT_TRUE(request.protocol==ModifiedTwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // emulate successful undo execution
    execution_result.ID = id;
    execution_result.success =2;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_EQ(StatusAbort, journal_entry.status);

    // Acknowledge sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(MTPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // Client response
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE((*(char*)request.operation)==0);
    ASSERT_TRUE(request.protocol==ModifiedTwoPhaseCommit);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // No further activity TODO ?
}

TEST(TestSuite, CoordinatorUnsuccessfulM2PCCoordinator)
{
    // set up test behaviour for emulation
    CommunicationHandler::set_test_behavior(true, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry = 1;
    participants.push_back(tree);

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, Rename, participants, 0);
    ASSERT_TRUE(id!=0);

    // log begin
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==ModifiedTwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // emulate unsuccessful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success =1;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_EQ(StatusAbort, journal_entry.status);

    // Client response
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE((*(char*)request.operation)==0);
    ASSERT_TRUE(request.protocol==ModifiedTwoPhaseCommit);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // No further activity TODO ?
}

/* Participant tests*/

TEST(TestSuite, ParticipantSuccessfulM2PC)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(true, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=MTPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // log begin
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==ModifiedTwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success =0;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_EQ(MTPCPCommit, journal_entry.status);

    ASSERT_TRUE(Journal::get_queue().empty());
    // success result sent to coordinator
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(MTPCCommit, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate receiving of acknowledgement
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=MTPCAck;
    dao.handle_request(message);

    // Commit log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_EQ(StatusCommit, journal_entry.status);

    // No further activity TODO ?
}

TEST(TestSuite, ParticipantUnsuccessfulM2PCParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=MTPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // log begin
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==ModifiedTwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success =1;
    dao.provide_operation_execution_result(execution_result);

    // own unsuccess vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_EQ(MTPCPAbort, journal_entry.status);

    ASSERT_TRUE(Journal::get_queue().empty());
    // not success result sent to coordinator
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(MTPCAbort, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate receiving of acknowledgement
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=MTPCAck;
    dao.handle_request(message);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_EQ(StatusAbort, journal_entry.status);

    // No further activity TODO ?
}

/* Testing the two phase commit protocol*/

/* Coordinator tests*/

TEST(TestSuite, CoordinatorSuccessful2PC)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(true, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants;
    Subtree tree, tree2;
    tree.server = string("127.0.0.1");
    tree.subtree_entry = 1;
    tree2.server = string("127.0.0.2");
    tree2.subtree_entry = 2;
    participants.push_back(tree);
    participants.push_back(tree2);

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, Rename, participants, 0);
    ASSERT_TRUE(id!=0);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // message sent to participants
    SendRequest send_message;
    // first participant
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);
    //second participant
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success =0;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCIVoteStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    // vote request sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCVoteReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate vote result received
    ExtractedMessage message, message2;
    //first participant
    message.sending_server = string("127.0.0.1");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteY;
    dao.handle_request(message);
    //second participant
    message2.sending_server = string("127.0.0.2");
    message2.sending_module=COM_DistributedAtomicOp;
    message2.message_id=0;
    message2.data_length = sizeof(char)+sizeof(uint64_t);
    message2.data = malloc(message2.data_length);
    *(uint64_t*)(message2.data+sizeof(char))=id;
    *((char*)message2.data)=TPCVoteY;
    dao.handle_request(message2);

    // global result log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCICommiting, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);
    ASSERT_TRUE(Journal::get_queue().empty());
    // result sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCCommit, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // Client response
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE((*(char*)request.operation)==1);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate participants' acknowledgement
    //first participant
    message.sending_server = string("127.0.0.1");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAck;
    dao.handle_request(message);
    //second participant
    message.sending_server = string("127.0.0.2");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAck;
    dao.handle_request(message);

    // Commit log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusCommit, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // No further activity TODO ?
}

TEST(TestSuite, CoordinatorUnsuccessful2PCParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants;
    Subtree tree, tree2;
    tree.server = string("127.0.0.1");
    tree.subtree_entry = 1;
    tree2.server = string("127.0.0.2");
    tree2.subtree_entry = 2;
    participants.push_back(tree);
    participants.push_back(tree2);

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, Rename, participants, 0);
    ASSERT_TRUE(id!=0);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // message sent to participants
    SendRequest send_message;
    // first participant
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);
    //second participant
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success =0;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCIVoteStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    // vote request sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCVoteReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate vote result received
    ExtractedMessage message;
    //first participant
    message.sending_server = string("127.0.0.1");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteY;
    dao.handle_request(message);
    //second participant
    message.sending_server = string("127.0.0.2");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteN;
    dao.handle_request(message);

    // global result log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCIAborting, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    // result sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAbort, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // Client response
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE((*(char*)request.operation)==0);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate participants' acknowledgement
    //first participant
    message.sending_server = string("127.0.0.1");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAck;
    dao.handle_request(message);
    //second participant
    message.sending_server = string("127.0.0.2");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAck;
    dao.handle_request(message);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // No further activity TODO ?

}

TEST(TestSuite, CoordinatorUnsuccessfulOperationUndoneAcksFirst2PCParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, true);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants;
    Subtree tree, tree2;
    tree.server = string("127.0.0.1");
    tree.subtree_entry = 1;
    tree2.server = string("127.0.0.2");
    tree2.subtree_entry = 2;
    participants.push_back(tree);
    participants.push_back(tree2);

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, MoveSubtree, participants, 0);
    ASSERT_TRUE(id!=0);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // message sent to participants
    SendRequest send_message;
    // first participant
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);
    //second participant
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success =0;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCIVoteStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    // vote request sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCVoteReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate vote result received
    ExtractedMessage message;
    //first participant
    message.sending_server = string("127.0.0.1");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteN;
    dao.handle_request(message);

    // global result log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCIAborting, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    // result sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAbort, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // operation undo execution requested
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAOUndoRequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    //second participant (vote result)
    message.sending_server = string("127.0.0.2");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteY;
    dao.handle_request(message);

    // emulate participants' acknowledgement
    //first participant
    message.sending_server = string("127.0.0.1");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAck;
    dao.handle_request(message);
    //second participant
    message.sending_server = string("127.0.0.2");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAck;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // emulate successful undo execution
    execution_result.ID = id;
    execution_result.success =2;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Client response
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE((*(char*)request.operation)==0);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // No further activity TODO ?

}

TEST(TestSuite, CoordinatorUnsuccessfulOperationUndoneUndoFirst2PCParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, true);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants;
    Subtree tree, tree2;
    tree.server = string("127.0.0.1");
    tree.subtree_entry = 1;
    tree2.server = string("127.0.0.2");
    tree2.subtree_entry = 2;
    participants.push_back(tree);
    participants.push_back(tree2);

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, MoveSubtree, participants, 0);
    ASSERT_TRUE(id!=0);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // message sent to participants
    SendRequest send_message;
    // first participant
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);
    //second participant
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success =0;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCIVoteStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    // vote request sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCVoteReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate vote result received
    ExtractedMessage message;
    //first participant
    message.sending_server = string("127.0.0.1");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteN;
    dao.handle_request(message);

    // global result log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCIAborting, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    // result sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAbort, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // operation undo execution requested
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAOUndoRequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    //second participant (vote result)
    message.sending_server = string("127.0.0.2");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteN;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // emulate successful undo execution
    execution_result.ID = id;
    execution_result.success =2;
    dao.provide_operation_execution_result(execution_result);

    // Client response
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE((*(char*)request.operation)==0);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate participants' acknowledgement
    //first participant
    message.sending_server = string("127.0.0.1");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAck;
    dao.handle_request(message);
    //second participant
    message.sending_server = string("127.0.0.2");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAck;
    dao.handle_request(message);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // No further activity TODO ?

}

TEST(TestSuite, CoordinatorUnsuccessful2PCCoordinator)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(true, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants;
    Subtree tree, tree2;
    tree.server = string("127.0.0.1");
    tree.subtree_entry = 1;
    tree2.server = string("127.0.0.2");
    tree2.subtree_entry = 2;
    participants.push_back(tree);
    participants.push_back(tree2);

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, Rename, participants, 0);
    ASSERT_TRUE(id!=0);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // message sent to participants
    SendRequest send_message;
    // first participant
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);
    //second participant
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // emulate unsuccessful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 1;
    dao.provide_operation_execution_result(execution_result);

    // global result log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCIAborting, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    // result sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAbort, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // Client response
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE((*(char*)request.operation)==0);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate participants' acknowledgement
    ExtractedMessage message;
    //first participant
    message.sending_server = string("127.0.0.1");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAck;
    dao.handle_request(message);
    //second participant
    message.sending_server = string("127.0.0.2");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAck;
    dao.handle_request(message);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // No further activity TODO ?

}

/* Participant tests*/

TEST(TestSuite, ParticipantOperationSuccessRequestResultSuccessful2PC)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(true, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCPVoteYes, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate incoming vote request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteReq;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    // own vote sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCVoteY, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate incoming commit request
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCCommit;
    dao.handle_request(message);

    // Commit log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusCommit, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, ParticipantOperationFailureRequestResultUnsuccessful2PCParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    // emulate unsuccessful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 1;
    dao.provide_operation_execution_result(execution_result);

    // own unsuccess vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCPVoteNo, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate incoming vote request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteReq;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    // own vote sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCVoteN, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate incoming abort request
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAbort;
    dao.handle_request(message);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?
}

TEST(TestSuite, ParticipantRequestOperationSuccessResultSuccessful2PC)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(true, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate incoming vote request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteReq;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCPVoteYes, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    // own vote sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCVoteY, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate incoming commit request
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCCommit;
    dao.handle_request(message);

    // Commit log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusCommit, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?
}

TEST(TestSuite, ParticipantRequestOperationFailureResultUnsuccessful2PCParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate incoming vote request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteReq;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate unsuccessful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 1;
    dao.provide_operation_execution_result(execution_result);

    // own unsuccess vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCPVoteNo, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    // own vote sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCVoteN, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // emulate incoming abort request
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAbort;
    dao.handle_request(message);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?
}

TEST(TestSuite, ParticipantRequestResultOperationSuccessUnsuccessful2PC)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate incoming vote request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteReq;
    dao.handle_request(message);

    // emulate incoming abort request
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAbort;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, ParticipantRequestResultOperationSuccessUnsuccessfulOperationUndone2PC)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, true);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=MoveSubtree;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate incoming vote request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteReq;
    dao.handle_request(message);

    // emulate incoming abort request
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAbort;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCPVoteYes, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation undo execution requested
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAOUndoRequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // emulate successful undo execution
    execution_result.ID = id;
    execution_result.success =2;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, ParticipantRequestResultOperationFailureUnsuccessful2PCParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate incoming vote request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCVoteReq;
    dao.handle_request(message);

    // emulate incoming abort request
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAbort;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate unsuccessful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 1;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, ParticipantOperationSuccessResult2PCUnsuccessfulCoordinator)
{
// set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCPVoteYes, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate incoming abort request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAbort;
    dao.handle_request(message);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, ParticipantOperationSuccessResultOperationUndone2PCUnsuccessfulCoordinator)
{
// set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, true);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=MoveSubtree;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCPVoteYes, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate incoming abort request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAbort;
    dao.handle_request(message);

    // operation undo execution requested
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAOUndoRequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // emulate successful undo execution
    execution_result.ID = id;
    execution_result.success =2;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, ParticipantOperationFailureResult2PCUnsuccessfulCoordinator)
{
// set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    // emulate unsuccessful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 1;
    dao.provide_operation_execution_result(execution_result);

    // own unsuccess vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCPVoteNo, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate incoming abort request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAbort;
    dao.handle_request(message);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, ParticipantResultOperationSuccess2PCUnsuccessfulCoordinator)
{
// set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate incoming abort request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAbort;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, ParticipantResultOperationSuccessOperationUndone2PCUnsuccessfulCoordinator)
{
// set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, true);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=MoveSubtree;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate incoming abort request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAbort;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(TPCPVoteYes, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation undo execution requested
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAOUndoRequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // emulate successful undo execution
    execution_result.ID = id;
    execution_result.success =2;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, ParticipantResultOperationFailure2PCUnsuccessfulCoordinator)
{
// set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);
    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=Rename;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==TwoPhaseCommit);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    // emulate incoming abort request
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=TPCAbort;
    dao.handle_request(message);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate unsuccessful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 1;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Acknowledgement sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(TPCAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

/* Testing the ordered operation execution protocol*/

/* First in order*/

TEST(TestSuite, CoordinatorSuccessfulOOE)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(true, false);

    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants; // at the beginning no participant is known, next participant set after execution of operation

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, OrderedOperationTest, participants, 0);
    ASSERT_TRUE(id!=0);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry=1;
    execution_result.next_participant = tree;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(OOEStartNext, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // next request sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    //provide successful next operations
    ExtractedMessage message;
    message.sending_server = string("127.0.0.1");
    message.sending_module = COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEAck;
    dao.handle_request(message);

    // Commit log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusCommit, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Client response
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE((*((char*)request.operation))==1);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);

    // No further activity TODO ?

}

TEST(TestSuite, CoordinatorUnsuccessfulOOEParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);

    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants; // at the beginning no participant is known, next participant set after execution of operation

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, OrderedOperationTest, participants, 0);
    ASSERT_TRUE(id!=0);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry=1;
    execution_result.next_participant = tree;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(OOEStartNext, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // next request sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    //provide unsuccessful next operations
    ExtractedMessage message;
    message.sending_server = string("127.0.0.1");
    message.sending_module = COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEAborted;
    dao.handle_request(message);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Client response
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE((*((char*)request.operation))==0);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);

    // No further activity TODO ?

}

TEST(TestSuite, CoordinatorUnsuccessfulOperationUndoneOOEParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, true);

    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants; // at the beginning no participant is known, next participant set after execution of operation

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, OOE_LBTest, participants, 0);
    ASSERT_TRUE(id!=0);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry=1;
    execution_result.next_participant = tree;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(OOEStartNext, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // next request sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    //provide unsuccessful next operations
    ExtractedMessage message;
    message.sending_server = string("127.0.0.1");
    message.sending_module = COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEAborted;
    dao.handle_request(message);

    // unsuccess of next participant logged
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(OOEUndo, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation undo execution requested
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp(operation, (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAOUndoRequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(strlen(operation)+2, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // emulate successful undo execution
    execution_result.ID = id;
    execution_result.success =2;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Client response
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE((*((char*)request.operation))==0);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);

    // No further activity TODO ?

}

TEST(TestSuite, CoordinatorUnsuccessfulOOECoordinator)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);

    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());

    // Set up operation content
    char* operation = (char*)malloc(5);
    operation[0] = 't';
    operation[1] = 'e';
    operation[2] = 's';
    operation[3] = 't';
    operation[4] = '\0';
    vector<Subtree> participants; // at the beginning no participant is known, next participant set after execution of operation

    // start operation
    uint64_t id = dao.start_da_operation(operation, strlen(operation)+1, OrderedOperationTest, participants, 0);
    ASSERT_TRUE(id!=0);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate unsuccessful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 1;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry=1;
    execution_result.next_participant = tree;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Client response
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE((*((char*)request.operation))==0);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(0, request.operation_length);
    ASSERT_EQ(id, request.ID);

    // No further activity TODO ?

}

/* Intermediate in order*/

TEST(TestSuite, IntermediateParticipantSuccessfulOOE)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(true, false);

    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=OrderedOperationTest;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=0;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=2;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry=1;
    execution_result.next_participant = tree;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(OOEStartNext, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // next request sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    //provide successful next operations
    message.sending_server = string("127.0.0.1");
    message.sending_module = COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEAck;
    dao.handle_request(message);

    // Commit log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusCommit, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Result sent to previous executor
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, IntermediateParticipantUnsuccessfulOOESameParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);

    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=OrderedOperationTest;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=0;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=2;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate unsuccessful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 1;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry=1;
    execution_result.next_participant = tree;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Result sent to previous executor
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEAborted, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, IntermediateParticipantUnsuccessfulOOEOtherParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);

    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=OrderedOperationTest;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=0;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=2;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry=1;
    execution_result.next_participant = tree;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(OOEStartNext, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // next request sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    //provide successful next operations
    message.sending_server = string("127.0.0.1");
    message.sending_module = COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEAborted;
    dao.handle_request(message);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Result sent to previous executor
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEAborted, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, IntermediateParticipantUnsuccessfulOperationUndoneOOEOtherParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, true);

    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=OOE_LBTest;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=0;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=2;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    Subtree tree;
    tree.server = string("127.0.0.1");
    tree.subtree_entry=1;
    execution_result.next_participant = tree;
    dao.provide_operation_execution_result(execution_result);

    // own success vote log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(OOEStartNext, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // next request sent
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEOpReq, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    //provide unsuccessful next operations
    message.sending_server = string("127.0.0.1");
    message.sending_module = COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = sizeof(char)+sizeof(uint64_t);
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEAborted;
    dao.handle_request(message);

    // unsuccess of next participant logged
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(OOEUndo, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation undo execution requested
    (*(dao.get_queue(DAO_LoadBalancing))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)(request.operation+1))==0);
    ASSERT_EQ(((char*)request.operation)[0], DAOUndoRequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    ASSERT_TRUE(dao.get_queue(DAO_LoadBalancing)->empty());
    // emulate successful undo execution
    execution_result.ID = id;
    execution_result.success =2;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Result sent to previous executor
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEAborted, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

/* Last in order*/
TEST(TestSuite, LastParticipantSuccessfulOOE)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(true, false);

    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=OrderedOperationTest;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate successful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 0;
    dao.provide_operation_execution_result(execution_result);

    // Commit log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusCommit, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Result sent to previous executor
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEAck, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);

    // No further activity TODO ?

}

TEST(TestSuite, LastParticipantUnsuccessfulOOESameParticipant)
{
    // set up test behavior for emulation
    CommunicationHandler::set_test_behavior(false, false);

    // test if queues are empty
    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());

    // set ID of distributed atomic operation
    uint64_t id = 0;

    // emulate incoming execution request
    // set up received message
    ExtractedMessage message;
    message.sending_server = string("127.0.0.0");
    message.sending_module=COM_DistributedAtomicOp;
    message.message_id=0;
    message.data_length = 2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t)+5;
    message.data = malloc(message.data_length);
    *(uint64_t*)(message.data+sizeof(char))=id;
    *((char*)message.data)=OOEOpReq;
    *((char*)(message.data+sizeof(char)+sizeof(uint64_t)))=OrderedOperationTest;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)))=1;
    *((InodeNumber*)(message.data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)))=0;
    *((uint32_t*)(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))=5;
    memcpy(message.data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)+sizeof(uint32_t),"test",5);

    // Get distributed atomic operations singleton
    DistributedAtomicOperations& dao = DistributedAtomicOperations::get_instance();
    ASSERT_TRUE(dao.get_queue(DAO_MetaData)->empty());
    // trigger execution of message
    dao.handle_request(message);

    // begin log
    JournalRequest journal_entry;
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusStart, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // operation execution requested
    OutQueue request;
    (*(dao.get_queue(DAO_MetaData))).wait_and_pop(request);
    ASSERT_TRUE(strcmp("test", (char*)request.operation+1)==0);
    ASSERT_EQ(((char*)request.operation)[0], DAORequest);
    ASSERT_TRUE(request.protocol==OrderedOperationExecution);
    ASSERT_EQ(6, request.operation_length);
    ASSERT_EQ(id, request.ID);
    free(request.operation);

    ASSERT_TRUE(Journal::get_queue().empty());
    ASSERT_TRUE(CommunicationHandler::get_queue().empty());
    // emulate unsuccessful execution
    InQueue execution_result;
    execution_result.ID = id;
    execution_result.success = 1;
    dao.provide_operation_execution_result(execution_result);

    // Abort log
    Journal::get_queue().wait_and_pop(journal_entry);
    ASSERT_EQ(StatusAbort, journal_entry.status);
    ASSERT_EQ(id, journal_entry.ID);

    // Result sent to previous executor
    SendRequest send_message;
    CommunicationHandler::get_queue().wait_and_pop(send_message);
    ASSERT_EQ(OOEAborted, send_message.message_type);
    ASSERT_EQ(id, send_message.ID);


    // No further activity TODO ?

}