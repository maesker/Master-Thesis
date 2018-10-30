/**
 * \file RecCommunicationServer.h
 *
 * \author Marie-Christine Jakobs 	Benjamin Raddatz
 *
 * \brief Header for ReCommunicationServer.cpp. Contains description of class RecCommunicationServer which is responsible for reception of messages from other metadata servers.
 */
#ifndef RECCOMMUNICATIONSERVER_H_
#define RECCOMMUNICATIONSERVER_H_

#include <string>
#include "ConcurrentQueue.h"
#include "CommonCommunicationTypes.h"
#include "CommunicationLogger.h"
#include <pthread.h>

/**
 * \brief RecCommunicationServer realizes low level reception of messages sent by other metadata servers.
 *
 * Class realizes a singleton pattern. Only one instance of the class will be available.
 */
class RecCommunicationServer
{
public:
    /**
     * \brief Destructor of RecCommunicationServer
     *
     * Cleans up,closing connection to other metadata servers and terminates zmq context properly.
     */
    ~RecCommunicationServer();
    /**
     * \brief Registers a module for receiving external messages
     *
     * \param module_id - identification of the module which wants to register for reception of external messages
     * \param module_message_buffer - reference to data structure in which the message for the module should be put
     *
     * Saves the reference to module_message_buffer if it is unequal to NULL and the module with module_id is allowed to receive external
     * messages. In all other cases nothing happens.
     */
    void register_module(CommunicationModule module_id, ConcurrentQueue<ExternalMessage>* module_message_buffer);

    /**
     * \brief Waits for incoming messages and forwards them to registered modules.
     *
     * Blocks until a message can be received. Then the message is analyzed. If the receiving module is known and registered, the message is
     * put into the respective data structure of the module. In any other case the message is dropped. Afterwards the previous course of
     * actions is repeated. This method is expected to run in a separat thread.
     *
     * \param ptr - needed to use in pthread_t thread, not used, can be called was NULL argument
     * \return Executes an endless loop, nothing returned, only needed to be pthread compliant
     */
    void receive();

    /**
     *\brief Closes and creates the new external sending socket specified currently in the Mlt.
     *
     *\return 0 if successful otherwise a failure code.
     *
     * First will close receive socket if it exists. Then the new receive socket is created and connected.
     */
    int set_up_receive_socket();

    /*!\brief Returns reference to the receive communication server.
     *
     * This method is needed to get a reference to the singleton instance.
     */
    static RecCommunicationServer& get_instance()
    {
        return instance;
    }
private:
    /**
     * Singleton instance of RecCommunicationServer.
     */
    static RecCommunicationServer instance;
    /**
     * Reference to zero message queue context which is needed to create the socket and receive message. It is initialized in the constructor
     * and terminated in the deconstructor.
     */
    void * zmq_context;
    /**
     * Reference to the socket used for receiving. The socket is of type ZMQ_XREP (ZMQ_ROUTER). Set to NULL in the constructor. It is created
     * in <code>set_up_receive_socket</code> and closed  in the deconstructor.
     */
    void * zmq_used_socket;
    /**
     * Array of references to the respective queues of the modules which are expected to receive external messages. Initially the references
     * are set to NULL. The correct reference must be set with register_module.
     */
    ConcurrentQueue<ExternalMessage>* module_queues[NUM_MODULES];
    /**
     * Binary semaphor which is used to ensure that either the receive socket is changed or a message is received. This is necessary to prohibit
     * concurrent access to the socket which may cause problems.
     */
    pthread_mutex_t receive_mutex;
    /**
     * Pointer to instance which is used for logging errors, warning, etc. during execution.
     */
#ifdef LOGGING_ENABLED
    Logger* logging_instance;
#endif
    /**
     * \brief Constructor of RecCommunicationServer. Initializes object variables.
     *
     * Creates the zmq context and the socket for receiving external messages.
     */
    RecCommunicationServer();
    /**
     * \brief Copy constructor of RecCommunicationServer
     *
     * \param server - RecCommunicationServer whose values are copied into the new object
     *
     * Private copy constructor to ensure that no other class can use it. It is not used inside the class to ensure that only one single
     * instance of the class is available.
     */
    RecCommunicationServer(const RecCommunicationServer& asyn_tcp_server);
};

#endif //#ifndef RECCOMMUNICATIONSERVER_H_