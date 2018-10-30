/**
 * \file SendCommunicationServer.cpp
 *
 * \author Marie-Christine Jakobs	Benjamin Raddatz
 *
 * \brief Realization of the SendCommunicationServer.h header file
 *
 * Implements the sending of external messages and the request part of the request reply pattern between e.g. load balancing module and Ganesha.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <error.h>
#include <zmq.hpp>

#include "global_types.h"
#include "communication.h"

#include "coco/communication/SendCommunicationServer.h"
#include "coco/communication/CommunicationFailureCodes.h"
#include "coco/coordination/MltHandler.h"

#include "pc2fsProfiler/Pc2fsProfiler.h"


using namespace std;

/**
 * Declaration and definition of class variable instance which represents the singleton instance.
 */
SendCommunicationServer SendCommunicationServer::instance = SendCommunicationServer();

/**
 *\brief Sends the message together with sending and receiving module identifiers to the specified metadata servers.
 *
 *\param message - message which will be sent to other metadata servers, structure of message is determined by higher level protocol, must be on globally accessible storage and must be freed by the calling instance afterwards
 *\param message_length - length of message in number of bytes
 *\param sending_module - module identifier of the module which wants to send the message
 *\param receiving_module - identifier of module which is the intended receiver
 *\param server_list - IP addresses of the metadata servers which should receive the message. If VARIABLE_PORTS defined unequal to 0 the IP address must include the port as well. In the other case the IP address alone is necessary. If the list is empty, a broadcast will be sent including all known metadata servers.
 *
 *\return 0 if sending was successful otherwise one of the failure codes (MutexFailed, NotEnoughMemory, UnableToCreateMessage,SendingFailed, SocketNotExisting) defined in CommunicationFailureCodes.h
 *
 * The message is sent iteratively to all specified servers if they exist. The sending and receiving module is add to the message before. If a
 * server does not exist or failure occurs while trying to send the message to the specified servers, sending is stopped and the respective
 * failure code is returned. If a socket is not existing or sending failed, the errno will return the position of the respective server in
 * server_list. When a broadcast is sent, meaning the server_list is empty, no indication to the respective socket can be given. At most one
 * send can be executed at a time to prevent failures due to concurrent access to the same socket. If another instance is sending the calling
 * instance will be blocked until it is allowed to enter the critical sending section.
 */
int SendCommunicationServer::asynch_send(void* message_data, int message_length, CommunicationModule sending_module, CommunicationModule receiving_module, vector<string>& server_list)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    Pc2fsProfiler::get_instance()->function_sleep();
    failure = pthread_mutex_lock( &external_send_mutex );
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
    void* sending_socket;
    zmq_msg_t message;
    // message will be message_length + 2 (two char representing receiving and sending module)
    int message_size = message_length+2;
    //set up message content
    void* data = malloc(message_size);
    if (data == NULL)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Cannot get enough memory to create message which will be sent.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return NotEnoughMemory;
    }
    //receiving module as character in front to avoid problems with different int sizes on different machines
    *((char*)data)=receiving_module;
    *(((char*)data)+1)=sending_module;
    memcpy(((char*)data)+2,message_data,message_length);
    if (server_list.size()>0)
    {
        // multicast or unicast
        for (int i=0;i<server_list.size();i++)
        {
            //create message never use init_data destroys data
            //zmq_msg_init_data(&message, data ,message_size, NULL, NULL);
            failure = zmq_msg_init_size(&message, message_size);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                (*logging_instance).error_log("Initializing message failed.");
#endif
                free(data);
                failure = pthread_mutex_unlock(&external_send_mutex);
#ifdef LOGGING_ENABLED
                if (failure)
                {
                    stringstream warning_message;
                    warning_message<<"Mutex cannot be unlocked due to "<<failure;
                    (*logging_instance).warning_log(warning_message.str().c_str());
                }
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return UnableToCreateMessage;
            }
            memcpy(zmq_msg_data(&message), data, message_size);

            //getting correct socket
            unordered_map<string,void*>::iterator it = connections.find(server_list[i]);
            if (it!=connections.end())
            {
                sending_socket = (*it).second;
                //send message
                do
                {
                    Pc2fsProfiler::get_instance()->function_sleep();
                    failure = zmq_send(sending_socket, &message,ZMQ_NOBLOCK);
                    Pc2fsProfiler::get_instance()->function_wakeup();
                }
                while (failure && errno == EINTR);
                if (failure)
                {
#ifdef LOGGING_ENABLED
                    stringstream error_message2;
                    error_message2<< "Sending to server "<<server_list[i]<<" failed due to error "<<errno;
                    (*logging_instance).error_log(error_message2.str().c_str());
#endif
                    //tidy up
                    zmq_msg_close(&message);
                    free(data);
                    failure = pthread_mutex_unlock(&external_send_mutex);
#ifdef LOGGING_ENABLED
                    if (failure)
                    {
                        stringstream warning_message2;
                        warning_message2<<"Mutex cannot be unlocked due to "<<failure;
                        (*logging_instance).warning_log(warning_message2.str().c_str());
                    }
#endif
                    errno = i;

                    // ends profiling
                    Pc2fsProfiler::get_instance()->function_end();

                    return SendingFailed;
                }
            }
            else
            {
#ifdef LOGGING_ENABLED
                stringstream error_message3;
                error_message3<<"Server "<<server_list[i]<<" does not exist.";
                (*logging_instance).error_log(error_message3.str().c_str());
#endif
                //tidy up
                zmq_msg_close(&message);
                free(data);
                pthread_mutex_unlock(&external_send_mutex);
#ifdef LOGGING_ENABLED
                if (failure)
                {
                    stringstream warning_message3;
                    warning_message3<<"Mutex cannot be unlocked due to "<<failure;
                    (*logging_instance).warning_log(warning_message3.str().c_str());
                }
#endif
                errno = i;
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return SocketNotExisting;
            }
        }
    }
    else
    {
        //broadcast
        unordered_map<string,void*>::iterator it = connections.begin();
        while (it!=connections.end())
        {
            //create message never use init_data destroys data
            failure = zmq_msg_init_size(&message, message_size);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                (*logging_instance).error_log("Initializing message failed.");
#endif
                free(data);
                failure = pthread_mutex_unlock(&external_send_mutex);
#ifdef LOGGING_ENABLED
                if (failure)
                {
                    stringstream warning_message4;
                    warning_message4<<"Mutex cannot be unlocked due to "<<failure;
                    (*logging_instance).warning_log(warning_message4.str().c_str());
                }
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return UnableToCreateMessage;
            }
            memcpy(zmq_msg_data(&message), data, message_size);
            //send message to next socket
            sending_socket = (*it).second;
            //send message
            do
            {
                Pc2fsProfiler::get_instance()->function_sleep();
                failure = zmq_send(sending_socket, &message,ZMQ_NOBLOCK);
                Pc2fsProfiler::get_instance()->function_wakeup();
            }
            while (failure && errno == EINTR);
            if (failure)
            {
#ifdef LOGGING_ENABLED
                stringstream error_message4;
                error_message4<<"Sending to server "<<(*it).first<<" failed due to error "<<errno;
                (*logging_instance).error_log(error_message4.str().c_str());
#endif
                //tidy up
                zmq_msg_close(&message);
                free(data);
                failure=pthread_mutex_unlock(&external_send_mutex);
#ifdef LOGGING_ENABLED
                if (failure)
                {
                    stringstream warning_message5;
                    warning_message5<<"Mutex cannot be unlocked due to "<<failure;
                    (*logging_instance).warning_log(warning_message5.str().c_str());
                }
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return SendingFailed;
            }
            //next socket
            it++;
        }
    }
//tidy up
    zmq_msg_close(&message);
    free(data);
    failure = pthread_mutex_unlock( &external_send_mutex );
#ifdef LOGGING_ENABLED
    if (failure)
    {
        stringstream warning_message6;
        warning_message6<<"Mutex cannot be unlocked due to "<<failure;
        (*logging_instance).warning_log(warning_message6.str().c_str());
    }
#endif
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

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
 *
 * Notice that you must free the data pointer of the returned RequestResult instance.
 */
RequestResult SendCommunicationServer::ganesha_send(void* message, int message_length)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    //set up message for request
    zmq_msg_t request;
    int failure = zmq_msg_init_size(&request, message_length);
#ifdef LOGGING_ENABLED
    (*logging_instance).error_log("Initializing message failed.");
#endif
    int receive_without_return_result=0;
    if (failure)
    {
        RequestResult result;
        result.data=NULL;
        result.length=0;
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();
        return result;
    }
    memcpy(zmq_msg_data(&request), message, message_length);
    // no two requests are allowed at the same time --> begin of critical section
    Pc2fsProfiler::get_instance()->function_sleep();
    failure = pthread_mutex_lock(&ganesha_mutex);
    Pc2fsProfiler::get_instance()->function_wakeup();
    if (failure)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Cannot lock Ganesha mutex.");
#endif
        //close request message
        zmq_msg_close(&request);
        failure = pthread_mutex_unlock(&ganesha_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message;
            warning_message<<"Mutex cannot be unlocked due to "<<failure;
            (*logging_instance).warning_log(warning_message.str().c_str());
        }
#endif
        RequestResult result;
        result.data=NULL;
        result.length=0;

        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return result;
    }
    //blocking send of message
    do
    {
        Pc2fsProfiler::get_instance()->function_sleep();
        failure = zmq_send(ganesha_socket, &request,0);
        Pc2fsProfiler::get_instance()->function_wakeup();
    }
    while (failure && errno==EINTR);
    //close request message
    zmq_msg_close(&request);
    //test for other failure of send
    if (failure)
    {
        if (errno==EFSM)
        {
#ifdef LOGGING_ENABLED
            (*logging_instance).warning_log("Send was wrong operation, a receive was expected.");
#endif
            receive_without_return_result=1;
        }
        else
        {
#ifdef LOGGING_ENABLED
            stringstream error_message;
            error_message<<"Sending to Ganesha failed due to error "<<errno;
            (*logging_instance).error_log(error_message.str().c_str());
#endif
            RequestResult result;
            result.data=NULL;
            result.length=0;
            //leave critical section
            failure = pthread_mutex_unlock(&ganesha_mutex);
#ifdef LOGGING_ENABLED
            if (failure)
            {
                stringstream warning_message2;
                warning_message2<<"Mutex cannot be unlocked due to "<<failure;
                (*logging_instance).warning_log(warning_message2.str().c_str());
            }
#endif
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return result;
        }
    }
    //set up message for reply
    zmq_msg_t reply;
    zmq_msg_init(&reply);
    //blocking receive of message
    do
    {
        Pc2fsProfiler::get_instance()->function_sleep();
        failure = zmq_recv(ganesha_socket, &reply, 0);
        Pc2fsProfiler::get_instance()->function_wakeup();
    }
    while (failure && errno==EINTR);
    if (failure || receive_without_return_result)
    {
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream error_message2;
            error_message2<<"Receiving from Ganesha failed due to error "<<errno;
            (*logging_instance).error_log(error_message2.str().c_str());
        }
#endif
        //close reply message
        zmq_msg_close(&reply);
        RequestResult result;
        result.data=NULL;
        result.length=0;
        //leave critical section
        failure = pthread_mutex_unlock(&ganesha_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message3;
            warning_message3<<"Mutex cannot be unlocked due to "+failure;
            (*logging_instance).warning_log(warning_message3.str().c_str());
        }
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return result;
    }
    //get message content
    RequestResult message_result;
    message_result.length=zmq_msg_size(&reply);
    message_result.data = malloc(message_result.length);
    if (message_result.data == NULL)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Cannot get enough memory to create message which will be sent.");
#endif
        message_result.length = 0;
    }
    else
    {
        memcpy(message_result.data, zmq_msg_data(&reply), message_result.length);
    }
    //close message
    zmq_msg_close(&reply);
    //leave critical section
    failure = pthread_mutex_unlock(&ganesha_mutex);
#ifdef LOGGING_ENABLED
    if (failure)
    {
        stringstream warning_message4;
        warning_message4<<"Mutex cannot be unlocked due to "<<failure;
        (*logging_instance).warning_log(warning_message4.str().c_str());
    }
#endif

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return message_result;
}

/**
 *\brief Sets up everything needed for communication with Ganesha.
 *
 *\return 0 if the execution was not successful otherwise a value unequal to 0.
 *
 * Creates ganesha_socket and connects it. If the socket creation or the binding fails ganesha_socket is set to NULL.
 */
int SendCommunicationServer::initialize_ganesha_communication()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    //initializing mutex

    failure = pthread_mutex_init(&ganesha_mutex, NULL);
    if (failure)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message;
        error_message<<"Mutex for ganesha communication cannot be created due to "<<failure;
        (*logging_instance).error_log(error_message.str().c_str());
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        throw runtime_error("Mutex for communication with Ganesha cannot be created.");
    }
    //creating socket
    ganesha_socket = zmq_socket(zmq_context, ZMQ_REQ);
    if (ganesha_socket == NULL)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Initializing Ganesha socket failed.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return 0;
    }
    //connecting to ganesha server
    failure = zmq_connect(ganesha_socket,MDS_TO_GANESHA_LB_INTERFACE);
    if (failure)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Connecting Ganesha socket failed");
#endif
        zmq_close(ganesha_socket);
        ganesha_socket = NULL;
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return !failure;
}

/**
 *\brief Tidy up all Ganesha communication related objects.
 *
 *\return 0 if the execution was successful otherwise a value unequal to 0
 *
 * Closes the ganesha_socket.
 */
int SendCommunicationServer::delete_ganesha_communication()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    //close socket, no failure expected so ignore possible failures
    int failure = zmq_close(ganesha_socket);
#ifdef LOGGING_ENABLED
    if (failure)
    {
        stringstream warning_message;
        warning_message<<"Cannot close ganesha socket due to "<<failure;
        (*logging_instance).warning_log(warning_message.str().c_str());
    }
#endif
    int failure2 = pthread_mutex_destroy(&ganesha_mutex);
#ifdef LOGGING_ENABLED
    if (failure2)
    {
        stringstream warning_message2;
        warning_message2<<"Mutex for Ganesha communication cannot be destroyed properly due to "<<failure2;
        (*logging_instance).warning_log(warning_message2.str().c_str());
    }
#endif

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return failure || failure2;
}

/**
 *\brief Adds a sever socket which can send messages to the specified address.
 *
 *\param ip_address - ip address of the server. If VARIABLE_PORTS defined unequal to 0 the IP address must include the port as well. In the other case the IP address alone is necessary
 *
 *\return 0 if removing was successful, otherwise one of the failure codes defined in CommunicationFailureCodes.h
 *
 * Creates a new zmq socket for sending messages to the specified ip_address. The newly created socket is saved in the respective datastructure
 * and can be used for sending messages afterwards.
 */
int SendCommunicationServer::add_server_send_socket(string ip_address)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    Pc2fsProfiler::get_instance()->function_sleep();
    failure = pthread_mutex_lock( &external_send_mutex );
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
    //check if socket already existing
    unordered_map<string,void*>::iterator it = connections.find(ip_address);
    if (it!=connections.end())
    {
        failure = pthread_mutex_unlock( &external_send_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message;
            warning_message<<"Mutex cannot be unlocked due to "<<failure;
            (*logging_instance).warning_log(warning_message.str().c_str());
        }
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return SocketAlreadyExisting;
    }
    // create new socket
    void* temp_socket = zmq_socket(zmq_context, ZMQ_XREQ);
    if (temp_socket == NULL)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Creating socket failed.");
#endif
        failure = pthread_mutex_unlock( &external_send_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message2;
            warning_message2<<"Mutex cannot be unlocked due to "<<failure;
            (*logging_instance).warning_log(warning_message2.str().c_str());
        }
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return SocketCreationFailed;
    }
    const char* temp_server_id = ip_address.c_str();
    //try to set identity if it was interrupted retry
    do
    {
        failure = zmq_setsockopt(temp_socket, ZMQ_IDENTITY, temp_server_id, strlen(temp_server_id)+1);
    }
    while (failure && errno==EINTR);
    //if another unexpected failure occured
    if (failure)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message;
        error_message<<"Identity of sending socket for server "<<temp_server_id<<"cannot be set.";
        (*logging_instance).error_log(error_message.str().c_str());
#endif
        zmq_close(temp_socket);
        failure = pthread_mutex_unlock(&external_send_mutex);
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
    //setting full server address
    string server_address = "tcp://";
    server_address.append(ip_address);
    //add port number if not part of address yet (no variable_ports)
    if (!VARIABLE_PORTS)
    {
        server_address.append(PORT_NUMBER);
    }
    //connecting socket
    failure = zmq_connect(temp_socket,server_address.c_str());
    // if failure occurs close socket and go to beginning of loop
    if (failure)
    {
#ifdef LOGGING_ENABLED
        stringstream error_message2;
        error_message2<<"Socket for server "<<temp_server_id<<"cannot be connected.";
        (*logging_instance).error_log(error_message2.str().c_str());
#endif
        zmq_close(temp_socket);
        failure = pthread_mutex_unlock(&external_send_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message4;
            warning_message4<<"Mutex cannot be unlocked due to "<<failure;
            (*logging_instance).warning_log(warning_message4.str().c_str());
        }
#endif
    }
    // saving connection
    connections.insert(unordered_map<string,void*>::value_type(ip_address,temp_socket));
    failure = pthread_mutex_unlock(&external_send_mutex);
#ifdef LOGGING_ENABLED
    if (failure)
    {
        stringstream warning_message5;
        warning_message5<<"Mutex cannot be unlocked due to "<<failure;
        (*logging_instance).warning_log(warning_message5.str().c_str());
    }
#endif

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 *\brief Will remove the specified server socket if it exists.
 *
 *\param ip_address - ip address of the server. If VARIABLE_PORTS defined unequal to 0 the IP address must include the port as well. In the other case the IP address alone is necessary
 *
 *\return 0 if removing was successful, otherwise one of the failure codes defined in CommunicationFailureCodes.h
 *
 * Checks if a socket with the specified ip_address is in the respective datastructure. If this is the case the socket is closed and
 * removed from the respective datastructure.
 */
int SendCommunicationServer::delete_server_send_socket(string ip_address)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    Pc2fsProfiler::get_instance()->function_sleep();
    failure = pthread_mutex_lock( &external_send_mutex );
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
    // Check if such a socket is known
    unordered_map<string,void*>::iterator it = connections.find(ip_address);
    if (it!=connections.end())
    {
        //close socket
        failure = zmq_close((*it).second);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message;
            warning_message<<"Cannot close socket due to "<<failure;
            (*logging_instance).warning_log(warning_message.str().c_str());
        }
#endif
        // remove from datastructure
        connections.erase(it);
    }
    else // socket unknown
    {
        failure = pthread_mutex_unlock(&external_send_mutex);
#ifdef LOGGING_ENABLED
        if (failure)
        {
            stringstream warning_message2;
            warning_message2<<"Mutex cannot be unlocked due to "<<failure;
            (*logging_instance).warning_log(warning_message2.str().c_str());
        }
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return SocketNotExisting;
    }

    failure = pthread_mutex_unlock(&external_send_mutex);
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

/**
 *\brief Reads the IP addresses other metadata servers from a file.
 *
 *\return List of IP addresses of other metadata servers
 *
 * Reads the IP addresses from file IP_SYMBOLIC_FILE. If the file cannot be found or opened a runtime_error is thrown. Otherwise the IP addresses
 * which are terminated by whitespace character are read from the file and put into the vector. If all IP addresses are read the file is closed
 * and the vector is returned. If VARIABLE_PORTS is defined unequal to 0 the IP addresses must contain the port (ip_address:port) otherwise
 * ip_address is required.
 *
 * Notice that for correct reading from file every IP address represented by a string must be terminated by a whitespace character.
 *
 *\throw runtime_error reading from file failed
 */
vector<string> SendCommunicationServer::get_ip_addresses()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    ifstream input;
    vector<string> server_list;
    string current_address;
    //open file, if file could not be opened an exception is thrown and not catched
    string ip_config_path(DEFAULT_CONF_DIR);
    ip_config_path.append(DELIM);
    ip_config_path.append(IP_SYMBOLIC_FILE);
    input.open(ip_config_path);
    if (input.fail())
    {
        try
        {
            input.close();
        }
        catch (exception e)
        {
            //do nothing
        }
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("File could not be opened. Possibly opened by someone else or it does not exist.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        throw runtime_error("File with IP addresses cannot be opened.");
    }
    // for correct functionality ip addresses must be terminated by a whitespace character
    while (!input.eof())
    {
        //read next ip address
        input>>current_address;
        //only add if reading was possible
        if (!input.fail() )
        {
            // copy into vector
            server_list.push_back(current_address);
        }
#ifdef LOGGING_ENABLED
        else
        {
            (*logging_instance).warning_log("Reading next string from file failed.");
        }
#endif
    }
    //tidy up
    try
    {
        input.close();
        input.clear();
    }
    catch (exception e)
    {
        //possible just do nothing here
#ifdef LOGGING_ENABLED
        stringstream warning_message;
        warning_message<<"Closing file failed."<<e.what();
        (*logging_instance).warning_log(warning_message.str().c_str());
#endif
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return server_list;
}


/**
 *\brief Creates the sockets for sending external messages to other metadata servers and connects them.
 *
 *\return 0 if the method was executed without any failure, an error code if the mutex cannot be acquired, a socket could not be created or connected properly or the information got from the mlt is incomplete.
 *
 * Receives the IP addresses of the other metadata servers using the MltHandler to access the mlt which knows the data. Afterwards a socket
 * of type ZMQ_XREQ (ZMQ_DEALER) is created per IP address, its identitiy is set to this sever's IP address and it is connected. Different
 * metadata servers are connected by a tcp connection. If creation, identity setting or connecting failed, the socket is not saved in the
 * respective hashmap and the failure is indicated in the return value of the method.
 *
 * Notice that the number of possible sockets is limited in zmq. The maximal number of sockets can be changed in config.hpp by changing
 * max_sockets in the zero message queue implementation. Afterwards the library must be recompiled. For this method it is expected that the
 * maximum number of sockets is greater than the number of other metadata servers. So for every other metadata server an own socket can be
 * created and maintained during the execution of the metadata server.
 */
int SendCommunicationServer::connect()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int success = 0;
    int submethod_failure;
    int failure;
    Pc2fsProfiler::get_instance()->function_sleep();
    failure = pthread_mutex_lock( &external_send_mutex );
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
    //get list of IP addresses of available MDS through the mlt handler
    vector<Server> server_list;
    MltHandler& mlt_handler = MltHandler::get_instance();
    // get server list from mlt
    if (mlt_handler.get_server_list(server_list))
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Mlt not available.");
#endif

        failure = pthread_mutex_unlock(&external_send_mutex);
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
    // check if at least one server is contained
    if (server_list.size()<1)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Too few servers saved in mlt.");
#endif
        failure = pthread_mutex_unlock(&external_send_mutex);
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
        failure = pthread_mutex_unlock(&external_send_mutex);
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
    string server_address_string = asyn_tcp_server.address;
    // add port to own address if part of address,
    if (VARIABLE_PORTS)
    {
        stringstream my_port;
        my_port<<":"<<asyn_tcp_server.port;
        server_address_string.append(my_port.str());
    }

    const char* server_id = server_address_string.c_str();

    void* temp_socket;
    string sending_address;
    for (int i=0;i<server_list.size();i++)
    {
        //creating socket
        temp_socket = zmq_socket(zmq_context, ZMQ_XREQ);
        if (temp_socket == NULL)
        {
#ifdef LOGGING_ENABLED
            (*logging_instance).error_log("Creating socket failed.");
#endif
            success = SocketCreationFailed;
            // if socket creation failed go to beginning of for loop
            continue;
        }

        // setting server identity
        do
        {
            submethod_failure = zmq_setsockopt(temp_socket, ZMQ_IDENTITY, server_id, strlen(server_id)+1);
        }
        while (submethod_failure && errno==EINTR);
        //if another unexpected failure occured set success to false
        if (submethod_failure)
        {
#ifdef LOGGING_ENABLED
            stringstream error_message;
            error_message<<"Identity of sending socket for server "<<server_id<<"cannot be set.";
            (*logging_instance).error_log(error_message.str().c_str());
#endif
            zmq_close(temp_socket);
            success = SocketCreationFailed;
            continue;
        }
        //setting full server address
        sending_address = string("tcp://");
        sending_address.append(server_list[i].address);
        if (!VARIABLE_PORTS)
        {
            sending_address.append(PORT_NUMBER);
        }
        else
        {
            sending_address.append(":");
            stringstream out;
            out << server_list[i].port;
            sending_address.append(out.str());
        }
        //connecting socket
        submethod_failure = zmq_connect(temp_socket,sending_address.c_str());
        // if failure occurs close socket and go to beginning of loop
        if (submethod_failure)
        {

#ifdef LOGGING_ENABLED
            stringstream error_message2;
            error_message2<<"Socket for server "<<server_address<<"cannot be connected.";
            (*logging_instance).error_log(error_message2.str().c_str());
#endif
            zmq_close(temp_socket);
            success = SocketCreationFailed;
            continue;
        }
        // saving connection
        if (!VARIABLE_PORTS)
        {
            connections.insert(unordered_map<string,void*>::value_type(server_list[i].address, temp_socket));
        }
        else
        {
            std::cout<<sending_address;
            connections.insert(unordered_map<string,void*>::value_type(sending_address,temp_socket));
        }
    }
    failure = pthread_mutex_unlock(&external_send_mutex);
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

    return success;
}

/**
 *\brief Removes all existing external sending sockets and creates the new external sending socket specified currently in the Mlt.
 *
 *\return 0 if successful otherwise a failure code.
 *
 * First removes all existing external sending sockets, meaning that they are closed and removed from the datastructure. Then the new sockets
 * specified in the Mlt are created, connected and saved in the respective datastructure.
 */
int SendCommunicationServer::set_up_external_sockets()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int success = 0;
    // external sockets already existing
    if (not_first_send_init)
    {
        success = deconnect();
        if (success)
        {
            // ends profiling
            Pc2fsProfiler::get_instance()->function_end();

            return SocketDecreationFailed ;
        }
        Pc2fsProfiler::get_instance()->function_sleep();
        sleep(1);
        Pc2fsProfiler::get_instance()->function_wakeup();
    }
    else
    {
        not_first_send_init = true;
    }
    //set up sockets for external communication
    success = connect();
    if (success)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return SocketCreationFailed;
    }
    Pc2fsProfiler::get_instance()->function_end();
    return success;
}

/**
 *\brief Closes all sockets for external communication
 *
 *\return 0 if successful otherwise a failure code
 *
 * Closes all sockets saved in connections and removes their reference from connections.
 */
int SendCommunicationServer::deconnect()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    Pc2fsProfiler::get_instance()->function_sleep();
    failure = pthread_mutex_lock( &external_send_mutex );
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
    int success = 0;
    //closing all open sockets for external communication
    unordered_map<string,void*>::const_iterator it =  connections.begin();
    while (it!=connections.end())
    {
        //no failure expected, method, failure ignored
        failure = zmq_close((*it).second);
        if (failure)
        {
            success--;
#ifdef LOGGING_ENABLED
            stringstream warning_message;
            warning_message<<"Cannot close socket due to "<<failure;
            (*logging_instance).warning_log(warning_message.str().c_str());
#endif
        }
        //erase from datastructure
        it = connections.erase(it);
    }
    failure = pthread_mutex_unlock(&external_send_mutex);
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

    return success;
}

/**
 * \brief Constructor of SendCommunicationServer. Initializes object variables.
 *
 * Creates the zmq context and the sockets for sending external messages, the socket for communication with Ganesha. A runtime_error is thrown
 * if the context cannot be intialized, the creation of sockets for external communication using the connect method failed or the socket for
 * Ganesha communication could not be created.
 *
 *\throw runtime_error if creation of a socket or the context failed
 */
SendCommunicationServer::SendCommunicationServer()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    //set up logging instance
#ifdef LOGGING_ENABLED
    logging_instance = CommunicationLogger::get_instance();
#endif
    failure = pthread_mutex_init(&external_send_mutex, NULL);
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
        throw runtime_error("Mutex for external sending cannot be created.");
    }
    zmq_context = zmq_init(1);
    //check if context was initialized properly
    if (zmq_context == NULL)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("ZMQ context cannot be set up.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        throw runtime_error("ZMQ context could not be initialized.");
    }
    // init variable
    not_first_send_init = false;
    //set up socket for communication with ganesha
    int success = initialize_ganesha_communication();
    if (!success)
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        //TODO do something reasonable
        throw runtime_error("Problem with creation of socket for ganesha communication.");
    }
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
 * \brief Destructor of SendCommunicationServer
 *
 * Cleans up,closing connection to other metadata servers, ganesha and terminates zmq context properly.
 */
SendCommunicationServer::~SendCommunicationServer()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int failure;
    // close sockete for external communication
    failure = deconnect();
    //destroy mutex for external communication
    failure = pthread_mutex_destroy(&external_send_mutex);
#ifdef LOGGING_ENABLED
    if (failure)
    {
        stringstream warning_message2;
        warning_message2<<"Mutex for external communication cannot be destroyed properly due to "<<failure;
        (*logging_instance).warning_log(warning_message2.str().c_str());
    }
#endif

    //close socket for communication with ganesha
    delete_ganesha_communication();
    //try to terminate context if it was interrupted retry
    do
    {
        failure = zmq_term(zmq_context);
    }
    while (failure && errno==EINTR);
#ifdef LOGGING_ENABLED
    if (failure)
    {
        stringstream warning_message3;
        warning_message3<<"ZMQ context cannot be destroyed properly due to "<<failure;
        (*logging_instance).warning_log(warning_message3.str().c_str());
    }
#endif
    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}
