/* 
 * File:   CPdata.h
 * Author: markus
 *
 * Created on 13. Januar 2012, 13:42
 */

#ifndef CPDATA_H
#define	CPDATA_H

#include "custom_protocols/global_protocol_data.h"
#include "components/OperationManager/OpManager.h"
#include "mm/mds/ByterangeLockManager.h"

struct cp_socket_t {
    void*       zmq_socket;
    void*       zmq_context;
    void*       zmq_memspace;
    uint32_t    sequencenumber;
    pthread_mutex_t mutex;
};

struct cp_multicast_object {
    struct cp_socket_t *p_sock;
    void *msgdata;
    uint32_t msgsize;
    int *retval;
};




struct CPN_HelloWorld_req_t{
    struct custom_protocol_reqhead_t head;
    char data[8];
};

struct CPN_HelloWorld_resp_t{
    struct custom_protocol_resphead_t head;
    char data[8];
};



struct CPN_LockDistribution_req_t{
    struct custom_protocol_reqhead_t head;
    lock_exchange_struct lock;
};

struct CPN_LockDistribution_resp_t{
    struct custom_protocol_resphead_t head;    

};
#endif	/* CPDATA_H */

