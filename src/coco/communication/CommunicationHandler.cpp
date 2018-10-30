#include "pc2fsProfiler/Pc2fsProfiler.h"

#include "coco/communication/CommunicationHandler.h"
#include "coco/communication/CommunicationFailureCodes.h"

#include <cstring>
#include <iostream>
#include <sstream>

CommunicationHandler::CommunicationHandler()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    //set up logging instance
#ifdef LOGGING_ENABLED
    logging_instance = CommunicationLogger::get_instance();
#endif
    send_server = &(SendCommunicationServer::get_instance());
    recv_server = &(RecCommunicationServer::get_instance());

    executed = false;

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

CommunicationHandler::~CommunicationHandler()
{

}

/**
 * Initializing the communication, register with the receiving server.
 * @param module_type Type of the module.
 * @return 0 - now known possible errors here
 */
int CommunicationHandler::init(CommunicationModule module_type)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    this->module_type = module_type;
    recv_server->register_module(module_type, &raw_queue);

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 * Send a message
 * @param message_data Pointer to the message.
 * @param message_length Length of the message.
 * @param receiving_module The target module.
 * @param server_list The target servers.
 * @return 0 - send was successful; otherwise error code
 */
int CommunicationHandler::send(void* message_data, int32_t message_length,
                               CommunicationModule receiving_module, vector<string>& server_list)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    void* extended_data;
    int32_t extended_length;
    int32_t rtnValue;

    extended_length = message_length + sizeof(uint64_t);

    // allocate memory for data, message_id and message type
    extended_data = malloc(extended_length);
    if (extended_data == NULL)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Cannot get enough memory to create message which will be sent.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return NotEnoughMemory;
    }

    // assign message id=0 => normal send
    *((uint64_t*) extended_data) = 0;

    memcpy(((char*) extended_data) + sizeof(uint64_t), message_data,
           message_length);

    Pc2fsProfiler::get_instance()->function_sleep();
    // send the message
    rtnValue = send_server->asynch_send(extended_data, extended_length,
                                        module_type, receiving_module, server_list);
    Pc2fsProfiler::get_instance()->function_wakeup();
#ifdef LOGGING_ENABLED
    if (rtnValue != 0)
    {
        (*logging_instance).warning_log("Sending was not successful.");
    }
#endif

    // free sent message
    free(extended_data);

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return rtnValue;
}

/**
 * Send a request to the desired servers.
 * An unique id will be generated to map the corresponding reply.
 * This id will be stored in the message map, after the request, the caller must call receive_reply
 * and register a buffer using the unique id.
 * @param message_id Pointer to the message_id, here the message_id will be written.
 * @param message_data Pointer to the message.
 * @param messege_length Length of the message.
 * @param receiving_module The receiving module.
 * @param server_list Target list.
 * @param message_list buffer for the messages, DO NOT ACCESS till recieve succeeded
 * @return 0 - request was successful; otherwise error code
 */
int32_t CommunicationHandler::request_reply(uint64_t* message_id,
        void* message_data, int32_t message_length, CommunicationModule receiving_module,
        vector<string>& server_list, vector<ExtractedMessage>& message_list)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    MessMapEntry* msg_entry;

    void* extended_data;
    int32_t extended_length;
    int32_t rtnValue;

    // First generate a message id which is unequal to 0 and is not already in the message map set.
    uint64_t msg_id;
    do
    {
        msg_id = id_generator.get_next_id();
    }
    while (msg_id == 0 || message_map.find_entry(msg_id) != NULL);

    msg_entry = new MessMapEntry();
    msg_entry->set_expected_msgs(server_list.size());
    msg_entry->set_msg_list(&message_list);
    msg_entry->set_assigned(true);
    message_map.add_message(msg_id, msg_entry);

    // assign the id and work with the value msg_id, not with the pointer message_id
    *message_id = msg_id;

    extended_length = message_length + sizeof(int32_t) + sizeof(uint64_t);

    // allocate memory for data, message_id and message type
    extended_data = malloc(extended_length);
    if (extended_data == NULL)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Cannot get enough memory to create message which will be sent.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return NotEnoughMemory;
    }

    // assign message id and message type to the data
    *((uint64_t*) extended_data) = msg_id;
    *((int32_t*) (((uint64_t*) extended_data) + 1)) = REQUEST;

    // create a copy the message
    memcpy(((char*) extended_data) + sizeof(int32_t) + sizeof(uint64_t),
           message_data, message_length);

    // send the message
    rtnValue = send_server->asynch_send(extended_data, extended_length,
                                        module_type, receiving_module, server_list);

    // if send was not successful, remove the message entry
    if ( rtnValue != 0)
    {
        message_map.remove_message(msg_id);
        garbage_queue.push(msg_entry);
#ifdef LOGGING_ENABLED

        (*logging_instance).warning_log("Sending was not successful.");
#endif
    }

    // free sent message
    free(extended_data);

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return rtnValue;
}

/**
 * Receive a reply.
 * This function must be called after request_reply, with the given message_id from the request_reply method.
 * This function blocks until the reply arrives or the timeout exceeds.
 * The reply messages are stored as external messages objects in the vector, that was that on the request_reply call.
 * The allocated data in the external message object must be free by the receiving module!
 * @param message_id The id to the corresponding message.
 * @param timeout The timeout in milliseconds.
 * @return 0 - everything was fine; otherwise a timeout error code;
 */

int32_t CommunicationHandler::receive_reply(uint64_t message_id,
        int32_t timeout)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    MessMapEntry* msg_entry;
    int32_t rtnValue = 0;

    // Check if the received message id is known.
    if ((msg_entry = message_map.find_entry(message_id)) == NULL)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).warning_log("Either the message id never existed or possible message were removed by the gargabe collector.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return UnknownMessageId;
    }

    // check if the message entry was already marked by the garbage collection.
    if (msg_entry->is_garbage())
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).warning_log("Message id exists, but already marked by the garbage collection.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return UnknownMessageId;
    }

    // check if messages already arrived
    if (msg_entry->get_expected_msgs() > (int32_t) msg_entry->get_msg_list()->size())
    {

        // wait until message arrives or timeout exceeds
        // if returns true, reply received
        if (msg_entry->wait_for_message(timeout))
        {
            rtnValue = 0;
        }
        // timeout
        else
        {
            rtnValue = Timeout;
        }
    }
    // all expected messages are already available
    else
    {
        rtnValue = 0;
    }

    // tidy up
    message_map.remove_message(message_id);
    // delete later by garbage collection, put the message to the garbage queue
    garbage_queue.push(msg_entry);

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return rtnValue;
}

/**
 *	Send an reply.
 *	The message must be called in the implementation of the handle_request method to reply on a request.
 *	@param message_id Message id of the reply.
 *	@param message_data Pointer to the message.
 *	@param message_length Length of the message.
 *	@param receiving_module The receiving module.
 *	@param server_address Address of the target server.
 *	@return 0 - reply was successful; otherwise error code
 */
int CommunicationHandler::reply(uint64_t message_id, void* message_data,
                                int32_t message_length, CommunicationModule receiving_module, string server_address)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    void* extended_data;
    int32_t extended_length;
    int32_t rtnValue;

    vector<string> server_list;
    server_list.push_back(server_address);

    extended_length = message_length + sizeof(int32_t) + sizeof(uint64_t);
    // allocate memory for data, message_id and message type
    extended_data = malloc(extended_length);
    if (extended_data == NULL)
    {
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Cannot get enough memory to create message which will be sent.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return NotEnoughMemory;
    }

    // assign message id and message type to the data
    *((uint64_t*) extended_data) = message_id;
    *((int32_t*) (((uint64_t*) extended_data) + 1)) = REPLY;

    // copy the message
    memcpy(((char*) extended_data) + sizeof(int32_t) + sizeof(uint64_t),
           message_data, message_length);

    // send the message
    rtnValue = send_server->asynch_send(extended_data, extended_length,
                                        module_type, receiving_module, server_list);
    
#ifdef LOGGING_ENABLED
    if (rtnValue != 0)
    {
        (*logging_instance).warning_log("Sending was not successful.");
    }
#endif
    // free sent message
    free(extended_data);

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return rtnValue;
}


/**
 * Send a message to Ganesha and receive a reply
 * @param message - message which will be sent to Ganesha, structure of message is determined by higher level protocol
 * @param message_length - length of message in number of bytes
 * @return pointer to result, which is of type RequestResult. On success result.data != NULL
 */
RequestResult CommunicationHandler::ganesha_send(void* message, int message_length)
{
    return send_server->ganesha_send(message, message_length);
}


/**
 * Start the communication handler thread.
 * @return 0 if everything was successful otherwise a number unequal to 0
 */
int32_t CommunicationHandler::start()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    executed = true;
    int32_t failure1, failure2, failure3; 

    //remove this if garbage collection thread is re-enabled!!!
    failure3 = 0;	

    failure1 = pthread_create(&handling_thread, NULL,
                              &CommunicationHandler::handle_messages, this);
#ifdef LOGGING_ENABLED
    if (failure1)
    {
        (*logging_instance).error_log("Thread which will handle incoming messages cannot be created.");
    }
#endif
    failure2 = pthread_create(&request_thread, NULL,
                              &CommunicationHandler::run_request_thread, this);
#ifdef LOGGING_ENABLED
    if (failure2)
    {
        (*logging_instance).error_log("Thread which will handle incoming requests cannot be created.");
    }
#endif

//    failure3 = pthread_create(&garbage_collection_thread, NULL,
//                              &CommunicationHandler::run_garbage_collection, this);

#ifdef LOGGING_ENABLED
    if (failure3)
        (*logging_instance).error_log("Thread which will handle the garbage collection cannot be created.");
#endif

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return failure1 || failure2 || failure3;
}

void* CommunicationHandler::run_request_thread(void* ptr)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    CommunicationHandler* com = static_cast<CommunicationHandler*> (ptr);
    com->handle_requests();

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return NULL;
}
/**
 * Stop the thread. TODO Currently not able to stop the threads.
 * @return
 */
int32_t CommunicationHandler::stop()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    executed = false;
    pthread_join(garbage_collection_thread, NULL);
    pthread_join(request_thread, NULL);
    pthread_join(handling_thread, NULL);
    pthread_detach( garbage_collection_thread );
    pthread_detach( request_thread );
    pthread_detach(handling_thread);

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;//TODO put useful return value here
}

/**
 * Thread function.
 * This thread is responsible for the garbage collection.
 */
void* CommunicationHandler::run_garbage_collection(void *ptr)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    CommunicationHandler* com = static_cast<CommunicationHandler*> (ptr);
    com->handle_garbage_collection();

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return NULL;
}

/**
 * Thread function.
 * The thread continuous receives messages and handles them.
 */
void* CommunicationHandler::handle_messages(void* ptr)
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    CommunicationHandler* com = static_cast<CommunicationHandler*> (ptr);
    int32_t r;
    while (com->is_executed())
    {
        r = com->extract_message();
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return NULL;
}

/**
 * Thread function.
 * The thread continuous takes the received requests and handles them.
 */
int32_t CommunicationHandler::handle_requests()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    ExtractedMessage message;
    while (executed)
    {
        //data_queue.wait_and_pop(message);
        if (data_queue.try_wait_and_pop(message, THREAD_BLOCKING_TIME))
        {
            handle_request(message);
        }
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 * Extracts the message.
 * The message handling depends on the type of the message.
 * The first distinction is made by determining the id of the message.
 * If the message id = 0, no particular handling is necessary, and the messages will be put to the data queue.
 * Otherwise the type of the messages is determined, REPLY / REQUEST.
 */
int32_t CommunicationHandler::extract_message()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    uint64_t message_id;
    int32_t message_type;
    ExternalMessage incoming_message;
    ExtractedMessage extractd_message;
    MessMapEntry* msg_entry;

    // wait until a message arrives
    //raw_queue.wait_and_pop(incoming_message);
    if (!raw_queue.try_wait_and_pop(incoming_message, THREAD_BLOCKING_TIME))
    {
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return Timeout;
    }

    // First we look at the message_id.
    message_id = (uint64_t) (*((uint64_t*) incoming_message.data));

    /*
     * If message_id = 0 => the messages does not need a particular handling.
     * Here we need just extract the message_id (=0) out of the message.
     */
    if (message_id == 0)
    {
        // copy the message, without the message id
        copy_message(incoming_message, extractd_message, 0);

        // put the extracted message into the data queue
        data_queue.push(extractd_message);

        // we do not need the received messaged any more
        free(incoming_message.data);
    }

    /*
     * We received a message with a particular id.
     */
    else
    {

        // Determine the type of the message, request or reply.
        message_type = (int32_t) (*((uint64_t*) incoming_message.data + 1));

        // copy the message without message_id and message_type
        copy_message(incoming_message, extractd_message, sizeof(message_type));

        // we do not need the received messaged any more
        free(incoming_message.data);

        // if the message is a request, handle it
        if (message_type == REQUEST)
        {
            // put the extracted message into the data queue
            data_queue.push(extractd_message);

        }

        // the message is a reply
        else if (message_type == REPLY)
        {
            // The received message id is unknown.
            if ((msg_entry = message_map.find_entry(message_id)) == NULL)
            {
#ifdef LOGGING_ENABLED
                (*logging_instance).warning_log("The message id never existed or the message were removed by the gargabe collector or the reply was requested and received a time out.");
#endif
                // ends profiling
                Pc2fsProfiler::get_instance()->function_end();

                return UnknownMessageId;
            }

            // push the message into the message entry list
            msg_entry->get_msg_list()->push_back(extractd_message);

            // check if all expected messages are arrived
            if (msg_entry->get_expected_msgs() == (int32_t) msg_entry->get_msg_list()->size())
                msg_entry->wake_up(); // if all messages are arrived, wakeup the waiting thread
        }
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 * Copy the message, beginning at the offset.
 * @param source The source of message to be copied.
 * @param dest The destination where the content is to be copied.
 * @param offset Defines the part of the message, that will be skipped.
 * @return 0 if successful otherwise an error code
 */
int32_t CommunicationHandler::copy_message(ExternalMessage& source,
        ExtractedMessage& dest, int32_t offset)
{

    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    uint64_t message_id;
    dest.data_length = source.data_length - (offset + sizeof(message_id));
    dest.message_id = (uint64_t) (*((uint64_t*) source.data));

    // copy the values
    dest.sending_module = source.sending_module;
    dest.sending_server = source.sending_server;

    // allocate memory for the extracted message
    dest.data = malloc(dest.data_length);
    if (dest.data == NULL)
    {
        dest.data_length = 0;
#ifdef LOGGING_ENABLED
        (*logging_instance).error_log("Cannot get enough memory to extract message content.");
#endif
        // ends profiling
        Pc2fsProfiler::get_instance()->function_end();

        return NotEnoughMemory;
    }

    // copy the message beginning with the offset
    memcpy(dest.data, ((char*) source.data) + offset + sizeof(message_id),
           dest.data_length);

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return 0;
}

/**
 * Tests if the communication handler thread is in execution.
 */
bool CommunicationHandler::is_executed() const
{
    return executed;
}

/**
 * Handling the garbage collection.
 * A thread collects all request message objects that did not received a reply periodically and clean them up.
 * TODO change garbage collection
 */
void CommunicationHandler::handle_garbage_collection()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    int32_t life_time = MSG_LIFE_TIME;
    std::unordered_map<uint64_t, MessMapEntry*>::iterator it;
    MessMapEntry* e;

    boost::posix_time::ptime temp_time;
    boost::posix_time::ptime current_time;
    while (executed)
    {
        message_map.lock();
        current_time = boost::posix_time::second_clock::universal_time();

        uint64_t i;
        it = message_map.get_m_map().begin();
        while (it != message_map.get_m_map().end())
        {
            i = (*it).first;
            e = (*it).second;
            if (!e->is_garbage())
            {
                /*
                 * Try to lock the entry, if the entry is already locked return and try to get the next entry.
                 * If the lock was successful, check if another thread is waiting for this message, if not do garbage collection.
                 */
                if (!e->is_waiting())
                {
                    // add constant value to the time stamp
                    temp_time = e->get_time_stamp()
                                + boost::posix_time::seconds(life_time);
                    // if the time value exceeds a defined value, remove the entry
                    if (temp_time < current_time)
                    {
                        // set as garbage
                        e->set_garbage(true);
                    }

                }
            }

            else
            {
                if (!e->is_waiting())
                {
                    message_map.get_m_map().erase(it);
                    garbage_queue.push(e); // delete later
                }

            }

            it++;
        }

        message_map.unlock();
        Pc2fsProfiler::get_instance()->function_sleep();
        sleep(MSG_LIFE_TIME);
        Pc2fsProfiler::get_instance()->function_wakeup();
        clear_garbage_queue();
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

/**
 * Clear the garbage queue and delete all elements.
 */
void CommunicationHandler::clear_garbage_queue()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    MessMapEntry* e;
    while (garbage_queue.try_pop(e))
    {
        delete e;
    }

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

