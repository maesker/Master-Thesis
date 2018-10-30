/**
 * \file IPCGanesha.c
 *
 * \author Marie-Christine Jakobs
 *
 * \brief Realization of the IPCGanesha.h header file
 *
 * Implements the receiving request and the corresponding reply part of the communication between e.g. the load balancing module and Ganesha
 */

#include "coco/communication/IPCGanesha.h"
#include "GaneshaProfiler.h"
#include <zmq.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <unistd.h>
#include <stdio.h>
#include "communication.h"

/**
 * \brief Method realizing reception of requests, calls message_treatment for their processing and replies
 *
 *\param message_treatment - function pointer to a function with signature RequestResult method_name (void*, int) which is able to handle incoming requests
 *
 * Realizes the reply part of the request reply pattern between e.g. the load balancing module and Ganesha. Only one open request at a time is
 * allowed. The request is received. Then the message is processed by message_treatment and the result specified by RequestResult is sent back
 * afterwards.
 */
void receive_ipc_request(RequestResult (*message_treatment) (void* request, int lenght))
{   
    int msg_length;
    void* msg_data;
    int zmq_request_result;
    RequestResult function_return;
    //set up context
    void* zmq_context = zmq_init(1);
    //checking if an error occurred
    if (zmq_context==NULL)
    {        
	//exit function, do not call exit
        return;
    }
    //create socket
    void* server_socket = zmq_socket(zmq_context, ZMQ_REP);
    //checking if an error occurred
    if (server_socket==NULL)
    {
        //exit function, do not call exit
        return;
    }

    zmq_request_result = zmq_bind(server_socket, MDS_TO_GANESHA_LB_INTERFACE);
    //checking if an error occurred
    if (zmq_request_result)
    {
        //exit programme
        exit(EXIT_FAILURE);
    }
    // be ready for receiving requests
    while (1)
    {
        //set up message for receiving
        zmq_msg_t request;
        zmq_request_result = zmq_msg_init(&request);
        //checking if message was initalized correctly
        if (!zmq_request_result)
        {
            //receive next request (block as long as message available), retry if interrupted
            zmq_request_result = TEMP_FAILURE_RETRY (zmq_recv(server_socket,&request,0));
            //wrong operation, send must be next operation
            if (zmq_request_result && errno==EFSM)
            {
                //send an empty message
                function_return.length=0;
                function_return.data=NULL;
            }
            else
            {
                //other errors are not expected to happen
                //get message content
                msg_length=zmq_msg_size(&request);
                msg_data = zmq_msg_data(&request);
                //treat message
                function_return = message_treatment(msg_data, msg_length);
            }
            //close received message, no errors expected if one occurs it is ignored
            zmq_request_result = zmq_msg_close(&request);
            //set up message for reply
            zmq_msg_t reply;
            do
            {
                //try to initialize message until it was succesful
                zmq_request_result = zmq_msg_init_size(&reply, function_return.length);
            }
            //checking if an error occurred
            while (zmq_request_result);
            // copy data into message
            memcpy(zmq_msg_data(&reply), function_return.data, function_return.length);
            //send reply (block as long message was sent), retry if interrupted, other errors are ignored
            zmq_request_result = TEMP_FAILURE_RETRY (zmq_send(server_socket,&reply,0));
            //close sent message, no errors expected if one occurs it is ignored
            zmq_request_result = zmq_msg_close(&reply);
            //free space of returned data
            free(function_return.data);
        }
    }
}
