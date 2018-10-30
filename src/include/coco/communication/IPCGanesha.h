/**
 * \file IPCGanesha.h
 *
 * \author Marie-Christine Jakobs
 *
 * \brief Header for IPCGanesha.c which realizes the reception and treatment of requests.
 *
 *
 */

#ifndef _IPCGANESHA_H_
#define _IPCGANESHA_H_

/**
 * Needed to use Makro TEMP_FAILURE_RETRY in IPCGanesha.c
 */
#define _GNU_SOURCE

#include "CommonGaneshaCommunicationTypes.h"

/**
 * \brief Method realizing reception of requests, calls message_treatment for their processing and replies
 *
 *\param message_treatment - function pointer to a function with signature RequestResult method_name (void*, int) which is able to handle incoming requests
 *
 * Realizes the reply part of the request reply pattern between e.g. the load balancing module and Ganesha. Only one open request at a time is
 * allowed. The request is received. Then the message is processed by message_treatment and the result specified by RequestResult is sent back
 * afterwards.
 */
void receive_ipc_request(RequestResult (*message_treatment) (void* request, int lenght));

#endif //IPCGANESHA_H_