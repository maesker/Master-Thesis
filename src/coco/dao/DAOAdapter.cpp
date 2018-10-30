/**
 * \file DAOAdapter.cpp
 *
 * \author	Marie-Christine Jakobs
 *
 * \brief Realization of the DAOAdapter.h header file
 *
 *
 */

#include "coco/dao/DAOAdapter.h"
#include "coco/dao/DistributedAtomicOperations.h"

#include "pc2fsProfiler/Pc2fsProfiler.h"

#include <stdexcept>

using namespace std;

/**
 *\brief
 *
 * Starts the thread which listens to receive requests from the distributed atomic operations module. The reference to the queue which is
 * needed to receive requests is set to NULL to allow a constructor without parameters. The actual reference can be set with another
 * method and starts the reception of requests.
 */
DAOAdapter::DAOAdapter()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    provide_requests_queue = NULL;
    int failure = pthread_create(&handle_dao_requests_thread, NULL,&DAOAdapter::receive_dao_requests, this);
    if (failure)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        // thread cannot be created
        throw runtime_error("Thread for handling incoming requests of the distributed atomic operations module cannot be created.");
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
 *\brief Sets the module identification of this module and starts the reception of requests
 *
 *\param my_module_id - identifier of the module which uses this adapter
 *
 * Sets the identification of the module for which the requests should be received and executed. Furthermore the reference to the
 * respective queue in which the requests are put is set. This method also allows to change the module for which requests should be
 * received.
 */
void DAOAdapter::set_module(DistributedAtomicOperationModule my_module_id)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    corresponding_module = my_module_id;
    provide_requests_queue = DistributedAtomicOperations::get_instance().get_queue(my_module_id);

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

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
void* DAOAdapter::receive_dao_requests(void *ptr)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    OutQueue next_request;
    InQueue request_result;
    bool request_success, result;
    Subtree empty;
    // set reference to object
    DAOAdapter* adapter = (DAOAdapter*) ptr;
    if (adapter == NULL)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return NULL;
    }
    while (1)
    {
        if (adapter->provide_requests_queue == NULL)
        {
            Pc2fsProfiler::get_instance()->function_sleep();
            sleep(1);
            Pc2fsProfiler::get_instance()->function_wakeup();
        }
        else
        {
            // get next request
            //Pc2fsProfiler::get_instance()->function_sleep();
            (adapter->provide_requests_queue)->wait_and_pop(next_request);
            //Pc2fsProfiler::get_instance()->function_wakeup();
            if (next_request.operation_length>0)  // execution request
            {
                result = true;
                // analyze request type
                switch (((char*)next_request.operation)[0])
                {
                case DAORequest:
                {
                    //execute request
                    request_success = adapter->handle_operation_request(next_request.ID,  ((char*)next_request.operation)+1,next_request.operation_length-1);
                    // build request result
                    request_result.ID = next_request.ID;
                    if (request_success)
                    {
                        request_result.success = ExecutionSuccessful;
                    }
                    else
                    {
                        request_result.success = ExecutionUnsuccessful;
                    }
                    if (next_request.protocol == OrderedOperationExecution)
                    {
                        request_result.next_participant = adapter->get_next_participant(((char*)next_request.operation)+1, next_request.operation_length-1);
                    }
                    else
                    {
                        request_result.next_participant = empty;
                    }
                    // free operation pointer
                    free(next_request.operation);
                    break;
                }
                case DAORedoRequest:
                {
                    //execute request
                    request_success = adapter->handle_operation_request(next_request.ID,  ((char*)next_request.operation)+1,next_request.operation_length-1);
                    // build request result
                    request_result.ID = next_request.ID;
                    if (request_success)
                    {
                        request_result.success = ExecutionSuccessful;
                    }
                    else
                    {
                        request_result.success = ExecutionUnsuccessful;
                    }
                    if (next_request.protocol == OrderedOperationExecution)
                    {
                        request_result.next_participant = adapter->get_next_participant(((char*)next_request.operation)+1,next_request.operation_length-1);
                    }
                    else
                    {
                        request_result.next_participant = empty;
                    }
                    // free operation pointer
                    free(next_request.operation);
                    break;
                }
                case DAOUndoRequest:
                {
                    //execute request
                    request_success = adapter->handle_operation_request(next_request.ID,  ((char*)next_request.operation)+1,next_request.operation_length-1);
                    // free operation pointer
                    free(next_request.operation);
                    // build request result
                    request_result.ID = next_request.ID;
                    if (request_success)
                    {
                        request_result.success = UndoSuccessful;
                    }
                    else
                    {
                        request_result.success = UndoUnsuccessful;
                    }
                    request_result.next_participant = empty;
                    break;
                }
                case DAOReundoRequest:
                {
                    //execute request
                    request_success = adapter->handle_operation_request(next_request.ID,  ((char*)next_request.operation)+1,next_request.operation_length-1);
                    // free operation pointer
                    free(next_request.operation);
                    // build request result
                    request_result.ID = next_request.ID;
                    if (request_success)
                    {
                        request_result.success = UndoSuccessful;
                    }
                    else
                    {
                        request_result.success = UndoUnsuccessful;
                    }
                    request_result.next_participant = empty;
                    break;
                }
                default:
                {
                    result = false;
                }
                }
                if (result)
                {
#ifdef DAO_TESTING
                    adapter->test_output.push(request_result);
#else
                    //provide request result to dao module
                    DistributedAtomicOperations::get_instance().provide_operation_execution_result(request_result);
#endif
                }
            }
            else
            {
                // handle result of distributed atomic operation
                adapter->handle_operation_result(next_request.ID, ((char*)next_request.operation)[0] == 1);
                // free operation pointer
                free(next_request.operation);
            }
        }
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return NULL;
}

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
bool DAOAdapter::is_coordinator(DAOperation* uncomplete_operation)
{
    return false;
}
/**
 *\brief Sets the participants part of the distriubted atomic operation
 *
 *\param uncomplete_operation - distributed atomic operation for which the participants should be set
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
int set_sending_addresses(DAOperation* uncomplete_operation)
{
    return 0;
}
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
int set_subtree_entry_point(DAOperation* uncomplete_operation)
{
    return 0;
}