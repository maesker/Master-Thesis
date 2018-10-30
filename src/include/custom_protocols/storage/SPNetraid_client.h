/* 
 * File:   SPNetraid_client.h
 * Author: markus
 *
 * Created on 8. Januar 2012, 17:19
 */

#ifndef SPNETRAID_CLIENT_H
#define	SPNETRAID_CLIENT_H

#include "stdint.h"
#include <pthread.h>
#include <string>
#include <zmq.h>
#include <zmq.hpp>
#include <time.h>

#include "EmbeddedInode.h"
#include "components/network/AsynClient.h"
#include "custom_protocols/storage/SPdata.h"
#include "components/OperationManager/OpManager.h"
//#include "components/raidlibs/Libraid5.h"
#include "components/raidlibs/Libraid4.h"
#include "components/network/ServerManager.h"
#include "coco/communication/ConcurrentQueue.h"
#include "logging/Logger.h"
#include "components/network/asyn_tcp_server.h"
#include "tools/parity.h"

#define DO_TESTMODE
#ifdef DO_TESTMODE
  #define SPN_OPERATION_TIMEOUT 3600
#else
  #define SPN_OPERATION_TIMEOUT 2
#endif

class SPNetraid_client {
public:
    SPNetraid_client(Logger *p_log);
    SPNetraid_client(const SPNetraid_client& orig);
    virtual ~SPNetraid_client();
    
    int perform_operation(struct operation_composite *p_op);
    //int perform_operation(struct operation_client_write *p_op);
    int perform_operation(struct dstask_spn_send_read  *p_t);
    
    int perform_network_send(struct spn_task  *p_task);
    int perform_operationBCH(SPNBC_head* p_head);
    //uint32_t handle_DeviceLayout(serverid_t *id);
    int register_ds_address(serverid_t id, char *ip);
    int getDeviceLayouts(std::vector<serverid_t> *ds);
    int handle_write(ClientSessionId csid,struct EInode *p_einode, size_t offset, size_t length, void *data);
    int handle_write_lock(ClientSessionId csid, struct EInode *p_einode, size_t offset, size_t length, void *data);
    int handle_read(ClientSessionId csid,struct EInode *p_einode, size_t offset, size_t length, void **data);
    int handle_pingpong(ClientSessionId csid);
    
    bool                popqueue(void **p_res);
    bool                popqueueMSG(struct spn_task **p_res);
    bool                popqueueBCH(void **p_res);
    static int          pushQueue(queue_priorities priority, OPHead *p_head);
    static void         pushQueueBCH(void *msg);
    
private:
    //SocketManager<struct ds_socket_t> *sm;
    ConcurrentQueue<struct spn_task*> *p_netmsg;
    OpManager *p_opman;
    //Libraid5 *p_raid;
    Libraid4 *p_raid4;
    ServerManager *p_ds_manager;
    AsynClient<SPN_message> *p_asyn;
    asyn_tcp_server<SPNBC_head> *p_bch;
    std::vector<pthread_t> thread_vec;
    boost::asio::io_service *io_service;
    
    uint32_t ds_count;
    uint32_t sequence_num;
    bool pingpongflag;
    
    Logger *log;
    int send_and_recv(    void *p_data,
                          size_t data_size,
                          zmq_msg_t *p_response,
                          struct ds_socket_t *p_sock);
    
    pthread_mutex_t seqnum_mutex;
    //uint32_t getSequenceNumber();
    int run();
    int perform_stripewrite(struct operation_client_write *p_op);
    int perform_stripeunitwrite(struct operation_client_write *p_op);
    int perform_Direct_write(struct operation_client_write *p_op);
};

#endif	/* SPNETRAID_CLIENT_H */

