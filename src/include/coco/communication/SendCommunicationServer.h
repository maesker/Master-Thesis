/**
 * \file SendCommunicationServer.h
 *
 * \author Marie-Christine Jakobs 	Benjamin Raddatz
 *
 * \brief Header for SendCommunicationServer.cpp. Contains description of class SendCommunicationServer which is responsible for sending messages to other metadata servers and requests from e.g. the load balancing module to Ganesha.
 */
#ifndef SENDCOMMUNICATIONSERVER_H_
#define SENDCOMMUNICATIONSERVER_H_

#include <unordered_map>
#include <vector>
#include <pthread.h>
#include "CommonCommunicationTypes.h"
#include "CommonGaneshaCommunicationTypes.h"
#include "CommunicationLogger.h"

using namespace std;

/**
 * \brief SendCommunicationServer realizes low level sending of messages to other metadata servers.
 *
 * Class realizes a singleton pattern. Only one instance of the class will be available.
 */
class SendCommunicationServer
{
public:
    /**
     * \brief Destructor of SendCommunicationServer
     *
     * Cleans up,closing connection to other metadata servers, ganesha and terminates zmq context properly.
     */
    ~SendCommunicationServer();

    /*!\brief Returns an instance of "SendCommunicationServer"
     *
     * This method is needed to get a reference to the singleton instance.
     */
    static SendCommunicationServer& get_instance()
    {
        return instance;
    }
    /**
     *\brief Sends the message together with sending and receiving module identifiers to the specified metadata servers.
     *
     *\param message - message which will be sent to other metadata servers, structure of message is determined by higher level protocol, must be on globally accessible storage and must be freed by the calling instance afterwards
     *\param message_length - length of message in number of bytes
     *\param sending_module - module identifier of the module which wants to send the message
     *\param receiving_module - identifier of module which is the intended receiver
     *\param server_list - IP addresses of the metadata servers which should receive the message. If the list is empty, a broadcast will be sent including all known metadata servers.
     *
     *\return 0 if sending was successful otherwise one of the failure codes defined in CommunicationFailureCodes.h
     *
     * The message is sent iteratively to all specified servers if they exist. The sending and receiving module is add to the message. If a
     * server does not exist or failure occurs while trying to send the message to the specified servers, sending is stopped and the respective
     * failure code is returned. If a socket is not existing or sending failed, the errno will return the position of the respective server in
     * server_list. When a broadcast is sent, meaning the server_list is empty, no indication to the respective socket can be given. At
     * most one send can be executed at a time to prevent failures due to concurrent access to the same socket.
     */
    int asynch_send(void* message, int message_length, CommunicationModule sending_module, CommunicationModule receiving_module, vector<string>& server_list);

    /**
     *\brief Sends a request to Ganesha, waits for the response and returns the result.
     *
     *\param message - message which will be sent to Ganesha, structure of message is determined by higher level protocol
     *\param message_length - length of message in number of bytes
     *
     *\return If the operation was successful, ReqeustResult contains the replied data and the length. In failure case the replied data points to NULL.
     *
     * Realizes the request reply pattern for communication. At most one instance at a time is allowed to communicate with Ganesha using this
     * method. First a blocking request is sent to Ganesha. Thereafter it is blocked until a reply is received. Afterwards the result is
     * returned and another request can be sent.
     */
    RequestResult ganesha_send(void* message, int message_length);

    /**
     *\brief Adds a sever socket which can send messages to the specified address.
     *
     *\param ip_address - ip address of the server
     *
     *\return 0 if removing was successful, otherwise one of the failure codes defined in CommunicationFailureCodes.h
     *
     * Creates a new zmq socket for sending messages to the specified ip_address. The newly created socket is saved in the respective datastructure
     * and can be used for sending messages afterwards.
     */
    int add_server_send_socket(string ip_address);

    /**
     *\brief Will remove the specified server socket if it exists.
     *
     *\param ip_address - ip address of the server
     *
     *\return 0 if removing was successful, otherwise one of the failure codes defined in CommunicationFailureCodes.h
     *
     * Checks if a socket with the specified ip_address is in the respective datastructure. If this is the case the socket is closed and
     * removed from the respective datastructure.
     */
    int delete_server_send_socket(string ip_address);

    /**
    *\brief Removes all existing external sending sockets and creates the new external sending socket specified currently in the Mlt.
    *
    *\return 0 if successful otherwise a failure code.
    *
    * First removes all existing external sending sockets, meaning that they are closed and removed from the datastructure. Then the new sockets
    * specified in the Mlt are created, connected and saved in the respective datastructure.
    */
    int set_up_external_sockets();

private:
    /**
     * Singleton instance of SendCommunicationServer.
     */
    static SendCommunicationServer instance;
    /**
     * Reference to zero message queue context which is needed to create the socket and send messages. It is initialized in the constructor
     * and terminated in the deconstructor.
     */
    void * zmq_context;
    /**
     * Reference to the socket which is used for sending to Ganesha. The socket is of type ZMQ_REQ. It is created in the constructor and closed
     * in the deconstructor. Communication with Ganesha is realized by inter process communication using a shared file. If the socket creation
     * failed the pointer is set to NULL.
     */
    void* ganesha_socket;
    /**
     * Hashmap which contains a socket for each metadata server in the metadata cluster. The sockets are set up in
     * <code>set_up_external_sockets</code> and closed in the deconstructor. A socket is identified by its IP address and will be looked up in
     * the asynch_send method.
     */
    std::unordered_map<string, void*> connections;
    /**
     * Binary semaphor which is used to ensure that only one asynch_send operation can be performed at a time. This is needed to prohibit
     * concurrent access to the sockets which may cause problems.
     */
    pthread_mutex_t external_send_mutex;
    /**
     * Binary semaphor which is used to ensure that only one ganesha_send operation can be performed at a time. This is needed to prohibit
     * concurrent access to the ganesha_socket. Concurrent access causes problems because the ordering of send request and following receive
     * reply can be disturbed.
     */
    pthread_mutex_t ganesha_mutex;
    /**
     * Remembers if external send sockets were already created and therefore need to be closed if <code>set_up_external_sockets</code> is
     * called again. Initialized with false.
     */
    bool not_first_send_init;
    /**
     * Pointer to instance which is used for logging errors, warning, etc. during execution.
     */
#ifdef LOGGING_ENABLED
    Logger* logging_instance;
#endif
    /**
     * \brief Constructor of SendCommunicationServer. Initializes object variables.
     *
     * Creates the zmq context and the socket for communication with Ganesha.
     */
    SendCommunicationServer();
    /**
     * \brief Copy constructor of SendCommunicationServer
     *
     * \param server - SendCommunicationServer whose values are copied into the new object
     *
     * Private copy constructor to ensure that no other class can use it. It is not used inside the class to ensure that only one single
     * instance of the class is available.
     */
    SendCommunicationServer(const SendCommunicationServer& asyn_tcp_server);
    /**
     *\brief Reads the IP addresses other metadata servers from a file.
     *
     *\return List of IP addresses of other metadata servers
     */
    vector<string> get_ip_addresses();
    /**
     *\brief Creates the sockets for sending external messages to other metadata servers and connects them.
     *
     *\return 0 if the method was executed without any failure, an error code if a failure occurred.
     */
    int connect();
    /**
     *\brief Closes all sockets for external communication
     *
     *\return 0 if successful otherwise a failure code
     *
     * * Closes all sockets saved in connections and removes their reference from connections.
     */
    int deconnect();
    /**
     *\brief Sets up everything needed for communication with Ganesha.
     *
     *\return 0 if the execution was successful otherwise a value unequal to 0.
     *
     * Creates ganesha_socket and connects it.
     */
    int initialize_ganesha_communication();
    /**
     *\brief Tidy up all Ganesha communication related objects.
     *
     *\return 0 if the execution was successful otherwise a value unequal to 0
     *
     * Closes the ganesha_socket.
     */
    int delete_ganesha_communication();
};

#endif //#ifndef SENDCOMMUNICATIONSERVER_H_
