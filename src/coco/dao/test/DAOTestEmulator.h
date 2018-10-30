#ifndef _DAOTESTEMULATOR_H_
#define _DAOTESTEMULATOR_H_

#include <cstring>
#include <set>

#include "coco/communication/ConcurrentQueue.h"
#include "coco/communication/CommonCommunicationTypes.h"
#include "mm/journal/CommonJournalTypes.h"
#include "coco/dao/CommonDAOTypes.h"
#include "coco/dao/DAOAdapter.h"
#include "global_types.h"

struct JournalRequest
{
    uint64_t ID;
    int status;
};

struct SendRequest
{
    uint64_t ID;
    int message_type;
};

enum ExtraStatus
{
    WrongParameter = -1, StatusNotFitData = -2, WrongDataLength=-3, WrongBeginData = -4, StatusStart = -5, StatusCommit =-6, StatusAbort=-7,
    MessageTypeNotFitData = -8, UnknownMessageType = -9
};

/**
 * Emulates CommunicationHandler. Provides send and handle_request method. In send method the recognizable behaviour of the receiver of the
 *  message is emulated. Therefore the intended behavior is set at the beginning.
 */
class CommunicationHandler
{
public:
    int init(CommunicationModule model_type)
    {
        return 0;
    }

    int start()
    {
        return 0;
    }

    int send(void* message,  int message_length, CommunicationModule goal, vector<string>& server_list)
    {
        //TODO return what and how?, should not influence later failure treatment

        //check module
        if (goal!=COM_DistributedAtomicOp)
        {
            SendRequest sr;
            sr.message_type = WrongParameter;
            queue.push(sr);
            return 0;
        }

        uint64_t ID = 0;
        if (message_length<sizeof(char)+sizeof(uint64_t))
        {
            SendRequest sr;
            sr.message_type = WrongDataLength;
            queue.push(sr);
            return 0;
        }
        else
        {
            ID = *((uint64_t*)(message+sizeof(char)));
        }

        //Forward content of send request

        // Emulate recognizable counter parts to whom the message is sent to
        switch ((ExchangeMessages)(((char*)message)[0]))
        {
            // handle external modified two phase commit operation (MTPCOp)events
        case MTPCOpReq:
        {
            // check server list
            if (server_list.size()!=1 || strcmp(server_list.at(0).c_str(),"127.0.0.1")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length<2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t)+sizeof(uint32_t)
                    ||message_length!=2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t)+sizeof(uint32_t)+*((uint32_t*)(message+2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t))))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message content (operation type, subtree entry point participant & coordinator, message length, message description)
            if (((*(char*)((message+sizeof(char)+sizeof(uint64_t))))!= Rename && !op_undo)
                    || (op_undo && (*(char*)((message+sizeof(char)+sizeof(uint64_t))))!= MoveSubtree)
                    || *((InodeNumber*)(message+2*sizeof(char)+sizeof(uint64_t)))!=1
                    || ((InodeNumber*)(message+2*sizeof(char)+sizeof(uint64_t)))[1]!=0
                    || *((uint32_t*)(message+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))!=5
                    || strcmp("test",(char*)(message+2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t)+sizeof(uint32_t)))!=0)
            {
                SendRequest sr;
                sr.message_type = MessageTypeNotFitData;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }

            // message ok
            SendRequest sr;
            sr.message_type =MTPCOpReq;
            sr.ID = ID;
            queue.push(sr);
            // returning sending success
            return 0;
            break;
        }

        case MTPCCommit:
        {
            // check server list
            if (server_list.size()!=1 || strcmp(server_list.at(0).c_str(),"127.0.0.0")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length!=sizeof(char)+sizeof(uint64_t))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }

            // message ok
            SendRequest sr;
            sr.message_type =MTPCCommit;
            sr.ID = ID;
            queue.push(sr);
            // returning sending success
            return 0;
            break;
        }

        case MTPCAbort:
        {
            // check server list
            if (server_list.size()!=1 || strcmp(server_list.at(0).c_str(),"127.0.0.0")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length!=sizeof(char)+sizeof(uint64_t))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }

            // message ok
            SendRequest sr;
            sr.message_type =MTPCAbort;
            sr.ID = ID;
            queue.push(sr);
            // returning sending success
            return 0;
            break;
        }

        case MTPCAck:
        {
            // check server list
            if (server_list.size()!=1 || strcmp(server_list.at(0).c_str(),"127.0.0.1")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length!=sizeof(char)+sizeof(uint64_t))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }

            // message ok
            SendRequest sr;
            sr.message_type =MTPCAck;
            sr.ID = ID;
            queue.push(sr);
            return 0;
            break;
        }

        // handle external ordered operation execution (OOEOp) events
        case OOEOpReq:
        {
            // check server list
            if (server_list.size()!=1 || strcmp(server_list.at(0).c_str(),"127.0.0.1")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length<2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t)+sizeof(uint32_t)
                    ||message_length!=2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t)+sizeof(uint32_t)+*((uint32_t*)(message+2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t))))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message content (operation type, subtree entry point participant & coordinator, message length, message description)
            if ((*((char*)(message+sizeof(char)+sizeof(uint64_t)))!= OrderedOperationTest && !op_undo)
                    || (*((char*)(message+sizeof(char)+sizeof(uint64_t)))!= OOE_LBTest && op_undo)
                    || *((InodeNumber*)(message+2*sizeof(char)+sizeof(uint64_t)))!=1
                    || ((InodeNumber*)(message+2*sizeof(char)+sizeof(uint64_t)))[1]!=0
                    || *((uint32_t*)(message+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))!=5
                    || strcmp("test",(char*)(message+2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t)+sizeof(uint32_t)))!=0)
            {
                SendRequest sr;
                sr.message_type = MessageTypeNotFitData;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }

            // message ok
            SendRequest sr;
            sr.message_type =OOEOpReq;
            sr.ID = ID;
            queue.push(sr);

            return 0;

            break;
        }

        case OOEAck:
        {
            // check server list
            if (server_list.size()!=1 || strcmp(server_list.at(0).c_str(),"127.0.0.0")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length!=sizeof(char)+sizeof(uint64_t))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }

            // message ok
            SendRequest sr;
            sr.message_type =OOEAck;
            sr.ID = ID;
            queue.push(sr);
            // nothing to emulate intermediate or last participant's operation finished
            return 0;

            break;
        }

        case OOEAborted:
        {
            // check server list
            if (server_list.size()!=1 || strcmp(server_list.at(0).c_str(),"127.0.0.0")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length!=sizeof(char)+sizeof(uint64_t))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }

            // message ok
            SendRequest sr;
            sr.message_type =OOEAborted;
            sr.ID = ID;
            queue.push(sr);
            // nothing to emulate intermediate or last participant's operation finished
            return 0;

            break;
        }
        // handle external two phase commit operation (TPCOp) events
        case TPCOpReq:
        {
            // check server list
            if (server_list.size()!=1 || (first && strcmp(server_list.at(0).c_str(),"127.0.0.1")!=0) || (!first && strcmp(server_list.at(0).c_str(),"127.0.0.2")!=0))
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                first = !first;
                return 0;
            }
            // check message length
            if (message_length<2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t)+sizeof(uint32_t)
                    ||message_length!=2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t)+sizeof(uint32_t)+*((uint32_t*)(message+2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t))))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                first = !first;
                return 0;
            }
            // check message content (operation type, subtree entry point participant & coordinator, message length, message description)
            if ((*((char*)(message+sizeof(char)+sizeof(uint64_t)))!= Rename && !op_undo)
                    || (*((char*)(message+sizeof(char)+sizeof(uint64_t)))!= MoveSubtree && op_undo)
                    || (first && *((InodeNumber*)(message+2*sizeof(char)+sizeof(uint64_t)))!=1)
                    || (!first &&*((InodeNumber*)(message+2*sizeof(char)+sizeof(uint64_t)))!=2)
                    || ((InodeNumber*)(message+2*sizeof(char)+sizeof(uint64_t)))[1]!=0
                    || *((uint32_t*)(message+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber)))!=5
                    || strcmp("test",(char*)(message+2*sizeof(char)+2*sizeof(InodeNumber)+sizeof(uint64_t)+sizeof(uint32_t)))!=0)
            {
                SendRequest sr;
                sr.message_type = MessageTypeNotFitData;
                sr.ID = ID;
                queue.push(sr);
                first = !first;
                return 0;
            }

            // message ok
            SendRequest sr;
            sr.message_type = TPCOpReq;
            sr.ID = ID;
            queue.push(sr);
            // Nothing to do, not visible for coordinator
            first = !first;
            // returning sending success
            return 0;
            break;
        }

        case TPCVoteReq:
        {
            // check server list
            if (server_list.size()!=2 || strcmp(server_list.at(0).c_str(),"127.0.0.1")!=0 || strcmp(server_list.at(1).c_str(),"127.0.0.2")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length!=sizeof(char)+sizeof(uint64_t))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // message ok
            SendRequest sr;
            sr.message_type =TPCVoteReq;
            sr.ID = ID;
            queue.push(sr);

            // returning sending success
            return 0;
            break;
        }

        case TPCVoteY: //received by coordinator
        {
            // check server list
            if (server_list.size()!=1 || strcmp(server_list.at(0).c_str(),"127.0.0.0")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length!=sizeof(char)+sizeof(uint64_t))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // message ok
            SendRequest sr;
            sr.message_type =TPCVoteY;
            sr.ID = ID;
            queue.push(sr);

            // returning sending success
            return 0;
            break;
        }

        case TPCVoteN: //received by coordinator
        {
            // check server list
            if (server_list.size()!=1 || strcmp(server_list.at(0).c_str(),"127.0.0.0")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length!=sizeof(char)+sizeof(uint64_t))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // message ok
            SendRequest sr;
            sr.message_type =TPCVoteN;
            sr.ID = ID;
            queue.push(sr);

            // returning sending success
            return 0;
            break;
        }

        case TPCCommit:
        {
            // check server list
            if (server_list.size()!=2 || strcmp(server_list.at(0).c_str(),"127.0.0.1")!=0 || strcmp(server_list.at(1).c_str(),"127.0.0.2")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length!=sizeof(char)+sizeof(uint64_t))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // message ok
            SendRequest sr;
            sr.message_type =TPCCommit;
            sr.ID = ID;
            queue.push(sr);

            // returning sending success
            return 0;
            break;
        }

        case TPCAbort:
        {
            // check server list
            if (server_list.size()!=2 || strcmp(server_list.at(0).c_str(),"127.0.0.1")!=0 || strcmp(server_list.at(1).c_str(),"127.0.0.2")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length!=sizeof(char)+sizeof(uint64_t))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // message ok
            SendRequest sr;
            sr.message_type =TPCAbort;
            sr.ID = ID;
            queue.push(sr);

            // returning sending success
            return 0;
            break;
        }

        case TPCAck: //received by coordinator
        {
            // check server list
            if (server_list.size()!=1 || strcmp(server_list.at(0).c_str(),"127.0.0.0")!=0)
            {
                SendRequest sr;
                sr.message_type = WrongParameter;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // check message length
            if (message_length!=sizeof(char)+sizeof(uint64_t))
            {
                SendRequest sr;
                sr.message_type = WrongDataLength;
                sr.ID = ID;
                queue.push(sr);
                return 0;
            }
            // message ok
            SendRequest sr;
            sr.message_type =TPCAck;
            sr.ID = ID;
            queue.push(sr);
            // Nothing to do, protocol finished
            // returning sending success
            return 0;
            break;
        }
        default:
        {
            SendRequest sr;
            sr.message_type = UnknownMessageType;
            sr.ID = ID;
            queue.push(sr);
            cout<<"Send unexpected message content."<<endl;
        }
        }
    }

    /*void set_behaviour()
    {

    }*/

    static ConcurrentQueue<SendRequest>& get_queue()
    {
        return queue;
    }

    static void set_test_behavior(bool external_execution_success, bool operation_undo)
    {
        success = external_execution_success;
        op_undo = operation_undo;
        first = true;
    }

    virtual void handle_request(ExtractedMessage& message) = 0;

private:
    static ConcurrentQueue<SendRequest> queue;
    static bool success;
    static bool first;
    static bool op_undo;
};

class Operation
{
public:
    char* get_data()
    {
        return NULL;
    }
    const OperationData& get_operation_data() const
    {
        return my_data;
    }
private:
    static const OperationData my_data;
    static OperationData init_data()
    {
        OperationData init_op;
        return init_op;
    }
};

class Journal
{
public:
    int32_t add_distributed(uint64_t ID, Module module, OperationType type, OperationStatus status, char* data, uint32_t data_size)
    {
        // provide operation ID
        JournalRequest jr;
        jr.ID = ID;

        //TODO return what and how?, should not influence later failure treatment

        // check correct calling parameters for module, type and status
        if (module!= DistributedAtomicOp || type!=DistributedOp )
        {
            jr.status = WrongParameter;
            journal_queue.push(jr);
            return 0;
        }

        if (status==DistributedStart)
        {
            // check correspondence between status and data size
            if (data==NULL || data_size<sizeof(char)+sizeof(uint32_t)+((uint32_t*)(data+sizeof(char)))[0])
            {
                jr.status=StatusNotFitData;
                journal_queue.push(jr);
                return 0;
            }
            //check content of begin log entry, should be the same for every test
            //check stored data_size
            if (((uint32_t*)(data+sizeof(char)))[0] != 5 || data_size-sizeof(uint32_t)-sizeof(char)!=5)
            {
                jr.status=WrongDataLength;
                journal_queue.push(jr);
                return 0;
            }
            else
            {
                //check content
                if (strcmp("test",data+sizeof(uint32_t)+sizeof(char))!=0)
                {
                    jr.status=WrongBeginData;
                    journal_queue.push(jr);
                    return 0;
                }
                else //content of begin log entry ok
                {
                    jr.status=StatusStart;
                    journal_queue.push(jr);
                    return 0;
                }
            }
        }
        else
        {
            if (status==DistributedUpdate)
            {
                // check correspondence between status and data size
                if (data_size!=1 || data==NULL)
                {
                    jr.status=StatusNotFitData;
                    journal_queue.push(jr);
                    return 0;
                }
            }
            else
            {
                if (status==Committed)
                {
                    // check correspondence between status and data size
                    if (data_size!=0 || data!=NULL)
                    {
                        jr.status=StatusNotFitData;
                        journal_queue.push(jr);
                        return 0;
                    }
                    // tell that log entry is ok
                    jr.status=StatusCommit;
                    journal_queue.push(jr);
                    return 0;
                }
                else
                {
                    if (status==Aborted)
                    {
                        // check correspondence between status and data size
                        if (data_size!=0 || data!=NULL)
                        {
                            jr.status=StatusNotFitData;
                            journal_queue.push(jr);
                            return 0;
                        }
                        // tell that log entry is ok
                        jr.status=StatusAbort;
                        journal_queue.push(jr);
                        return 0;
                    }
                    else // wrong status
                    {
                        jr.status=WrongParameter;
                        journal_queue.push(jr);
                        return 0;
                    }
                }
            }
        }
        // provide log content (it is a status log)
        jr.status=data[0];
        journal_queue.push(jr);
        //free(data);
        return 0;
    }

    int32_t get_last_operation(uint64_t operation_id, Operation& operation)
    {
        return -1;
    }

    int32_t get_all_operations(uint64_t operation_id, vector<Operation>& operations)
    {
        return 0;
        // TODO
    }

    void get_open_operations(set<uint64_t> open_operations) const
    {
        // TODO
    }

    static ConcurrentQueue<JournalRequest>& get_queue()
    {
        return journal_queue;
    }

private:
    static ConcurrentQueue<JournalRequest> journal_queue;
};

class JournalManager
{
private:
    static JournalManager manager;
    Journal journal;
public:
    static JournalManager* get_instance()
    {
        return &manager;
    }

    Journal* get_journal(InodeNumber inode_number)
    {
        return &journal;
    }

    Journal* get_server_journal()
    {
        return &journal;
    }

    Journal* create_server_journal()
    {
        return &journal;
    }

    Journal* create_journal(InodeNumber inode_id)
    {
        return &journal;
    }


    void get_journals(set<InodeNumber> journal_set) const
    {
        // TODO
    }

    InodeNumber get_open_operations(uint64_t operation_id, vector<Operation>& operations)
    {
        // TODO
        return 0;
    }

};

class UniqueIdGenerator
{
public:
    UniqueIdGenerator()
    {
        id = 0;
    }

    uint64_t get_next_id()
    {
        id++;
        return id;
    }
private:
    uint64_t id;
};

class DAOAdapterTestRealization : public DAOAdapter
{
public:
    DAOAdapterTestRealization() {}

    virtual ~DAOAdapterTestRealization() {}

    static bool is_coordinator(DAOperation* uncomplete_operation)
    {
        return coordinator;
    }

    static int set_sending_addresses(DAOperation* uncomplete_operation)
    {
        return 0;
    }

    static int set_subtree_entry_point(DAOperation* uncomplete_operation)
    {
        return 0;
    }

    void set_test_behavior(bool success, bool coordinator)
    {
        this->success = success;
        this->coordinator = coordinator;
        ack_done = false;

    }

    bool operation_result_handled()
    {
        return ack_done;
    }

protected:

    virtual Subtree get_next_participant(void* operation, uint32_t operation_length)
    {
        Subtree x;
        x.server = string("127.0.0.1");
        x.subtree_entry = 0;
        return x;
    }

    virtual void handle_operation_result(uint64_t id, bool success)
    {
        if (id!= 0 || success == false)
        {
            throw runtime_error("Wrong acknowledgement received.");
        }
        ack_done = true;

    }

    virtual bool handle_operation_request(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        if ( id != 0 || operation_length != 5 || strcmp("test",(char*)operation_description) != 0)
        {
            return !success;
        }
        return success;
    }

    virtual bool handle_operation_rerequest(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        if ( id != 0 || operation_length != 5 || strcmp("test",(char*)operation_description) != 0)
        {
            return !success;
        }
        return success;
    }

    virtual bool handle_operation_undo_request(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        if ( id != 0 || operation_length != 5 || strcmp("test",(char*)operation_description) != 0)
        {
            return !success;
        }
        return success;
    }

    virtual bool handle_operation_reundo_request(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        if ( id != 0 || operation_length != 5 || strcmp("test",(char*)operation_description) != 0)
        {
            return !success;
        }
        return success;
    }

private:
    static bool coordinator;
    bool success;
    bool ack_done;
};

class DaoHandler: public DAOAdapter
{
public:
    DaoHandler() {}

    virtual ~DaoHandler() {}

    static bool is_coordinator(DAOperation* uncomplete_operation)
    {
        return true;
    }

    static int set_sending_addresses(DAOperation* uncomplete_operation)
    {
        return 0;
    }

    static int set_subtree_entry_point(DAOperation* uncomplete_operation)
    {
        return 0;
    }

protected:

    virtual Subtree get_next_participant(void* operation, uint32_t operation_length)
    {
        Subtree x;
        return x;
    }

    virtual void handle_operation_result(uint64_t id, bool success)
    {
    }

    virtual bool handle_operation_request(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        return true;
    }

    virtual bool handle_operation_rerequest(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        return true;
    }

    virtual bool handle_operation_undo_request(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        return true;
    }

    virtual bool handle_operation_reundo_request(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        return true;
    }
};

class ChangeOwnershipDAOAdapter :public DAOAdapter
{
public:
    ChangeOwnershipDAOAdapter() {}

    virtual ~ChangeOwnershipDAOAdapter() {}

    static bool is_coordinator(DAOperation* uncomplete_operation)
    {
        return true;
    }

    static int set_sending_addresses(DAOperation* uncomplete_operation)
    {
        return 0;
    }

    static int set_subtree_entry_point(DAOperation* uncomplete_operation)
    {
        return 0;
    }

protected:

    virtual Subtree get_next_participant(void* operation, uint32_t operation_length)
    {
        Subtree x;
        return x;
    }

    virtual void handle_operation_result(uint64_t id, bool success)
    {

    }

    virtual bool handle_operation_request(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        return true;
    }

    virtual bool handle_operation_rerequest(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        return true;
    }

    virtual bool handle_operation_undo_request(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        return true;
    }

    virtual bool handle_operation_reundo_request(uint64_t id, void* operation_description, uint32_t operation_length)
    {
        return true;
    }
};
#endif //_TESTEMULATOR_H_