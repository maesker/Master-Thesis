/** \brief This class handles the communication.
 *         It provides functions to send and receive messages.
 *
 *  Detailed description starts here.
 *
 * @author Viktor Gottfried
 */

#ifndef COMMUNICATIONHANDLER_H_
#define COMMUNICATIONHANDLER_H_

#include "RecCommunicationServer.h"
#include "SendCommunicationServer.h"
#include "UniqueIdGenerator.h"
#include "MessageMap.h"
#include "CommunicationLogger.h"
#include "CommonGaneshaCommunicationTypes.h"

#include <pthread.h>
#include <unordered_map>
#include <vector>

using namespace std;

class CommunicationHandler
{

public:
    CommunicationHandler();
    virtual ~CommunicationHandler();

    int init(CommunicationModule model_type);

    int start();
    int stop();

    int32_t send(void* message_data, int32_t message_length, CommunicationModule receiving_module, vector<string>& server_list);

    int32_t request_reply(uint64_t* message_id, void* message_data, int32_t message_length, CommunicationModule receiving_module,
                          vector<string>& server_list, vector<ExtractedMessage>& message_list);

    int32_t receive_reply(uint64_t message_id, int32_t timeout);

    int32_t reply(uint64_t message_id, void* message_data, int32_t message_length, CommunicationModule receiving_module, string server_address);

    RequestResult ganesha_send(void* message, int message_length);

    bool is_executed() const;

    const ConcurrentQueue<ExtractedMessage>& get_data_queue() const
    {
        return data_queue;
    }

protected:
    /**
     * Implement request handling.
     */
    virtual void handle_request(ExtractedMessage& message) = 0;

    void clear_garbage_queue();

    int32_t copy_message(ExternalMessage& source, ExtractedMessage& dest, int32_t offset);
    int32_t extract_message();
    static void* handle_messages(void *ptr);
    static void* run_request_thread(void *ptr);
    static void* run_garbage_collection(void *ptr);
    int32_t handle_requests();

    void handle_garbage_collection();

    bool executed;
    CommunicationModule module_type;
    UniqueIdGenerator id_generator;

    ConcurrentQueue<ExternalMessage> raw_queue;
    ConcurrentQueue<ExtractedMessage> data_queue;
    ConcurrentQueue<MessMapEntry*> garbage_queue;

    MessageMap message_map;

private:
    SendCommunicationServer* send_server;
    RecCommunicationServer* recv_server;
#ifdef LOGGING_ENABLED
    Logger* logging_instance;
#endif

    pthread_t request_thread;
    pthread_t handling_thread;
    pthread_t garbage_collection_thread;
};

#endif /* COMMUNICATIONHANDLER_H_ */
