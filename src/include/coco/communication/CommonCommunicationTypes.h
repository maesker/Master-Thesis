/**
 * \file CommonCommunicationTypes.h
 *
 * \author Marie-Christine Jakobs
 *
 * \brief Contains data structures and definitions which are used for external communication.
 *
 * Defines which modules are allowed to access external communication and their respective identifier, the structure of the external message,
 * the port number, the location where the IP addresses of other servers can be found and the constants to distinguish between request and
 * reply.
 */
#ifndef COMMONCOMMUNICATIONTYPES_H_
#define COMMONCOMMUNICATIONTYPES_H_

#include <string>
/**
 * \brief Enumeration containing the identifiers for the allowed modules.
 *
 * The identifier for a module is used to identify the module which sent the message as well as the module which should receive the message.
 */
enum CommunicationModule {COM_LoadBalancing, COM_DistributedAtomicOp, COM_AdminOp, COM_PrefixPerm};

/**
 * \brief Defines the structure of a received, low level message.
 *
 * Message built in RecCommunicationServer and put into the ConcurrentQueue belonging to the corresponding module. Afterwards this message
 * shall be processed further before it will be delivered for further processing by the module.
 */
struct ExternalMessage
{
    std::string sending_server;/**< IP address or other identifier of the server which sent the message*/
    CommunicationModule sending_module; /**< Module identifier of the module which invoked the sending of the message*/
    int32_t data_length; /**< Length of the message content, which can be accessed by pointer, in number of bytes*/
    void* data; /**< Pointer to the message content, type of pointer must be known from the receiving module and must be specified by the protocol on top, possibly NULL if not enough memory was available*/
};

/**
 * \brief Defines the structure of a received, intermediate level message.
 *
 * Message built in each module by the CommunicationHandler and put into the Dataqueue. Afterwards this message
 * is processed further by a method implemented by each module.
 */
struct ExtractedMessage
{
    std::string sending_server;/**< IP address or other identifier of the server which sent the message*/
    CommunicationModule sending_module; /**< Module identifier of the module which invoked the sending of the message*/
    uint64_t message_id; /**< Identifier of the Message*/
    int32_t data_length; /**< Length of the message content, which can be accessed by pointer, in number of bytes*/
    void* data; /**< Pointer to the message content, type of pointer must be known from the receiving module and must be specified by the protocol on top*/
};

/**
 * Constant representing how many modules are allowed to send. The number must identical with the size of the Module enumeration.
 */
#define NUM_MODULES 4

/**
 * Defines the port number which is used to realize the communication between different metadata servers needed by the above specified modules.
 */
#define PORT_NUMBER ":49152"

/**
 * Defines the location where the SendCommunicationServer will look up the IP addresses of the other metadata servers during its creation.
 */
#define IP_SYMBOLIC_FILE "ip.config"

/**
 * If is 0 (false) the address of another metadata server will be composed by the IP address plus the port number defined above. In all other
 * cases the address must be known completely. This only important for the SendCommunicationServer.
 */
#define VARIABLE_PORTS 0

/**
 * Defines the identification for a message which was sent as a request to another metadata server.
 */
#define REQUEST 0

/**
 * Defines the identification for a message which was sent as a reply to a previous request from another metadata server.
 */
#define REPLY 1

/**
 * Defines the maximum life time of a message in seconds.
 */
#define MSG_LIFE_TIME 20

/**
 * Defines the maximum blocking time of a thread in seconds.
 */
#define THREAD_BLOCKING_TIME 2

#endif //#ifndef COMMONCOMMUNICATIONTYPES_H_
