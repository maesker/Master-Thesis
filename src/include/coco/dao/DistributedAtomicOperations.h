/**
 * \file DistributedAtomicOperations.h
 *
 * \author Marie-Christine Jakobs 	Benjamin Raddatz
 *
 * \brief Header for DistributedAtomicOperations.cpp. Contains description of class DistributedAtomicOperations which is responsible for managing distributed atomic operations.
 *
 * Defines all necessary methods which are necessary to manage distributed atomic operations executed with different protocols. Furthermore
 * methods are provided which will realize the recovery of distributed atomic operations. Moreover all necessary datastructures for the
 * recovery and execution management are declared here.
 */
#ifndef DISTRIBUTEDATOMICOPERATIONS_H_
#define DISTRIBUTEDATOMICOPERATIONS_H_


#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <pthread.h>
#include <cstring>

#include "CommonDAOTypes.h"
#include "coco/communication/ConcurrentQueue.h"
#include "coco/coordination/MltHandler.h"

#ifdef LOGGING_ENABLED
#include "logging/Logger.h"
#define DAO_LOGLEVEL LOG_LEVEL_ERROR
#define DAO_LOGFILE "DistributedAtomicOperations.log"
#define DAO_VISUALOUTPUT false
#endif

#ifdef DAO_TESTING
#include "../coco/dao/test/DAOTestEmulator.h"

#else
#include "coco/communication/CommunicationHandler.h"
#include "mm/journal/JournalManager.h"
#include "coco/communication/UniqueIdGenerator.h"

#endif

#define DAO_LOG_LEVEL LOG_LEVEL_ERROR


/**
 * \brief Implementing the funcitionality for the execution of distributed atomic operations
 *
 * Class realizes a singleton pattern. Only one instance of the class will be available.
 */
class DistributedAtomicOperations:CommunicationHandler
{
public:
    /**
     * \brief Destructor of RecCommunicationServer
     *
     * Cleans up. Therefore destroys mutex, removes all open distributed atomic operations from temporary datastructure and frees operation
     * description.
     */
    ~DistributedAtomicOperations();

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
    uint64_t start_da_operation(void* operation, uint32_t operation_length, OperationTypes operation_type,
                                std::vector<Subtree>& participants, InodeNumber subtree_entry);

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
    ConcurrentQueue<OutQueue>* get_queue(DistributedAtomicOperationModule module);

    /**
     *\brief Inform distributed atomic operations module about an execution result
     *
     *\param result - Result of the execution of a part of a distributed atomic operation
     *
     * Provides the result of a requested part execution of a distributed atomic operation to the distributed atomic operation module.
     * Afterwards the distributed atomic operation module can and will process the result and continue the execution of the protocol.
     */
    void provide_operation_execution_result(InQueue& result);

    /*!\brief Returns reference to the receive communication server.
    *
    * This method is needed to get a reference to the singleton instance.
    */
    static DistributedAtomicOperations& get_instance()
    {
        return instance;
    }

    /**
      *\brief Recovers all open operations
      *
      *\return  0 if recovery successful, otherwise the error code DAONotAllOperationsRecoverable
      *
      * Recovers all distributed atomic operations which are not finished and for which this metadata server is responsible. This method must
      * be executed at every start of the metadata server before any distributed atomic operation can be started or incoming events can be
      * processed by this module.
      *
      * Notice: Before this method is called, the journals of this metadata server must be created (available) and journal recovery must be
      * done.
      */
    static int do_recovery();

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
    bool open_operation_exists(InodeNumber subtree);

#ifdef DAO_TESTING
    /**
     *\brief Implement request handling.
     *
     *\param message - external request sent to this module
     *
     * Implements the abstract method of the emulated communication handler. Need to be public to be called from outside to start
     * participant tests and sent further messages which will are not fix in the order of the execution.
     *
     *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched
     */
    virtual void handle_request(ExtractedMessage& message);
#endif

protected:
#ifndef DAO_TESTING
    /**
     *\brief Implement request handling
     *
     *\param message - external request sent to this module
     *
     * Implements the abstract method of the communication handler. Handles all incoming external messages. Incoming external messages
     * cause events which are processed during request handling.
     */
    virtual void handle_request(ExtractedMessage& message);
#endif

private:
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
     *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched
     */
    int start_da_operation_participant(void* operation, uint32_t operation_length, OperationTypes operation_type, std::vector<Subtree>& initiator, InodeNumber subtree_entry,  uint64_t ID, Protocol used_protocol);

    /**
     *\brief Processes the protocol part of the distributed atomic operation <code>ID</code> which is specified by <code>event</code>
     *
     *\param event - Identifies an event of a protocol which must be processed next by the distributed atomic operation with <code>ID</code>
     *\param operation - reference to distributed atomic operation for which the event occurred
     *
     *\return 0 if execution was successful, otherwise a failure code
     *
     * Processes the protocol part of the distributed atomic operation <code>operation</code> which is specified by <code>event</code>.
     * A new log may be written, messages sent,the operation status changed, the operation deleted from datastructure and the client informed.
     *
     *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched
     */
    int handle_event(Events event, DAOperation* operation);

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
     * \throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched
     */
    static void* listen_queue(void *ptr);

    /**
     *\brief Manages all open operations
     *
     * Saves all open distributed atomic operation. If a distributed atomic operation is requested because a new event for it occurred,
     * the respective operation can be looked up with the ID. Afterwards the operation's status can be modified.
     */
    std::unordered_map<uint64_t, DAOperation> open_operations;
    /**
     *\brief Ensures that an answers (vote, ack) is not counted multiple times in the two phase commit protocol
     *
     * Saves the identification, ID.sending_server_address, for a participant in a distributed atomic operation of the two phase commit
     * protocol. The identification will be stored if votes or acknowledges are received at coordinator side. If a coordinator does not
     * expect further votes and acknowledges, respectively, or they are unimportant, all previously saved identification must be deleted from the
     * datastructure and further votes and acknowledges, respectively, are ignored for the respective distributed atomic operation.
     */
    std::unordered_set<std::string> TPC_received_request_answers;

    /**
    * \brief Constructor of DistributedAtomicOperations
    *
    * \throw runtime_error if underlying communication handler cannot be initialized, thread or mutex cannot be created
    *
    * Initializes the communication handler. Starts the thread for processing execution results and creates the mutex.
    */
    DistributedAtomicOperations();

    /**
     * \brief Copy constructor of DistributedAtomicOperations
     *
     * \param example - DistributedAtomicOperations whose values are copied into the new object
     *
     * Private copy constructor to ensure that no other class can use it. It is not used inside the class to ensure that only one single
     * instance of the class is available.
     */
    DistributedAtomicOperations(const DistributedAtomicOperations& example);

    /**
     *\brief Checks if the two subtrees are equal
     *
     *\param tree1 - first subtree of comparison, subtree which should be compared to <code>tree2</code>
     *\param tree2 - second subtree of comparison, subtree which should be compared to <code>tree1</code>
     *
     *\return 0 if subtrees are equal, otherwise an indicator, a kind of failure code which describes where they are different
     *
     * Checks if the two subtrees <code>tree1, tree2</code> are equal. This means that the subtree entry point and the server address is
     * identical. A different server address while the subtree entry point is the same may indicate a subtree movement.
     */
    int same_subtrees(Subtree* tree1, Subtree* tree2);

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
    void provide_result(uint64_t id, bool commit, Protocol protocol, bool load_queue);

    /**
     *\brief  Inserts the operation request type to the operation description and sends the request to the respective module
     *
     *\param operation - reference to existing distributed atomic operation for which a request of type <code>request_type</code> should be forwarded to the respective queue
     *\param request_type - specifies which kind of request should be forwarded to the executing module
     *
     * Adds the operation request type to the operation description and puts the request into the queue which is used to communicate with
     * the executing module.
     *
     *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched, caused due to too few memory available
     */
    void forward_any_execution_request(DAOperation* operation, OperationRequestType request_type);

    /**
    *\brief Sends simple messages which contain the message_type only
    *
    *\param id- unique ID of the distributed atomic operation for which a message is sent
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
    int send_simple_message(uint64_t id, ExchangeMessages message_type, vector<string>& receivers);

    /**
    *\brief  Writes every log entry which is neither a commit, abort or begin log
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
    int write_update_log(uint64_t id, bool server_journal, InodeNumber journal_identifier, LogMessages log_content);

    /**
    *\brief Writes commit and abort logs
    *
    *\param id - unique identifier of the distributed atomic operation for which an abort or commit log should be written
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
    int write_finish_log(uint64_t id, bool server_journal, InodeNumber journal_identifier, bool commit);

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
    int retrieve_lost_operation(uint64_t id);

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
    int recover_missing_operation_data(DAOperation* operation, vector<Operation>& operation_logs);

    /**
     *\brief Removes all entries of received requests which belong the distributed atomic operation with ID <code>id</code>
     *
     *\param id - unique identifier of the atomic operation for which all stored request answers should be removed
     *
     * Iterates over all stored received request answers and removes all request answers which belong to the distributed atomic operation
     * with ID <code>id</code>.
     */
    void remove_all_entries_for_operation(uint64_t id);
    /**
     *\brief If journal must be available, it will be created.
     *
     *\param server_journal - inidicates if the journal of the metadata server is not available (server_journal == true) or if a subtree related journal is not available
     *\param subtree_entry - if server_journal == false, the subtree entry point of the subtree for which the journal is not available, otherwise it can be any number
     *\param first_call -
     *
     *\return 0 if journal was created successfully and can be used, otherwise a failure code indicating why the journal cannot be created or must not be created
     *
     * Checks if the respective journal must be available. If it must not be available or the check failed, an error code will be returned.
     * Otherwise it is tried to create the journal.
     *\throw  runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched
     */
    int handle_journal_not_existing(bool server_journal, InodeNumber subtree_entry, bool first_call);

    /**
     *\brief Adds a send socket with address <code> socket_address</code> to the SendCommunicationServer
     *
     *\param socket_address - identifier of the server for which a sending socket is not existing. The identifier is the IP address of the server. If VARIABLE_PORTS defined unequal to 0 the IP address must include the port as well.
     *
     *\return  a failure code if the socket cannot be created successfully, 0 otherwise if no exception occurred
     *
     * Tries to add a send socket to the SendCommunicationServer. If an internal failure like a mutex failure occurs this is handled
     * internally first. In general all failures occurring during adding of the socket lead to a negative method result. Only if the socket
     * can be used afterwards, a positive result will be returned.
     *
     *\throw runtime_error thrown by called method <code>handle_internal_failure(DAOFailure)</code>, exception is not catched, caused by mutex failure
     */
    int handle_socket_not_existing(string socket_address);

    /**
     *\brief Error handling for internal failures like mutex, journal log or insufficient memory failures.
     *
     *\param failure_type - indicates which kind of failure should be treated.
     * Error handling for internal failures like mutex, journal log or insufficient memory failures. Does not handle failures occuring during
     * creation or destruction of DistributedAtomicOperations singleton instance.
     *
     *\throw runtime_error if an internal error cannot be solved during runtime
     */
    void handle_internal_failure(DAOFailure failure_type);

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
     *\throw runtime_error if the status is not defined in CommonDAOTypes.h
     */
    Status get_protocol_status_of_opposite_side(DAOperation* operation);

    /**
     *\brief Handles the failure messages NotResponsible, EventReRequest, ContentRequest, ContentResponse, StatusRequest and StatusResponse.
     *
     *\param message_content - message received by the distributed atomic operation and which is a failure message
     *\param event_request - type of failure message
     *\param ID - identifier of the operation for which a failure message was send
     *\param affected_operation - reference to the operation for which a failure message was sent
     *\param sender - sender who sent the failure message to this server
     *
     *\return  0 if the handling of the failure message was successful, an error code otherwise
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
    int handle_failure_messages(void* message_content, ExchangeMessages* event_request, uint64_t ID, DAOperation* affected_operation, string sender);

    /**
     *\brief Reacts to the assumption that the external server with address <code>server_address</code> has failed.
     *
     *\param server_address - server address of the server which is assumed to be failed
     *
     * Since no global error handling and no global voting for failed servers is implemented, nothing is done so far.
     */
    void handle_supposed_external_server_failure(string server_address);

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
    void handle_unsuccessful_protocol_step(Events unsuccessful_event, int failure, DAOperation* operation, bool remove_operation, Status status_set_in_event, std::string sender);

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
    int check_allowed_request(ExchangeMessages event_request, std::string asyn_tcp_server, DAOperation* operation);

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
    static void* find_timeout(void *ptr);

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
    void handle_timeout(DAOperation * operation, boost::posix_time::ptime enter_time, double rel_timeout);

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
    void add_timeout(uint64_t ID, boost::posix_time::ptime enter_time, double rel_timeout, Status last_status);

    /**
     *\brief Deletes all timeouts with the given ID
     *
     *\param ID - The ID of the operation for that the timeouts shall be deleted
     *
     * Deletes all timeout of the operation with an ID of ID
     *
     */
    void delete_timeouts(uint64_t ID);

    /**
     *\brief Queue for saving execution results
     *
     * Saves execution results in FIFO manner. Execution results are put into the queue by <code>provide_operation_execution_result</code>
     * and requested by <code>listen_queue</code>.
     */
    ConcurrentQueue<InQueue> in_queue;

    /**
     *\brief Respective queue for providing execution requests and client responses
     *
     * Buffers execution requests and client responses until they are retrieved by the respective module, DAO_MetaData or DAO_LoadBalancing.
     * The queue is filled by the distributed atomic operations module. Furthermore elements are retrieved by the FIFO rule.
     */
    ConcurrentQueue<OutQueue> mdi_queue, load_queue;
    /**
     *\brief Instance of UniqueIdGenerator to create unique IDs for new distributed atomic operations
     *
     * The UniqueIdGenerator is used to create a globally, unique ID for a new distributed atomic operation. The ID is created if the
     * operation is started for the coordinator.
     */
    UniqueIdGenerator id_generator;
    /**
     *\brief Reference to journal_manageger which manages all journals belonging to subtrees this server is responsible for
     *
     * Reference to journal manager is used to retrieve the respective journal for a distributed atomic operation. The journal manager
     * is needed whenever a new log should be written.
     */
    JournalManager* journal_manager;
    /**
     *\brief Serializes the modification of distributed atomic operations
     *
     * Initialized in the constructor and destroyed in the destructor. Ensures that an operation is modified at a point in time either by
     * a trigger of <code>handle_request</code> or of <code>listen_queue</code>. Furthermore at most one operation can be modified at a time::
     */
    pthread_mutex_t inc_event_mutex;
    /**
     *\brief Thread for execution of <code>listen_queue</code>
     *
     * Thread is created in the constructor. It executes the endless loop method <code>listen_queue</code>.
     */
    pthread_t handling_thread;
    /**
     *\brief Thread for execution of <code>find_timeout</code>
     *
     * Thread is created in the constructor. It executes the endless loop method <code>find_timeout</code>.
     */
    pthread_t find_thread;

    /**
     *\brief Indicates if recovery has not been done so far after crash
     *
     * Indicates if the distributed atomic operations module is still waiting for recovery. Thus, new operations cannot be started and events
     * may be treated differently.
     */
    bool recovery_done;
    /**
     * \brief Singleton instance of DistributedAtomicOperations.
     *
     * DistributedAtomicOperations instance which is used by every module to start a new distributed atomic operation, to retrieve a result
     * or an execution request.
     */
    static DistributedAtomicOperations instance;
    /**
    * \brief Queue that contains the Timeout objects.
    *
    * This queue contains the timeout objects for each operation. It is ordered from the timeout that need to be regarded next to the one furthest away.
    */
    priority_queue < DAOTimeoutStruct, vector <DAOTimeoutStruct>, DAOComparison > timeout_queue;
#ifdef LOGGING_ENABLED
    /**
     *\brief Logger which logs errors and warnings caused during the execution of distributed atomic operation.
     *
     * Logger logging errors and warning occurring during the execution of distributed atomic operations. It dDoes not log what happens in
     * the underlying CommunicationHandler. Therefore another logger is used.
     */
    Logger dao_logger;
#endif
};

#endif //#ifndef DISTRIBUTEDATOMICOPERATIONS_H_
