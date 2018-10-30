/**
 * \file DistributedAtomicOperations.cpp
 *
 * \author 	Benjamin Raddatz	Marie-Christine Jakobs
 *
 * \brief Realization of the DistributedAtomicOperations.h header file
 *
 * Implements the course of three different protocols for the execution of distributed atomic operations. For every protocol different
 * events must be handled. The execution by other protocols is requested. External servers must be invited to participat in the operation,
 * internal and external execution results are considered.
 */

#include <iostream>
#include <cstdlib>
#include <error.h>
#include <stdexcept>
#include <exception>
#include <sstream>
#include <set>

#include "coco/dao/DistributedAtomicOperations.h"
#include "coco/communication/CommonCommunicationTypes.h"
#include "coco/communication/CommunicationFailureCodes.h"
#include "coco/communication/SendCommunicationServer.h"
#include "coco/coordination/CoordinationFailureCodes.h"
#include "coco/coordination/MltHandlerException.h"
#include "mm/journal/JournalException.h"


#ifndef DAO_TESTING
#include "mm/journal/Operation.h"
#include "coco/loadbalancing/DaoHandler.h"
#include "mm/storage/ChangeOwnershipDAOAdapter.h"
#endif

#include "global_types.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

using namespace std;

/**
 * Declaration and definition of class variable instance which represents the singleton instance.
 */
DistributedAtomicOperations DistributedAtomicOperations::instance = DistributedAtomicOperations();

/**
 * \brief Destructor of RecCommunicationServer
 *
 * Cleans up. Therefore destroys mutex, removes all open distributed atomic operations from temporary datastructure and frees operation
 * description.
 */
DistributedAtomicOperations::~DistributedAtomicOperations()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    // free all pointers in open_operations and remove all data
    unordered_map<uint64_t,DAOperation>::iterator it = open_operations.begin();
    while (it!=open_operations.end())
    {
        try
        {
            free(((*it).second).operation);
            it = open_operations.erase(it);
        }
        catch (exception e)
        {
            it = open_operations.begin();
        }
    }

    // clean up TPC_received_request_answers
    TPC_received_request_answers.erase(TPC_received_request_answers.begin(), TPC_received_request_answers.end());

    //Destroy mutex
    failure = pthread_mutex_destroy(&inc_event_mutex);
#ifdef LOGGING_ENABLED
    stringstream error_message;
    error_message<<"Mutex for internal event synchronization cannot be destroyed due to "<<failure;
    dao_logger.error_log(error_message.str().c_str());
#endif
    //TODO remove content from queues due to malloc elements there? or queues static?

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
* \brief Constructor of DistributedAtomicOperations
*
* \throw runtime_error if underlying communication handler cannot be initialized, thread or mutex cannot be created
*
* Initializes the communication handler. Starts the thread for processing execution results and creates the mutex.
*/
DistributedAtomicOperations::DistributedAtomicOperations()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

#ifdef LOGGING_ENABLED
    dao_logger.set_log_location(DAO_LOGFILE);
    dao_logger.set_console_output(DAO_VISUALOUTPUT);
    dao_logger.set_log_level(DAO_LOGLEVEL);
#endif
    int failure;
    journal_manager = JournalManager::get_instance();

    failure = pthread_mutex_init(&inc_event_mutex, NULL);
    if (failure)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message;
        error_message<<"Mutex for internal event synchronization cannot be created due to "<<failure;
        dao_logger.error_log(error_message.str().c_str());
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        //mutex cannot be created
        throw runtime_error("Mutex for event synchronization cannot be created.");
    }
    failure = pthread_create(&handling_thread, NULL,&DistributedAtomicOperations::listen_queue, this);
    if (failure)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message2;
        error_message2<<"Thread for handling incoming results of the executing modules cannot be created due to "<<failure;
        dao_logger.error_log(error_message2.str().c_str());
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        // thread cannot be created
        throw runtime_error("Thread handling incoming operation request results cannot be created.");
    }
    failure = pthread_create(&find_thread, NULL,&DistributedAtomicOperations::find_timeout, this);
    if (failure)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message2;
        error_message2<<"Thread for finding timeouts cannot be created due to "<<failure;
        dao_logger.error_log(error_message2.str().c_str());
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        // thread cannot be created
        throw runtime_error("Thread for finding timeouts cannot be created.");
    }
    // no recovery done so far
    recovery_done = false;

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}


/**
 *\brief Returns a pointer to the queue used for communication from distributed atomic operation module to the respective module
 *
 *\param module - Module for which the queue containing execution requests and information about distributed atomic operation results should be retrieved
 *
 *\return Pointer to communication queue for the respective module or NULL if the module is not considered for distributed atomic operations
 *
 * Provides a reference to a queue. Every module has its own queue. The queue is used in producer consumer pattern. The distributed
 * atomic operation module produces execution requests and client responses which are consumed by the <code>module</code>.
 */
ConcurrentQueue<OutQueue>* DistributedAtomicOperations::get_queue(DistributedAtomicOperationModule module)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    if (module==DAO_LoadBalancing)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return &load_queue;
    }
    if (module==DAO_MetaData)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return &mdi_queue;
    }
#ifdef LOGGING_ENABLED
    stringstream warning_message;
    warning_message<<"The module "<<module<<" is not considered for distributed atomic operations.";
    dao_logger.warning_log(warning_message.str().c_str());
#endif
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return NULL;
}

/**
 *\brief Inform distributed atomic operations module about an execution result
 *
 *\param result - Result of the execution of a part of a distributed atomic operation
 *
 * Provides the result of a requested part execution of a distributed atomic operation to the distributed atomic operation module.
 * Afterwards the distributed atomic operation module can and will process the result and continue the execution of the protocol.
 */
void DistributedAtomicOperations::provide_operation_execution_result(InQueue& result)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    in_queue.push(result);

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
 *\brief Processes incoming execution results.
 *
 *\param ptr - required by pthread implementation, not needed for the implementation, can be NULL
 *
 *\return endless loop execution, no return value expected, would return NULL if end of method was reached
 *
 * This method is executed in a separate thread and runs an endless loop. The next execution result is taken from the <code>in_queue</code>.
 * The execution result is analyzed and the respective event is triggered and handled by <code>handle_event</code> afterwards.
 *
 *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched, failure caused by mutex
 */
void* DistributedAtomicOperations::listen_queue(void *ptr)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    InQueue operation_reply;
    while (1)
    {
        // take next event from internal modules, only finished operation events
        instance.in_queue.wait_and_pop(operation_reply);
        DAOperation* operation;
        Events event;
        //find corresponding operation
        unordered_map<uint64_t,DAOperation>::iterator it = instance.open_operations.find(operation_reply.ID);
        if (it!=instance.open_operations.end())
        {
            operation = &((*it).second);
        }
        else
        {
#ifdef LOGGING_ENABLED
            instance.dao_logger.warning_log("The required operation cannot be found in the temporary datastructure.");
#endif
            if (!instance.recovery_done)
            {
#ifdef LOGGING_ENABLED
                instance.dao_logger.warning_log("The distributed atomic operations have not been recovered yet.");
#endif
                continue;
            }
            // try to recover, only one recovery per time
            Pc2fsProfiler::get_instance()->function_sleep();
            //aquire mutex to avoid simultaneous changes on an open operation
            failure = pthread_mutex_lock( &(instance.inc_event_mutex) );
            Pc2fsProfiler::get_instance()->function_wakeup();
            if (failure)
            {
#ifdef LOGGING_ENABLED
                stringstream error_message;
                error_message<<"Mutex for internal event synchronization cannot be created due to "<<failure;
                instance.dao_logger.error_log(error_message.str().c_str());
#endif
                instance.handle_internal_failure(DAOMutexFailure);
            }
            failure = instance.retrieve_lost_operation(operation_reply.ID);
            failure = pthread_mutex_unlock(&(instance.inc_event_mutex));
#ifdef LOGGING_ENABLED
            if (failure)
            {
                stringstream warning_message;
                warning_message<<"Mutex cannot be unlocked due to "<<failure;
                instance.dao_logger.warning_log(warning_message.str().c_str());
            }
#endif
            if (failure)
            {
                continue;
            }
            else
            {
                it = instance.open_operations.find(operation_reply.ID);
                operation = &((*it).second);
            }
        }
        Pc2fsProfiler::get_instance()->function_sleep();
        //aquire mutex to avoid simultaneous changes on an open operation
        failure = pthread_mutex_lock( &(instance.inc_event_mutex) );
        Pc2fsProfiler::get_instance()->function_wakeup();
        if (failure)
        {
#ifdef LOGGING_ENABLED
            stringstream error_message;
            error_message<<"Mutex for internal event synchronization cannot be created due to "<<failure;
            instance.dao_logger.error_log(error_message.str().c_str());
#endif
            instance.handle_internal_failure(DAOMutexFailure);
        }
        string sender;
        switch (operation_reply.success)
        {
        case ExecutionSuccessful:   // suboperation successfully executed and loggied if logging is done for operation
        {
            if (operation->used_protocol==TwoPhaseCommit)   // create and process respective event for finishing operation
            {
                if (operation->status==TPCCoordinatorComp)   // coordinator
                {
                    failure = instance.handle_event(TPCIOperationFinSuccess, operation);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        instance.dao_logger.error_log("Treatment of successful two phase commit protocol operation execution failed.");
#endif
                        instance.handle_unsuccessful_protocol_step(TPCIOperationFinSuccess, failure, operation, false, TPCCoordinatorVReqSend,sender);
                    }
                }
                else   // participant
                {
                    failure = instance.handle_event(TPCPOperationFinSuccess, operation);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        instance.dao_logger.error_log("Treatment of successful two phase commit protocol operation execution failed.");
#endif
                        // try to redo operation
                        if (operation->status == TPCPartComp)
                        {
                            instance.handle_unsuccessful_protocol_step(TPCPOperationFinSuccess, failure, operation, false, TPCPartWaitVReqYes, sender);
                        }
                        else
                        {
                            if (operation->status==TPCPartVReqRec)
                            {
                                instance.handle_unsuccessful_protocol_step(TPCPOperationFinSuccess, failure, operation, false, TPCPartWaitVResultExpectYes, sender);
                            }
                            else
                            {
                                if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
                                {
                                    instance.handle_unsuccessful_protocol_step(TPCPOperationFinSuccess, failure, operation, false, TPCWaitUndoToFinish, sender);
                                }
                                else
                                {
                                    instance.handle_unsuccessful_protocol_step(TPCPOperationFinSuccess, failure, operation, true, (Status)0, sender);
                                }
                            }
                        }
                    }
                }
            }
            else   // create and process respective event for finishing operation
            {
                if (operation->used_protocol==ModifiedTwoPhaseCommit)
                {
                    if (operation->status==MTPCCoordinatorComp)   // coordinator
                    {
                        failure = instance.handle_event(MTPCIOperationFinSuccess, operation);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            instance.dao_logger.error_log("Treatment of successful modified two phase commit protocol operation execution failed.");
                            // failure treatment
#endif
                            instance.handle_unsuccessful_protocol_step(MTPCIOperationFinSuccess, failure, operation, false, MTPCCoordinatorReqSend, sender);
                        }
                    }
                    else   // participant
                    {
                        failure = instance.handle_event(MTPCPOperationFinSuccess, operation);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            instance.dao_logger.error_log("Treatment of successful modified two phase commit protocol operation execution failed.");
#endif
                            instance.handle_unsuccessful_protocol_step(MTPCPOperationFinSuccess, failure, operation, false, MTPCPartVoteSendYes, sender);
                        }
                    }
                }
                else // ordered operation execution
                {  // create and process respective event for finishing operation
                    if (operation_reply.next_participant.server.length()!=0)   // first or intermediate executing participant
                    {
                        if (operation->participants.size()==0 || (operation->participants.size()<2 &&
                                operation->participants.at(0).subtree_entry != operation_reply.next_participant.subtree_entry))
                        {
                            operation->participants.push_back(operation_reply.next_participant);
                        }
                        failure = instance.handle_event(OOEOperationFinishedSuccess, operation);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            instance.dao_logger.error_log("Treatment of successful ordered operation execution protocol operation execution failed.");
#endif
                            instance.handle_unsuccessful_protocol_step(OOEOperationFinishedSuccess, failure, operation, false, OOEWaitResult, sender);
                        }
                    }
                    else   // last executing participant
                    {
                        failure = instance.handle_event(OOELastOperationFinishedSuccess, operation);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            instance.dao_logger.error_log("Treatment of successful ordered operation execution protocol operation execution failed.");
#endif
                            instance.handle_unsuccessful_protocol_step(OOELastOperationFinishedSuccess, failure, operation, true, (Status)0, sender);
                        }
                    }
                }
            }
            break;
        }
        case ExecutionUnsuccessful:   //no successful execution of operation
        {
            if (operation->used_protocol==TwoPhaseCommit)   // create and process respective event for finishing operation
            {
                if (operation->status==TPCCoordinatorComp)   // coordinator
                {
                    failure = instance.handle_event(TPCIOperationFinFailure, operation);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        instance.dao_logger.error_log("Treatment of unsuccessful two phase commit protocol operation execution failed.");
#endif
                        if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
                        {
                            instance.handle_unsuccessful_protocol_step(TPCIOperationFinFailure, failure, operation, false, TPCWaitUndoAck, sender);
                        }
                        else
                        {
                            instance.handle_unsuccessful_protocol_step(TPCIOperationFinFailure, failure, operation, false, TPCAborting, sender);
                        }
                    }
                }
                else   // participant
                {
                    failure = instance.handle_event(TPCPOperationFinFailure, operation);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        instance.dao_logger.error_log("Treatment of unsuccessful two phase commit protocol operation execution failed.");
#endif
                        // try to redo
                        if (operation->status == TPCPartComp)
                        {
                            instance.handle_unsuccessful_protocol_step(TPCPOperationFinFailure, failure, operation, false, TPCPartWaitVReqNo, sender);
                        }
                        else
                        {
                            if (operation->status == TPCPartVReqRec)
                            {
                                instance.handle_unsuccessful_protocol_step(TPCPOperationFinFailure, failure, operation, false, TPCPartWaitVResultExpectNo, sender);
                            }
                            else
                            {
                                instance.handle_unsuccessful_protocol_step(TPCPOperationFinFailure, failure, operation, true, (Status)0, sender);
                            }
                        }
                    }
                }
            }
            else
            {
                // create and process respective event for finishing operation
                if (operation->used_protocol==ModifiedTwoPhaseCommit)
                {
                    if (operation->status==MTPCCoordinatorComp)   // coordinator
                    {
                        failure = instance.handle_event(MTPCIOperationFinFailure, operation);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            instance.dao_logger.error_log("Treatment of unsuccessful modified two phase commit protocol operation execution failed.");
#endif
                            instance.handle_unsuccessful_protocol_step(MTPCIOperationFinFailure, failure, operation, true, (Status) 0, sender);
                        }
                    }
                    else   // participant
                    {
                        failure = instance.handle_event(MTPCPOperationFinFailure, operation);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            instance.dao_logger.error_log("Treatment of unsuccessful modified two phase commit protocol operation execution failed.");
#endif
                            instance.handle_unsuccessful_protocol_step(MTPCPOperationFinFailure, failure, operation, false, MTPCPartVoteSendNo, sender);
                        }
                    }
                }
                else // ordered operation execution
                {  // create and process respective event for finishing operation
                    failure = instance.handle_event(OOEOperationFinishedFailure, operation);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        instance.dao_logger.error_log("Treatment of unsuccessful ordered operation execution protocol operation execution failed.");
#endif
                        instance.handle_unsuccessful_protocol_step(OOEOperationFinishedFailure, failure, operation, true, (Status) 0, sender);
                    }
                }
            }
            break;
        }
        case UndoSuccessful:   // successful undo of suboperation
        {
            if (operation->used_protocol==TwoPhaseCommit)   // create and process respective event for finishing operation
            {
                failure = instance.handle_event(TPCOperationUndone, operation);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    instance.dao_logger.error_log("Treatment of successful two phase commit protocol operation execution undo failed.");
#endif
                    if (operation->status == TPCWaitUndoAck)
                    {
                        instance.handle_unsuccessful_protocol_step(TPCOperationUndone, failure, operation, false, TPCAborting, sender);
                    }
                    else
                    {
                        instance.handle_unsuccessful_protocol_step(TPCOperationUndone, failure, operation, true, (Status)0, sender);
                    }
                }
            }
            else
            {
                // create and process respective event for finishing operation
                if (operation->used_protocol==ModifiedTwoPhaseCommit)
                {
                    failure = instance.handle_event(MTPCOperationUndone, operation);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        instance.dao_logger.error_log("Treatment of successful modified two phase commit protocol operation execution undo failed.");
#endif
                        instance.handle_unsuccessful_protocol_step(MTPCOperationUndone, failure, operation, true, (Status)0, sender);
                    }
                }
                else // ordered operation execution
                {  // create and process respective event for finishing operation
                    failure = instance.handle_event(OOEOperationUndone, operation);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        instance.dao_logger.error_log("Treatment of successful ordered operation execution protocol operation execution undo failed.");
#endif
                        instance.handle_unsuccessful_protocol_step(OOEOperationUndone, failure, operation, true, (Status)0, sender);
                    }
                }
            }
            break;
        }
        case UndoUnsuccessful:   //unsuccessful undo of suboperation
        {
#ifdef LOGGING_ENABLED
            instance.dao_logger.error_log("Requested undo failed.");
#endif
            // nothing to do wait for time out recovery/failure treatment or do failure treatment here
            break;
        }
        default:
        {
#ifdef LOGGING_ENABLED
            instance.dao_logger.error_log("Unexpected answer of executing module.");
#endif
        }
        }
        failure = pthread_mutex_unlock(&(instance.inc_event_mutex));
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message;
            warning_message<<"Mutex cannot be unlocked due to "<<failure;
            instance.dao_logger.warning_log(warning_message.str().c_str());
        }
#endif
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return NULL;
}

/**
 *\brief Implement request handling
 *
 *\param message - external request sent to this module
 *
 * Implements the abstract method of the communication handler. Handles all incoming external messages. Incoming external messages
 * cause events which are processed during request handling.
 *
 *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched
 */
void DistributedAtomicOperations::handle_request(ExtractedMessage& message)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    if (!recovery_done)
    {
#ifdef LOGGING_ENABLED
        dao_logger.warning_log("Recovery has not been done yet.");
#endif
        free(message.data);
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return;
    }
    // check if the correct module sends request
    if (message.sending_module != COM_DistributedAtomicOp)
    {
        free(message.data);
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();
        return;
    }
    // check if normal message is received (no request reply)
    if (message.message_id != 0)
    {
        free(message.data);
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();
        return;
    }
    uint64_t ID;
    //extract operation ID from external message, second part of message
    memcpy( &ID, message.data+ sizeof(char), sizeof(uint64_t));

    //aquire mutex to avoid simultaneous changes on an open operation
    int failure;
    Pc2fsProfiler::get_instance()->function_sleep();
    failure = pthread_mutex_lock( &inc_event_mutex );
    Pc2fsProfiler::get_instance()->function_wakeup();
    if (failure)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message;
        error_message<<"Mutex for internal event synchronization cannot be created due to "<<failure;
        dao_logger.error_log(error_message.str().c_str());
#endif
        handle_internal_failure(DAOMutexFailure);
    }

    ExchangeMessages event = (ExchangeMessages)(((char*)message.data)[0]);
    bool in_journal = true;
    bool operation_finished = false;
    // find corresponding operation if it is not an operation request
    DAOperation* operation = NULL;
    unordered_map<uint64_t,DAOperation>::iterator it;
    if (event != MTPCOpReq && event != OOEOpReq && event != TPCOpReq && event!= ContentResponse && event!= StatusResponse)
    {
        it = open_operations.find(ID);
        if (it!=open_operations.end())
        {
            operation = &((*it).second);

        }
        else
        {
#ifdef LOGGING_ENABLED
            dao_logger.warning_log("The required operation cannot be found in the temporary datastructure.");
#endif
            int failure = retrieve_lost_operation(ID);
            if (failure)
            {
                if (failure == DAOOperationNotInJournal || failure == DAOOperationFinished)
                {
                    operation = NULL;
                    operation_finished = (failure == DAOOperationFinished);
                }
                else
                {
                    if (failure == DAONoBeginLog || failure == DAOUnknownLog)
                    {
                        vector<string> receiver;
                        receiver.push_back(message.sending_server);
                        if (failure == DAONoBeginLog)
                        {
                            send_simple_message(ID, ContentRequest, receiver);
                        }
                        else
                        {
                            send_simple_message(ID, StatusRequest, receiver);
                        }
                        free(message.data);
                        failure = pthread_mutex_unlock(&inc_event_mutex);
#ifdef LOGGING_ENABLED
                        if (failure)
                        {
                            stringstream warning_message;
                            warning_message<<"Mutex cannot be unlocked due to "<<failure;
                            dao_logger.warning_log(warning_message.str().c_str());
                        }
#endif
                        // ends profiling
                        Pc2fsProfiler::get_instance()->function_end();

                        return;
                    }
                }

            }
            else
            {
                it = open_operations.find(ID);
                operation = &((*it).second);
            }
        }
        //check if request is allowed, operation requests need not be checked since always allowed
        failure = check_allowed_request(event, message.sending_server,operation);
        if (failure)
        {
            // treat different failures
            //request from unknown servers are ignored

            // event does not fit to protocol -> rerequest event
            if (failure == DAOWrongEvent)
            {
                vector<string> receiver;
                receiver.push_back(message.sending_server);
                send_simple_message(ID, EventReRequest, receiver);

            }
            if (failure == DAOSelfWrongServer)
            {
                // operation not finished, cannot be retrieved -> send notifier to sender, otherwise message ignored
                if (!(operation_finished && (event == MTPCAck || event == TPCAck || event == OOEAck || event ==  OOEAborted
                                             || event == TPCRCommit || event == MTPCRAbort || event == MTPCRCommit)))
                {
                    vector<string> receiver;
                    receiver.push_back(message.sending_server);
                    send_simple_message(ID, NotResponsible, receiver);
                }
            }
            if (failure == DAODifferentStatus)
            {
                // status does not fit request, possibly corrupted message --> request correct event
                vector<string> receiver;
                receiver.push_back(message.sending_server);
                send_simple_message(ID, EventReRequest, receiver);
            }
            // no further processing allowed -> tidy up
            free(message.data);
            failure = pthread_mutex_unlock(&inc_event_mutex);
#ifdef LOGGING_ENABLED
            if (failure)
            {
                stringstream warning_message;
                warning_message<<"Mutex cannot be unlocked due to "<<failure;
                dao_logger.warning_log(warning_message.str().c_str());
            }
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return;
        }
    }

    if (event == NotResponsible || event == EventReRequest || event == ContentRequest || event == ContentResponse || event == StatusRequest
            || event == StatusResponse)
    {
        failure = handle_failure_messages(message.data, &event, ID, operation, message.sending_server);
        if (failure || event == ((char*)message.data)[0])
        {
            free(message.data);
            failure = pthread_mutex_unlock(&inc_event_mutex);
#ifdef LOGGING_ENABLED
            if (failure)
            {
                stringstream warning_message;
                warning_message<<"Mutex cannot be unlocked due to "<<failure;
                dao_logger.warning_log(warning_message.str().c_str());
            }
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return;
        }
    }

    // distinguish incoming messages by message type, first part of message
    switch (event)
    {
        // handle external modified two phase commit operation (MTPCOp)events
    case MTPCOpReq:
    {
        OperationTypes operation_type;
        InodeNumber subtree_entry_this, subtree_entry_initiator;
        uint32_t operation_length;
        char* operation;
        vector<Subtree> initiator;

        // Get kind of operation which is executed, e.g. rename
        operation_type = (OperationTypes) (((char*)message.data)[sizeof(char)+sizeof(uint64_t)]);
        // Get subtree entry point, needed to get access to the correct journal
        memcpy( &subtree_entry_this, message.data+ 2*sizeof(char)+sizeof(uint64_t), sizeof(InodeNumber));
        // Get subtree entry point of coordinator
        memcpy( &subtree_entry_initiator, message.data+ 2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber), sizeof(InodeNumber));
        // Get operation description
        memcpy( &operation_length, message.data+ 2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)+sizeof(InodeNumber), sizeof(uint32_t));
        operation = (char*)malloc(operation_length);
        if (operation == NULL)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Cannot get enough memory to create operation description.");
#endif
            handle_internal_failure(DAOInsufficientMemory);
        }

        memcpy( operation, message.data+ 2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)+sizeof(InodeNumber)+sizeof(uint32_t), operation_length);

        // Build coordinator information
        Subtree initiator_subtree;
        initiator_subtree.server = message.sending_server;
        initiator_subtree.subtree_entry = subtree_entry_initiator;
        initiator.push_back(initiator_subtree);

        // start the distributed operation as participant
        failure = start_da_operation_participant( operation,  operation_length,  operation_type, initiator, subtree_entry_this, ID, ModifiedTwoPhaseCommit);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            stringstream error_message2;
            error_message2<<"Distributed operation with ID "<<ID<<" cannot be started due to "<<failure;
            dao_logger.error_log(error_message2.str().c_str());
#endif
            // failure == DAOOperationExisting || failure == DAONoSal || failure == DAONoMlt, nothing can be done
            if (failure == DAOSelfWrongServer)
            {
                // send notifier to sender that wrong server
                vector<string> receiver;
                receiver.push_back(message.sending_server);
                send_simple_message(ID, NotResponsible, receiver);
            }
            if (failure == DAOSubtreeNotExisting)
            {
                vector<Operation> journal_ops;
                InodeNumber correct_journal = (*journal_manager).get_open_operations(ID, journal_ops);
                if (correct_journal!= INVALID_INODE_ID)
                {
                    // reexecute start operation
                    start_da_operation_participant( operation,  operation_length,  operation_type, initiator, correct_journal, ID, ModifiedTwoPhaseCommit);
                }

            }
        }
        break;
    }

    case MTPCCommit:
    {
        failure = handle_event(MTPCICommit,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing an incoming commit event for MTPC protocol failed.");
#endif
            handle_unsuccessful_protocol_step(MTPCICommit, failure, operation, true, (Status)0, message.sending_server);
        }
        break;
    }

    case MTPCAbort:
    {
        failure = handle_event(MTPCIAbort,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing an incoming abort event for MTPC protocol failed.");
#endif
            if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
            {
                handle_unsuccessful_protocol_step(MTPCIAbort, failure, operation, false, MTPCIWaitResultUndone,message.sending_server);
            }
            else
            {
                handle_unsuccessful_protocol_step(MTPCIAbort, failure, operation, true, (Status)0,message.sending_server);
            }
        }
        break;
    }

    case MTPCAck:
    {
        failure = handle_event(MTPCPAckRec,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing acknowledgement for MTPC protocol failed.");
#endif
            handle_unsuccessful_protocol_step(MTPCPAckRec, failure, operation, true, (Status)0,message.sending_server);
        }
        break;
    }

    // handle external ordered operation execution (OOEOp) events
    case OOEOpReq:
    {
        OperationTypes operation_type;
        InodeNumber subtree_entry_this, subtree_entry_initiator;
        uint32_t operation_length;
        char* operation;
        vector<Subtree> initiator;

        // Get kind of operation which is executed, e.g. rename
        operation_type = (OperationTypes) (((char*)message.data)[sizeof(char)+sizeof(uint64_t)]);
        // Get subtree entry point, needed to get access to the correct journal
        memcpy( &subtree_entry_this, message.data+ 2*sizeof(char)+sizeof(uint64_t), sizeof(InodeNumber));
        // Get subtree entry point predecessor
        memcpy( &subtree_entry_initiator, message.data+ 2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber), sizeof(InodeNumber));
        // Get operation description
        memcpy( &operation_length, message.data+ 2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)+sizeof(InodeNumber), sizeof(uint32_t));
        operation = (char*) malloc(operation_length);
        if (operation == NULL)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Cannot get enough memory to create operation description.");
#endif
            handle_internal_failure(DAOInsufficientMemory);
        }
        memcpy( operation, message.data+ 2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)+sizeof(InodeNumber)+sizeof(uint32_t), operation_length);

        // Build predecessor information
        Subtree initiator_subtree;
        initiator_subtree.server = message.sending_server;
        initiator_subtree.subtree_entry = subtree_entry_initiator;
        initiator.push_back(initiator_subtree);

        // start the distributed operation as participant
        failure = start_da_operation_participant( operation,  operation_length,  operation_type,
                  initiator, subtree_entry_this, ID, OrderedOperationExecution);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            stringstream error_message3;
            error_message3<<"Distributed operation with ID "<<ID<<" cannot be started due to "<<failure;
            dao_logger.error_log(error_message3.str().c_str());
#endif
            // failure == DAOOperationExisting || failure == DAONoSal || failure == DAONoMlt, nothing can be done
            if (failure == DAOSelfWrongServer)
            {
                // send notifier to sender that wrong server
                vector<string> receiver;
                receiver.push_back(message.sending_server);
                send_simple_message(ID, NotResponsible, receiver);
            }
            if (failure == DAOSubtreeNotExisting)
            {
                vector<Operation> journal_ops;
                InodeNumber correct_journal = (*journal_manager).get_open_operations(ID, journal_ops);
                if (correct_journal!= INVALID_INODE_ID)
                {
                    // reexecute start operation
                    start_da_operation_participant( operation,  operation_length,  operation_type, initiator, correct_journal, ID, ModifiedTwoPhaseCommit);
                }

            }
        }
        break;
    }

    case OOEAck:
    {
        failure = handle_event(OOEAckRecieved,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing acknowledgement for ordered operation execution failed.");
#endif
            handle_unsuccessful_protocol_step(OOEAckRecieved, failure, operation, true, (Status)0,message.sending_server);
        }
        break;
    }

    case OOEAborted:
    {
        failure = handle_event(OOEAbortRecieved,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing abort for ordered operation execution failed.");
#endif
            if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership || operation->type==OOE_LBTest)
            {
                handle_unsuccessful_protocol_step(OOEAbortRecieved, failure, operation, false, OOEWaitResultUndone,message.sending_server);
            }
            else
            {
                handle_unsuccessful_protocol_step(OOEAbortRecieved, failure, operation, true, (Status)0,message.sending_server);
            }
        }
        break;
    }

    // handle external two phase commit operation (TPCOp) events
    case TPCOpReq:
    {
        OperationTypes operation_type;
        InodeNumber subtree_entry_this, subtree_entry_initiator;
        uint32_t operation_length;
        char* operation;
        vector<Subtree> initiator;

        // Get kind of operation which is executed, e.g. rename
        operation_type = (OperationTypes) (((char*)message.data)[sizeof(char)+sizeof(uint64_t)]);
        // Get subtree entry point, needed to get access to the correct journal
        memcpy( &subtree_entry_this, message.data+ 2*sizeof(char)+sizeof(uint64_t), sizeof(InodeNumber));
        // Get subtree entry point of coordinator
        memcpy( &subtree_entry_initiator, message.data+ 2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber), sizeof(InodeNumber));
        // Get the operation description
        memcpy( &operation_length, message.data+ 2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)+sizeof(InodeNumber), sizeof(uint32_t));
        operation = (char*)malloc(operation_length);
        if (operation == NULL)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Cannot get enough memory to create operation description.");
#endif
            handle_internal_failure(DAOInsufficientMemory);
        }
        memcpy( operation, message.data+ 2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber)+sizeof(InodeNumber)+sizeof(uint32_t), operation_length);

        // Build the coordinator information
        Subtree initiator_subtree;
        initiator_subtree.server = message.sending_server;
        initiator_subtree.subtree_entry = subtree_entry_initiator;
        initiator.push_back(initiator_subtree);
        // start the distributed operation as participant
        failure = start_da_operation_participant( operation,  operation_length,  operation_type,
                  initiator, subtree_entry_this, ID, TwoPhaseCommit);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            stringstream error_message4;
            error_message4<<"Distributed operation with ID "<<ID<<" cannot be started due to "<<failure;
            dao_logger.error_log(error_message4.str().c_str());
#endif
            // failure == DAOOperationExisting || failure == DAONoSal || failure == DAONoMlt, nothing can be done
            if (failure == DAOSelfWrongServer)
            {
                // send notifier to sender that wrong server
                vector<string> receiver;
                receiver.push_back(message.sending_server);
                send_simple_message(ID, NotResponsible, receiver);
            }
            if (failure == DAOSubtreeNotExisting)
            {
                vector<Operation> journal_ops;
                InodeNumber correct_journal = (*journal_manager).get_open_operations(ID, journal_ops);
                if (correct_journal!= INVALID_INODE_ID)
                {
                    // reexecute start operation
                    start_da_operation_participant( operation,  operation_length,  operation_type, initiator, correct_journal, ID, ModifiedTwoPhaseCommit);
                }

            }
        }
        break;
    }

    case TPCVoteReq:
    {
        failure = handle_event(TPCPStartVote,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing of two phase commit vote request failed.");
#endif
            if (operation->status == TPCPartComp)
            {
                handle_unsuccessful_protocol_step(TPCPStartVote, failure, operation, false, TPCPartVReqRec,message.sending_server);
            }
            else
            {
                if (operation->status == TPCPartWaitVReqNo)
                {
                    handle_unsuccessful_protocol_step(TPCPStartVote, failure, operation, false, TPCPartWaitVResultExpectNo,message.sending_server);
                }
                else
                {
                    handle_unsuccessful_protocol_step(TPCPStartVote, failure, operation, false, TPCPartWaitVResultExpectYes,message.sending_server);
                }
            }
        }
        break;
    }

    case TPCVoteY:   //received by coordinator
    {
        // decrease number of left votes only if still waiting for votes
        // only if not aborted, or all acknowledges already received
        if (operation->status!=TPCAborting && operation->status != TPCWaitUndoAck && operation->status !=TPCWaitUndoToFinish && operation->status!=TPCCoordinatorVResultSend)
        {
            //create identification for this vote
            stringstream vote_identification;
            vote_identification<<ID<<'.'<<message.sending_server;
            string vote_ID = vote_identification.str();
            // check if vote not received before
            if (TPC_received_request_answers.find(vote_ID)==TPC_received_request_answers.end())
            {
                // mark vote as received
                TPC_received_request_answers.insert(unordered_set<string>::value_type(vote_ID));
                // decrease number of received votes
                operation->recieved_votes--;

                if (operation->recieved_votes==0)   // all votes received
                {
                    int num_removed=0;
                    // remove all previous received votes from respective datastructure
                    if (operation->recieved_votes<operation->participants.size())
                    {
                        for (int i=0;operation->recieved_votes+num_removed<operation->participants.size() && i<operation->participants.size(); i++)
                        {
                            //create identification for possible received vote
                            stringstream vote_identification;
                            vote_identification<<ID<<'.'<<operation->participants.at(i).server;
                            string vote_ID = vote_identification.str();
                            if (TPC_received_request_answers.erase(vote_ID))
                            {
                                num_removed++;
                            }
                        }
                    }
                    if (operation->recieved_votes+num_removed!=operation->participants.size())
                    {
                        remove_all_entries_for_operation(ID);
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("More or less vote identification than received are removed from the datastructure.");
#endif
                    }

                    // set recieved_votes to #particpants to count acknowledges
                    operation->recieved_votes = operation->participants.size();

                    // handle that all participants voted with yes
                    failure = handle_event(TPCIVotesFin,operation);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("Processing of end of successful vote phase in the two phase commit protocol failed");
#endif
                        handle_unsuccessful_protocol_step(TPCIVotesFin, failure, operation, false, TPCCoordinatorVResultSend,message.sending_server);
                    }
                }
            }
        }
        break;
    }

    case TPCVoteN:   //received by coordinator
    {
        // only start abort process if the operation has not been aborted yet
        //send abort message if it is first no vote which is received
        if (operation->status!=TPCAborting && operation->status != TPCWaitUndoAck && operation->status !=TPCWaitUndoToFinish)
        {
            failure = write_update_log(ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Logging global result in two phase commit protocol failed.");
#endif
                if (failure == DAOSubtreeNotExisting)
                {
                    vector<Operation> journal_ops;
                    InodeNumber correct_journal = (*journal_manager).get_open_operations(operation->ID, journal_ops);
                    if (correct_journal!= INVALID_INODE_ID)
                    {
                        // set correct journal identifier
                        operation->subtree_entry = correct_journal;
                        // reexecute logging
                        failure = write_update_log(ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
                    }
                    else
                    {
                        InodeNumber current_subtree_entry = operation->subtree_entry;
                        if (operation->type == MoveSubtree)
                        {
                            DaoHandler::set_subtree_entry_point(operation);
                        }
                        else
                        {
                            ChangeOwnershipDAOAdapter::set_subtree_entry_point(operation);
                        }
                        if (current_subtree_entry != operation->subtree_entry && operation->subtree_entry != INVALID_INODE_ID)
                        {
                            // reexecute logging
                            failure = write_update_log(ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
                        }
                    }
                }
                if (failure == DAOSelfWrongServer)
                {
                    vector<string> server_list;
                    for (int i=0;i<operation->participants.size();i++)
                    {
                        server_list.push_back(operation->participants.at(i).server);
                    }
                    // send not responsible message either to coordinator, previous executor or participants/following executor
                    send_simple_message(operation->ID, NotResponsible, server_list);
                    break;
                }
                // if still any failure stop execution of protocol step
                if (failure)
                {
                    break;
                }
            }

            int num_removed=0;
            // remove all previous received votes from respective datastructure
            if (operation->recieved_votes<operation->participants.size())
            {
                for (int i=0;operation->recieved_votes+num_removed<operation->participants.size() && i<operation->participants.size(); i++)
                {
                    //create identification for possible received vote
                    stringstream vote_identification;
                    vote_identification<<ID<<'.'<<operation->participants.at(i).server;
                    string vote_ID = vote_identification.str();
                    if (TPC_received_request_answers.erase(vote_ID))
                    {
                        num_removed++;
                    }
                }
            }
            if (operation->recieved_votes+num_removed!=operation->participants.size())
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("More or less vote identification than received are removed from the datastructure.");
#endif
                remove_all_entries_for_operation(ID);
            }

            // set recieved_votes to #particpants to count acknowledges
            operation->recieved_votes = operation->participants.size();

            // send abort message
            // create server list
            vector<string> server_list;
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }
            // send message to external modules
            failure = send_simple_message(operation->ID,TPCAbort, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending abort message failed at least partly.");
#endif
                // check if repairable sending failure
                if (failure == SocketNotExisting || failure == SendingFailed)
                {
                    // try to resend, here a failure is ignored and must be processed by time out recovery
                    send_simple_message(operation->ID,TPCAbort, server_list);
                }
            }
            // reset counter for participant answers
            operation->recieved_votes = operation->participants.size();

            // check if operation need to be undone
            if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
            {
                forward_any_execution_request(operation, DAOUndoRequest);
                operation->status = TPCWaitUndoAck;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCWaitUndoAck);
            }
            else
            {
                // client negative acknowledgement (NACK)
                provide_result(operation->ID, false, TwoPhaseCommit, operation->type==MoveSubtree);
                // set status to aborting
                operation->status=TPCAborting;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCAborting);
            }
        }
        break;
    }

    case TPCCommit:
    {
        failure = handle_event(TPCPCommit,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing the commit request in the two phase commit protocol failed.");
#endif
            handle_unsuccessful_protocol_step(TPCPCommit, failure, operation, true, (Status)0,message.sending_server);
        }
        break;
    }

    case TPCAbort:
    {
        failure = handle_event(TPCPAbort,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing the abort request in the two phase commit protocol failed.");
#endif
            if (operation->status==TPCPartComp || operation->status == TPCPartVReqRec)
            {
                handle_unsuccessful_protocol_step(TPCPAbort, failure, operation, false, TPCAborting,message.sending_server);
            }
            else
            {
                if ((operation->type == MoveSubtree || operation->type == ChangePartitionOwnership) && (operation->status == TPCPartWaitVReqYes || operation->status == TPCPartWaitVResultExpectYes))
                {
                    handle_unsuccessful_protocol_step(TPCPAbort, failure, operation, false, TPCWaitUndoToFinish,message.sending_server);
                }
                else
                {
                    handle_unsuccessful_protocol_step(TPCPAbort, failure, operation, true, (Status)0,message.sending_server);
                }
            }
        }
        break;
    }

    case TPCAck:   //received by coordinator
    {
        //create identification for this vote
        stringstream vote_identification;
        vote_identification<<ID<<'.'<<message.sending_server;
        string vote_ID = vote_identification.str();
        // check if vote not received before
        if (TPC_received_request_answers.find(vote_ID)==TPC_received_request_answers.end())
        {
            // mark vote as received
            TPC_received_request_answers.insert(unordered_set<string>::value_type(vote_ID));
            // decrease number of received votes
            operation->recieved_votes--;

            if (operation->recieved_votes==0)   // all votes received
            {
                // set recieved_votes to #particpants to count acknowledges
                operation->recieved_votes = operation->participants.size();
                failure = handle_event(TPCIAckRecv,operation);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Processing of all acknowledgements received in two phase commit protocol failed.");
#endif
                    if (operation->status==TPCWaitUndoAck || operation->status==TPCWaitUndoToFinish)
                    {
                        handle_unsuccessful_protocol_step(TPCIAckRecv, failure, operation, false, TPCWaitUndoToFinish,message.sending_server);
                    }
                    else
                    {
                        handle_unsuccessful_protocol_step(TPCIAckRecv, failure, operation, true, (Status)0,message.sending_server);
                    }
                }
            }
        }

        break;
    }
    // Timeout message treatment
    case TPCPRAbort:
    {


        if (operation->status!=TPCAborting)
        {
            failure = handle_event(TPCPAbort,operation);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Processing the abort request in the two phase commit protocol failed.");
#endif
                if (operation->status==TPCPartComp || operation->status == TPCPartVReqRec)
                {
                    handle_unsuccessful_protocol_step(TPCPAbort, failure, operation, false, TPCAborting,message.sending_server);
                }
                else
                {
                    if ((operation->type == MoveSubtree || operation->type == ChangePartitionOwnership) && (operation->status == TPCPartWaitVReqYes || operation->status == TPCPartWaitVResultExpectYes))
                    {
                        handle_unsuccessful_protocol_step(TPCPAbort, failure, operation, false, TPCWaitUndoToFinish,message.sending_server);
                    }
                    else
                    {
                        handle_unsuccessful_protocol_step(TPCPAbort, failure, operation, true, (Status)0,message.sending_server);
                    }
                }
            }
        }
        break;
    }
    case TPCRVoteN:
    {
        if (operation->status != TPCCoordinatorVResultSend)
        {
            // only start abort process if the operation has not been aborted yet
            //send abort message if it is first no vote which is received
            if (operation->status!=TPCAborting && operation->status != TPCWaitUndoAck && operation->status !=TPCWaitUndoToFinish)
            {
                failure = write_update_log(ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Logging global result in two phase commit protocol failed.");
#endif
                    if (failure == DAOSubtreeNotExisting)
                    {
                        vector<Operation> journal_ops;
                        InodeNumber correct_journal = (*journal_manager).get_open_operations(operation->ID, journal_ops);
                        if (correct_journal!= INVALID_INODE_ID)
                        {
                            // set correct journal identifier
                            operation->subtree_entry = correct_journal;
                            // reexecute logging
                            failure = write_update_log(ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
                        }
                        else
                        {
                            InodeNumber current_subtree_entry = operation->subtree_entry;
                            if (operation->type == MoveSubtree)
                            {
                                DaoHandler::set_subtree_entry_point(operation);
                            }
                            else
                            {
                                ChangeOwnershipDAOAdapter::set_subtree_entry_point(operation);
                            }
                            if (current_subtree_entry != operation->subtree_entry && operation->subtree_entry != INVALID_INODE_ID)
                            {
                                // reexecute logging
                                failure = write_update_log(ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
                            }
                        }
                    }
                    if (failure == DAOSelfWrongServer)
                    {
                        vector<string> server_list;
                        for (int i=0;i<operation->participants.size();i++)
                        {
                            server_list.push_back(operation->participants.at(i).server);
                        }
                        // send not responsible message either to coordinator, previous executor or participants/following executor
                        send_simple_message(operation->ID, NotResponsible, server_list);
                        break;
                    }
                    // if still any failure stop execution of protocol step
                    if (failure)
                    {
                        break;
                    }
                }

                int num_removed=0;
                // remove all previous received votes from respective datastructure
                if (operation->recieved_votes<operation->participants.size())
                {
                    for (int i=0;operation->recieved_votes+num_removed<operation->participants.size() && i<operation->participants.size(); i++)
                    {
                        //create identification for possible received vote
                        stringstream vote_identification;
                        vote_identification<<ID<<'.'<<operation->participants.at(i).server;
                        string vote_ID = vote_identification.str();
                        if (TPC_received_request_answers.erase(vote_ID))
                        {
                            num_removed++;
                        }
                    }
                }
                if (operation->recieved_votes+num_removed!=operation->participants.size())
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("More or less vote identification than received are removed from the datastructure.");
#endif
                    remove_all_entries_for_operation(ID);
                }

                // set recieved_votes to #particpants to count acknowledges
                operation->recieved_votes = operation->participants.size();

                // send abort message
                // create server list
                vector<string> server_list;
                for (int i=0;i<operation->participants.size();i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }
                // send message to external modules
                failure = send_simple_message(operation->ID,TPCAbort, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending abort message failed at least partly.");
#endif
                    // check if repairable sending failure
                    if (failure == SocketNotExisting || failure == SendingFailed)
                    {
                        // try to resend, here a failure is ignored and must be processed by time out recovery
                        send_simple_message(operation->ID,TPCAbort, server_list);
                    }
                }
                // reset counter for participant answers
                operation->recieved_votes = operation->participants.size();

                // check if operation need to be undone
                if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
                {
                    forward_any_execution_request(operation, DAOUndoRequest);
                    operation->status = TPCWaitUndoAck;
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCWaitUndoAck);
                }
                else
                {
                    // client negative acknowledgement (NACK)
                    provide_result(operation->ID, false, TwoPhaseCommit, operation->type==MoveSubtree);
                    // set status to aborting
                    operation->status=TPCAborting;
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCAborting);
                }
            }
        }
        else
        {
            // send abort message again
            // create server list
            vector<string> server_list;

            server_list.push_back(message.sending_server);
            // send message to external modules
            if (operation->status==TPCAborting)
            {
                failure = send_simple_message(operation->ID,TPCAbort, server_list);
            }
            else
            {
                failure = send_simple_message(operation->ID,TPCCommit, server_list);
            }
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending abort message failed at least partly.");
#endif
                // check if repairable sending failure
                if (failure == SocketNotExisting || failure == SendingFailed)
                {
                    // try to resend, here a failure is ignored and must be processed by time out recovery
                    send_simple_message(operation->ID,TPCAbort, server_list);
                }
            }
        }
        break;
    }
    case TPCRVoteY:
    {
        if (operation->status == TPCAborting)
        {
            // send abort message again
            // create server list
            vector<string> server_list;

            server_list.push_back(message.sending_server);
            // send message to external modules
            if (operation->status==TPCAborting)
            {
                failure = send_simple_message(operation->ID,TPCAbort, server_list);
            }
            else
            {
                failure = send_simple_message(operation->ID,TPCCommit, server_list);
            }
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending abort message failed at least partly.");
#endif
                // check if repairable sending failure
                if (failure == SocketNotExisting || failure == SendingFailed)
                {
                    // try to resend, here a failure is ignored and must be processed by time out recovery
                    send_simple_message(operation->ID,TPCAbort, server_list);
                }
            }
        }
        else
        {
            // decrease number of left votes only if still waiting for votes
            // only if not aborted, or all acknowledges already received
            if (operation->status!=TPCAborting && operation->status != TPCWaitUndoAck && operation->status !=TPCWaitUndoToFinish && operation->status!=TPCCoordinatorVResultSend)
            {
                //create identification for this vote
                stringstream vote_identification;
                vote_identification<<ID<<'.'<<message.sending_server;
                string vote_ID = vote_identification.str();
                // check if vote not received before
                if (TPC_received_request_answers.find(vote_ID)==TPC_received_request_answers.end())
                {
                    // mark vote as received
                    TPC_received_request_answers.insert(unordered_set<string>::value_type(vote_ID));
                    // decrease number of received votes
                    operation->recieved_votes--;

                    if (operation->recieved_votes==0)   // all votes received
                    {
                        int num_removed=0;
                        // remove all previous received votes from respective datastructure
                        if (operation->recieved_votes<operation->participants.size())
                        {
                            for (int i=0;operation->recieved_votes+num_removed<operation->participants.size() && i<operation->participants.size(); i++)
                            {
                                //create identification for possible received vote
                                stringstream vote_identification;
                                vote_identification<<ID<<'.'<<operation->participants.at(i).server;
                                string vote_ID = vote_identification.str();
                                if (TPC_received_request_answers.erase(vote_ID))
                                {
                                    num_removed++;
                                }
                            }
                        }
                        if (operation->recieved_votes+num_removed!=operation->participants.size())
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("More or less vote identification than received are removed from the datastructure.");
#endif
                            remove_all_entries_for_operation(ID);
                        }

                        // set recieved_votes to #particpants to count acknowledges
                        operation->recieved_votes = operation->participants.size();

                        // handle that all participants voted with yes
                        failure = handle_event(TPCIVotesFin,operation);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("Processing of end of successful vote phase in the two phase commit protocol failed");
#endif
                            handle_unsuccessful_protocol_step(TPCIVotesFin, failure, operation, false, TPCCoordinatorVResultSend,message.sending_server);
                        }
                    }
                }
            }
        }
        break;
    }
    case TPCRVoteReq:
    {
        if (operation->status == TPCPartWaitVReqYes || operation->status == TPCPartWaitVReqNo || operation->status == TPCPartComp)
        {
            failure = handle_event(TPCPStartVote,operation);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Processing of two phase commit vote request failed.");
#endif
                if (operation->status == TPCPartComp)
                {
                    handle_unsuccessful_protocol_step(TPCPStartVote, failure, operation, false, TPCPartVReqRec,message.sending_server);
                }
                else
                {
                    if (operation->status == TPCPartWaitVReqNo)
                    {
                        handle_unsuccessful_protocol_step(TPCPStartVote, failure, operation, false, TPCPartWaitVResultExpectNo,message.sending_server);
                    }
                    else
                    {
                        handle_unsuccessful_protocol_step(TPCPStartVote, failure, operation, false, TPCPartWaitVResultExpectYes,message.sending_server);
                    }
                }
            }
        }
        else
        {
            // set up vote message
            // set coordinator to server_list
            vector<string> server_list;
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }
            // send message to coordinator

            if (operation->status == TPCPartWaitVResultExpectNo)
            {
                failure =  send_simple_message(operation->ID,TPCVoteN, server_list);
            }
            else
            {
                failure =  send_simple_message(operation->ID,TPCVoteY, server_list);
            }
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Cannot send vote message to coordinator in two phase commit protocol.");
#endif
            }

        }

        break;
    }
    case TPCRCommit:
    {
        failure = handle_event(TPCPCommit,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing the commit request in the two phase commit protocol failed.");
#endif
            handle_unsuccessful_protocol_step(TPCPCommit, failure, operation, true, (Status)0,message.sending_server);
        }

        break;
    }
    case MTPCRStatusReq:
    {
        if (operation->status == MTPCPartVoteSendYes)
        {
            vector<string> server_list;
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }

            // send operation result
            failure = send_simple_message(operation->ID,MTPCRCommit, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending operation result to coordinator in modified two phase commit protocol failed.");
#endif
            }
        }
        else
        {
            if (operation->status == MTPCPartVoteSendNo)
            {
                vector<string> server_list;
                for (int i=0;i<operation->participants.size();i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }

                // send operation result
                failure = send_simple_message(operation->ID,MTPCRAbort, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending operation result to coordinator in modified two phase commit protocol failed.");
#endif
                }

            }
        }

        break;
    }
    case MTPCRAbort:
    {
        failure = handle_event(MTPCIAbort,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing an incoming abort event for MTPC protocol failed.");
#endif
            if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
            {
                handle_unsuccessful_protocol_step(MTPCIAbort, failure, operation, false, MTPCIWaitResultUndone,message.sending_server);
            }
            else
            {
                handle_unsuccessful_protocol_step(MTPCIAbort, failure, operation, true, (Status)0,message.sending_server);
            }
        }

        break;
    }
    case MTPCRCommit:
    {
        failure = handle_event(MTPCICommit,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing an incoming commit event for MTPC protocol failed.");
#endif
            handle_unsuccessful_protocol_step(MTPCICommit, failure, operation, true, (Status)0, message.sending_server);
        }

        break;
    }
    case OOERAborted:
    {
        failure = handle_event(OOEAbortRecieved,operation);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Processing an incoming abort event for MTPC protocol failed.");
#endif
            if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
            {
                handle_unsuccessful_protocol_step(OOEAbortRecieved, failure, operation, false, MTPCIWaitResultUndone,message.sending_server);
            }
            else
            {
                handle_unsuccessful_protocol_step(OOEAbortRecieved, failure, operation, true, (Status)0,message.sending_server);
            }
        }

        break;
    }

    default:   // unexpected event
    {
#ifdef LOGGING_ENABLED
        dao_logger.warning_log("Unexpected event type was sent in message.");
#endif
    }
    }
    failure = pthread_mutex_unlock(&inc_event_mutex);
#ifdef LOGGING_ENABLED
    if (failure)
    {
        stringstream warning_message;
        warning_message<<"Mutex cannot be unlocked due to "<<failure;
        dao_logger.warning_log(warning_message.str().c_str());
    }
#endif

    //release memory for message data
    free(message.data);

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
   *\brief Starts a new distributed atomic operation with the respective parameters.
   *
   *\param operation - Description of the operation which is needed to execute the respective parts of the distributed atomic operation. The pointer will be freed after the operation is finished.
   *\param operation_length - Length of the operation description <code>operation</code> in number of bytes
   *\param operation_type - Defines which kind of operation should be exectued
   *\param participants - participants of the distributed atomic operation, empty list if the ordered operation execution protocol is used for this operation
   *\param subtree_entry - root of the subtree which is affected by the coordinator part of the distributed atomic operation, needed to access the journal
   *
   *\return the unique identification of the distributed atomic operation or 0 if a failure occurred
   *
   * Starts a new distributed atomic operation. The begin is logged and the operation is saved in the temporary datastructure.
   * Furthermore the execution of the coordinator is triggered and the respective protocol started. The started operation is executed by
   * this module until the operation is complete. If the result is known the client will be informed with the help of the respective <code>
   * ConcurrentQueue<OutQueue></code> which can be retrieved by <code>get_queue</code>.
   *
   *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched
   */
uint64_t DistributedAtomicOperations::start_da_operation(void* operation, uint32_t operation_length, OperationTypes operation_type, vector<Subtree>& participants, InodeNumber subtree_entry)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    // participant size of 0 only allowed for operations using ordered operation execution
    if ((participants.size()<1 && operation_type<=SetAttr) || operation == NULL || operation_length == 0)
    {
#ifdef LOGGING_ENABLED
        dao_logger.warning_log("Operation is not started because the parameters are not specified correctly.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return 0;
    }
    // check if recovery done
    if (!recovery_done)
    {
#ifdef LOGGING_ENABLED
        dao_logger.warning_log("Recovery has not been done yet.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return 0;
    }
    // check if same operation already executed -> check all open operations
    DAOperation compare_with;
    Pc2fsProfiler::get_instance()->function_sleep();
    //aquire mutex to avoid simultaneous changes on an open operation
    failure = pthread_mutex_lock( &(instance.inc_event_mutex) );
    Pc2fsProfiler::get_instance()->function_wakeup();
    if (failure)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message;
        error_message<<"Mutex for internal event synchronization cannot be created due to "<<failure;
        instance.dao_logger.error_log(error_message.str().c_str());
#endif
        instance.handle_internal_failure(DAOMutexFailure);
    }
    unordered_map<uint64_t,DAOperation>::iterator it = open_operations.begin();
    while (it!=open_operations.end())
    {
        // compare with operation
        compare_with = (*it).second;
        it++;
        // check if the same
        if (compare_with.type == operation_type && compare_with.operation_length == operation_length &&
                compare_with.subtree_entry == subtree_entry && memcpy(compare_with.operation, operation, operation_length)==0)
        {
            // check for same participants, only possible for (modfied) two phase commit protocol
            bool equal = participants.size() == compare_with.participants.size();
            if (compare_with.used_protocol!=OrderedOperationExecution && equal)
            {
                int same_tree;
                for (int i=0;i<participants.size() && equal ;i++)
                {
                    if (compare_with.participants.at(i).subtree_entry != participants.at(i).subtree_entry)
                    {
                        equal == false;
                    }

                }
            }
            if (equal || compare_with.used_protocol == OrderedOperationExecution)
            {
                failure = pthread_mutex_unlock(&inc_event_mutex);
#ifdef LOGGING_ENABLED
                if (failure)
                {
                    stringstream warning_message;
                    warning_message<<"Mutex cannot be unlocked due to "<<failure;
                    dao_logger.warning_log(warning_message.str().c_str());
                }
#endif
                compare_with.overall_timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::milliseconds(OVERALL_TIMEOUT);
                // if operation far enough provide client result, only possible for two phase commit protocol, all other protocols delete operation when client result sent
                if (compare_with.status == TPCAborting)   // negative acknowledge (NACK)
                {
                    provide_result(compare_with.ID, false, TwoPhaseCommit, compare_with.type == MoveSubtree);
                }
                else
                {
                    if (compare_with.status == TPCCoordinatorVResultSend)   // client acknowledge
                    {
                        provide_result(compare_with.ID, true, TwoPhaseCommit, compare_with.type == MoveSubtree);
                    }
                }
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return compare_with.ID;
            }
        }
    }
    failure = pthread_mutex_unlock(&inc_event_mutex);
#ifdef LOGGING_ENABLED
    if (failure)
    {
        stringstream warning_message;
        warning_message<<"Mutex cannot be unlocked due to "<<failure;
        dao_logger.warning_log(warning_message.str().c_str());
    }
#endif

    // Operation currently not executed, start normal process
    //Get the journal
    Journal* journal = NULL;
    if (operation_type == MoveSubtree || operation_type == ChangePartitionOwnership)
    {
        journal = (*journal_manager).get_server_journal();
    }
    else
    {
        journal = (*journal_manager).get_journal(subtree_entry);
    }
    if (journal == NULL)
    {
#ifdef LOGGING_ENABLED
        dao_logger.warning_log("Wrong subtree entry point for coordinator or server journal not existing.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        // nothing is done here, operation is not started
        return 0;
    }

    // create data structure for new operation
    DAOperation new_operation;

    new_operation.operation = operation;
    new_operation.operation_length = operation_length;

    new_operation.type = operation_type;
    new_operation.subtree_entry = subtree_entry;
    new_operation.participants = participants;
    new_operation.overall_timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::milliseconds(OVERALL_TIMEOUT);

    Pc2fsProfiler::get_instance()->function_sleep();
    //aquire mutex to avoid simultaneous changes on an open operation and to prevent that an ID is used twice
    failure = pthread_mutex_lock( &(instance.inc_event_mutex) );
    Pc2fsProfiler::get_instance()->function_wakeup();

    if (failure)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message;
        error_message<<"Mutex for internal event synchronization cannot be created due to "<<failure;
        instance.dao_logger.error_log(error_message.str().c_str());
#endif
        instance.handle_internal_failure(DAOMutexFailure);
    }
    // get ID for operation
    do
    {
        new_operation.ID = id_generator.get_next_id();
    }
    while (new_operation.ID==0 || open_operations.find(new_operation.ID)!=open_operations.end());

    // set protocol
    if (operation_type<=SetAttr)
    {
        if (participants.size()==1)
        {
            new_operation.used_protocol = ModifiedTwoPhaseCommit;
            new_operation.status = MTPCCoordinatorComp;
            add_timeout(new_operation.ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCCoordinatorComp);
        }
        else
        {
            new_operation.used_protocol = TwoPhaseCommit;
            new_operation.status = TPCCoordinatorComp;
            // set recieved_votes
            new_operation.recieved_votes = participants.size();
            add_timeout(new_operation.ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCCoordinatorComp);
        }
    }
    else
    {
        if (operation_type == OrderedOperationTest || operation_type == OOE_LBTest)
        {
            new_operation.used_protocol = OrderedOperationExecution;
            new_operation.status = OOEComp;
            add_timeout(new_operation.ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEComp);
        }
    }



    // create begin log content
    char log_entry[sizeof(char)+sizeof(uint32_t)+operation_length];
    //save operation type
    log_entry[0]=operation_type;
    //save operation length
    memcpy(log_entry+sizeof(char), &operation_length, sizeof(uint32_t));
    //save operation
    memcpy(log_entry+sizeof(char)+sizeof(uint32_t), operation, operation_length);
    Pc2fsProfiler::get_instance()->function_sleep();
    //create begin log entry in respective journal
    failure = journal->add_distributed(new_operation.ID, DistributedAtomicOp, DistributedOp, DistributedStart , log_entry,
                                       sizeof(char)+sizeof(uint32_t)+operation_length);
    Pc2fsProfiler::get_instance()->function_wakeup();
    if (failure)
    {
#ifdef LOGGING_ENABLED
        dao_logger.error_log("Logging begin of distributed atomic operation failed.");
#endif
        failure = pthread_mutex_unlock(&inc_event_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message;
            warning_message<<"Mutex cannot be unlocked due to "<<failure;
            dao_logger.warning_log(warning_message.str().c_str());
        }
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return 0;
    }

    // insert operation into temporary datastructure
    open_operations.insert(unordered_map<uint64_t,DAOperation>::value_type(new_operation.ID,new_operation));

    failure = pthread_mutex_unlock(&inc_event_mutex);
#ifdef LOGGING_ENABLED
    if (failure)
    {
        stringstream warning_message;
        warning_message<<"Mutex cannot be unlocked due to "<<failure;
        dao_logger.warning_log(warning_message.str().c_str());
    }
#endif

    // in case of two phase commit protocol, inform all participants
    if (new_operation.used_protocol==TwoPhaseCommit)
    {
        //Message: MESSAGEtype (char), ID (uint64_t), operationtype (char), subtree_entry_this(InodeNumber)
        //         , subtree_entry_Initiator(InodeNumber), operation_lenght (uint32_t), Operation

        // set up message
        char messagetype;
        int message_length;
        char* message_data;
        message_length = sizeof(char)+sizeof(char)+sizeof(uint32_t)+ 2* sizeof(InodeNumber)+ sizeof(uint64_t) + new_operation.operation_length;
        message_data = (char*)malloc(message_length);
        if (message_data==NULL)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Cannot get enough memory to create two phase commit protocol's execution request message.");
#endif
            handle_internal_failure(DAOInsufficientMemory);
        }
        vector<string> server_list;

        // set up particpant independent message data
        messagetype = TPCOpReq;
        //set event type
        memcpy(message_data, &messagetype, sizeof(char));
        // set operation ID
        memcpy(message_data+sizeof(char), &(new_operation.ID), sizeof(uint64_t));
        // set operation type
        *(((char*)message_data)+sizeof(char)+sizeof(uint64_t)) = new_operation.type;
        //participant's subtree entry point is integrated later
        // set coordinator subtree entry point
        memcpy(message_data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber), &(new_operation.subtree_entry), sizeof(InodeNumber));
        // set operation description
        memcpy(message_data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber), &(new_operation.operation_length), sizeof(uint32_t));
        memcpy(message_data+2*sizeof(char)+sizeof(uint32_t)+sizeof(uint64_t)+2*sizeof(InodeNumber), new_operation.operation, new_operation.operation_length);

        //include particpant depend message data and send message
        for (int i=0;i<new_operation.participants.size();i++)
        {
            server_list.push_back(new_operation.participants.at(i).server);
            // set participant's subtree entry point
            memcpy(message_data+2*sizeof(char)+sizeof(uint64_t), &(new_operation.participants.at(i).subtree_entry), sizeof(InodeNumber));
            Pc2fsProfiler::get_instance()->function_sleep();
            //send message
            failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
            Pc2fsProfiler::get_instance()->function_wakeup();
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending two phase commit execution request failed at least for one server");
#endif
                if (failure==MutexFailed)
                {
                    handle_internal_failure(DAOMutexFailure);
                }
                else
                {
                    if (failure == UnableToCreateMessage || failure == NotEnoughMemory)
                    {
                        handle_internal_failure(DAOInsufficientMemory);
                    }
                    else
                    {
                        if (failure == SendingFailed)
                        {
                            if (errno>=0 && errno<server_list.size())
                            {
                                handle_supposed_external_server_failure(server_list.at(errno));
                            }
                        }
                        else
                        {
                            if (errno>=0 && errno<server_list.size())
                            {
                                int creation_failure = handle_socket_not_existing(server_list.at(errno));
                                if (creation_failure == 0)
                                {
                                    Pc2fsProfiler::get_instance()->function_sleep();
                                    failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
                                    Pc2fsProfiler::get_instance()->function_wakeup();
                                }
                                else
                                {
                                    if (creation_failure == DAOUnknownAddress)
                                    {
                                        DAOperation* operation;
                                        if (open_operations.find(new_operation.ID)!=open_operations.end())
                                        {
                                            operation = &((*(open_operations.find(new_operation.ID))).second);
                                            // ask the respective executing module for the correct address
                                            if (new_operation.type == MoveSubtree)
                                            {
                                                failure = DaoHandler::set_sending_addresses(operation);
                                            }
                                            else
                                            {
                                                failure = ChangeOwnershipDAOAdapter::set_sending_addresses(operation);
                                            }
                                            if (!failure)
                                            {
                                                failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
                                            }
#ifdef LOGGING_ENABLED
                                            else
                                            {
                                                dao_logger.error_log("Getting TPC participants which were at least partly incorrect failed.");
                                            }
#endif
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            server_list.clear();
        }
        free(message_data);
    }

    // put the operation request to execute into the correct queue
    forward_any_execution_request(&new_operation, DAORequest);

    return new_operation.ID;
}

/**
 *\brief Starts a participant of a distributed atomic operation with the respective parameters
 *
 *\param operation - Description of the operation which is needed to execute the respective parts of the distributed atomic operation. The pointer will be freed after the operation is finished.
 *\param operation_length - Length of the operation description <code>operation</code> in number of bytes
 *\param operation_type - Defines which kind of operation should be exectued
 *\param initiator - coordinator or previous executor of the distributed atomic operation
 *\param subtree_entry - root of the subtree which is affected by this participant part of the distributed atomic operation, needed to access the journal
 *\param ID - unique identifier of the distributed atomic operation, determined by the coordinator
 *\param used_protocol - protocol which describes the execution of this distributed atomic operation
 *
 *\return 0 if the method execution was successful, a failure code otherwise
 *
 * Starts a distributed atomic operation for a participant. The begin is logged and the operation is saved in the temporary datastructure.
 * Furthermore the execution of this participant is triggered. The operation is executed by the participant until the participant part of
 * the operation is complete.
 *
 * \throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched
 */
int DistributedAtomicOperations::start_da_operation_participant(void* operation, uint32_t operation_length, OperationTypes operation_type, vector<Subtree>& initiator, InodeNumber subtree_entry, uint64_t ID, Protocol protocol)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;

    //Get the journal
    Journal* journal = NULL;
    if (operation_type == MoveSubtree || operation_type == ChangePartitionOwnership)
    {
        journal = (*journal_manager).get_server_journal();
    }
    else
    {
        journal = (*journal_manager).get_journal(subtree_entry);
    }
    if (journal == NULL)
    {
#ifdef LOGGING_ENABLED
        dao_logger.error_log("Journal is not available. Try to repair.");
#endif
        failure = handle_journal_not_existing(operation_type == MoveSubtree || operation_type == ChangePartitionOwnership, subtree_entry, true);
        if (failure)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }
    }

    // check if same operation already executed
    // check if it is in open operations
    if (open_operations.find(ID)!=open_operations.end())
    {
        ((*(open_operations.find(ID))).second).overall_timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::milliseconds(OVERALL_TIMEOUT);
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return DAOOperationExisting;
    }
    // check if it is a lost operation
    Operation journal_op;
    failure = journal->get_last_operation(ID, journal_op);
    if (!failure && journal_op.get_operation_data().status != Committed && journal_op.get_operation_data().status != Aborted)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return DAOOperationExisting;
    }

    // create data structure for new operation
    DAOperation new_operation;
    new_operation.ID = ID;
    new_operation.operation = operation;
    new_operation.operation_length = operation_length;
    new_operation.type = operation_type;
    new_operation.subtree_entry = subtree_entry;
    new_operation.participants = initiator;
    new_operation.used_protocol = protocol;
    new_operation.overall_timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::milliseconds(OVERALL_TIMEOUT);
    // set status
    if (operation_type<=SetAttr)
    {
        if (protocol==ModifiedTwoPhaseCommit)
        {

            new_operation.status = MTPCPartComp;
            add_timeout(new_operation.ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCPartComp);
        }
        else
        {
            new_operation.status = TPCPartComp;
            add_timeout(new_operation.ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartComp);
        }
    }
    else
    {
        if (new_operation.used_protocol == OrderedOperationExecution)
        {
            new_operation.status = OOEComp;
            add_timeout(new_operation.ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEComp);
        }
    }


    // create begin log content
    char log_entry[sizeof(char)+sizeof(uint32_t)+operation_length];
    //save operation type
    log_entry[0]=operation_type;
    //save operation length
    memcpy(log_entry+sizeof(char), &operation_length, sizeof(uint32_t));
    //save operation
    memcpy(log_entry+sizeof(char)+sizeof(uint32_t), operation, operation_length);
    Pc2fsProfiler::get_instance()->function_sleep();
    //create begin log entry in respective journal
    failure = journal->add_distributed(new_operation.ID, DistributedAtomicOp ,DistributedOp, DistributedStart , log_entry, sizeof(uint32_t)+operation_length+sizeof(char));
    Pc2fsProfiler::get_instance()->function_wakeup();
    if (failure)
    {
#ifdef LOGGING_ENABLED
        dao_logger.error_log("Logging begin of distributed atomic operation failed.");
#endif
        handle_internal_failure(DAOJournalFailure);
    }

    open_operations.insert(unordered_map<uint64_t,DAOperation>::value_type(new_operation.ID,new_operation));

    // put the operation request to execute into the correct queue
    forward_any_execution_request(&new_operation, DAORequest);
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
    return 0;
}

/**
 *\brief Processes the protocol part of the distributed atomic operation <code>ID</code> which is specified by <code>event</code>
 *
 *\param event - Identifies an event of a protocol which must be processed next by the distributed atomic operation with <code>ID</code>
 *\param operation -reference to distributed atomic operation for which the event occurred
 *
 *\return 0 if execution was successful, otherwise a failure code
 *
 * Processes the protocol part of the distributed atomic operation <code>operations</code> which is specified by <code>event</code>. A new log
 * may be written, messages sent,the operation status changed, the operation deleted from datastructure and the client informed.
 *
 *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched
 */
int DistributedAtomicOperations::handle_event(Events event, DAOperation* operation)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
// distinguish between different events, each event is processed on its own
    switch (event)
    {
        // ordered operation execution
    case OOEOperationFinishedSuccess:
    {
        if (operation->status == OOEComp)
        {
            // write beginning of next server into log
            failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, OOEStartNext);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Logging of start next participant execution in ordered operation execution protocol failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

            //Message: MESSAGEtype (int), ID (long), operationtype (int), subtree_entry_this(unsigned long)
            //         , subtree_entry_Initiator(unsigned long), operation_lenght (int), Operation (void*)

            // set up operation begin request message
            char messagetype;
            int message_length;
            char* message_data;
            message_length = 2*sizeof(char)+sizeof(uint32_t)+ sizeof(uint64_t)+2* sizeof(InodeNumber)+ operation->operation_length;
            message_data = (char*)malloc(message_length);
            if (message_data==NULL)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Cannot get enough memory to create ordered operation execution protocol's next participant execution request message.");
#endif
                handle_internal_failure(DAOInsufficientMemory);
            }
            // set up message participant independent message content
            messagetype = OOEOpReq;
            //set event type
            memcpy(message_data, &messagetype, sizeof(char));
            // set operation ID
            memcpy(message_data+sizeof(char), &(operation->ID), sizeof(uint64_t));
            // set operation type
            *(((char*)message_data)+sizeof(char)+sizeof(uint64_t)) = operation->type;
            // particpant's subtree entry point set later
            // set coordinator subtree entry point
            memcpy(message_data+2*sizeof(char)+sizeof(uint64_t)+sizeof(InodeNumber), &(operation->subtree_entry), sizeof(InodeNumber));
            // set operation description
            memcpy(message_data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber), &(operation->operation_length), sizeof(uint32_t));
            memcpy(message_data+2*sizeof(char)+sizeof(uint32_t)+sizeof(uint64_t)+2*sizeof(InodeNumber), operation->operation, operation->operation_length);

            vector<string> server_list;
            //include particpant depend message data and send message
            int i;
            if (operation->participants.size()==1)   // first participant/coordinator
            {
                i=0;
            }
            else  	//intermediate participant
            {
                i=1;
            }
            for (;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
                // set participant's subtree entry point
                memcpy(message_data+2*sizeof(char)+sizeof(uint64_t), &(operation->participants.at(i).subtree_entry), sizeof(InodeNumber));
                Pc2fsProfiler::get_instance()->function_sleep();
                // send message
                failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
                Pc2fsProfiler::get_instance()->function_wakeup();
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending next participant execution request sending failed.");
#endif
                    if (failure==MutexFailed)
                    {
                        handle_internal_failure(DAOMutexFailure);
                    }
                    else
                    {
                        if (failure == UnableToCreateMessage || failure == NotEnoughMemory)
                        {
                            handle_internal_failure(DAOInsufficientMemory);
                        }
                        else
                        {
                            if (failure == SendingFailed)
                            {
                                if (errno>=0 && errno<server_list.size())
                                {
                                    handle_supposed_external_server_failure(server_list.at(errno));
                                }
                            }
                            else
                            {
                                if (errno>=0 && errno<server_list.size())
                                {
                                    int creation_failure = handle_socket_not_existing(server_list.at(errno));
                                    if (creation_failure == 0)
                                    {
                                        Pc2fsProfiler::get_instance()->function_sleep();
                                        failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
                                        Pc2fsProfiler::get_instance()->function_wakeup();
                                    }
                                    else
                                    {
                                        if (creation_failure == DAOUnknownAddress)
                                        {
                                            // ask the respective executing module for the correct address
                                            if (operation->type == MoveSubtree)
                                            {
                                                failure = DaoHandler::set_sending_addresses(operation);
                                            }
                                            else
                                            {
                                                failure = ChangeOwnershipDAOAdapter::set_sending_addresses(operation);
                                            }
                                            if (!failure)
                                            {
                                                failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
                                            }
#ifdef LOGGING_ENABLED
                                            else
                                            {
                                                dao_logger.error_log("Getting next participant which was incorrect failed.");
                                            }
#endif
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                server_list.clear();
            }
            free(message_data);
            // operation request sent
            operation->status=OOEWaitResult;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEWaitResult);
        }
        break;
    }

    case OOELastOperationFinishedSuccess:
    {
        // write commit of own part into log
        failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, true);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Commit log failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }

        // set up message for acknowledgement
        // set coordinator to server_list
        vector<string> server_list;
        server_list.push_back(operation->participants.at(0).server);
        // send acknowledgement
        failure =  send_simple_message(operation->ID,OOEAck, server_list);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Sending commit message to previous executor in ordered operation execution protocol failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }

        //delete operation from temporary data structure
        free(operation->operation);
        if (open_operations.erase(operation->ID)==0)
        {
#ifdef LOGGING_ENABLED
            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
        }
        break;
    }

    case OOEOperationFinishedFailure:
    {
        //unsuccessful execution
        // write abort into log
        failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Abort log failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }

        //check if intermediate or last execution failed
        if (operation->participants.size()!=0)
        {
            // set up message for abort (negative acknowledgement)
            // set coordinator to server_list
            vector<string> server_list;
            server_list.push_back(operation->participants.at(0).server);

            // send negative acknowledgement
            failure = send_simple_message(operation->ID,OOEAborted, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending abort message to previous executor in ordered operation execution failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

        }
        else   // first operation failed
        {
            // client negative acknowledgement (NACK)
            provide_result(operation->ID, false, OrderedOperationExecution, operation->type==MoveSubtree || operation->type==OOE_LBTest);
        }

        //delete operation from temporary data structure
        free(operation->operation);
        if (open_operations.erase(operation->ID)==0)
        {
#ifdef LOGGING_ENABLED
            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
        }
        break;
    }

    case OOEAckRecieved:
    {
        // write commit of own part into log
        failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, true);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Commit log failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }
        //check if first
        if (operation->participants.size()==1)
        {
            // client acknowledgement (ACK)
            provide_result(operation->ID, true, OrderedOperationExecution, operation->type==MoveSubtree||operation->type==OOE_LBTest);
        }
        else
        {
            // set content of acknowledgement (message type, operation ID)
            // set coordinator to server_list
            vector<string> server_list;
            server_list.push_back(operation->participants.at(0).server);

            // send acknowledgement
            failure =  send_simple_message(operation->ID,OOEAck, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Cannot send commit acknowledgement to previous executor.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }
        }

        //delete operation from temporary data structure
        free(operation->operation);
        if (open_operations.erase(operation->ID)==0)
        {
#ifdef LOGGING_ENABLED
            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
        }
        break;
    }

    case OOEAbortRecieved:
    {
        // check if it is an operation which need to be undone first
        if ((operation->type == MoveSubtree || operation->type == ChangePartitionOwnership || operation->type==OOE_LBTest))
        {
            if ((operation->status!= OOEWaitResultUndone))
            {
                // write operation undone requested into log
                failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, OOEUndo);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Logging of undo operation in ordered operation execution protocol failed.");
#endif
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return failure;
                }
                forward_any_execution_request(operation, DAOUndoRequest);
                operation->status=OOEWaitResultUndone;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEWaitResultUndone);
            }
        }
        else
        {
            // write abort of own part into log
            failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Abort log failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

            if (operation->participants.size()==1)
            {
                // client negative acknowledgement (NACK)
                provide_result(operation->ID, false, OrderedOperationExecution, operation->type==MoveSubtree||operation->type == OOE_LBTest);
            }
            else
            {
                // set up message for negative acknowledgement (abort)
                // set coordinator to server_list
                vector<string> server_list;
                server_list.push_back(operation->participants.at(0).server);

                // send negative acknowledgement
                failure =  send_simple_message(operation->ID,OOEAborted, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Cannot send abort message to previous executor in ordered operation execution protocol.");
#endif
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return failure;
                }
            }

            //delete operation from temporary data structure
            free(operation->operation);
            if (open_operations.erase(operation->ID)==0)
            {
#ifdef LOGGING_ENABLED
                dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
            }
        }
        break;
    }
    case OOEOperationUndone:
    {
        // write abort of own part into log
        failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Abort log failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }

        if (operation->participants.size()==1)
        {
            // client negative acknowledgement (NACK)
            provide_result(operation->ID, false, OrderedOperationExecution, operation->type==MoveSubtree||operation->type==OOE_LBTest);
        }
        else
        {
            // set up message for negative acknowledgement (abort)
            // set coordinator to server_list
            vector<string> server_list;
            server_list.push_back(operation->participants.at(0).server);

            // send negative acknowledgement
            failure =  send_simple_message(operation->ID,OOEAborted, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Cannot send abort message to previous executor in ordered operation execution protocol.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }
        }

        //delete operation from temporary data structure
        free(operation->operation);
        if (open_operations.erase(operation->ID)==0)
        {
#ifdef LOGGING_ENABLED
            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
        }
        break;
    }

//two phase commit protocol
    case TPCPOperationFinSuccess:
    {
        if (operation->status==TPCPartComp)   // only start operation request received
        {
            // write log entry for successful execution
            failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCPVoteYes);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Own vote log in two phase commit protocol failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }
            // set status to operation finsihed and no following request received
            operation->status=TPCPartWaitVReqYes;
        }
        else
        {
            //vote request received and not aborted yet
            if (operation->status==TPCPartVReqRec)
            {
                //write journal entry for successful execution
                failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCPVoteYes);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Own vote log in two phase commit protocol failed.");
#endif
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return failure;
                }

                // set up vote message
                // set coordinator to server_list
                vector<string> server_list;
                for (int i=0;i<operation->participants.size();i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }

                // send message to coordinator
                failure =  send_simple_message(operation->ID,TPCVoteY, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending vote to coordinator failed in two phase commit protocol.");
#endif

                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return failure;
                }
                // vote sent
                operation->status=TPCPartWaitVResultExpectYes;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartWaitVResultExpectYes);
            }
            else   // no vote request and reexecution or abort request received earlier
            {
                if (operation->status == TPCAborting)
                {
                    // check if it is an operation which need to be undone first
                    if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
                    {
                        // write journal entry for successful execution
                        failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCPVoteYes);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("Own vote log in two phase commit protocol failed.");
#endif
                            // ends profiling
                            Pc2fsProfiler::get_instance()->function_end();

                            return failure;
                        }
                        forward_any_execution_request(operation, DAOUndoRequest);
                        operation->status=TPCWaitUndoToFinish;
                        add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCWaitUndoToFinish);
                    }
                    else
                    {
                        // write the abort journal entry for operation
                        failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("Own vote log in two phase commit protocol failed.");
#endif
                            // ends profiling
                            Pc2fsProfiler::get_instance()->function_end();

                            return failure;
                        }

                        // set up message for acknowledgement
                        // set coordinator to server_list
                        vector<string> server_list;
                        for (int i=0;i<operation->participants.size();i++)
                        {
                            server_list.push_back(operation->participants.at(i).server);
                        }

                        // send acknowledgement
                        failure =  send_simple_message(operation->ID,TPCAck, server_list);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("Sending vote to coordinator in two phase commit protocol failed.");
#endif
                            // ends profiling
                            Pc2fsProfiler::get_instance()->function_end();

                            return failure;
                        }

                        // delete operation from temporary data structure
                        free(operation->operation);
                        if (open_operations.erase(operation->ID)==0)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
                        }
                    }
                }
            }
        }
        break;
    }

    case TPCPOperationFinFailure:
    {
        if (operation->status==TPCPartComp)   // only start operation request received
        {
            // write log entry for non-successful execution
            failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCPVoteNo);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Logging own vote in two phase commit protocol failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }
            // set status to operation finsihed and no following request received
            operation->status=TPCPartWaitVReqNo;
        }
        else
        {
            if (operation->status==TPCPartVReqRec)   //vote request already received
            {
                //write journal entry for successful execution
                failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCPVoteNo);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Logging own vote in two phase commit protocol failed.");
#endif
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return failure;
                }

                // set up vote message
                // set coordinator to server_list
                vector<string> server_list;
                for (int i=0;i<operation->participants.size();i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }

                // send message to coordinator
                failure =  send_simple_message(operation->ID,TPCVoteN, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending vote to coordinator in two phase commit protocol failed.");
#endif
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return failure;
                }

                // vote sent
                operation->status=TPCPartWaitVResultExpectNo;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartWaitVResultExpectNo);
            }
            else   // abort request already received
            {
                if (operation->status ==TPCAborting)
                {
                    // write the abort journal entry for operation
                    failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("Abort log failed.");
#endif
                        // ends profiling
                        Pc2fsProfiler::get_instance()->function_end();

                        return failure;
                    }

                    // set up message for acknowledgement
                    // set coordinator to server_list
                    vector<string> server_list;
                    for (int i=0;i<operation->participants.size();i++)
                    {
                        server_list.push_back(operation->participants.at(i).server);
                    }

                    // send acknowledgement
                    failure = send_simple_message(operation->ID,TPCAck, server_list);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("Sending acknowledgement to coordinator in two phase commit protocol failed");
#endif
                        // ends profiling
                        Pc2fsProfiler::get_instance()->function_end();

                        return failure;
                    }

                    // delete operation from temporary data structure
                    free(operation->operation);
                    if (open_operations.erase(operation->ID)==0)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
                    }
                }
            }
        }

        break;
    }

    case TPCPStartVote:   // receiving vote request
    {
        // operation not finished
        if (operation->status==TPCPartComp || operation->status == TPCPartVReqRec)
        {
            // set status to vote request received
            operation->status=TPCPartVReqRec;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartVReqRec);
        }
        else
        {
            // set up vote message
            // set coordinator to server_list
            vector<string> server_list;
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }
            // send message to coordinator
            if (operation->status==TPCPartWaitVReqYes || operation->status == TPCPartWaitVResultExpectYes)
            {
                failure =  send_simple_message(operation->ID,TPCVoteY, server_list);
                operation->status=TPCPartWaitVResultExpectYes;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartWaitVResultExpectYes);
            }
            else
            {
                failure =  send_simple_message(operation->ID,TPCVoteN, server_list);
                operation->status=TPCPartWaitVResultExpectNo;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartWaitVResultExpectNo);
            }

            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Cannot send vote message to coordinator in two phase commit protocol.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }
        }

        break;
    }

    case TPCPCommit:
    {
        // write the final journal entry for operation
        failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, true);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Commit log failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }

        // set up message for acknowledgement
        // set coordinator to server_list
        vector<string> server_list;
        for (int i=0;i<operation->participants.size();i++)
        {
            server_list.push_back(operation->participants.at(i).server);
        }

        // send acknowledgement
        failure = send_simple_message(operation->ID,TPCAck, server_list);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Sending acknowledgement to coordinator failed in two phase commit protocol.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }

        //delete operation from temporary data structure
        free(operation->operation);
        if (open_operations.erase(operation->ID)==0)
        {
#ifdef LOGGING_ENABLED
            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
        }
        break;
    }

    case TPCPAbort:
    {
        // still waiting for computation result
        if (operation->status==TPCPartComp || operation->status == TPCPartVReqRec || operation->status == TPCAborting)
        {
            // remember in memory that the operation is aborted
            operation->status = TPCAborting;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCAborting);
        }
        else
        {
            // check if operation need to be undone
            if ((operation->type == MoveSubtree || operation->type == ChangePartitionOwnership) && (operation->status == TPCPartWaitVReqYes || operation->status == TPCPartWaitVResultExpectYes || operation->status == TPCWaitUndoToFinish))
            {
                if (operation->status != TPCWaitUndoToFinish)
                {
                    forward_any_execution_request(operation, DAOUndoRequest);
                    operation->status = TPCWaitUndoToFinish;
                }
            }
            else   // computation finished, result known/logged and undo done if needed
            {
                // write the abort journal entry for operation
                failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Abort log failed.");
#endif
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return failure;
                }

                // set up message for acknowledgement
                // set coordinator to server_list
                vector<string> server_list;
                for (int i=0;i<operation->participants.size();i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }

                // send acknowledgement
                failure = send_simple_message(operation->ID,TPCAck, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending acknowledgement to coordinator failed in two phase commit protocol.");
#endif
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return failure;
                }

                // delete operation from temporary data structure
                free(operation->operation);
                if (open_operations.erase(operation->ID)==0)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
                }
            }
        }
        break;
    }

    case TPCIOperationFinSuccess:
    {
        // successful operation execution, only first time
        if (operation->status == TPCCoordinatorComp)
        {
            // write beginning of vote phase into log
            failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIVoteStart);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Logging own vote in two phase commit protocol failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

            // set up vote request message
            // add all participants
            vector<string> server_list;
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }

            // send vote request to participants
            failure = send_simple_message(operation->ID,TPCVoteReq, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending vote request in two phase commit protocol failed at least for one server.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

            // coordinator sent vote requests
            operation->status=TPCCoordinatorVReqSend;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCCoordinatorVReqSend);
        }
        break;
    }

    case TPCIOperationFinFailure:  	//immediately abort
    {
        if (operation->status == TPCCoordinatorComp)
        {
            // unsuccessful operation execution
            // write beginning of vote phase into log
            failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Logging global vote in two phase commit protocol failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

            // set up message for aborting
            // add all participants to server_list
            vector<string> server_list;
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }

            // send message to all participants
            failure = send_simple_message(operation->ID,TPCAbort, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending abort request in two phase commit protocol failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

            // client negative acknowledgement (NACK)
            provide_result(operation->ID, false, TwoPhaseCommit, operation->type==MoveSubtree);
            operation->status=TPCAborting;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCAborting);
        }
        break;
    }

    case TPCIVotesFin:   //all votes received and all particpants and coordinator voted with yes
    {
        //write vote result into log
        failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCICommiting);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Logging global vote in two phase commit protocol failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }

        // set up message for vote result
        // add all participants to server_list
        vector<string> server_list;
        for (int i=0;i<operation->participants.size();i++)
        {
            server_list.push_back(operation->participants.at(i).server);
        }

        // send message to all participants
        failure = send_simple_message(operation->ID,TPCCommit, server_list);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Sending global result failed in two phase commit protocol at least for one server.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }

        // client acknowledgement (ACK)
        provide_result(operation->ID, true, TwoPhaseCommit, operation->type==MoveSubtree);

        // vote result sent
        operation->status=TPCCoordinatorVResultSend;
        add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCCoordinatorVResultSend);
        break;
    }

    case TPCIAckRecv:   // all acknowledges received
    {
        if (operation->status!=TPCAborting && operation->status!=TPCWaitUndoAck && operation->status!=TPCWaitUndoToFinish)
        {
            // commit operation
            failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, true);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Commit log failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }
            // delete operation from temporal data structure
            free(operation->operation);
            if (open_operations.erase(operation->ID)==0)
            {
#ifdef LOGGING_ENABLED
                dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
            }
        }
        else
        {
            // still need to wait until undo finished
            if (operation->status==TPCWaitUndoAck || operation->status==TPCWaitUndoToFinish)
            {
                operation->status = TPCWaitUndoToFinish;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCWaitUndoToFinish);
            }
            else
            {
                // abort operation
                failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Abort log failed.");
#endif
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return failure;
                }
                // delete operation from temporal data structure
                free(operation->operation);
                if (open_operations.erase(operation->ID)==0)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
                }

                int num_removed=0;
                // remove all previous received votes from respective datastructure
                if (operation->recieved_votes<operation->participants.size())
                {
                    for (int i=0;operation->recieved_votes+num_removed<operation->participants.size() && i<operation->participants.size(); i++)
                    {
                        //create identification for possible received vote
                        stringstream vote_identification;
                        vote_identification<<operation->ID<<'.'<<operation->participants.at(i).server;
                        string vote_ID = vote_identification.str();
                        if (TPC_received_request_answers.erase(vote_ID))
                        {
                            num_removed++;
                        }
                    }
                }
                if (operation->recieved_votes+num_removed<operation->participants.size())
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("More or less vote identification than received are removed from the datastructure.");
#endif
                    remove_all_entries_for_operation(operation->ID);
                }
            }
        }
        break;
    }
    case TPCOperationUndone:
    {
        switch (operation->status)
        {
        case TPCWaitUndoToFinish:
        {
            // abort operation
            failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Abort log failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

            if ((operation->participants).size()>1)   // coordinator
            {
                // client acknowledgement (NACK)
                provide_result(operation->ID, false, TwoPhaseCommit, operation->type==MoveSubtree);
                // tidy up only coordinator
                int num_removed=0;
                // remove all previous received votes from respective datastructure
                if (operation->recieved_votes<operation->participants.size())
                {
                    for (int i=0;operation->recieved_votes+num_removed<operation->participants.size() && i<operation->participants.size(); i++)
                    {
                        //create identification for possible received vote
                        stringstream vote_identification;
                        vote_identification<<operation->ID<<'.'<<operation->participants.at(i).server;
                        string vote_ID = vote_identification.str();
                        if (TPC_received_request_answers.erase(vote_ID))
                        {
                            num_removed++;
                        }
                    }
                }
                if (operation->recieved_votes+num_removed<operation->participants.size())
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("More or less vote identification than received are removed from the datastructure.");
#endif
                    remove_all_entries_for_operation(operation->ID);
                }
            }
            else   //participant
            {
                // set up message for acknowledgement
                // set coordinator to server_list
                vector<string> server_list;
                for (int i=0;i<operation->participants.size();i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }

                // send acknowledgement
                failure = send_simple_message(operation->ID,TPCAck, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending acknowledgement to coordinator failed in two phase commit protocol.");
#endif
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return failure;
                }
            }
            // delete operation from temporal data structure
            free(operation->operation);
            if (open_operations.erase(operation->ID)==0)
            {
#ifdef LOGGING_ENABLED
                dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
            }
            break;
        }
        case TPCWaitUndoAck:
        {
            // client negative acknowledgement (NACK)
            provide_result(operation->ID, false, TwoPhaseCommit, operation->type==MoveSubtree);
            operation->status = TPCAborting;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCAborting);
            break;
        }
        default:
        {
#ifdef LOGGING_ENABLED
            dao_logger.warning_log("Unexpected case after operaion undone. Should be treated.");
#endif
        }
        }
        break;
    }

// modified two phase commit protocol
    case MTPCPOperationFinSuccess:
    {
        if (operation->status == MTPCPartComp)
        {
            // successful execution
            failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, MTPCPCommit);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Global vote result log failed in modfied two phase commit protocol.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

            // set up message for complete operation result
            // add coordinator to server_list
            vector<string> server_list;
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }

            // send operation result
            failure = send_simple_message(operation->ID,MTPCCommit, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending operation result to coordinator in modified two phase commit protocol failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

            // operation result sent
            operation->status=MTPCPartVoteSendYes;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCPartVoteSendYes);
        }
        break;
    }

    case MTPCPOperationFinFailure:
    {
        if (operation->status == MTPCPartComp)
        {
            // non-successful execution
            failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, MTPCPAbort);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Global vote log result failed in modified two phase commit protocol.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

            // set up message for complete operation result
            // add coordinator to server_list
            vector<string> server_list;
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }

            // send operation result
            failure = send_simple_message(operation->ID,MTPCAbort, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending operation result to coordinator in modified two phase commit protocol failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }
            // operation result sent
            operation->status=MTPCPartVoteSendNo;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCPartVoteSendNo);
        }
        break;
    }

    case MTPCPAckRec:
    {
        if (operation->status==MTPCPartVoteSendNo)   //aborting
        {
            failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Abort log failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }
        }
        else   //committing
        {
            failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, true);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Commit log failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }
        }
        // delete operation from temporal data structure
        free(operation->operation);
        if (open_operations.erase(operation->ID)==0)
        {
#ifdef LOGGING_ENABLED
            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
        }
        break;
    }

    case MTPCIOperationFinSuccess:
    {
        if (operation->status == MTPCCoordinatorComp)
        {
            //successful execution
            // write beginning of second server into log
            failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, MTPCIStartP);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Coordinator vote log in modified two phase commit protocol failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }
            //Message: MESSAGEtype (char), ID (uint64_t), operationtype (char), subtree_entry_this(InodeNumber)
            //         , subtree_entry_Initiator(InodeNumber), operation_lenght (uint32_t), Operation

            // set up operation begin request message
            char messagetype;
            int message_length;
            char* message_data;
            message_length = sizeof(char)+ sizeof(uint32_t)+ sizeof(char)+ sizeof(uint64_t) +2* sizeof(InodeNumber)+ operation->operation_length;
            message_data = (char*)malloc(message_length);
            if (message_data == NULL)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Cannot get enough memory to create execution request message in modified two phase commit protocol.");
#endif
                handle_internal_failure(DAOInsufficientMemory);
            }

            // set up message participant independent message content
            messagetype = MTPCOpReq;
            //set event type
            memcpy(message_data, &messagetype, sizeof(char));
            // set operation ID
            memcpy(message_data+sizeof(char), &(operation->ID), sizeof(uint64_t));
            // set operation type
            *(((char*)message_data)+sizeof(char)+sizeof(uint64_t)) = operation->type;
            // participant's subtree entry point set later
            // set coordinator subtree entry point
            memcpy(message_data+2*sizeof(char)+ sizeof(uint64_t)+sizeof(InodeNumber), &(operation->subtree_entry), sizeof(InodeNumber));
            // set operation description
            memcpy(message_data+2*sizeof(char)+sizeof(uint64_t)+2*sizeof(InodeNumber), &(operation->operation_length), sizeof(uint32_t));
            memcpy(message_data+2*sizeof(char)+sizeof(uint32_t)+sizeof(uint64_t)+2*sizeof(InodeNumber), operation->operation, operation->operation_length);

            vector<string> server_list;
            //include particpant depend message data and send message
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
                // set participant's subtree entry point
                memcpy(message_data+2*sizeof(char)+sizeof(uint64_t), &(operation->participants.at(i).subtree_entry), sizeof(InodeNumber));
                Pc2fsProfiler::get_instance()->function_sleep();
                // send message
                failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
                Pc2fsProfiler::get_instance()->function_wakeup();
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending execution request in modified two phase commit protocol failed.");
#endif
                    if (failure==MutexFailed)
                    {
                        handle_internal_failure(DAOMutexFailure);
                    }
                    else
                    {
                        if (failure == UnableToCreateMessage || failure == NotEnoughMemory)
                        {
                            handle_internal_failure(DAOInsufficientMemory);
                        }
                        else
                        {
                            if (failure == SendingFailed)
                            {
                                if (errno>=0 && errno<server_list.size())
                                {
                                    handle_supposed_external_server_failure(server_list.at(errno));
                                }
                            }
                            else
                            {
                                if (errno>=0 && errno<server_list.size())
                                {
                                    int creation_failure = handle_socket_not_existing(server_list.at(errno));
                                    if (creation_failure == 0)
                                    {
                                        Pc2fsProfiler::get_instance()->function_sleep();
                                        failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
                                        Pc2fsProfiler::get_instance()->function_wakeup();
                                    }
                                    else
                                    {
                                        if (creation_failure == DAOUnknownAddress)
                                        {
                                            // ask the respective executing module for the correct address
                                            if (operation->type == MoveSubtree)
                                            {
                                                DaoHandler::set_sending_addresses(operation);
                                            }
                                            else
                                            {
                                                ChangeOwnershipDAOAdapter::set_sending_addresses(operation);
                                            }
                                            if (!failure)
                                            {
                                                failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
                                            }
#ifdef LOGGING_ENABLED
                                            else
                                            {
                                                dao_logger.error_log("Getting MTPC participant which was incorrect failed.");
                                            }
#endif
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                server_list.clear();
            }
            free(message_data);
            // operation request sent
            operation->status=MTPCCoordinatorReqSend;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCCoordinatorReqSend);
        }
        break;
    }

    case MTPCIOperationFinFailure:
    {
        //non-successful execution
        // write abort into log
        failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Abort log failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }
        // delete operation from temporary data structure
        free(operation->operation);
        if (open_operations.erase(operation->ID)==0)
        {
#ifdef LOGGING_ENABLED
            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
        }

        // client negative acknowledgement (NACK)
        provide_result(operation->ID, false, ModifiedTwoPhaseCommit, operation->type==MoveSubtree);
        break;
    }

    case MTPCICommit:
    {
        // write commit into log
        failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, true);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Commit log failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }

        // set up message for acknowledgement
        // add participant
        vector<string> server_list;
        for (int i=0;i<operation->participants.size();i++)
        {
            server_list.push_back(operation->participants.at(i).server);
        }

        // send acknowledgement
        failure = send_simple_message(operation->ID,MTPCAck, server_list);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Sending acknowledgement message in modified two phase commit protocol failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }

        // client acknowledgement (ACK)
        provide_result(operation->ID, true, ModifiedTwoPhaseCommit, operation->type==MoveSubtree);

        // delete operation from temporary datastructure
        free(operation->operation);
        if (open_operations.erase(operation->ID)==0)
        {
#ifdef LOGGING_ENABLED
            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
        }
        break;
    }

    case MTPCIAbort:
    {
        // check if it is an operation which need to be undone first
        if ((operation->type == MoveSubtree || operation->type == ChangePartitionOwnership))
        {
            if (operation->status != MTPCIWaitResultUndone)
            {
                forward_any_execution_request(operation, DAOUndoRequest);
                operation->status=MTPCIWaitResultUndone;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCIWaitResultUndone);
            }
        }
        else
        {
            failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Abort log failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }
            // set up message for acknowledgement
            // add coordinator to server_list
            vector<string> server_list;
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }

            // send acknowledgement
            failure = send_simple_message(operation->ID,MTPCAck, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending acknowledgement in modified two phase commit protocol failed.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return failure;
            }

            // client negative acknowledgement (NACK)
            provide_result(operation->ID, false, ModifiedTwoPhaseCommit, operation->type==MoveSubtree);

            //delete from temporary datastructure
            free(operation->operation);
            if (open_operations.erase(operation->ID)==0)
            {
#ifdef LOGGING_ENABLED
                dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
            }
        }
        break;
    }
    case MTPCOperationUndone:
    {
        // operation undone at coordinator side, finish operation, send acknowledgement and inform client
        failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Abort log failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }
        // set up message for acknowledgement
        // add coordinator to server_list
        vector<string> server_list;
        for (int i=0;i<operation->participants.size();i++)
        {
            server_list.push_back(operation->participants.at(i).server);
        }

        // send acknowledgement
        failure = send_simple_message(operation->ID,MTPCAck, server_list);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Sending acknowledgement in modified two phase commit protocol failed.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }

        // client negative acknowledgement (NACK)
        provide_result(operation->ID, false, ModifiedTwoPhaseCommit, operation->type==MoveSubtree);

        //delete from temporary datastructure
        free(operation->operation);
        if (open_operations.erase(operation->ID)==0)
        {
#ifdef LOGGING_ENABLED
            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
        }
        break;
    }

    default:
    {

#ifdef LOGGING_ENABLED
        dao_logger.warning_log("Unexpected event should be treated.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();
        return DAOWrongEvent;
    }
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 *\brief Checks if the two subtrees are equal
 *
 *\param tree1 - first subtree of comparison, subtree which should be compared to <code>tree2</code>
 *\param tree2 - second subtree of comparison, subtree which should be compared to <code>tree1</code>
 *
 *\return 0 if subtrees are equal, otherwise an indicator, a kind of failure code which describes where they are different. DAOWrongParameter is returned if one of the two subtrees cannot be a correct subtree.
 *
 * Checks if the two subtrees <code>tree1, tree2</code> are equal. This means that the subtree entry point and the server address is
 * identical. A different server address while the subtree entry point is the same may indicate a subtree movement.
 */
int DistributedAtomicOperations::same_subtrees(Subtree* tree1, Subtree* tree2)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    if (tree1->server.size()== 0 || tree2->server.size() == 0)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return DAOWrongParameter;
    }
    if (tree1->subtree_entry!=tree2->subtree_entry)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return DAOSubtreeUnequal;
    }
    if (tree1->server.compare(tree2->server) == 0)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return DAODifferentServerAddresses;
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 *\brief Informs the initiating module about the operation result
 *
 *\param id - unique identifier of the atomic operation which should tell the initiating module the operation result
 *\param commit - indicator if the operation was successful (commit) or unsuccessful
 *\param protocol - protocol which was used to execute the respective distributed atomic operation
 *\param load_queue - tells if the result must be sent to the load balancing module or if it is sent to the metadata server module
 *
 * Informs the initiating module about the operation result of the distributed atomic operation <code>id</code>. The result is put into
 * the respective queue which is used for communication between the module and the distributed atomic operations module.
 *
 *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched, caused by insufficient memory available
 */
void DistributedAtomicOperations::provide_result(uint64_t id, bool commit, Protocol protocol, bool use_load_queue)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    OutQueue ack;
    ack.ID = id;
    ack.operation_length = 0;
    ack.protocol =  protocol;
    ack.operation = malloc(sizeof(char));
    if (ack.operation == NULL)
    {
#ifdef LOGGING_ENABLED
        dao_logger.error_log("Cannot get enough memory to provide client acknowledgement.");
#endif
        handle_internal_failure(DAOInsufficientMemory);
    }
    if (commit)
    {
        *((char*)(ack.operation))=1;
    }
    else
    {
        *((char*)(ack.operation))=0;
    }
    // inform client about result
    if (use_load_queue)
    {
        load_queue.push(ack);
    }
    else
    {
        mdi_queue.push(ack);
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
 *\brief Inserts the operation request type to the operation description and sends the request to the respective module
 *
 *\param operation - reference to existing distributed atomic operation for which a request of type <code>request_type</code> should be forwarded to the respective queue
 *\param request_type - specifies which kind of request should be forwarded to the executing module
 *
 * Adds the operation request type to the operation description and puts the request into the queue which is used to communicate with
 * the executing module.
 *
 *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched, caused due to too few memory available
 */
void DistributedAtomicOperations::forward_any_execution_request(DAOperation* operation, OperationRequestType request_type)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    // create the execution request
    OutQueue out_operation;
    out_operation.ID=operation->ID;
    out_operation.operation=malloc(operation->operation_length+1);
    if (out_operation.operation == NULL)
    {
        handle_internal_failure(DAOInsufficientMemory);
    }
    // set request type
    switch (request_type)
    {
    case DAORequest:
    {
        char request_specification = DAORequest;
        memcpy(out_operation.operation, &request_specification, 1);
        break;
    }
    case DAORedoRequest:
    {
        char request_specification = DAORedoRequest;
        memcpy(out_operation.operation, &request_specification, 1);
        break;
    }
    case DAOUndoRequest:
    {
        char request_specification = DAOUndoRequest;
        memcpy(out_operation.operation, &request_specification, 1);
        break;
    }
    case DAOReundoRequest:
    {
        char request_specification = DAOReundoRequest;
        memcpy(out_operation.operation,&request_specification, 1);
        break;
    }
    }
    // add operation data
    memcpy(out_operation.operation+1, operation->operation, operation->operation_length);
    out_operation.operation_length=(operation->operation_length)+1;
    out_operation.protocol=operation->used_protocol;
    // send request
    if (operation->type==MoveSubtree || operation->type == OOE_LBTest)
    {
        load_queue.push(out_operation);
    }
    else
    {
        mdi_queue.push(out_operation);
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
 *\brief Sends simple messages which contain the message_type only
 *
 *\param id - unique ID of the distributed atomic operation for which a message is sent
 *\param message_type - content of the message which is sent, only those types are allowed which are selfdescribing, thus no additional information need to be sent (everything except operation requests)
 *\param receivers - list of servers which should receive the message sent by this method, at least one server needs to be contained
 *
 *\return 0 if execution was successful otherwise an error code
 *
 * Sends messages which only contain the message_type. The message is sent to the servers specified in <code>receivers</code>. Failures
 * during sending are handled.
 *
 *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched
 */
int DistributedAtomicOperations::send_simple_message(uint64_t id, ExchangeMessages message_type, vector<string>& receivers)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    if (receivers.size()<1 || message_type == MTPCOpReq || message_type == OOEOpReq || message_type == TPCOpReq)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return DAOWrongParameter;
    }
    char* message_data;
    int message_length = sizeof(char)+sizeof(uint64_t);
    message_data = (char*)malloc(message_length);
    if (message_data == NULL)
    {
#ifdef LOGGING_ENABLED
        dao_logger.error_log("Cannot get enough memory to create simple message.");
#endif
        handle_internal_failure(DAOInsufficientMemory);
    }

    // set up message content (message type, operation ID)
    char message= message_type;
    memcpy( message_data, &message, sizeof(char));
    memcpy( message_data + sizeof(char), &id, sizeof(uint64_t));

    Pc2fsProfiler::get_instance()->function_sleep();
    // send message
    int failure =  send( message_data,  message_length, COM_DistributedAtomicOp, receivers);
    Pc2fsProfiler::get_instance()->function_wakeup();
    if (failure)
    {
#ifdef LOGGING_ENABLED
        dao_logger.error_log("Sending simple message failed.");
#endif
        if (failure==MutexFailed)
        {
            handle_internal_failure(DAOMutexFailure);
        }
        else
        {
            if (failure == UnableToCreateMessage|| failure == NotEnoughMemory)
            {
                handle_internal_failure(DAOInsufficientMemory);
            }
            else
            {
                if (failure == SocketNotExisting)
                {
                    if (errno >= 0 && errno < receivers.size())
                    {

                        int creation_failure = handle_socket_not_existing(receivers.at(errno));
                        if (creation_failure == 0)
                        {
                            Pc2fsProfiler::get_instance()->function_sleep();
                            failure = send( message_data,  message_length, COM_DistributedAtomicOp, receivers);
                            Pc2fsProfiler::get_instance()->function_wakeup();
                        }
                        else
                        {
                            if (creation_failure == DAOUnknownAddress)
                            {
                                unordered_map<uint64_t,DAOperation>::iterator it = instance.open_operations.find(id);
                                if (it !=open_operations.end())
                                {
                                    // ask the respective executing module for the correct address
                                    if ((*it).second.type == MoveSubtree)
                                    {
                                        creation_failure = DaoHandler::set_sending_addresses(&((*it).second));
                                    }
                                    else
                                    {
                                        creation_failure = ChangeOwnershipDAOAdapter::set_sending_addresses(&((*it).second));
                                    }
#ifdef LOGGING_ENABLED
                                    if (creation_failure)
                                    {
                                        dao_logger.error_log("Getting participants and coordinator respect which were incorrect failed.");
                                    }
#endif
                                }
                            }
                        }
                    }
                }
                else
                {
                    if (errno>=0 && errno<receivers.size())
                    {
                        handle_supposed_external_server_failure(receivers.at(errno));
                    }
                }
            }
        }
    }

    free(message_data);
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return failure;
}

/**
 *\brief Writes every log entry which is neither a commit, abort or begin log
 *
 *\param id - unique identifier of the distributed atomic operation for which an update log should be written
 *\param server_journal - specifies if the distributed atomic operation status is logged in the global server log or in the journal of the respective subtree
 *\param journal_identifier - if server_journal is false, the subtree entry inode number of the corresponding subtree, if server_journal is true this value is not considered
 *\param log_content - content which should be logged to describe the current status of the respective distributed atomic operation
 *
 *\return 0 if log writing was successful otherwise a failure code
 *
 * Writes every log entry which is neither a commit, abort or begin log. For this kind of logs it is sufficient to specifiy the log
 * message as defined in <code>LogMessages</code>.
 *
 *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched, caused by unsuccessful writing into journal
 */
int DistributedAtomicOperations::write_update_log(uint64_t id, bool server_journal, InodeNumber journal_identifier, LogMessages log_content)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    Journal* journal = NULL;
    // get correct journal
    if (server_journal)
    {
        journal = (*journal_manager).get_server_journal();
    }
    else
    {
        journal = (*journal_manager).get_journal(journal_identifier);
    }

    if (journal == NULL)
    {
#ifdef LOGGING_ENABLED
        dao_logger.error_log("Journal is not available. Try to repair.");
#endif
        failure = handle_journal_not_existing(server_journal, journal_identifier, true);
        if (failure)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }
    }
    char log_message = (char) log_content;
    Pc2fsProfiler::get_instance()->function_sleep();
    // write log entry
    failure = journal->add_distributed(id, DistributedAtomicOp, DistributedOp , DistributedUpdate , &log_message, 1);
    Pc2fsProfiler::get_instance()->function_wakeup();
    if (failure)
    {
#ifdef LOGGING_ENABLED
        dao_logger.error_log("Update log failed.");
#endif
        handle_internal_failure(DAOJournalFailure);
        // further things need to be done here after if handle_internal_failure does not throw exception
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 *\brief Writes commit and abort logs
 *
 *\param id - unique identifier of the distributed atomic operation for which an commit or abort log should be written
 *\param server_journal - specifies if the distributed atomic operation status is logged in the global server log or in the journal of the respective subtree
 *\param journal_identifier - if server_journal is false, the subtree entry inode number of the corresponding subtree, if server_journal is true this value is not considered
 *\param commit - indicates if a commit or an abort should be logged
 *
 *\return 0 if log writing was successful otherwise a failure code
 *
 * Writes the commit or abort log for the respective distributed atomic operation into the specified journal. Afterwards the operation
 * is known as finished by the journal.
 *
 *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched, caused by unsuccessful writing into journal
 */
int DistributedAtomicOperations::write_finish_log(uint64_t id, bool server_journal, InodeNumber journal_identifier, bool commit)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    Journal* journal = NULL;
    // get correct journal
    if (server_journal)
    {
        journal = (*journal_manager).get_server_journal();
    }
    else
    {
        journal = (*journal_manager).get_journal(journal_identifier);
    }

    if (journal == NULL)
    {
#ifdef LOGGING_ENABLED
        dao_logger.error_log("Journal is not available. Try to repair.");
#endif
        failure = handle_journal_not_existing(server_journal, journal_identifier, true);
        if (failure)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return failure;
        }
    }
    // write log entry
    if (commit)
    {
        Pc2fsProfiler::get_instance()->function_sleep();
        failure = journal->add_distributed(id, DistributedAtomicOp, DistributedOp , Committed , NULL, 0);
        Pc2fsProfiler::get_instance()->function_wakeup();
    }
    else
    {
        Pc2fsProfiler::get_instance()->function_sleep();
        failure = journal->add_distributed(id, DistributedAtomicOp, DistributedOp , Aborted , NULL, 0);
        Pc2fsProfiler::get_instance()->function_wakeup();
    }

    if (failure)
    {
#ifdef LOGGING_ENABLED
        if (commit)
        {
            dao_logger.error_log("Commit log failed.");
        }
        else
        {
            dao_logger.error_log("Abort log failed.");
        }
#endif
        handle_internal_failure(DAOJournalFailure);
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}


/**
 *\brief Recovers a distributed atomic operation which cannot be found in the temporary datastructure
 *
 *\param id - unique identifier of the operation which is expected to be lost and should be recovered from journal now
 *
 *\return 0 if the operation is lost and can be retrieved, DAOOperationNotInJournal if it cannot be retrieved because it never existed, it is already finished or the corresponding subtree of the operation has moved, otherwise a failure code
 *
 * If an operation cannot be found in the temporary datastructure, the operation will be searched in the journals of the metadata server.
 * If the operation is found, it will be added to the temporary datastructure. The data for the temporary datastructure is provided by
 * the journal content as far as possible. The rest of the data is added by the module which executes the corresponding suboperation
 * for the distributed atomic operation on this server.
 */
int DistributedAtomicOperations::retrieve_lost_operation(uint64_t id)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    // check if really not existing
    if (open_operations.find(id)!=open_operations.end())
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return 0;
    }
    // try to retrieve operation
    DAOperation new_operation;
    // set id
    new_operation.ID = id;
    // vector which will contain all parts from the operation if can be found in a journal of this metadata server
    vector<Operation> found_operations;
    // get operation from journal
    new_operation.subtree_entry = (*journal_manager).get_open_operations(id, found_operations);
    if (new_operation.subtree_entry == INVALID_INODE_ID)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        // operation not found
        return DAOOperationNotInJournal;
    }


    // get rest of data
    int failure = recover_missing_operation_data(&new_operation, found_operations);
    if (failure)
    {
#ifdef LOGGING_ENABLED
        dao_logger.error_log("Operation content cannot be retrieved.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return failure;
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 *\brief Sets all data of a recovered distributed atomic operation except for the ID
 *
 *\param operation - reference to operation for in which the missing data should be inserted
 *\param operation_logs - all logs which are written into the journal for the recovered operation, the logs are ordered in the order of creation, the first entry is the oldest log entry
 *
 *\return 0 if all parameters can be set successfully otherwise a failure code
 *
 * Sets all data of a recovered distributed atomic operation except for the ID. Furthermore the operation is saved in the temporary
 * datastructure.
 */
int DistributedAtomicOperations::recover_missing_operation_data(DAOperation* operation, vector<Operation>& operation_logs)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    OperationData begin_op, last_op;
    // get the begin log information
    for (int i=0;i<operation_logs.size();)
    {
        begin_op = operation_logs.at(i).get_operation_data();
        // check if it is begin log
        if (begin_op.status == DistributedStart && begin_op.module == DistributedAtomicOp && begin_op.operation_type == DistributedOp)
        {
            break;
        }
        i++;
        if (i==operation_logs.size())
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return DAONoBeginLog;
        }
    }

    // set operation type
    operation->type = (OperationTypes) begin_op.data[0];

    // set the operation content (length and data)
    memcpy(&(operation->operation_length), begin_op.data+sizeof(char), sizeof(uint32_t));;
    operation->operation = malloc(operation->operation_length);
    if (operation->operation == NULL)
    {
        handle_internal_failure(DAOInsufficientMemory);
    }
    memcpy(operation->operation, begin_op.data+sizeof(char)+sizeof(uint32_t),operation->operation_length);

    LogMessages last_log;
    // get the last log for this operation
    for (int i=operation_logs.size()-1;i>=0;i--)
    {
        last_op = operation_logs.at(i).get_operation_data();
        if ((last_op.status == DistributedStart || last_op.status == DistributedUpdate || last_op.status == Committed ||
                last_op.status == Aborted) && last_op.module == DistributedAtomicOp && last_op.operation_type == DistributedOp)
        {
            // check if operation already finished
            if (last_op.status == Committed || last_op.status == Aborted)
            {
                free(operation->operation);
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAOOperationFinished;
            }
            else
            {
                if (last_op.status != DistributedStart)
                {
                    // read log message
                    last_log = (LogMessages) last_op.data[0];
                }
                break;
            }
        }
    }

    // ask the respective module for the rest of the operation information (participants)
    int failure;
    if (operation->type == MoveSubtree)
    {
        failure = DaoHandler::set_sending_addresses(operation);
        if (failure)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();
            return DAOSettingAddressesFailed;
        }
    }
    else
    {
        ChangeOwnershipDAOAdapter::set_sending_addresses(operation);
        if (failure)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();
            return DAOSettingAddressesFailed;
        }
    }

    // set protocol
    if (operation->type<=SetAttr)
    {
        if (operation->participants.size()==1)
        {
            operation->used_protocol = ModifiedTwoPhaseCommit;
            operation->status = MTPCCoordinatorComp;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCCoordinatorComp);
        }
        else
        {
            operation->used_protocol = TwoPhaseCommit;
            operation->status = TPCCoordinatorComp;
            // set recieved_votes
            operation->recieved_votes = operation->participants.size();
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCCoordinatorComp);
        }
    }
    else
    {
        if (operation->type == OrderedOperationTest || operation->type == OOE_LBTest)
        {
            operation->used_protocol = OrderedOperationExecution;
            operation->status = OOEComp;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEComp);
        }
    }

    // set overall timeout
    operation->overall_timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::milliseconds(OVERALL_TIMEOUT);

    // set status
    if (last_op.status == DistributedStart)
    {
        if (operation->used_protocol == ModifiedTwoPhaseCommit)
        {
            if (operation->type == MoveSubtree)
            {
                if (DaoHandler::is_coordinator(operation))
                {
                    operation->status = MTPCCoordinatorComp;
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, MTPCCoordinatorComp);
                }
                else
                {
                    operation->status = MTPCPartComp;
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, MTPCPartComp);
                }
            }
            else
            {
                if (ChangeOwnershipDAOAdapter::is_coordinator(operation))
                {
                    operation->status = MTPCCoordinatorComp;
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, MTPCCoordinatorComp);
                }
                else
                {
                    operation->status = MTPCPartComp;
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, MTPCPartComp);
                }
            }
        }
        else
        {
            if (operation->used_protocol == TwoPhaseCommit)
            {
                if (operation->participants.size()== 1)   //participant
                {
                    operation->status = TPCPartComp;
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartComp);
                }
                else   // coordinator
                {
                    operation->status = TPCCoordinatorComp;
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCCoordinatorComp);
                }
            }
            else
            {
                operation->status = OOEComp;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEComp);
            }
        }
    }
    else
    {
        switch (last_log)
        {
        case TPCPVoteYes:
        {
            operation->status = TPCPartWaitVReqYes;
            break;
        }
        case TPCPVoteNo:
        {
            operation->status = TPCPartWaitVReqNo;
            break;
        }
        case TPCIVoteStart:
        {
            operation->status = TPCCoordinatorVReqSend;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCCoordinatorVReqSend);
            break;
        }
        case TPCIAborting:
        {
            if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
            {
                operation->status = TPCWaitUndoAck;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCWaitUndoAck);
            }
            else
            {
                // set status to aborting
                operation->status = TPCAborting;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCAborting);
            }
            break;
        }
        case TPCICommiting:
        {
            operation->status = TPCCoordinatorVResultSend;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCCoordinatorVResultSend);
            break;
        }
        case MTPCPCommit:
        {
            operation->status = MTPCPartVoteSendYes;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCPartVoteSendYes);
            break;
        }
        case MTPCPAbort:
        {
            operation->status = MTPCPartVoteSendNo;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCPartVoteSendNo);
            break;
        }
        case MTPCIStartP:
        {
            operation->status = MTPCCoordinatorReqSend;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCCoordinatorReqSend);
            break;
        }
        case OOEStartNext:
        {
            operation->status = OOEWaitResult;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEWaitResult);
            break;
        }
        case OOEUndo:
        {
            operation->status = OOEWaitResultUndone;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEWaitResultUndone);
            break;
        }
        default:
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Unknown log content.");
#endif
            free(operation->operation);
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return DAOUnknownLog;
        }
        }
    }

    // insert the operation into main memory
    open_operations.insert(unordered_map<uint64_t,DAOperation>::value_type(operation->ID,*operation));
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 *\brief Removes all entries of received requests which belong the distributed atomic operation with ID <code>id</code>
 *
 *\param id - unique identifier of the atomic operation for which all stored request answers should be removed
 *
 * Iterates over all stored received request answers and removes all request answers which belong to the distributed atomic operation
 * with ID <code>id</code>.
 */
void DistributedAtomicOperations::remove_all_entries_for_operation(uint64_t id)
{
    // start profiling
    Pc2fsProfiler::get_instance()->function_start();

    unordered_set<std::string>::size_type pos;
    // build prefix determining all strings which must be removed
    stringstream s;
    s<<id<<'.';
    string prefix = s.str();
    unordered_set<std::string>::iterator it = TPC_received_request_answers.begin();
    while (it!= TPC_received_request_answers.end())
    {
        // check if it is an item which must be removed
        pos = (*it).find(prefix);
        if (pos!= string::npos && pos == 0)
        {
            it = TPC_received_request_answers.erase(it);
        }
        else
        {
            it++;
        }
    }
    // end profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
 *\brief If journal must be available, it will be created.
 *
 *\param server_journal - inidicates if the journal of the metadata server is not available (server_journal == true) or if a subtree related journal is not available
 *\param subtree_entry - if server_journal == false, the subtree entry point of the subtree for which the journal is not available, otherwise it can be any number
 *\param first_call - indicates if the method was called in the first iteration, always true if the method is called ...
 *
 *\return 0 if journal was created successfully and can be used, otherwise a failure code indicating why the journal cannot be created or must not be created
 *
 * Checks if the respective journal must be available. If it must not be available or the check failed, an error code will be returned.
 * Otherwise it is tried to create the journal.
 *
 *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched
 */
int DistributedAtomicOperations::handle_journal_not_existing(bool server_journal, InodeNumber subtree_entry, bool first_call)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    if (server_journal)   //create server journal
    {
        try
        {
            (*journal_manager).create_server_journal();
        }
        catch (JournalException e)
        {
            handle_internal_failure(DAOSalNotSet);
            if (first_call)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return handle_journal_not_existing(true, subtree_entry, false);
            }
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return DAONoSal;
        }
    }
    else
    {
        MltHandler& handler = MltHandler::get_instance();
        // check if subtree entry point reflects a known root of a subtree
        bool is_root = false;
        try
        {
            is_root = handler.is_partition_root(subtree_entry);
        }
        catch (MltHandlerException e)
        {
            handle_internal_failure(DAOMltNotExisting);
            if (first_call)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return handle_journal_not_existing(false, subtree_entry, false);
            }
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return DAONoMlt;
        }
        if (!is_root)   // no known root of subtree
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return DAOSubtreeNotExisting;
        }
        else
        {
            // check if it is the correct server
            try
            {
                Server corresponding_mds = handler.get_mds(subtree_entry);
                if (corresponding_mds.address.compare(handler.get_my_address().address)!=0)
                {
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return DAOSelfWrongServer;
                }
            }
            catch (MltHandlerException e)
            {
                handle_internal_failure(DAOMltNotExisting);
                if (first_call)
                {
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return handle_journal_not_existing(false, subtree_entry, false);
                }
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAONoMlt;
            }
        }
        try  // create journal which must be available but it is not
        {
            (*journal_manager).create_journal(subtree_entry);
        }
        catch (JournalException e)
        {
            handle_internal_failure(DAOSalNotSet);
            if (first_call)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return handle_journal_not_existing(true, subtree_entry, false);
            }
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return DAONoSal;
        }
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 *\brief Adds a send socket with address <code> socket_address</code> to the SendCommunicationServer
 *
 *\param socket_address - identifier of the server for which a sending socket is not existing. The identifier is the IP address of the server. If VARIABLE_PORTS defined unequal to 0 the IP address must include the port as well.
 *
 *\return a failure code if the socket cannot be created successfully, 0 otherwise if no exception occurred
 *
 * Tries to add a send socket to the SendCommunicationServer. If an internal failure like a mutex failure occurs this is handled
 * internally first. In general all failures occurring during adding of the socket lead to a negative method result. Only if the socket
 * can be used afterwards, a positive result will be returned.
 *
 *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched, caused by mutex failure
 */
int DistributedAtomicOperations::handle_socket_not_existing(string socket_address)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    // get known servers from mlt
    vector<Server> server_list;
    int failure = MltHandler::get_instance().get_server_list(server_list);
    if (failure)
    {
        handle_internal_failure(DAOMltNotExisting);
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return DAOInternal;
    }

    const char*  char_address = socket_address.c_str();
    const char* mlt_address;
    // check if the server for which a socket cannot be found is known by the mlt
    int mlt_address_size;
    int i=0;
    for (;i<server_list.size();i++)
    {
        // build mlt server address
        if (VARIABLE_PORTS)
        {
            stringstream my_port;
            my_port<<":"<<server_list.at(i).port;
            string socket = server_list.at(i).address;
            socket.append(my_port.str());
            mlt_address_size = socket.size();
            mlt_address = socket.c_str();
        }
        else
        {
            mlt_address_size = server_list.at(i).address.size();
            mlt_address = server_list.at(i).address.c_str();
        }
        if (mlt_address_size==socket_address.size() && (memcmp(mlt_address, char_address, mlt_address_size) == 0))
        {
            break;
        }
    }
    if (i==server_list.size())
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return DAOUnknownAddress;
    }
    // add socket to sending server
    failure = SendCommunicationServer::get_instance().add_server_send_socket(socket_address);
    if (failure == MutexFailed)
    {
        handle_internal_failure(DAOMutexFailure);
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return DAOInternal;
    }
    else
    {
        if (failure == SocketCreationFailed)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return DAOInternal;
        }
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 *\brief Error handling for internal failures like mutex, journal log or insufficient memory failures.
 *
 *\param failure_type - indicates which kind of failure should be treated.
 *
 * Error handling for internal failures like mutex, journal log or insufficient memory failures. Does not handle failures occuring during
 * creation or destruction of DistributedAtomicOperations singleton instance.
 *
 *\throw runtime_error if an internal error cannot be solved during runtime, do not catch this exception if the problem cannot be solved
 */
void DistributedAtomicOperations::handle_internal_failure(DAOFailure failure_type)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    switch (failure_type)
    {
    case DAOOperationCorrupted:
    {
        throw runtime_error("Timed out operation is corrupted (unknown status or protocoll).");
        break;
    }
    case DAOMutexFailure:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        throw runtime_error("Mutex cannot be acquired.");
        break;
    }
    case DAOJournalFailure:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        throw runtime_error("Writing journal log failed due to problems with the backend.");
        break;
    }
    case DAOInsufficientMemory:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        throw runtime_error("Not enough memory available.");
        break;
    }
    case DAOMltNotExisting:
    {
        MltHandler& handler = MltHandler::get_instance();
        Pc2fsProfiler::get_instance()->function_sleep();
        // read MLT
        int failure = handler.read_from_file(MLT_PATH);
        Pc2fsProfiler::get_instance()->function_wakeup();
        if (failure == UnableToReadMlt)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            throw runtime_error("Reading the mlt failed due to problems with the backend.");
        }
        break;
    }
    case DAOSalNotSet:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        throw runtime_error("Journal manager does not know a storage abstraction layer.");
        break;
    }
    default:
    {
    }
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

}

/**
 *\brief Determines the protocol state of the coordinator or participant executing the counter part of the operation
 *
 *\param operation - reference to the distributed atomic operation for which the state should be encountered
 *
 *\return the status of the counter part of the distributed atomic operation
 *
 * Uses the own status to reason about the status of the coordinator or participant executing the counter part of the operation. This means
 * that the last log is reasoned and the corresponding status is returned.
 *
 *\throw runtime_error if the status is not defined
 */
Status DistributedAtomicOperations::get_protocol_status_of_opposite_side(DAOperation* operation)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    switch (operation->status)
    {
    case TPCCoordinatorComp:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return TPCPartComp;
    }
    case TPCCoordinatorVReqSend:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return TPCPartVReqRec;
    }
    case TPCCoordinatorVResultSend:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return TPCPartWaitVResultExpectYes;
    }
    case TPCWaitUndoAck:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return TPCWaitUndoToFinish;
    }
    case TPCPartComp:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return TPCCoordinatorComp;
    }
    case TPCPartWaitVReqYes:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return TPCCoordinatorComp;
    }
    case TPCPartWaitVReqNo:
    {
        return TPCCoordinatorComp;
    }
    case TPCPartVReqRec:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return TPCCoordinatorVReqSend;
    }
    case TPCPartWaitVResultExpectYes:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return TPCCoordinatorVReqSend;
    }
    case TPCPartWaitVResultExpectNo:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return TPCCoordinatorVReqSend;
    }
    case TPCAborting:
    {
        // check if protocol status must be reasoned for coordinator
        if (operation->participants.size()==1)
        {
            // check for undo
            if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership || operation->type == OOE_LBTest)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return TPCWaitUndoAck;
            }
            else
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return TPCAborting;
            }
        }
        else
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return TPCAborting;
        }
    }
    case TPCWaitUndoToFinish:
    {
        // check if protocol status must be reasoned for coordinator
        if (operation->participants.size()==1)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return TPCWaitUndoAck;
        }
        else
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return TPCAborting;
        }
    }
    case MTPCCoordinatorComp:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return MTPCCoordinatorComp;
    }
    case MTPCPartComp:
    {
        return MTPCCoordinatorReqSend;
    }
    case MTPCCoordinatorReqSend:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return MTPCPartComp;
    }
    case MTPCIWaitResultUndone:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return MTPCPartVoteSendNo;
    }
    case MTPCPartVoteSendYes:
    {
        Pc2fsProfiler::get_instance()->function_end();
        return MTPCCoordinatorReqSend;
    }
    case MTPCPartVoteSendNo:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return MTPCCoordinatorReqSend;
    }
    case OOEComp:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return OOEComp;
    }
    case OOEWaitResult:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return OOEComp;
    }
    case OOEWaitResultUndone:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return OOEWaitResultUndone;
    }
    default:
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        throw runtime_error("This status is not existing");
    }
    }
}

/**
 *\brief Handles the failure messages NotResponsible, EventReRequest, ContentRequest, ContentResponse, StatusRequest and StatusResponse.
 *
 *\param message_content - message received by the distributed atomic operation and which is a failure message
 *\param event_request - type of failure message
 *\param ID - identifier of the operation for which a failure message was send
 *\param affected_operation - reference to the operation for which a failure message was sent
 *\param sender - sender who sent the failure message to this server
 *
 *\return 0 if the handling of the failure message was successful, an error code otherwise
 *
 * If it is a NotResponsible message, it will be checked if the sender was the correct receiver of the last message. If this is the case,
 * it will be checked if the operation may be already finished on the other side. Then the corresponding event will be set. If the other
 * side did not get a operation request message, the sender would be treated as if it aborted. If the sender is not the correct participant,
 * the participant list is updated.
 * If a EventReRequest message is received, the corresponding event will be reasoned from the status of the operation and the event will be
 * sent to the sender of this request.
 * For a ContentRequest the operation data and the status is sent to the sender of the request. The same holds for a StatusRequest but only
 * the status is sent.
 * If a ContentResponse or StatusResponse is received, the operation will be recovered completely and added to the temporary datastructure.
 */
int DistributedAtomicOperations::handle_failure_messages(void* message_content, ExchangeMessages* event_request, uint64_t ID, DAOperation* affected_operation, string sender)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    if (affected_operation == NULL && *event_request != ContentResponse && *event_request != StatusResponse)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return DAONoFailureTreatmentPossible;
    }
    //  get corresponding DAOperation
    DAOperation* operation = affected_operation;

    switch (*event_request)
    {
    case NotResponsible:
    {
        InodeNumber subtree_entry = INVALID_INODE_ID;
        bool correct_participant = true;
        // get subtree entry of sender
        for (int i=0;i <affected_operation->participants.size(); i++)
        {
            if ( affected_operation->participants.at(i).server.size() == sender.size() &&
                    strcmp(affected_operation->participants.at(i).server.c_str(),sender.c_str())== 0)
            {
                subtree_entry = affected_operation->participants.at(i).subtree_entry;
                break;
            }
        }
        if (subtree_entry == INVALID_INODE_ID)
        {
            correct_participant = false;
        }

        // check if really not responsible using MltHandler
        MltHandler& handler = MltHandler::get_instance();
        if (correct_participant)
        {

            try
            {
                correct_participant = handler.is_partition_root(subtree_entry);
            }
            catch (MltHandlerException e)
            {
                handle_internal_failure(DAOMltNotExisting);
                correct_participant = handler.is_partition_root(subtree_entry);
            }
        }

        if (correct_participant)
        {
            try
            {
                const char* mlt_address;
                int mlt_address_size;
                Server server_entry = handler.get_mds(subtree_entry);
                // build mlt server address
                if (VARIABLE_PORTS)
                {
                    stringstream my_port;
                    my_port<<":"<<server_entry.port;
                    string socket = server_entry.address;
                    socket.append(my_port.str());
                    mlt_address_size = socket.size();
                    mlt_address = socket.c_str();
                }
                else
                {
                    mlt_address_size = server_entry.address.size();
                    mlt_address = server_entry.address.c_str();
                }
                // check if address is not the same
                if (mlt_address_size != sender.size()
                        || memcmp(mlt_address, sender.c_str(), mlt_address_size) != 0)
                {
                    correct_participant = false;
                }

            }
            catch (MltHandlerException e)
            {
                correct_participant = false;
            }
        }

        int failure;
        // if not the responsible participant, update participant list
        if (!correct_participant)
        {
            if (affected_operation->type ==MoveSubtree)
            {
                failure = DaoHandler::set_sending_addresses(affected_operation);
            }
            else
            {
                failure = ChangeOwnershipDAOAdapter::set_sending_addresses(affected_operation);
            }
            if (failure)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAOSettingAddressesFailed;
            }
        }
        else
        {
            // check if it may be operation for which the operation result may be already committed, aborted
            if (affected_operation->status == TPCCoordinatorVResultSend || affected_operation->status == TPCWaitUndoAck ||
                    (affected_operation->status == TPCAborting && affected_operation->participants.size()>1) ||
                    affected_operation->status == MTPCPartVoteSendYes || affected_operation->status ==  MTPCPartVoteSendNo)
            {
                if (affected_operation->status == MTPCPartVoteSendYes || affected_operation->status ==  MTPCPartVoteSendNo)
                {
                    *event_request = MTPCAck;
                }
                else
                {
                    *event_request = TPCAck;
                }
            }
            else
            {
                int failure;
                // check if possible that never got the operation request
                // TPC protocol
                if (affected_operation->status == TPCCoordinatorVReqSend)
                {
                    // handle as if an abort vote sent
                    if (affected_operation->status!=TPCAborting && affected_operation->status != TPCWaitUndoAck && affected_operation->status !=TPCWaitUndoToFinish)
                    {
                        failure = write_update_log(ID, ((affected_operation->type) == MoveSubtree || (affected_operation->type) == ChangePartitionOwnership), affected_operation->subtree_entry, TPCIAborting);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("Logging global result in two phase commit protocol failed.");
#endif
                            if (failure == DAOSubtreeNotExisting)
                            {
                                vector<Operation> journal_ops;
                                InodeNumber correct_journal = (*journal_manager).get_open_operations(affected_operation->ID, journal_ops);
                                if (correct_journal!= INVALID_INODE_ID)
                                {
                                    // set correct journal identifier
                                    operation->subtree_entry = correct_journal;
                                    // reexecute logging
                                    failure = write_update_log(ID, ((affected_operation->type) == MoveSubtree || (affected_operation->type) == ChangePartitionOwnership), affected_operation->subtree_entry, TPCIAborting);
                                }
                                else
                                {
                                    InodeNumber current_subtree_entry = affected_operation->subtree_entry;
                                    if (affected_operation->type == MoveSubtree)
                                    {
                                        DaoHandler::set_subtree_entry_point(affected_operation);
                                    }
                                    else
                                    {
                                        ChangeOwnershipDAOAdapter::set_subtree_entry_point(affected_operation);
                                    }
                                    if (current_subtree_entry != affected_operation->subtree_entry && affected_operation->subtree_entry != INVALID_INODE_ID)
                                    {
                                        // reexecute logging
                                        failure = write_update_log(ID, ((affected_operation->type) == MoveSubtree || (affected_operation)->type == ChangePartitionOwnership), affected_operation->subtree_entry, TPCIAborting);
                                    }
                                }
                            }
                            // if still any failure stop execution of protocol step
                            if (failure)
                            {
                                // ends profiling
                                Pc2fsProfiler::get_instance()->function_end();

                                return DAONoFailureTreatmentPossible;
                            }
                        }

                        int num_removed=0;
                        // remove all previous received votes from respective datastructure
                        if (affected_operation->recieved_votes<affected_operation->participants.size())
                        {
                            for (int i=0;affected_operation->recieved_votes+num_removed<affected_operation->participants.size() && i<affected_operation->participants.size(); i++)
                            {
                                //create identification for possible received vote
                                stringstream vote_identification;
                                vote_identification<<ID<<'.'<<affected_operation->participants.at(i).server;
                                string vote_ID = vote_identification.str();
                                if (TPC_received_request_answers.erase(vote_ID))
                                {
                                    num_removed++;
                                }
                            }
                        }
                        if (affected_operation->recieved_votes+num_removed!=affected_operation->participants.size())
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("More or less vote identification than received are removed from the datastructure.");
#endif
                            remove_all_entries_for_operation(ID);
                        }

                        // set recieved_votes to #particpants to count acknowledges
                        affected_operation->recieved_votes = affected_operation->participants.size();

                        // send abort message
                        // create server list
                        vector<string> server_list;
                        for (int i=0;i<affected_operation->participants.size();i++)
                        {
                            server_list.push_back(affected_operation->participants.at(i).server);
                        }
                        // send message to external modules
                        failure = send_simple_message(operation->ID,TPCAbort, server_list);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("Sending abort message failed at least partly.");
#endif
                            // check if repairable sending failure
                            if (failure == SocketNotExisting || failure == SendingFailed)
                            {
                                // try to resend, here a failure is ignored and must be processed by time out recovery
                                send_simple_message(affected_operation->ID,TPCAbort, server_list);
                            }
                        }
                        // reset counter for participant answers
                        affected_operation->recieved_votes = affected_operation->participants.size();

                        // check if operation need to be undone
                        if (affected_operation->type == MoveSubtree || affected_operation->type == ChangePartitionOwnership)
                        {
                            forward_any_execution_request(affected_operation, DAOUndoRequest);
                            affected_operation->status = TPCWaitUndoAck;
                            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCWaitUndoAck);
                        }
                        else
                        {
                            // client negative acknowledgement (NACK)
                            provide_result(affected_operation->ID, false, TwoPhaseCommit, affected_operation->type==MoveSubtree);
                            // set status to aborting
                            affected_operation->status=TPCAborting;
                            add_timeout(affected_operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCAborting);
                        }
                    }
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return 0;
                }
                // MTPC protocol
                if (affected_operation->status == MTPCCoordinatorReqSend)
                {
                    // react as if an abort was sent
                    // check if it is an operation which need to be undone first
                    if (affected_operation->type == MoveSubtree || affected_operation->type == ChangePartitionOwnership)
                    {
                        forward_any_execution_request(affected_operation, DAOUndoRequest);
                        affected_operation->status=MTPCIWaitResultUndone;
                        add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCIWaitResultUndone);
                    }
                    else
                    {
                        failure = write_finish_log(affected_operation->ID, ((affected_operation->type)==MoveSubtree || (affected_operation->type) == ChangePartitionOwnership),affected_operation->subtree_entry, false);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("Abort log failed.");
#endif
                            if (failure == DAOSubtreeNotExisting)
                            {
                                vector<Operation> journal_ops;
                                InodeNumber correct_journal = (*journal_manager).get_open_operations(affected_operation->ID, journal_ops);
                                if (correct_journal!= INVALID_INODE_ID)
                                {
                                    // set correct journal identifier
                                    operation->subtree_entry = correct_journal;
                                    // reexecute logging
                                    failure = write_finish_log(ID, ((affected_operation->type) == MoveSubtree || (affected_operation->type) == ChangePartitionOwnership), affected_operation->subtree_entry, false);
                                }
                                else
                                {
                                    InodeNumber current_subtree_entry = affected_operation->subtree_entry;
                                    if (affected_operation->type == MoveSubtree)
                                    {
                                        DaoHandler::set_subtree_entry_point(affected_operation);
                                    }
                                    else
                                    {
                                        ChangeOwnershipDAOAdapter::set_subtree_entry_point(affected_operation);
                                    }
                                    if (current_subtree_entry != affected_operation->subtree_entry && affected_operation->subtree_entry != INVALID_INODE_ID)
                                    {
                                        // reexecute logging
                                        failure = write_finish_log(ID, ((affected_operation->type) == MoveSubtree || (affected_operation->type) == ChangePartitionOwnership), affected_operation->subtree_entry, false);
                                    }
                                }
                            }
                            // if still any failure stop execution of protocol step
                            if (failure)
                            {
                                // ends profiling
                                Pc2fsProfiler::get_instance()->function_end();

                                return DAONoFailureTreatmentPossible;
                            }
                        }
                        // client negative acknowledgement (NACK)
                        provide_result(affected_operation->ID, false, ModifiedTwoPhaseCommit, affected_operation->type==MoveSubtree);

                        //delete from temporary datastructure
                        free(affected_operation->operation);
                        if (open_operations.erase(affected_operation->ID)==0)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
                        }
                    }
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return 0;
                }

                //OOE protocol
                if (affected_operation->status == OOEWaitResult)
                {
                    // react as if an abort was sent
                    // check if it is an operation which need to be undone first
                    if (affected_operation->type == MoveSubtree || affected_operation->type == ChangePartitionOwnership || affected_operation->type==OOE_LBTest)
                    {
                        // write operation undone requested into log
                        failure = write_update_log(affected_operation->ID, ((affected_operation->type) == MoveSubtree || (affected_operation->type) == ChangePartitionOwnership),affected_operation->subtree_entry, OOEUndo);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("Logging of undo operation in ordered operation execution protocol failed.");
#endif
                            if (failure == DAOSubtreeNotExisting)
                            {
                                vector<Operation> journal_ops;
                                InodeNumber correct_journal = (*journal_manager).get_open_operations(affected_operation->ID, journal_ops);
                                if (correct_journal!= INVALID_INODE_ID)
                                {
                                    // set correct journal identifier
                                    operation->subtree_entry = correct_journal;
                                    // reexecute logging
                                    failure = write_update_log(ID, ((affected_operation->type) == MoveSubtree || (affected_operation->type) == ChangePartitionOwnership), affected_operation->subtree_entry, OOEUndo);
                                }
                                else
                                {
                                    InodeNumber current_subtree_entry = affected_operation->subtree_entry;
                                    if (affected_operation->type == MoveSubtree)
                                    {
                                        DaoHandler::set_subtree_entry_point(affected_operation);
                                    }
                                    else
                                    {
                                        ChangeOwnershipDAOAdapter::set_subtree_entry_point(affected_operation);
                                    }
                                    if (current_subtree_entry != affected_operation->subtree_entry && affected_operation->subtree_entry != INVALID_INODE_ID)
                                    {
                                        // reexecute logging
                                        failure = write_update_log(ID, ((affected_operation->type) == MoveSubtree || (affected_operation->type) == ChangePartitionOwnership), affected_operation->subtree_entry, OOEUndo);
                                    }
                                }
                            }
                            // if still any failure stop execution of protocol step
                            if (failure)
                            {
                                // ends profiling
                                Pc2fsProfiler::get_instance()->function_end();

                                return DAONoFailureTreatmentPossible;
                            }
                        }
                        forward_any_execution_request(affected_operation, DAOUndoRequest);
                        affected_operation->status=OOEWaitResultUndone;
                        add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEWaitResultUndone);
                    }
                    else
                    {
                        // write abort of own part into log
                        failure = write_finish_log(affected_operation->ID, ((affected_operation->type)==MoveSubtree || (affected_operation->type) == ChangePartitionOwnership),affected_operation->subtree_entry, false);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("Abort log failed.");
#endif
                            if (failure == DAOSubtreeNotExisting)
                            {
                                vector<Operation> journal_ops;
                                InodeNumber correct_journal = (*journal_manager).get_open_operations(operation->ID, journal_ops);
                                if (correct_journal!= INVALID_INODE_ID)
                                {
                                    // set correct journal identifier
                                    operation->subtree_entry = correct_journal;
                                    // reexecute logging
                                    failure = write_finish_log(ID, ((affected_operation->type) == MoveSubtree || (affected_operation->type) == ChangePartitionOwnership), affected_operation->subtree_entry, false);
                                }
                                else
                                {
                                    InodeNumber current_subtree_entry = affected_operation->subtree_entry;
                                    if (affected_operation->type == MoveSubtree)
                                    {
                                        DaoHandler::set_subtree_entry_point(affected_operation);
                                    }
                                    else
                                    {
                                        ChangeOwnershipDAOAdapter::set_subtree_entry_point(affected_operation);
                                    }
                                    if (current_subtree_entry != affected_operation->subtree_entry && affected_operation->subtree_entry != INVALID_INODE_ID)
                                    {
                                        // reexecute logging
                                        failure = write_finish_log(ID, ((affected_operation->type) == MoveSubtree || (affected_operation->type) == ChangePartitionOwnership), affected_operation->subtree_entry, false);
                                    }
                                }
                            }
                            // if still any failure stop execution of protocol step
                            if (failure)
                            {
                                // ends profiling
                                Pc2fsProfiler::get_instance()->function_end();

                                return DAONoFailureTreatmentPossible;
                            }
                        }

                        if (affected_operation->participants.size()==1)
                        {
                            // client negative acknowledgement (NACK)
                            provide_result(affected_operation->ID, false, OrderedOperationExecution, affected_operation->type==MoveSubtree||affected_operation->type == OOE_LBTest);
                        }
                        else
                        {
                            // set up message for negative acknowledgement (abort)
                            // set coordinator to server_list
                            vector<string> server_list;
                            server_list.push_back(affected_operation->participants.at(0).server);

                            // send negative acknowledgement
                            failure =  send_simple_message(affected_operation->ID,OOEAborted, server_list);
                            if (failure)
                            {
#ifdef LOGGING_ENABLED
                                dao_logger.error_log("Cannot send abort message to previous executor in ordered operation execution protocol.");
#endif
                            }
                        }

                        //delete operation from temporary data structure
                        free(affected_operation->operation);
                        if (open_operations.erase(affected_operation->ID)==0)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
                        }
                    }
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return 0;
                }

                return DAONoFailureTreatmentPossible;
            }
        }

        break;
    }
    case EventReRequest:
    {
        vector<string> receiver;
        receiver.push_back(sender);
        // resend intermediate requests only, never get an event rerequest for begin operations, cannot handle rerequests for closed operations
        if ( affected_operation->status == TPCCoordinatorVReqSend)
        {
            send_simple_message(affected_operation->ID, TPCRVoteReq, receiver);
        }
        if (affected_operation->status == TPCCoordinatorVResultSend)
        {
            send_simple_message(affected_operation->ID, MTPCRCommit, receiver);
        }
        if (affected_operation->status == TPCWaitUndoAck || (TPCAborting && affected_operation->participants.size()>1))
        {
            send_simple_message(affected_operation->ID, TPCPRAbort, receiver);
        }
        if (affected_operation->status == TPCPartWaitVResultExpectYes)
        {
            send_simple_message(affected_operation->ID, TPCRVoteY, receiver);
        }
        if (affected_operation->status == TPCPartWaitVResultExpectNo || (TPCAborting && affected_operation->participants.size()==1))
        {
            send_simple_message(affected_operation->ID, TPCRVoteN, receiver);
        }
        if (affected_operation-> status == MTPCPartVoteSendYes)
        {
            send_simple_message(affected_operation->ID, MTPCRCommit, receiver);
        }
        if (affected_operation->status == MTPCPartVoteSendNo)
        {
            send_simple_message(affected_operation->ID, MTPCRAbort, receiver);
        }
        break;
    }
    case ContentRequest:
    {
        int message_length = sizeof(char)+sizeof(uint64_t)+sizeof(char)+sizeof(uint32_t)+affected_operation->operation_length;
        void* message_data = malloc(message_length);
        if (message_data == NULL)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Cannot get enough memory to create simple message.");
#endif
            handle_internal_failure(DAOInsufficientMemory);
        }
        // set up particpant independent message data
        char messagetype = ContentResponse;
        //set event type
        memcpy(message_data, &messagetype, sizeof(char));
        // set operation ID
        memcpy(message_data+sizeof(char), &(affected_operation->ID), sizeof(uint64_t));

        // set operation type
        *(((char*)message_data)+sizeof(char)+sizeof(uint64_t)) = affected_operation->type;
        // set operation description
        memcpy(message_data+2*sizeof(char)+sizeof(uint64_t), &(affected_operation->operation_length), sizeof(uint32_t));
        memcpy(message_data+2*sizeof(char)+sizeof(uint32_t)+sizeof(uint64_t),affected_operation->operation, affected_operation->operation_length);
        // add protocol status
        char participant_status = (char) (get_protocol_status_of_opposite_side(affected_operation));
        memcpy(message_data+2*sizeof(char)+sizeof(uint32_t)+sizeof(uint64_t)+affected_operation->operation_length, &participant_status , sizeof(char));

        vector<string> server_list;
        server_list.push_back(sender);

        Pc2fsProfiler::get_instance()->function_sleep();
        //send message
        int failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
        Pc2fsProfiler::get_instance()->function_wakeup();
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Sending two phase commit execution request failed at least for one server");
#endif
            if (failure==MutexFailed)
            {
                handle_internal_failure(DAOMutexFailure);
            }
            else
            {
                if (failure == UnableToCreateMessage || failure == NotEnoughMemory)
                {
                    handle_internal_failure(DAOInsufficientMemory);
                }
                else
                {
                    if (failure == SendingFailed)
                    {
                        if (errno>=0 && errno<server_list.size())
                        {
                            handle_supposed_external_server_failure(server_list.at(errno));
                        }
                    }
                    else
                    {
                        if (errno>=0 && errno<server_list.size())
                        {
                            int creation_failure = handle_socket_not_existing(server_list.at(errno));
                            if (creation_failure == 0)
                            {
                                Pc2fsProfiler::get_instance()->function_sleep();
                                failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
                                Pc2fsProfiler::get_instance()->function_wakeup();
                            }
                        }
                    }
                }
            }
        }
        free(message_data);
        break;
    }
    case ContentResponse:
    {
        if (affected_operation == NULL && open_operations.find(ID) == open_operations.end())
        {
            DAOperation new_operation;
            // set ID
            new_operation.ID = ID;
            // set subtree entry point
            vector<Operation> found_operations;
            new_operation.subtree_entry = (*journal_manager).get_open_operations(ID, found_operations);
            if (new_operation.subtree_entry == INVALID_INODE_ID)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                // operation not found
                return DAONoFailureTreatmentPossible;
            }
            // set operation type
            new_operation.type = (OperationTypes) *(((char*)message_content)+sizeof(char)+sizeof(uint64_t));
            // set operation data
            memcpy(&(new_operation.operation_length), message_content+2*sizeof(char)+sizeof(uint64_t),sizeof(uint32_t));
            new_operation.operation = malloc(new_operation.operation_length);
            if (new_operation.operation == NULL)
            {
                handle_internal_failure(DAOInsufficientMemory);
            }
            memcpy(new_operation.operation, message_content+2*sizeof(char)+sizeof(uint64_t)+sizeof(uint32_t), new_operation.operation_length);

            // ask the respective module for the addresses
            if (new_operation.type == MoveSubtree)
            {
                int failure = DaoHandler::set_sending_addresses(&new_operation);
                if (failure)
                {
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    // operation not found
                    return DAOSettingAddressesFailed;
                }
            }
            else
            {
                int failure = ChangeOwnershipDAOAdapter::set_sending_addresses(&new_operation);
                if (failure)
                {
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    // operation not found
                    return DAOSettingAddressesFailed;
                }
            }

            // set protocol
            if (new_operation.type<=SetAttr)
            {
                if (new_operation.participants.size()==1)
                {
                    new_operation.used_protocol = ModifiedTwoPhaseCommit;
                    new_operation.status = MTPCCoordinatorComp;
                    add_timeout(ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCCoordinatorComp);
                }
                else
                {
                    new_operation.used_protocol = TwoPhaseCommit;
                    new_operation.status = TPCCoordinatorComp;
                    // set recieved_votes
                    new_operation.recieved_votes = new_operation.participants.size();
                    add_timeout(ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCCoordinatorComp);
                }
            }
            else
            {
                if (new_operation.type == OrderedOperationTest || new_operation.type == OOE_LBTest)
                {
                    new_operation.used_protocol = OrderedOperationExecution;
                    new_operation.status = OOEComp;
                    add_timeout(ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEComp);
                }
            }

            // set overall timeout
            new_operation.overall_timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::milliseconds(OVERALL_TIMEOUT);

            // set status
            memcpy(&(new_operation.status),message_content+2*sizeof(char)+sizeof(uint64_t)+sizeof(uint32_t)+new_operation.operation_length,sizeof(char));
            // put into temporal datastructure
            open_operations.insert(unordered_map<uint64_t,DAOperation>::value_type(new_operation.ID,new_operation));
        }
        break;
    }
    case StatusRequest:
    {
        int message_length = sizeof(char)+sizeof(uint64_t)+sizeof(char);
        void* message_data = malloc(message_length);
        if (message_data == NULL)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Cannot get enough memory to create simple message.");
#endif
            handle_internal_failure(DAOInsufficientMemory);
        }
        // set up particpant independent message data
        char messagetype = StatusResponse;
        //set event type
        memcpy(message_data, &messagetype, sizeof(char));
        // set operation ID
        memcpy(message_data+sizeof(char), &(affected_operation->ID), sizeof(uint64_t));

        // add protocol status
        char participant_status = (char) (get_protocol_status_of_opposite_side(affected_operation));
        memcpy(message_data+sizeof(char)+sizeof(uint64_t),&participant_status , sizeof(char));

        vector<string> server_list;
        server_list.push_back(sender);

        Pc2fsProfiler::get_instance()->function_sleep();
        //send message
        int failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
        Pc2fsProfiler::get_instance()->function_wakeup();
        if (failure)
        {
#ifdef LOGGING_ENABLED
            dao_logger.error_log("Sending two phase commit execution request failed at least for one server");
#endif
            if (failure==MutexFailed)
            {
                handle_internal_failure(DAOMutexFailure);
            }
            else
            {
                if (failure == UnableToCreateMessage || failure == NotEnoughMemory)
                {
                    handle_internal_failure(DAOInsufficientMemory);
                }
                else
                {
                    if (failure == SendingFailed)
                    {
                        if (errno>=0 && errno<server_list.size())
                        {
                            handle_supposed_external_server_failure(server_list.at(errno));
                        }
                    }
                    else
                    {
                        if (errno>=0 && errno<server_list.size())
                        {
                            int creation_failure = handle_socket_not_existing(server_list.at(errno));
                            if (creation_failure == 0)
                            {
                                Pc2fsProfiler::get_instance()->function_sleep();
                                failure = send( message_data,  message_length, COM_DistributedAtomicOp, server_list);
                                Pc2fsProfiler::get_instance()->function_wakeup();
                            }
                        }
                    }
                }
            }
        }
        free(message_data);
        break;
    }
    case StatusResponse:
    {
        if (affected_operation == NULL && open_operations.find(ID)==open_operations.end())
        {

            // try to retrieve operation
            DAOperation new_operation;
            // set id
            new_operation.ID =ID;
            // vector which will contain all parts from the operation if can be found in a journal of this metadata server
            vector<Operation> operation_logs;
            // get operation from journal
            new_operation.subtree_entry = (*journal_manager).get_open_operations(ID, operation_logs);
            if (new_operation.subtree_entry == INVALID_INODE_ID)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                // operation not found
                return DAONoFailureTreatmentPossible;
            }
            OperationData begin_op;
            // get the begin log information
            for (int i=0;i<operation_logs.size();)
            {
                begin_op = operation_logs.at(i).get_operation_data();
                // check if it is begin log
                if (begin_op.status == DistributedStart && begin_op.module == DistributedAtomicOp && begin_op.operation_type == DistributedOp)
                {
                    break;
                }
                i++;
                if (i==operation_logs.size())
                {
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return DAONoBeginLog;
                }
            }

            // set operation type
            new_operation.type = (OperationTypes) begin_op.data[0];

            // set the operation content (length and data)
            memcpy(&(new_operation.operation_length), begin_op.data+sizeof(char), sizeof(uint32_t));;
            new_operation.operation = malloc(operation->operation_length);
            if (new_operation.operation == NULL)
            {
                handle_internal_failure(DAOInsufficientMemory);
            }
            memcpy(new_operation.operation, begin_op.data+sizeof(char)+sizeof(uint32_t),new_operation.operation_length);

            // ask the respective module for the addresses
            if (new_operation.type == MoveSubtree)
            {
                int failure = DaoHandler::set_sending_addresses(&new_operation);
                if (failure)
                {
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    // operation not found
                    return DAOSettingAddressesFailed;
                }
            }
            else
            {
                int failure = ChangeOwnershipDAOAdapter::set_sending_addresses(&new_operation);
                if (failure)
                {
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    // operation not found
                    return DAOSettingAddressesFailed;
                }
            }

            // set protocol
            if (new_operation.type<=SetAttr)
            {
                if (new_operation.participants.size()==1)
                {
                    new_operation.used_protocol = ModifiedTwoPhaseCommit;
                    new_operation.status = MTPCCoordinatorComp;
                    add_timeout(ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCCoordinatorComp);
                }
                else
                {
                    new_operation.used_protocol = TwoPhaseCommit;
                    new_operation.status = TPCCoordinatorComp;
                    // set recieved_votes
                    new_operation.recieved_votes = new_operation.participants.size();
                    add_timeout(ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCCoordinatorComp);
                }
            }
            else
            {
                if (new_operation.type == OrderedOperationTest || new_operation.type == OOE_LBTest)
                {
                    new_operation.used_protocol = OrderedOperationExecution;
                    new_operation.status = OOEComp;
                    add_timeout(ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEComp);
                }
            }

            // set overall timeout
            new_operation.overall_timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::milliseconds(OVERALL_TIMEOUT);

            // set status
            memcpy(&(new_operation.status),message_content+sizeof(char)+sizeof(uint64_t),sizeof(char));
            // put into temporal datastructure
            open_operations.insert(unordered_map<uint64_t,DAOperation>::value_type(new_operation.ID,new_operation));
        }
        break;
    }
    default:
    {
        // nothing is done, no known failure message
    }
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 *\brief Reacts to the assumption that the external server with address <code>server_address</code> has failed.
 *
 *\param server_address - server address of the server which is assumed to be failed
 *
 * Since no global error handling and no global voting for failed servers is implemented, nothing is done so far.
 */
void DistributedAtomicOperations::handle_supposed_external_server_failure(string server_address)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    // currently nothing done since no global error handling existing, no global voting for external server failure

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
 *\brief Checks if there exist at least one open operation for the subtree <code>subtree</code>
 *
 *\param subtree - unique identifier for the subtree
 *
 *\return true if an open operation is stored in memory and thus not finished false otherwise
 *
 * Checks if there exists a distributed atomic operation which is not finished completely and affects the specified subtree. Notice that
 * if a distributed atomic operation is not stored in main memory because it is lost or has not been stored yet, it will not be considered.
 * This is why this method possibly returns a wrong result.
 */
bool DistributedAtomicOperations::open_operation_exists(InodeNumber subtree)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    unordered_map<uint64_t,DAOperation>::iterator it = open_operations.begin();
    while (it!=open_operations.end())
    {
        // check if this operation belongs to the specified subtree
        if (((*it).second).subtree_entry == subtree)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return true;
        }
        it++;
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return false;
}


/**
 *\brief Reexceutes the protocol step after necessary repair if possible or requests the event again if it is not correct
 *
 *\param unsuccessful_event - specifies the protocol step which was unsuccessful
 *\param failure - kind of failure which caused the unsuccessful protocol step execution
 *\param operation - distributed atomic operation for which the protocol step cannot be executed successfully
 *\param remove_operation - true if the operation is removed from the datastructure in a successful execution of the protocol, otherwise false
 *\param status_set_in_event - the status the distributed atomic operation would be set to in a successful execution of the protocol step ,only needed if remove_operation = false
 *\param sender - only needed if the protocol step was caused by a message reception, then it contains the address of the sender of the message
 *
 * If it is the wrong event, the correct event will be requested from the sender. If logging failed due to a wrong subtree_entry point, it
 * will be tried to get the correct entry point and reexecute the protocol step. If it is a repairable sending failure, the protocol will
 * be reexecuted. If reexecution is unsuccesful but logging was done for the step, the status is set as in the protocol step and the
 * operation is removed from the temporal datastructure, respectively.
 */
void DistributedAtomicOperations::handle_unsuccessful_protocol_step(Events unsuccessful_event, int failure, DAOperation* operation, bool remove_operation, Status status_set_in_event, string sender)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int unsuccessful;
    // unknown event received
    if (failure == DAOWrongEvent)
    {
        // build receivers list
        vector<string> receivers;
        receivers.push_back(sender);
        // send event rerequest
        send_simple_message(operation->ID, EventReRequest, receivers);
    }
    // journal failure
    if (failure == DAONoSal || failure == DAONoMlt || failure ==  DAOSubtreeNotExisting || failure == DAOSelfWrongServer)
    {
        if (failure == DAOSubtreeNotExisting)
        {
            vector<Operation> journal_ops;
            InodeNumber correct_journal = (*journal_manager).get_open_operations(operation->ID, journal_ops);
            if (correct_journal!= INVALID_INODE_ID)
            {
                // set correct journal identifier
                operation->subtree_entry = correct_journal;
                // reexecute protocol step
                unsuccessful = handle_event(unsuccessful_event, operation);
                // only if no journal failure, recursive call
                if (failure && failure != DAONoSal && failure != DAONoMlt && failure !=  DAOSubtreeNotExisting && failure != DAOSelfWrongServer)
                {
                    handle_unsuccessful_protocol_step(unsuccessful_event, unsuccessful, operation, remove_operation, status_set_in_event, sender);
                }
            }
            else
            {
                InodeNumber current_subtree_entry = operation->subtree_entry;
                if (operation->type == MoveSubtree)
                {
                    DaoHandler::set_subtree_entry_point(operation);
                }
                else
                {
                    ChangeOwnershipDAOAdapter::set_subtree_entry_point(operation);
                }
                if (current_subtree_entry != operation->subtree_entry && operation->subtree_entry != INVALID_INODE_ID)
                {
                    // reexecute protocol step
                    unsuccessful = handle_event(unsuccessful_event, operation);
                    // only if no journal failure, recursive call
                    if (failure && failure != DAONoSal && failure != DAONoMlt && failure !=  DAOSubtreeNotExisting && failure != DAOSelfWrongServer)
                    {
                        handle_unsuccessful_protocol_step(unsuccessful_event, unsuccessful, operation, remove_operation, status_set_in_event, sender);
                    }
                }
            }
        }
        if (failure == DAOSelfWrongServer)
        {
            vector<string> server_list;
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }
            // send not responsible message either to coordinator, previous executor or participants/following executor
            send_simple_message(operation->ID, NotResponsible, server_list);
        }
    }
    // sending failure
    if (failure == DAOWrongParameter || failure == MutexFailed || failure == NotEnoughMemory || failure == SocketNotExisting || failure == UnableToCreateMessage || failure == SendingFailed)
    {
        // check if repairable sending failure
        if (failure == SocketNotExisting || failure == SendingFailed)
        {
            // logging at least previously successful
            unsuccessful = handle_event(unsuccessful_event, operation);
            if (unsuccessful)
            {
                //sending again unsuccessful --> set status as in event/remove and wait for dao recovery to send again
                if (remove_operation)
                {
                    // remove operation from temporary datastructure
                    free(operation->operation);
                    if (open_operations.erase(operation->ID)==0)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
                    }
                }
                else
                {
                    // set status as done in correct handle_event execution
                    operation->status = status_set_in_event;
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, status_set_in_event);
                }
            }
        }
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
 *\brief Checks if an external request received in message is allowed
 *
 *\param event_request - received request type which causes a protocol operation step
 *\param sender - address of sender who sent request message
 *\param operation - reference to distributed atomic operation the request was sent for. If operation cannot be found in temporal datastructure and cannot be recoverd from journal, NULL will be used.
 *
 *\return 0 if the request is allowed, otherwise a failure code
 *
 * Checks if the message is allowed for the distributed atomic operation. This means this is the correct server for handling the
 * distributed atomic operation. Furthermore the request must fit to the protocol type and the status of the distributed atomic operations.
 * Notice: Operation requests are always allowed.
 */
int DistributedAtomicOperations::check_allowed_request(ExchangeMessages event_request, string asyn_tcp_server, DAOperation* operation)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    // failure messages always allowed
    if (event_request == NotResponsible || event_request ==  EventReRequest || event_request == ContentRequest ||
            event_request ==  StatusRequest || event_request == ContentResponse || event_request == StatusResponse)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return 0;
    }
    // operation already deleted from journal
    if (operation == NULL)
    {
        // check for events which will only occur if operation in memory
        if (event_request == OOEAck || event_request ==  OOEAborted || event_request == MTPCAck || event_request == TPCVoteReq ||
                event_request == TPCVoteY|| event_request == TPCVoteN|| event_request == TPCAck || event_request == TPCPRAbort ||
                event_request == TPCRVoteN || event_request == TPCRVoteY || event_request == TPCRVoteReq || event_request == TPCRCommit ||
                event_request == MTPCRStatusReq || event_request == MTPCRAbort || event_request == MTPCRCommit || event_request == OOERAborted
                || event_request == OOERStatusReq || event_request == ReqOpReq)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return DAOSelfWrongServer;
        }
    }
    else
    {
        bool correct_server = false;
        bool first_mlt_not_exist = true;
        bool wrong_subtree_entry = false;
        vector<InodeNumber> subtree_entries;
        // check if sender is known
        for (int i=0;i<operation->participants.size();i++)
        {
            // check if same server address
            if ((operation->participants).at(i).server.size() == asyn_tcp_server.size()
                    && memcmp((operation->participants).at(i).server.c_str(), asyn_tcp_server.c_str(), asyn_tcp_server.size()) == 0)
            {
                correct_server = true;
                break;
            }
            subtree_entries.push_back((operation->participants).at(i).subtree_entry);
        }
        // check if new server responsible when server unknown
        if (!correct_server)
        {
            MltHandler& handler = MltHandler::get_instance();
            Server current_server;
            const char* mlt_address;
            int mlt_address_size;
            for (int i=0;i<subtree_entries.size();i++)
            {
                try
                {
                    // get server address for respective subtree entry
                    current_server = handler.get_mds(subtree_entries.at(i));
                    // build mlt server address
                    if (VARIABLE_PORTS)
                    {
                        stringstream my_port;
                        my_port<<":"<<current_server.port;
                        string socket = current_server.address;
                        socket.append(my_port.str());
                        mlt_address_size = socket.size();
                        mlt_address = socket.c_str();
                    }
                    else
                    {
                        mlt_address_size = current_server.address.size();
                        mlt_address = current_server.address.c_str();
                    }
                    // check if address is the same
                    if (mlt_address_size == asyn_tcp_server.size()
                            && memcmp(mlt_address, asyn_tcp_server.c_str(), mlt_address_size) == 0)
                    {
                        correct_server = true;
                        // set server address to correct address
                        Subtree new_entry;
                        new_entry.subtree_entry = (operation->participants).at(i).subtree_entry;
                        new_entry.server = string(mlt_address);
                        (operation->participants).erase((operation->participants).begin()+i);
                        (operation->participants).insert((operation->participants).begin()+i, new_entry);
                        break;
                    }
                }
                catch (MltHandlerException e)
                {
                    try
                    {
                        // check if subtree entry is correct
                        if (!handler.is_partition_root(subtree_entries.at(i)))
                        {
                            wrong_subtree_entry = true;
                        }
                    }
                    catch (MltHandlerException e)
                    {
                        if (first_mlt_not_exist)
                        {
                            handle_internal_failure(DAOMltNotExisting);
                            i--;
                            first_mlt_not_exist = false;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        }
        // server unknown
        if (!correct_server)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return DAOUnknownAddress;
        }

        // check if event fits to protocol
        if (operation->used_protocol == OrderedOperationExecution && event_request < OOEOpReq && event_request > OOEAborted)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return DAOWrongEvent;
        }
        else
        {
            if (operation->used_protocol ==TwoPhaseCommit && event_request< TPCOpReq && event_request > TPCAck)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAOWrongEvent;
            }
            else
            {
                if (operation->used_protocol == ModifiedTwoPhaseCommit && event_request< MTPCOpReq && event_request > MTPCAck)
                {
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return DAOWrongEvent;
                }
            }
        }
        // check if event fits to course of protocol
        // operation requests always allowed
        if (event_request == MTPCOpReq || event_request == OOEOpReq || event_request == TPCOpReq)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return 0;
        }
        // check normal protocol messages
        switch (event_request)
        {
        case MTPCCommit:
        {
            if (operation->status != MTPCCoordinatorReqSend)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAODifferentStatus;
            }
            break;
        }
        case MTPCAbort:
        {
            if (operation->status != MTPCCoordinatorReqSend && operation->status != MTPCIWaitResultUndone)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAODifferentStatus;
            }
            break;
        }
        case MTPCAck:
        {
            if (operation->status != MTPCPartVoteSendYes && operation->status != MTPCPartVoteSendNo)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAODifferentStatus;
            }
            break;
        }
        case OOEAck:
        {
            if (operation->status != OOEWaitResult)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAODifferentStatus;
            }
            break;
        }
        case OOEAborted:
        {
            if (operation->status != OOEWaitResult && operation->status != OOEWaitResultUndone)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAODifferentStatus;
            }
            break;
        }
        case TPCVoteReq:
        {
            if (operation->status != TPCPartComp && operation->status != TPCPartWaitVReqYes && operation->status != TPCPartWaitVReqNo
                    && operation->status != TPCPartVReqRec)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAODifferentStatus;
            }
            break;
        }
        case TPCVoteY:
        {
            if (operation->status != TPCCoordinatorVReqSend && operation->status != TPCCoordinatorVResultSend &&
                    operation->status!=TPCAborting && operation->status != TPCWaitUndoAck && operation->status !=TPCWaitUndoToFinish)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAODifferentStatus;
            }
            break;
        }
        case TPCVoteN:
        {
            if (operation->status != TPCCoordinatorVReqSend && operation->status!=TPCAborting && operation->status != TPCWaitUndoAck
                    && operation->status !=TPCWaitUndoToFinish)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAODifferentStatus;
            }
            break;
        }
        case TPCCommit:
        {
            if (operation->status != TPCPartWaitVResultExpectYes)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAODifferentStatus;
            }
            break;
        }
        case TPCAbort:
        {
            if (operation->status != TPCPartComp && operation->status != TPCPartWaitVReqYes && operation->status !=  TPCPartWaitVReqNo &&
                    operation->status != TPCPartVReqRec && operation->status != TPCPartWaitVResultExpectYes &&
                    operation->status != TPCPartWaitVResultExpectNo && operation->status != TPCAborting &&
                    operation->status != TPCWaitUndoToFinish)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAODifferentStatus;
            }
            break;
        }
        case TPCAck:
        {
            if (operation->status != TPCCoordinatorVResultSend && operation->status != TPCWaitUndoAck && operation->status != TPCAborting
                    && operation->status !=TPCWaitUndoToFinish)
            {
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return DAODifferentStatus;
            }
            break;
        }
        default:
        {
            // no normal protocol step
        }
        }
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 *\brief Recovers all open operations
 *
 *\return 0 if recovery successful, otherwise the error code DAONotAllOperationsRecoverable
 *
 * Recovers all distributed atomic operations which are not finished and for which this metadata server is responsible. This method must
 * be executed at every start of the metadata server before any distributed atomic operation can be started or incoming events can be
 * processed by this module.
 *
 * Notice: Before this method is called, the journals of this metadata server must be created (available) and journal recovery must be
 * done.
 */
int DistributedAtomicOperations::do_recovery()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    bool recovery_successful = true;
    if (!instance.recovery_done)
    {
        // get identifiers for all subtree journals
        set<InodeNumber> journal_ids;
        (instance.journal_manager)->get_journals(journal_ids);

        // per subtree journal recover open operations
        set<uint64_t>::iterator operation;
        set<InodeNumber>::iterator journal = journal_ids.begin();
        InodeNumber current_journal_id;
        Journal* current_journal;
        while (journal != journal_ids.end())
        {
            // get identifier of next journal
            current_journal_id = *journal;
            // get next journal
            current_journal = (instance.journal_manager)->get_journal(current_journal_id);
            if (current_journal == NULL)
            {
#ifdef LOGGING_ENABLED
                instance.dao_logger.error_log("Journal is not available. Try to repair.");
#endif
                failure = instance.handle_journal_not_existing(false, current_journal_id, true);
                if (failure)
                {
                    journal++;
                    continue;
                }
                else
                {
                    //successfully repaired
                    continue;
                }
            }

            // get all ids for open distributed atomic operations for the current journal
            set<uint64_t> op_ids;
            current_journal->get_open_operations(op_ids);
            operation = op_ids.begin();
            while (operation != op_ids.end())
            {
                // set operation ID
                DAOperation new_operation;
                new_operation.ID = *operation;
                // set subtree entry point
                new_operation.subtree_entry = current_journal_id;
                // get logs for this operation
                vector<Operation> log_entries;
                failure = current_journal->get_all_operations(new_operation.ID, log_entries);
                if (failure == 0)  // no operation found
                {
#ifdef LOGGING_ENABLED
                    stringstream warning_message;
                    warning_message<<"No log entry saved for atomic operation."<<new_operation.ID;
                    instance.dao_logger.warning_log(warning_message.str().c_str());
#endif
                    operation++;
                    continue;
                }
                // recover operation data
                failure = instance.recover_missing_operation_data(&new_operation, log_entries);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    instance.dao_logger.error_log("Unknown log content.");
#endif
                    recovery_successful = false;
                }

                // next operation
                operation++;
            }

            // next journal
            journal++;
        }

        // recover server journal
        bool server_journal = true;
        // get server journal
        current_journal =  (instance.journal_manager)->get_server_journal();
        if (current_journal == NULL)
        {
#ifdef LOGGING_ENABLED
            instance.dao_logger.error_log("Journal is not available. Try to repair.");
#endif
            failure = instance.handle_journal_not_existing(false, current_journal_id, true);
            if (failure)
            {
                server_journal = false;
                recovery_successful = false;
            }
            else
            {
                current_journal = (instance.journal_manager)->get_server_journal();
            }
        }
        if (server_journal && current_journal!= NULL)
        {
            // get all ids for open distributed atomic operations for the current journal
            set<uint64_t> op_ids;
            current_journal->get_open_operations(op_ids);
            operation = op_ids.begin();
            while (operation != op_ids.end())
            {
                // set operation ID
                DAOperation new_operation;
                new_operation.ID = *operation;
                // get logs for this operation
                vector<Operation> log_entries;
                failure = current_journal->get_all_operations(new_operation.ID, log_entries);
                if (failure == 0)  // no operation found
                {
#ifdef LOGGING_ENABLED
                    stringstream warning_message;
                    warning_message<<"No log entry saved for atomic operation."<<new_operation.ID;
                    instance.dao_logger.warning_log(warning_message.str().c_str());
#endif
                    operation++;
                    continue;
                }
                // recover operation data
                failure = instance.recover_missing_operation_data(&new_operation, log_entries);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    instance.dao_logger.error_log("Unknown log content.");
#endif
                    recovery_successful = false;
                }

                // next operation
                operation++;
            }
        }
        instance.recovery_done = true;
        if (!recovery_successful)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return DAONotAllOperationsRecoverable;
        }

        //set up for sending

        failure = instance.init(COM_DistributedAtomicOp);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            instance.dao_logger.error_log("Underlying CommunicationHandler could not be initialized.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();
            //failed start of communication handler
            throw runtime_error("Underlying CommunicationHandler could not be initialized.");
        }

        //set up for sending

        failure = instance.start();
        if (failure)
        {
#ifdef LOGGING_ENABLED
            instance.dao_logger.error_log("Underlying CommunicationHandler cannot be started.");
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();
            //failed start of communication handler
            throw runtime_error("Underlying CommunicationHandler cannot be started.");
        }
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
*\brief Method that observes the timeout queue. It has to be run in a thread.
*
*\param *ptr - Required by thread implementation. Not used in implementation, can be NULL
*
*\return endless loop, no return expected
*
* Runs in a seperate Thread. Checks if an atomic operation timed out. If an operation has a timeout calls handle_timeout, else sleeps for a certain
* timeperiode and checks for timeouts again.
*
*/
void *DistributedAtomicOperations::find_timeout(void *ptr)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    while (1)
    {
        // only if recovery done
        if (instance.recovery_done && !instance.timeout_queue.empty())
        {

            //check if next timeout in the queue needs to be handled
            while ((instance.timeout_queue.top().enter_time + boost::posix_time::milliseconds(instance.timeout_queue.top().rel_timeout)) <= boost::posix_time::second_clock::universal_time())
            {
                // mutex to avoid simultaneous changes of operation by timeout handling and event processing
                Pc2fsProfiler::get_instance()->function_sleep();
                //aquire mutex to avoid simultaneous changes on an open operation
                int failure = pthread_mutex_lock( &(instance.inc_event_mutex) );
                Pc2fsProfiler::get_instance()->function_wakeup();
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    stringstream error_message;
                    error_message<<"Mutex for internal event synchronization cannot be created due to "<<failure;
                    instance.dao_logger.error_log(error_message.str().c_str());
#endif
                    instance.handle_internal_failure(DAOMutexFailure);
                }
                //get the operation belonging to the timeout
                DAOperation *operation;
                unordered_map < uint64_t, DAOperation >::iterator it = instance.open_operations.find(instance.timeout_queue.top().ID);
                if (it != instance.open_operations.end())
                {
                    operation = &((*it).second);
                }
                else
                {
#ifdef LOGGING_ENABLED
                    instance.dao_logger.warning_log("The required operation cannot be found in the temporary datastructure.");
#endif
                    failure = instance.retrieve_lost_operation(instance.timeout_queue.top().ID);
                    if (failure)
                    {
                        instance.timeout_queue.pop();
                        failure = pthread_mutex_unlock(&(instance.inc_event_mutex));
#ifdef LOGGING_ENABLED
                        if (failure)
                        {
                            stringstream warning_message;
                            warning_message<<"Mutex cannot be unlocked due to "<<failure;
                            instance.dao_logger.warning_log(warning_message.str().c_str());
                        }
#endif
                        continue;
                    }
                    else
                    {
                        it = instance.open_operations.find(instance.timeout_queue.top().ID);
                        operation = &((*it).second);
                    }
                }
                //if the status has not changed call handle timeout, else ignore timeout
                if (operation->status == instance.timeout_queue.top().last_status)
                {
                    instance.handle_timeout(operation, instance.timeout_queue.top().enter_time, instance.timeout_queue.top().rel_timeout);
                    instance.timeout_queue.pop();
                }
                else
                {
                    instance.timeout_queue.pop();
                }
                failure = pthread_mutex_unlock(&(instance.inc_event_mutex));
#ifdef LOGGING_ENABLED
                if (failure)
                {
                    stringstream warning_message;
                    warning_message<<"Mutex cannot be unlocked due to "<<failure;
                    instance.dao_logger.warning_log(warning_message.str().c_str());
                }
#endif
            }
        }
        Pc2fsProfiler::get_instance()->function_sleep();
        sleep(DAO_MIN_SLEEP_TIME);
        Pc2fsProfiler::get_instance()->function_wakeup();
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
*\brief Method that handles the timeouts after it is detected by find_timeout.
*
*\param *operation - Operation that has timed out
*\param enter_time - Time when the operation has started
*\param rel_timeout - relative timeout for this operation
*
* Handels a timeout found. Actions taken depend on the protocol of the coresponding operation and its current status.
*
*/
void DistributedAtomicOperations::handle_timeout(DAOperation * operation, boost::posix_time::ptime enter_time, double rel_timeout)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    if (operation->used_protocol == TwoPhaseCommit)
    {
        switch (operation->status)
        {
        case TPCCoordinatorComp:
            //if overall timeout reached abort, else try to execute the own part of the operation again
        {
            if (operation->overall_timeout < boost::posix_time::second_clock::universal_time() + boost::posix_time::milliseconds(rel_timeout))
            {
                // only start abort process if the operation has not been aborted yet
                //send abort message if it is first no vote which is received
                if (operation->status != TPCAborting && operation->status != TPCWaitUndoAck && operation->status != TPCWaitUndoToFinish)
                {
                    failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("Logging global result in two phase commit protocol failed.");
#endif
                        if (failure == DAOSubtreeNotExisting)
                        {
                            vector<Operation> journal_ops;
                            InodeNumber correct_journal = (*journal_manager).get_open_operations(operation->ID, journal_ops);
                            if (correct_journal!= INVALID_INODE_ID)
                            {
                                // set correct journal identifier
                                operation->subtree_entry = correct_journal;
                                // reexecute logging
                                failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
                            }
                            else
                            {
                                InodeNumber current_subtree_entry = operation->subtree_entry;
                                if (operation->type == MoveSubtree)
                                {
                                    DaoHandler::set_subtree_entry_point(operation);
                                }
                                else
                                {
                                    ChangeOwnershipDAOAdapter::set_subtree_entry_point(operation);
                                }
                                if (current_subtree_entry != operation->subtree_entry && operation->subtree_entry != INVALID_INODE_ID)
                                {
                                    // reexecute logging
                                    failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
                                }
                            }
                        }
                        // if still any failure stop execution of protocol step
                        if (failure)
                        {
                            // ends profiling
                            Pc2fsProfiler::get_instance()->function_end();
                            return;
                        }
                    }

                    int num_removed = 0;
                    // remove all previous received votes from respective datastructure
                    if (operation->recieved_votes < operation->participants.size())
                    {
                        for (int i = 0; operation->recieved_votes + num_removed < operation->participants.size() && i < operation->participants.size(); i++)
                        {
                            //create identification for possible received vote
                            stringstream vote_identification;
                            vote_identification << operation->ID << '.' << operation->participants.at(i).server;
                            string vote_ID = vote_identification.str();
                            if (TPC_received_request_answers.erase(vote_ID))
                            {
                                num_removed++;
                            }
                        }
                    }
                    if (operation->recieved_votes + num_removed != operation->participants.size())
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("More or less vote identification than received are removed from the datastructure.");
#endif
                        remove_all_entries_for_operation(operation->ID);
                    }
                    // set recieved_votes to #particpants to count acknowledges
                    operation->recieved_votes = operation->participants.size();

                    // send abort message
                    // create server list
                    vector < string > server_list;
                    for (int i = 0; i < operation->participants.size(); i++)
                    {
                        server_list.push_back(operation->participants.at(i).server);
                    }
                    // send message to external modules
                    failure = send_simple_message(operation->ID, TPCPRAbort, server_list);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("Sending abort message failed at least partly.");
#endif
                    }
                    // reset counter for participant answers
                    operation->recieved_votes = operation->participants.size();

                    // check if operation need to be undone
                    if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
                    {
                        forward_any_execution_request(operation, DAOUndoRequest);
                        operation->status == TPCWaitUndoAck;
                        add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCWaitUndoAck);
                    }
                    else
                    {
                        // client negative acknowledgement (NACK)
                        provide_result(operation->ID, false, TwoPhaseCommit, operation->type == MoveSubtree);
                        // set status to aborting
                        operation->status = TPCAborting;
                        add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCAborting);
                    }
                }
                //put item in queue
            }
            else
            {
                //add an operationredo request
                forward_any_execution_request(operation, DAORedoRequest);
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), rel_timeout, operation->status);
            }
            break;
        }
        case TPCAborting:
        {
            if (operation->participants.size()>1)
            {
                int num_removed = 0;
                // remove all previous received votes from respective datastructure
                if (operation->recieved_votes < operation->participants.size())
                {
                    for (int i = 0; operation->recieved_votes + num_removed < operation->participants.size() && i < operation->participants.size(); i++)
                    {
                        //create identification for possible received vote
                        stringstream vote_identification;
                        vote_identification << operation->ID << '.' << operation->participants.at(i).server;
                        string vote_ID = vote_identification.str();
                        if (TPC_received_request_answers.erase(vote_ID))
                        {
                            num_removed++;
                        }
                    }
                }
                if (operation->recieved_votes + num_removed != operation->participants.size())
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("More or less vote identification than received are removed from the datastructure.");
#endif
                    remove_all_entries_for_operation(operation->ID);
                }
                // set recieved_votes to #particpants to count acknowledges
                operation->recieved_votes = operation->participants.size();

                // send abort message
                // create server list
                vector < string > server_list;
                for (int i = 0; i < operation->participants.size(); i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }
                // send message to external modules
                failure = send_simple_message(operation->ID, TPCPRAbort, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending abort message failed at least partly.");
#endif
                }
                // reset counter for participant answers
                operation->recieved_votes = operation->participants.size();

                // check if operation need to be undone
                if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
                {
                    forward_any_execution_request(operation, DAOReundoRequest);
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCWaitUndoAck);
                }
                else
                {
                    // client negative acknowledgement (NACK)
                    provide_result(operation->ID, false, TwoPhaseCommit, operation->type == MoveSubtree);
                    // set another timeout
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCAborting);
                }
            }
            break;
        }
        case TPCWaitUndoAck:
        {
            int num_removed = 0;
            // remove all previous received votes from respective datastructure
            if (operation->recieved_votes < operation->participants.size())
            {
                for (int i = 0; operation->recieved_votes + num_removed < operation->participants.size() && i < operation->participants.size(); i++)
                {
                    //create identification for possible received vote
                    stringstream vote_identification;
                    vote_identification << operation->ID << '.' << operation->participants.at(i).server;
                    string vote_ID = vote_identification.str();
                    if (TPC_received_request_answers.erase(vote_ID))
                    {
                        num_removed++;
                    }
                }
            }
            if (operation->recieved_votes + num_removed != operation->participants.size())
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("More or less vote identification than received are removed from the datastructure.");
#endif
                remove_all_entries_for_operation(operation->ID);
            }
            // set recieved_votes to #particpants to count acknowledges
            operation->recieved_votes = operation->participants.size();

            // send abort message
            // create server list
            vector < string > server_list;
            for (int i = 0; i < operation->participants.size(); i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }
            // send message to external modules
            failure = send_simple_message(operation->ID, TPCPRAbort, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending abort message failed at least partly.");
#endif
            }
            // reset counter for participant answers
            operation->recieved_votes = operation->participants.size();
            //resend the undo request
            forward_any_execution_request(operation, DAOReundoRequest);
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCWaitUndoAck);
            break;
        }
        case TPCWaitUndoToFinish:
        {
            //resend the undo request
            forward_any_execution_request(operation, DAOReundoRequest);
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCWaitUndoToFinish);
            break;
        }
        case TPCPartComp:
            //Try to execute the own part of the operation again
        {
            forward_any_execution_request(operation, DAORedoRequest);
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), rel_timeout, operation->status);
            break;
        }

        case TPCPartWaitVReqYes:
        {
            //should not happen (no timeout item created when enterring this state)
            break;
        }
        case TPCPartWaitVReqNo:
        {
            //should not happen   (no timeout item created when enterring this state)
            break;
        }
        case TPCPartWaitVResultExpectNo:
            //send no vote again
        {
            vector < string > server_list;
            for (int i = 0; i < operation->participants.size(); i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }
            // send message to coordinator
            if (operation->status == TPCPartWaitVReqNo)
            {
                failure = send_simple_message(operation->ID, TPCRVoteN, server_list);
                operation->status = TPCPartWaitVResultExpectNo;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartWaitVResultExpectNo);
            }
            else
            {
                failure = send_simple_message(operation->ID, TPCRVoteY, server_list);
                operation->status = TPCPartWaitVResultExpectYes;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartWaitVResultExpectYes);
            }

            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Cannot send vote message to coordinator in two phase commit protocol.");
#endif
            }
            break;
        }
        case TPCPartWaitVResultExpectYes:
            //send yes vote again
        {
            vector < string > server_list;
            for (int i = 0; i < operation->participants.size(); i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }
            // send message to coordinator
            if (operation->status == TPCPartWaitVReqNo)
            {
                failure = send_simple_message(operation->ID, TPCVoteN, server_list);
                operation->status = TPCPartWaitVResultExpectNo;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartWaitVResultExpectNo);
            }
            else
            {
                failure = send_simple_message(operation->ID, TPCVoteY, server_list);
                operation->status = TPCPartWaitVResultExpectYes;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartWaitVResultExpectYes);
            }

            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Cannot send vote message to coordinator in two phase commit protocol.");
#endif
            }
            break;
        }
        case TPCPartVReqRec:
            //send no vote to coordinator and log own vote
        {
            //write journal entry for unsuccessful execution

            failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCPVoteNo);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Logging own vote in two phase commit protocol failed.");
#endif
                if (failure == DAOSubtreeNotExisting)
                {
                    vector<Operation> journal_ops;
                    InodeNumber correct_journal = (*journal_manager).get_open_operations(operation->ID, journal_ops);
                    if (correct_journal!= INVALID_INODE_ID)
                    {
                        // set correct journal identifier
                        operation->subtree_entry = correct_journal;
                        // reexecute logging
                        failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCPVoteNo);
                    }
                    else
                    {
                        InodeNumber current_subtree_entry = operation->subtree_entry;
                        if (operation->type == MoveSubtree)
                        {
                            DaoHandler::set_subtree_entry_point(operation);
                        }
                        else
                        {
                            ChangeOwnershipDAOAdapter::set_subtree_entry_point(operation);
                        }
                        if (current_subtree_entry != operation->subtree_entry && operation->subtree_entry != INVALID_INODE_ID)
                        {
                            // reexecute logging
                            failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCPVoteNo);
                        }
                    }
                }
                // if still any failure stop execution of protocol step
                if (failure)
                {
                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();
                    return;
                }
            }
            // set up vote message
            // set coordinator to server_list
            vector < string > server_list;
            for (int i = 0; i < operation->participants.size(); i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }
            // send message to coordinator
            failure = send_simple_message(operation->ID, TPCRVoteN, server_list);
            operation->status = TPCPartWaitVResultExpectNo;
            add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCPartWaitVResultExpectNo);

            break;
        }
        case TPCCoordinatorVReqSend:
            //if overall timeout reached abort, else send vote request again
        {
            if (operation->overall_timeout < boost::posix_time::second_clock::universal_time() + boost::posix_time::milliseconds(rel_timeout))
            {
                // only start abort process if the operation has not been aborted yet
                //send abort message if it is first no vote which is received
                if (operation->status != TPCAborting && operation->status != TPCWaitUndoAck && operation->status != TPCWaitUndoToFinish)
                {
                    failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("Logging global result in two phase commit protocol failed.");
#endif
                        if (failure == DAOSubtreeNotExisting)
                        {
                            vector<Operation> journal_ops;
                            InodeNumber correct_journal = (*journal_manager).get_open_operations(operation->ID, journal_ops);
                            if (correct_journal!= INVALID_INODE_ID)
                            {
                                // set correct journal identifier
                                operation->subtree_entry = correct_journal;
                                // reexecute logging
                                failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
                            }
                            else
                            {
                                InodeNumber current_subtree_entry = operation->subtree_entry;
                                if (operation->type == MoveSubtree)
                                {
                                    DaoHandler::set_subtree_entry_point(operation);
                                }
                                else
                                {
                                    ChangeOwnershipDAOAdapter::set_subtree_entry_point(operation);
                                }
                                if (current_subtree_entry != operation->subtree_entry && operation->subtree_entry != INVALID_INODE_ID)
                                {
                                    // reexecute logging
                                    failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, TPCIAborting);
                                }
                            }
                        }
                        // if still any failure stop execution of protocol step
                        if (failure)
                        {
                            // ends profiling
                            Pc2fsProfiler::get_instance()->function_end();
                            return;
                        }
                    }

                    int num_removed = 0;
                    // remove all previous received votes from respective datastructure
                    if (operation->recieved_votes < operation->participants.size())
                    {
                        for (int i = 0; operation->recieved_votes + num_removed < operation->participants.size() && i < operation->participants.size(); i++)
                        {
                            //create identification for possible received vote
                            stringstream vote_identification;
                            vote_identification << operation->ID << '.' << operation->participants.at(i).server;
                            string vote_ID = vote_identification.str();
                            if (TPC_received_request_answers.erase(vote_ID))
                            {
                                num_removed++;
                            }
                        }
                    }
                    if (operation->recieved_votes + num_removed != operation->participants.size())
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("More or less vote identification than received are removed from the datastructure.");
#endif
                        remove_all_entries_for_operation(operation->ID);
                    }
                    // set recieved_votes to #particpants to count acknowledges
                    operation->recieved_votes = operation->participants.size();

                    // send abort message
                    // create server list
                    vector < string > server_list;
                    for (int i = 0; i < operation->participants.size(); i++)
                    {
                        server_list.push_back(operation->participants.at(i).server);
                    }
                    // send message to external modules
                    failure = send_simple_message(operation->ID, TPCPRAbort, server_list);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("Sending abort message failed at least partly.");
#endif
                    }
                    // reset counter for participant answers
                    operation->recieved_votes = operation->participants.size();

                    // check if operation need to be undone
                    if (operation->type == MoveSubtree || operation->type == ChangePartitionOwnership)
                    {
                        forward_any_execution_request(operation, DAOUndoRequest);
                        operation->status == TPCWaitUndoAck;
                        add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCWaitUndoAck);
                    }
                    else
                    {
                        // client negative acknowledgement (NACK)
                        provide_result(operation->ID, false, TwoPhaseCommit, operation->type == MoveSubtree);
                        // set status to aborting
                        operation->status = TPCAborting;
                        add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), TPC_REL_TIMEOUT, TPCAborting);
                    }
                }

            }
            else
            {
                // set up vote request message
                // add all participants
                vector < string > server_list;
                for (int i = 0; i < operation->participants.size(); i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }

                // send vote request to participants
                failure = send_simple_message(operation->ID, TPCRVoteReq, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending vote request in two phase commit protocol failed at least for one server.");
#endif
                }

                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), rel_timeout, operation->status);
            }
            break;
        }
        case TPCCoordinatorVResultSend:
            //send vote result again
        {
            // set up message for vote result
            // add all participants to server_list
            vector<string> server_list;
            for (int i=0;i<operation->participants.size();i++)
            {
                server_list.push_back(operation->participants.at(i).server);
            }

            // send message to all participants
            failure = send_simple_message(operation->ID,TPCRCommit, server_list);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                dao_logger.error_log("Sending global result failed in two phase commit protocol at least for one server.");
#endif
            }

            break;
        }
        default:
        {
            handle_internal_failure(DAOOperationCorrupted);
            break;
        }
        }
    }
    else
    {
        if (operation->used_protocol == ModifiedTwoPhaseCommit)
        {
            switch (operation->status)
            {
            case MTPCCoordinatorComp:
                //abort
            {
                failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Abort log failed.");
#endif
                    if (failure == DAOSubtreeNotExisting)
                    {
                        vector<Operation> journal_ops;
                        InodeNumber correct_journal = (*journal_manager).get_open_operations(operation->ID, journal_ops);
                        if (correct_journal!= INVALID_INODE_ID)
                        {
                            // set correct journal identifier
                            operation->subtree_entry = correct_journal;
                            // reexecute logging
                            failure = write_finish_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, false);
                        }
                        else
                        {
                            InodeNumber current_subtree_entry = operation->subtree_entry;
                            if (operation->type == MoveSubtree)
                            {
                                DaoHandler::set_subtree_entry_point(operation);
                            }
                            else
                            {
                                ChangeOwnershipDAOAdapter::set_subtree_entry_point(operation);
                            }
                            if (current_subtree_entry !=operation->subtree_entry && operation->subtree_entry != INVALID_INODE_ID)
                            {
                                // reexecute logging
                                failure = write_finish_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, false);
                            }
                        }
                    }
                    // if still any failure stop execution of protocol step
                    if (failure)
                    {
                        // ends profiling
                        Pc2fsProfiler::get_instance()->function_end();

                        return;
                    }

                }
                // delete operation from temporary data structure
                free(operation->operation);
                if (open_operations.erase(operation->ID)==0)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
                }

                // client negative acknowledgement (NACK)
                provide_result(operation->ID, false, ModifiedTwoPhaseCommit, operation->type==MoveSubtree);

                break;
            }
            case MTPCCoordinatorReqSend:
                //request the current status of the participant
            {
                //send MTPCStatusReq
                vector < string > server_list;
                for (int i = 0; i < operation->participants.size(); i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }

                // send vote request to participants
                failure = send_simple_message(operation->ID, MTPCRStatusReq, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending vote request in two phase commit protocol failed at least for one server.");
#endif
                }
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCCoordinatorReqSend);
                break;
            }

            case MTPCPartComp:
                //abort
            {

                // non-successful execution
                failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, MTPCPAbort);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Global vote log result failed in modified two phase commit protocol.");
#endif
                    if (failure == DAOSubtreeNotExisting)
                    {
                        vector<Operation> journal_ops;
                        InodeNumber correct_journal = (*journal_manager).get_open_operations(operation->ID, journal_ops);
                        if (correct_journal!= INVALID_INODE_ID)
                        {
                            // set correct journal identifier
                            operation->subtree_entry = correct_journal;
                            // reexecute logging
                            failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, MTPCPAbort);
                        }
                        else
                        {
                            InodeNumber current_subtree_entry = operation->subtree_entry;
                            if (operation->type == MoveSubtree)
                            {
                                DaoHandler::set_subtree_entry_point(operation);
                            }
                            else
                            {
                                ChangeOwnershipDAOAdapter::set_subtree_entry_point(operation);
                            }
                            if (current_subtree_entry != operation->subtree_entry && operation->subtree_entry != INVALID_INODE_ID)
                            {
                                // reexecute logging
                                failure = write_update_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, MTPCPAbort);
                            }
                        }
                    }
                    // if still any failure stop execution of protocol step
                    if (failure)
                    {
                        // ends profiling
                        Pc2fsProfiler::get_instance()->function_end();
                        return;
                    }
                }

                // set up message for complete operation result
                // add coordinator to server_list
                vector<string> server_list;
                for (int i=0;i<operation->participants.size();i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }

                // send operation result
                failure = send_simple_message(operation->ID,MTPCRAbort, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending operation result to coordinator in modified two phase commit protocol failed.");
#endif
                }
                // operation result sent
                operation->status=MTPCPartVoteSendNo;
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCPartVoteSendNo);

                break;
            }
            case MTPCPartVoteSendYes:
                //send yes vote again
            {
                // set up message for complete operation result
                // add coordinator to server_list
                vector<string> server_list;
                for (int i=0;i<operation->participants.size();i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }

                // send operation result
                failure = send_simple_message(operation->ID,MTPCRCommit, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending operation result to coordinator in modified two phase commit protocol failed.");
#endif
                }

                break;
            }
            case MTPCPartVoteSendNo:
                //send no vote again
            {
                vector<string> server_list;
                for (int i=0;i<operation->participants.size();i++)
                {
                    server_list.push_back(operation->participants.at(i).server);
                }

                // send operation result
                failure = send_simple_message(operation->ID,MTPCRAbort, server_list);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    dao_logger.error_log("Sending operation result to coordinator in modified two phase commit protocol failed.");
#endif
                }

                break;
            }
            case MTPCIWaitResultUndone:
            {
                forward_any_execution_request(operation, DAOReundoRequest);
                add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), MTPC_REL_TIMEOUT, MTPCIWaitResultUndone);
                break;
            }
            default:
            {
                handle_internal_failure(DAOOperationCorrupted);
                break;
            }
            }


        }
        else
        {
            if (operation->used_protocol == OrderedOperationExecution)
            {
                switch (operation->status)
                {
                case OOEComp:
                    //abort
                {
                    //unsuccessful execution
                    // write abort into log
                    failure = write_finish_log(operation->ID, ((operation->type)==MoveSubtree || (operation->type) == ChangePartitionOwnership),operation->subtree_entry, false);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("Abort log failed.");
#endif
                        if (failure == DAOSubtreeNotExisting)
                        {
                            vector<Operation> journal_ops;
                            InodeNumber correct_journal = (*journal_manager).get_open_operations(operation->ID, journal_ops);
                            if (correct_journal!= INVALID_INODE_ID)
                            {
                                // set correct journal identifier
                                operation->subtree_entry = correct_journal;
                                // reexecute logging
                                failure = write_finish_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, false);
                            }
                            else
                            {
                                InodeNumber current_subtree_entry = operation->subtree_entry;
                                if (operation->type == MoveSubtree)
                                {
                                    DaoHandler::set_subtree_entry_point(operation);
                                }
                                else
                                {
                                    ChangeOwnershipDAOAdapter::set_subtree_entry_point(operation);
                                }
                                if (current_subtree_entry != operation->subtree_entry && operation->subtree_entry != INVALID_INODE_ID)
                                {
                                    // reexecute logging
                                    failure = write_finish_log(operation->ID, ((operation->type) == MoveSubtree || (operation->type) == ChangePartitionOwnership), operation->subtree_entry, false);
                                }
                            }
                        }
                        // if still any failure stop execution of protocol step
                        if (failure)
                        {
                            // ends profiling
                            Pc2fsProfiler::get_instance()->function_end();

                            return;
                        }
                    }

                    //check if intermediate or last execution failed
                    if (operation->participants.size()!=0)
                    {
                        // set up message for abort (negative acknowledgement)
                        // set coordinator to server_list
                        vector<string> server_list;
                        server_list.push_back(operation->participants.at(0).server);

                        // send negative acknowledgement
                        failure = send_simple_message(operation->ID,OOERAborted, server_list);
                        if (failure)
                        {
#ifdef LOGGING_ENABLED
                            dao_logger.error_log("Sending abort message to previous executor in ordered operation execution failed.");
#endif
                        }

                    }
                    else // first operation failed
                    {
                        // client negative acknowledgement (NACK)
                        provide_result(operation->ID, false, OrderedOperationExecution, operation->type==MoveSubtree || operation->type==OOE_LBTest);
                    }

                    //delete operation from temporary data structure
                    free(operation->operation);
                    if (open_operations.erase(operation->ID)==0)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.warning_log("Operation deletion from temporary data structure failed.");
#endif
                    }

                    break;
                }
                case OOEWaitResult:
                    //send status request to predecesor
                {
                    //send MTPCStatusReq
                    vector < string > server_list;
                    for (int i = 0; i < operation->participants.size(); i++)
                    {
                        server_list.push_back(operation->participants.at(i).server);
                    }

                    // send vote request to participants
                    failure = send_simple_message(operation->ID, OOERStatusReq, server_list);
                    if (failure)
                    {
#ifdef LOGGING_ENABLED
                        dao_logger.error_log("Sending vote request in two phase commit protocol failed at least for one server.");
#endif
                    }
                    break;
                }
                case OOEWaitResultUndone:
                {
                    forward_any_execution_request(operation, DAOReundoRequest);
                    add_timeout(operation->ID, boost::posix_time::second_clock::universal_time(), OOE_REL_TIMEOUT, OOEWaitResultUndone);
                    break;
                }
                default:
                {
                    handle_internal_failure(DAOOperationCorrupted);
                    break;
                }
                }
            }
            else
            {
                handle_internal_failure(DAOOperationCorrupted);
            }
        }
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
*\brief Method to add a timeout object into the queue.
*
*\param ID - The ID of the operation for that a timeout is added
*\param enter_time - The time when that operation started (needed for overall timeout)
*\param rel_timeout - The relative timeout this operation uses for one step
*\param last_status - The current status of the operation
*
* Add a timeout object in the queue that is observed by find_timeout.
*
*/
void DistributedAtomicOperations::add_timeout(uint64_t ID, boost::posix_time::ptime enter_time, double rel_timeout, Status last_status)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    DAOTimeoutStruct timeout_object;
    timeout_object.enter_time = enter_time;
    timeout_object.ID = ID;
    timeout_object.rel_timeout = rel_timeout;
    timeout_object.last_status = last_status;
    timeout_queue.push(timeout_object);

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
 *\brief Deletes all timeouts with the given ID
 *
 *\param ID - The ID of the operation for that the timeouts shall be deleted
 *
 * Deletes all timeout of the operation with an ID of ID
 *
 */
void DistributedAtomicOperations::delete_timeouts(uint64_t ID)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    priority_queue < DAOTimeoutStruct, vector <DAOTimeoutStruct>, DAOComparison > zwischen;
    while (!timeout_queue.empty())
    {
        if (timeout_queue.top().ID!=ID)
        {
            zwischen.push(timeout_queue.top());
        }
        timeout_queue.pop();
    }
    timeout_queue = zwischen;
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}
