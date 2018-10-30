/*
 * @file communication.h
 * 
 * @brief define communication between Ganesha and our MDS. Default is IPC.
 * 
 *  This header defines all zmq sockets which are 
 * used for the communcation
 */

#define MDS_TO_GANESHA_LB_INTERFACE "tcp://*:5551"

#ifdef MDS_USE_IPC
        #define ZMQ_ROUTER_FRONTEND "ipc:///tmp/zmq_backend"
        #define ZMQ_ROUTER_BACKEND "ipc:///tmp/zmq_frontend"
#else
        #define ZMQ_ROUTER_FRONTEND "tcp://*:5552"
        #define ZMQ_ROUTER_PORT "5552"
        #define ZMQ_ROUTER_BACKEND "ipc:///tmp/zmq_frontend"
#endif
