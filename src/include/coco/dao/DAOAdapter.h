/**
 * \file DAOAdapter.h
 *
 * \author Marie-Christine Jakobs
 *
 * \brief Realizes the adapter between the distributed atomic operations module and the executing module
 *
 * The adapter is realized as an abstract class. All class methods provided do not have any functionality but are meant to be abstract and
 * should be implemented by the class realizing the adapter. More than one adapter can be used for a module but the functionality should be
 * the same for every adapter of a module. The adapter receives requests from the distributed atomic operations module, decodes the requests,
 * calls the respective, abstract functions to handle the request and afterwards provide a result to the distributed atomic operations module
 * if necessary.
 */

#ifndef DAOADAPTER_H_
#define DAOADAPTER_H_

#include <pthread.h>
#include "coco/communication/ConcurrentQueue.h"
#include "coco/dao/CommonDAOTypes.h"

class DAOAdapter
{
public:
    /**
     *\brief Constructor of the abstract class
     *
     * Starts the thread which listens to receive requests from the distributed atomic operations module. The reference to the queue which is
     * needed to receive requests is set to NULL to allow a constructor without parameters. The actual reference can be set with another
     * method and starts the reception of requests.
     */
    DAOAdapter();
    /**
     *\brief Destructor of the adapter
     *
     */
    virtual ~DAOAdapter() {}
    /**
      *\brief Checks if this server executes the coordinator part of the distributed atomic operation
      *
      *\param uncomplete_operation - reference to the distributed atomic operation for which it should be decided if it is the coordinator execution part, reference to distributed atomic operation as it is saved in the temporary datastructure,
      *
      *\return true if this server started the distributed atomic operation and is still responsible for this part or if this server is now responsible for the part of the operation which normally causes the start of the distributed atomic operation, false otherwise
      *
      * Check if this server is responsible for the execution of the coordinator part of this distributed atomic operation. The coordinator part
      * means that it the server is responsible for the start of distributed atomic operation or the distributed atomic operation was started
      * for one subtree the server is now responsible for. The operation description may give a hint to answer this question. Currently this
      * operation is only needed for the two phase commit protocol.
      *
      * Note: This class method does not do anything. It is meant to be an abstract class method and it is expected that every class which
      * inherits from this class also implement this method.
      */
    static bool is_coordinator(DAOperation* uncomplete_operation);
    /**
     *\brief Sets the participants part of the distriubted atomic operation
     *
     *\param uncomplete_operation - reference to the distributed atomic operation for which the participants should be set, references structure in main memory
     *
     *\return 0 if successful otherwise a failure code defined in DAOFailureCodes enumeration
     *
     * Sets the addresses (uncomplete_operation->participants), server address and subtree entry point, the distributed atomic operation needs
     * to send messages during protocol execution. For the ordered execution protocol the previous, followed by the following participant must
     * be set. If it is the first participant in the order, only the following participant is set. If it is the last participant, only the
     * previous participant needs to be set. If it is the (modified) two phase commit protocol, the participants which did not start the
     * operation execution, need to set the coordinator. The coordinator sets the addresses of the other participants, only one participant in
     * the modified two phase commit protocol.
     *
     * Note: This class method does not do anything. It is meant to be an abstract class method and it is expected that every class which
     * inherits from this class also implement this method.
     */
    static int set_sending_addresses(DAOperation* uncomplete_operation);
    /**
     *\brief Sets the identification of the subtree entry point for the subtree the distributed atomic operation part of this server belongs to
     *
     *\param uncomplete_operation - reference to the distributed atomic operation for which the subtree entry point should be set, references structure in main memory
     *
     *\return 0 if successful otherwise a failure code defined in DAOFailureCodes enumeration
     *
     * Sets the subtree entry point (uncomplete_operation->subtree_entry) for the part of the distributed atomic operation this server is
     * responsible for.
     *
     * Note: This class method does not do anything. It is meant to be an abstract class method and it is expected that every class which
     * inherits from this class also implement this method.
     */
    static int set_subtree_entry_point(DAOperation* uncomplete_operation);
    /**
     *\brief Sets the module identification of this module and starts the reception of requests
     *
     *\param my_module_id - identifier of the module which uses this adapter
     *
     * Sets the identification of the module for which the requests should be received and executed. Furthermore the reference to the
     * respective queue in which the requests are put is set. This method also allows to change the module for which requests should be
     * received.
     */
    void set_module(DistributedAtomicOperationModule my_module_id);

#ifdef DAO_TESTING
    ConcurrentQueue<InQueue> test_output;
#endif

protected:

    /**
     *\brief Retrieves the next participant in the ordered operation execution protocol for the described operation
     *
     *\param operation - description of the operation which is executed atomically
     *\param operation_length - length of the operation description in number of bytes
     *
     *\return Subtree describing the participant that is the next participant in the ordered operation execution order which follows directly after this server, if there is no following participant the server part of the Subtree is an empty string (length 0)
     *
     * Computes the next participant, its address and the subtree entry point of the corresponding subtree, in the ordered operation execution
     * protocol. The computed participant directly follows after this participant. If no next participant exists, the server of the Subtree
     * struct will be set to an empty string. The method will be only called if a request for an ordered operation execution protocol operation
     * occurs.
     */
    virtual Subtree get_next_participant(void* operation, uint32_t operation_length) = 0;
    /**
     *\brief Treats the acknowledgement, negative acknowledgement for the distributed atomic operation with ID <code>id</code>
     *
     *\param id - unique identifier of the distributed atomic operation, the identifier should be known because it was returned when the distributed atomic operation was started
     *\param success - true if the distributed atomic operation was executed successfully, otherwise false
     *
     * Action of the module which uses this adapter if an atomic distributed operation, started by the module on this server or by another
     * server and this server is now responsible, is finished. To be able to relate the finished operation with the started one, the ID
     * of the operation should be stored after the start of the operation. Two possible finish results must be treated, the successful finish
     * (acknowledgement) and the unsuccessful finish (the negative acknowledgement). The distributed atomic operations module provides the
     * result of an operation as soon as possible, meaning that the result is provided after the result of the operation cannot be changed and
     * the undo operation execution for this server is done when it was necessary. Note that an acknowledgement does not mean that the protocol
     * for the operation is finished.
     */
    virtual void handle_operation_result(uint64_t id, bool success) = 0;
    /**
     *\brief Executes the subpart of the distributed atomic operation for which this server is responsible
     *
     *\param id - unique identification of the distributed atomic operation for which the request should be executed
     *\param operation_description - description of the distributed atomic operation for which the part of this server should be executed
     *\param operation_length - length of the operation description
     *
     *\return true if the request can be executed successfully, false otherwise
     *
     * Executes this server's subpart of the distributed atomic operation with <code>id</code>. The operation description which was provided
     * at the start is contained in <code>operation_description</code>. If the operation cannot be executed successfully and the changes are
     * not logged in a journal, but changes effect the state of the metadata server, the changes must be undone before the method returns with
     * false. If the operation result is logged in the journal, the ID must be used to state that the operation execution results are part
     * of a distributed atomic operation.
     * Note: Due to the implemented recovery an operation request may be requested more than once instead of using the rerequest. So it may
     * be helpful to check if parts of the operation are already executed and thus avoid unnecessary operations. Nevertheless checking does
     * only make sense if it is faster than reexecution.
     */
    virtual bool handle_operation_request(uint64_t id, void* operation_description, uint32_t operation_length) = 0;
    /**
     *\brief Executes the subpart of the distributed atomic operation for which this server is responsible
     *
     *\param id - unique identification of the distributed atomic operation for which the request should be executed
     *\param operation_description - description of the distributed atomic operation for which the part of this server should be (re)executed
     *\param operation_length - length of the operation description
     *
     *\return true if the request can be executed successfully, false otherwise
     *
     * If the operation is not executed so far, executes this server's subpart of the distributed atomic operation with <code>id</code>.
     * The operation description which was provided at the start is contained in <code>operation_description</code>. If the operation cannot
     * be executed successfully and the changes are  not logged in a journal, but changes effect the state of the metadata server, the changes
     * must be undone before the method returns with false. If the operation result is logged in the journal, the ID must be used to state that the operation execution results are part
     * of a distributed atomic operation.
     * Note: Execution may be realized with the help of <code>handle_operation_request</code>. This operation is called whenever the
     * distributed atomic operations module has requested an operation request before. This does not mean that the operation request was
     * performed. Maybe the executing module crashed and is recovered.
     */
    virtual bool handle_operation_rerequest(uint64_t id, void* operation_description, uint32_t operation_length) = 0;
    /**
     *\brief Undos the subpart of the distributed atomic operation for which this server is responsible
     *
     *\param id - unique identification of the distributed atomic operation for which the request should be executed
     *\param operation_description - description of the distributed atomic operation for which the part of this server should be undone
     *\param operation_length - length of the operation description
     *
     *\return true if the request can be executed successfully, false otherwise
     *
     * Undos the subpart of the distributed atomic operation with id <code>id</code>. The operation description provided at the start of the
     * distributed atomic operation is given in <code>operation_description</code>. After a successful undo every changes of the metadata
     * server which is directly visible was undone. If all the changes are logged, no undo will be necessary. If the undo is unsuccessful,
     * nothing or only parts of the operation may be undone.
     * Note: Due to recovery in the distributed atomic operations module an undo request will be requested more than once instead of using
     * the reundo request.
     */
    virtual bool handle_operation_undo_request(uint64_t id, void* operation_description, uint32_t operation_length) = 0;
    /**
     *\brief Undos the subpart of the distributed atomic operation for which this server is responsible
     *
     *\param id - unique identification of the distributed atomic operation for which the request should be executed
     *\param operation_description - description of the distributed atomic operation for which the part of this server should be undone
     *\param operation_length - length of the operation description
     *
     *\return true if the request can be executed successfully, false otherwise
     *
     * If the operation has not been undone so far, the operation may be undone using <code>handle_operation_undo_request</code>. The operation
     * description which was provided at the start is contained in <code>operation_description</code>. If the operation was successful, all
     * visible effects of the operation are undone. An undo is only necessary if the effects of the operation directly affect the state of
     * the metadata server and are not logged in the journal.
     * Note: This operation is called whenever the distributed atomic operations module has requested an operation undo request before. This
     * does not mean that the operation undo request was performed. Maybe the executing module crashed and is recovered.
     */
    virtual bool handle_operation_reundo_request(uint64_t id, void* operation_description, uint32_t operation_length) = 0;

private:
    /**
     * Thread which listens to the queue <code>provide_requests_queue</code> to receive requests from the distributed atomic operations
     * module and provide the requested result to the distributed atomic operations module. The thread realizes the communication between
     * the distributed atomic operations module and the executing modulel during operation execution. Notice: An operation cannot be started
     * with this thread. The thread is started in the constructor.
     */
    pthread_t handle_dao_requests_thread;
    /**
     * Reference to the queue which provides the request to the executing module. It is set to NULL at the creation of the object and is set
     * by <code>set_module(DistributedAtomicOperationModule my_module_id)</code>.
     */
    ConcurrentQueue<OutQueue>* provide_requests_queue;
    /**
     * If the object is created, it will have an arbitrary value. After <code>set_module(DistributedAtomicOperationModule my_module_id)</code>
     * was called, it has its intended value.
     */
    DistributedAtomicOperationModule corresponding_module;
    /**
     *\brief Method executed by <code>handle_dao_requests_thread</code>
     *
     *\param ptr - must be a reference to the DAOAdapter object to which the thread executing the method belongs to
     *
     *\return NULL because return needed to run method in thread. Since it is an endless loop method, the return statement is never reached
     *
     * Method which is executed in the <code>handle_dao_requests_thread</code>. As long as no module is set, meaning the <code>
     * provide_requests_queue</code> is still NULL, the thread sleeps for a second and then starts from the beginning. Otherwise it is waited
     * for the next request which is put into <code>provide_requests_queue</code>. If the requests is retrieved, the kind of request will be
     * decoded and the respective method is called for the treatment of this kind of request. After the request was treated and it was an
     * execution request, the result is forwarded to the distributed atomic operations module. For an acknowledgement of a completed operation
     * only the treatment is called.
     */
    static void* receive_dao_requests(void *ptr);
};

#endif //DAOADAPTER_H_
