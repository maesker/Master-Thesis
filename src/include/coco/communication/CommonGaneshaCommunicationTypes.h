/**
 * \file CommonGaneshaCommunicationTypes.h
 *
 * \author Marie-Christine Jakobs
 *
 * \brief Contains the data structures which are needed to send a request to Ganesha and get the result to get the necessary information for load balancing.
 *
 * Defines the location where the file can be found for the shared memory based inter process communication and the structure of the result.
 */
#ifndef _COMMONGANESHACOMMUNICATIONTYPES_H_
#define _COMMONGANESHACOMMUNICATIONTYPES_H_

/**
 * Location of the shared memory file from the requester's point of view.
 */
#define GANESHA_CONNECT_ADDRESS "ipc://InterProcessGanesha.ipc"

/**
 * Location of the shared memory file from the replier's point of view (Ganesha)
 */
#define GANESHA_BIND_ADDRESS "ipc://InterProcessGanesha.ipc"

/**
 * Structure of a message send as reply by Ganesha to a request e.g. of the load balancing module
 */
typedef struct
{
    int length; /**< Length of the request result content, which can be accessed by pointer, in number of bytes*/
    void* data; /**< Pointer to the message content, type of pointer must be known from the receiving module and must be specified by the protocol on top*/
} RequestResult;

#endif //_COMMONGANESHACOMMUNICATIONTYPES_H_