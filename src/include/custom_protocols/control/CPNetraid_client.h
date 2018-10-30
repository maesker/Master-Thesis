/* 
 * File:   CPNetraid_client.h
 * Author: markus
 *
 * Created on 10. Januar 2012, 19:13
 */

#ifndef CPNETRAID_CLIENT_H
#define	CPNETRAID_CLIENT_H

#include <unistd.h>
#include "stdint.h"
#include <pthread.h>
#include <string>
#include <zmq.h>
#include <zmq.hpp>
#include <list>


#include "logging/Logger.h"
//#include "custom_protocols/control/CPNetraid.h"
#include "custom_protocols/control/CPdata.h"
#include "custom_protocols/global_protocol_data.h"
#include "custom_protocols/SocketManager/SocketManager.h"
#include "mm/mds/ByterangeLockManager.h"
#include "components/network/sshwrapper.h"

/*
struct cp_multicast_object {
    struct cp_socket_t *p_sock;
    void *msgdata;
    uint32_t msgsize;
    int *retval;
};*/

class CPNetraid_client {
public:
    CPNetraid_client(Logger *p_log);
    CPNetraid_client(const CPNetraid_client& orig);
    virtual ~CPNetraid_client();
    
    
    int addDS(serverid_t id, std::string ip, uint16_t port);
    int connectDS(serverid_t id);
    
    int handle_helloworld(socketid_t id);
    int multicast_distribute_Lock(std::list<socketid_t> *p_sockets, lock_exchange_struct *lock);
    int handle_distribute_Lock(socketid_t *p_sock, lock_exchange_struct lock);
    
    int setupDS(std::string user, std::string bin, std::string pw);
    
    
private:
    Logger *log;
    SocketManager<struct cp_socket_t> *sm;
    sshwrapper *p_ssh;
    
    int send_and_recv(    void *p_data,
                          size_t data_size,
                          zmq_msg_t *p_response,
                          struct cp_socket_t *p_sock);
};

#endif	/* CPNETRAID_CLIENT_H */

