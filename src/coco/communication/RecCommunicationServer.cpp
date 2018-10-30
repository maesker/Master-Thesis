/**
 * \file RecCommunicationServer.cpp
 *
 * \author Marie-Christine Jakobs	Benjamin Raddatz
 *
 * \brief Realization of the RecCommunicationServer.h header file
 *
 * Implements the reception and forwarding of external messages sent by other metadata servers.
 */

#include <iostream>
#include <zmq.hpp>
#include <cstdlib>
#include <stdexcept>
#include <sstream>

#include "coco/communication/RecCommunicationServer.h"
#include "coco/communication/CommunicationFailureCodes.h"
#include "coco/coordination/MltHandler.h"

#include "pc2fsProfiler/Pc2fsProfiler.h"

using namespace std;

/**
 * Declaration and definition of class variable instance which represents the singleton instance.
 */
RecCommunicationServer RecCommunicationServer::instance = RecCommunicationServer();


/**
  * \brief Receives message by waiting for incoming messages and forwarding them to registered modules.
  *
  * Blocks until a message can be received. Then the message is analyzed. If the receiving module is known and registered, the message is
  * put into the respective data structure of the module. In any other case the message is dropped. If a message cannot be received completely
  * it is dropped as well.Afterwards the previous course of actions is repeated. This method is expected to run in a separat thread.
  *
  * Notice that the pointer to the message content of the delivered message must be freed after it is not needed any more.
  *
  * \param ptr - needed to use in pthread_t thread, not used, can be called was NULL argument
  * \return Executes an endless loop, nothing returned, only needed to be pthread compliant
  */
void RecCommunicationServer::receive()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    zmq_msg_t message_address;
    zmq_msg_t message;
    void* message_data;
    int message_size;
    CommunicationModule receiving_module;
    CommunicationModule sending_module;
    while (1)
    {
        // receiving first part of message (address of sender)
        failure = zmq_msg_init(&message_address);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            (*logging_instance).warning_log("Initializing message failed.");
#endif
            continue;
        }
        Pc2fsProfiler::get_instance()->function_sleep();
        failure = pthread_mutex_lock( &receive_mutex );
        Pc2fsProfiler::get_instance()->function_wakeup();
        if (failure)
        {
#ifdef LOGGING_ENABLED
            stringstream error_message;
            error_message<<"Unable to aquire mutex for sending external messages due to "<<failure;
            (*logging_instance).error_log(error_message.str().c_str());
#endif
            zmq_msg_close(&message_address);
            break;
        }
        do
        {
            Pc2fsProfiler::get_instance()->function_sleep();
            failure = zmq_recv(zmq_used_socket, &message_address,0);
            Pc2fsProfiler::get_instance()->function_wakeup();
        }
        while (failure && errno==EINTR);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            (*logging_instance).error_log("Receiving message containing sender ID failed.");
#endif
            zmq_msg_close(&message_address);
            failure = pthread_mutex_unlock(&receive_mutex);
#ifdef LOGGING_ENABLED
            if (failure)
            {
                stringstream warning_message3;
                warning_message3<<"Mutex cannot be unlocked due to "<<failure;
                (*logging_instance).warning_log(warning_message3.str().c_str());
            }
#endif
            continue;
        }

        ExternalMessage mess;
        // set sender of mess
        message_data=zmq_msg_data(&message_address);
        mess.sending_server = string((char*)message_data);

        // receiving second part of message (content)
        failure = zmq_msg_init(&message);	//at the moment only one message is sent, no failure expected ignored
#ifdef LOGGING_ENABLED
        (*logging_instance).warning_log("Initializing message failed unexpectedly.");
#endif
        do
        {
            Pc2fsProfiler::get_instance()->function_sleep();
            failure = zmq_recv(zmq_used_socket, &message,0);
            Pc2fsProfiler::get_instance()->function_wakeup();
        }
        while (failure && errno==EINTR);
        if (failure)
        {
#ifdef LOGGING_ENABLED
            (*logging_instance).error_log("Receiving message containing data failed.");
#endif
            zmq_msg_close(&message_address);
            zmq_msg_close(&message);
            failure = pthread_mutex_unlock(&receive_mutex);
#ifdef LOGGING_ENABLED
            if (failure)
            {
                stringstream warning_message3;
                warning_message3<<"Mutex cannot be unlocked due to "<<failure;
                (*logging_instance).warning_log(warning_message3.str().c_str());
            }
#endif
            continue;
        }

        failure = pthread_mutex_unlock(&receive_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message3;
            warning_message3<<"Mutex cannot be unlocked due to "<<failure;
            (*logging_instance).warning_log(warning_message3.str().c_str());
        }
#endif

        message_data = zmq_msg_data(&message);
        //extracting module number for receiver and sender represented by a character from message data
        receiving_module= (CommunicationModule)(*((char*)message_data));
        sending_module = (CommunicationModule)(*((char*)message_data+1));
        //setting sending module
        mess.sending_module = sending_module;
        // setting pointer to remaining message data
        message_data = ((char*)message_data)+2;
        // getting size of remaining message (two byte (two characters) smaller than original message)
        message_size = zmq_msg_size(&message)-2;
        mess.data_length = message_size;
        // set data into structure, recognize that the receiving module must free mess.data
        mess.data = malloc(message_size);
        if (mess.data == NULL)
        {
            // tidy up after receiving
            zmq_msg_close(&message_address);
            zmq_msg_close(&message);
#ifdef LOGGING_ENABLED
            (*logging_instance).error_log("Cannot allocate memory for received message data");
#endif
            continue;
        }
        memcpy(mess.data, message_data, message_size);

        //put message in respective message buffer
        if (receiving_module>=0 && receiving_module<NUM_MODULES && module_queues[receiving_module] != NULL)
        {
            (*module_queues[receiving_module]).push(mess);
        }
        else
        {
#ifdef LOGGING_ENABLED
            (*logging_instance).warning_log("Module number possibly higher than NUM_MODULES. Check if NUM_MODULE is identical with number of elements in enumeration Module ");
#endif
            //free the memory where the message content was placed, since it cannot be delivered it is not needed
            free(mess.data);
        }
        // tidy up after receiving
        zmq_msg_close(&message_address);
        zmq_msg_close(&message);
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
*\brief Closes and creates the new external sending socket specified currently in the Mlt.
*
*\return 0 if successful otherwise a failure code.
*
* First will close receive socket if it exists. Then the new receive socket is created and connected. If an error occurs during creation or
* connection the receive socket is set to NULL.
*/
int RecCommunicationServer::set_up_receive_socket()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    Pc2fsProfiler::get_instance()->function_sleep();
    int failure = pthread_mutex_lock( &receive_mutex );
    Pc2fsProfiler::get_instance()->function_wakeup();
    if (failure)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message;
        error_message<<"Unable to aquire mutex for sending external messages due to "<<failure;
        (*logging_instance).error_log(error_message.str().c_str());
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return MutexFailed;
    }
    if (zmq_used_socket!=NULL)
    {
        zmq_close(zmq_used_socket);
        sleep(1);
    }
    zmq_used_socket = zmq_socket(zmq_context, ZMQ_XREP);
    if (zmq_used_socket == NULL)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message2;
        error_message2<<"Receiving socket cannot be created due to error: "<<errno;
        (*logging_instance).error_log(error_message2.str().c_str());
#endif
        failure = pthread_mutex_unlock(&receive_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message3;
            warning_message3<<"Mutex cannot be unlocked due to "<<failure;
            (*logging_instance).warning_log(warning_message3.str().c_str());
        }
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return SocketCreationFailed;
    }
    // preparation for bind, getting address
    string address("tcp://*");
    // adding port
    if (VARIABLE_PORTS)
    {
        MltHandler& mlt_handler = MltHandler::get_instance();
        Server asyn_tcp_server;
        asyn_tcp_server = mlt_handler.get_my_address();
        // check if own server address is valid
        if (asyn_tcp_server.address.size()==0 || (VARIABLE_PORTS && asyn_tcp_server.port<49152))
        {
#ifdef LOGGING_ENABLED
            if (asyn_tcp_server.address.size()==0)
            {
                (*logging_instance).error_log("Unknown own server address.");
            }
            else
            {
                (*logging_instance).error_log("Unallowed port number.");
            }
#endif
            failure = pthread_mutex_unlock(&receive_mutex);
#ifdef LOGGING_ENABLED
            if (failure)
            {
                stringstream warning_message3;
                warning_message3<<"Mutex cannot be unlocked due to "<<failure;
                (*logging_instance).warning_log(warning_message3.str().c_str());
            }
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return WrongMltData;
        }
        else
        {
            stringstream my_port;
            my_port<<":"<<asyn_tcp_server.port;
            address.append(my_port.str());
        }
    }
    else
    {
        address.append(PORT_NUMBER);
    }
    // bind to address
    failure = zmq_bind(zmq_used_socket,address.c_str());
    if (failure)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Socket cannot be bound.");
#endif
        zmq_close(zmq_used_socket);
        failure = pthread_mutex_unlock(&receive_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message3;
            warning_message3<<"Mutex cannot be unlocked due to "<<failure;
            (*logging_instance).warning_log(warning_message3.str().c_str());
        }
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return SocketCreationFailed;
    }
    failure = pthread_mutex_unlock(&receive_mutex);
#ifdef LOGGING_ENABLED
    if (failure)
    {
        stringstream warning_message3;
        warning_message3<<"Mutex cannot be unlocked due to "<<failure;
        (*logging_instance).warning_log(warning_message3.str().c_str());
    }
#endif

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/*!\brief Constructor of "RecCommunicationServer". Initializes object variables.
 *
 * Initializes the references to the queues of the respective modules to NULL. Sets up the zeromq context and socket for receiving external
 * messages which is one half of low level MDS to MDS communication. The socket is of type ZMQ_XREP (ZMQ_ROUTER) to be able to receive multiple
 * messages without replying. Afterwards the socket is binded to the correct address for reception. If set up of context or socket or the
 * binding fails, a runtime_error is thrown.
 *
 * Notice the in the implementation the c methods of zmq are used because the c++ implementation only wraps and did not work correctly.
 * Furthermore it is faster.
 *
 *\throw runtime_error set up of context, socket or binding failed
 */
RecCommunicationServer::RecCommunicationServer()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    //set up logging instance
#ifdef LOGGING_ENABLED
    logging_instance = CommunicationLogger::get_instance();
#endif
    //initializing connection to modules
    for (int i=0;i<NUM_MODULES; i++)
    {
        module_queues[i]=NULL;
    }
    zmq_context = zmq_init(1);
    //check if context was initialized properly
    if (zmq_context == NULL)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message;
        error_message<<"ZMQ context cannot be initialized due to error: "<<errno;
        (*logging_instance).error_log(error_message.str().c_str());
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        throw runtime_error("ZMQ context could not be initialized.");
    }
    zmq_used_socket=NULL;
    //initialize mutex
    int failure = pthread_mutex_init(&receive_mutex, NULL);
    if (failure)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message;
        error_message<<"Mutex for external communication cannot be created due to "<<failure;
        (*logging_instance).error_log(error_message.str().c_str());
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        //mutex cannot be created
        throw runtime_error("Mutex for external receiving cannot be created.");
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/*!\brief Deconstructor of "RecCommunicationServer"
 *
 * Closes the zeromq socket and the context for MDS to MDS communication
 */
RecCommunicationServer::~RecCommunicationServer()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    zmq_close(zmq_used_socket);
    zmq_term(zmq_context);
    //destroy mutex for external communication
    int failure = pthread_mutex_destroy(&receive_mutex);
#ifdef LOGGING_ENABLED
    if (failure)
    {
        stringstream warning_message2;
        warning_message2<<"Mutex for external communication cannot be destroyed properly due to "<<failure;
        (*logging_instance).warning_log(warning_message2.str().c_str());
    }
#endif
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
  * \brief Registers a module for receiving external messages
  *
  * \param module_id - identification of the module which wants to register for reception of external messages
  * \param module_message_buffer - reference to data structure in which the message for the module should be put
  *
  * Saves the reference to module_message_buffer if it is unequal to NULL and the module with module_id is allowed to receive external
  * messages. In all other cases nothing happens.
  */
void RecCommunicationServer::register_module(CommunicationModule module_id, ConcurrentQueue<ExternalMessage>* module_message_buffer)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    if (module_id>=0 && module_id<NUM_MODULES && module_message_buffer!=NULL)
    {
#ifdef LOGGING_ENABLED
        if (module_queues[module_id]!=NULL)
        {
            (*logging_instance).warning_log("Previous queue of this module will be overwritten.");
        }
#endif
        module_queues[module_id]=module_message_buffer;
    }
#ifdef LOGGING_ENABLED
    else
    {
        (*logging_instance).warning_log("Module number possibly higher than NUM_MODULES. Check if NUM_MODULE is identical with number of elements in enumeration Module ");
    }
    if (module_message_buffer==NULL)
    {
        (*logging_instance).warning_log("Queue is set to a null reference.");
    }
#endif
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}
