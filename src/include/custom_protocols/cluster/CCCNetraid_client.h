/* 
 * File:   CCCNetraid_client.h
 * Author: markus
 *
 * Created on 8. Januar 2012, 17:19
 */

#ifndef CCCNETRAID_CLIENT_H
#define	CCCNETRAID_CLIENT_H

#include "stdint.h"
#include <pthread.h>
#include <string>
#include <zmq.h>
#include <zmq.hpp>

#include "EmbeddedInode.h"
#include "components/network/AsynClient.h"
#include "custom_protocols/cluster/CCCdata.h"

#include "components/OperationManager/OpManager.h"
#include "components/raidlibs/Libraid4.h"
#include "components/network/ServerManager.h"
#include "coco/communication/ConcurrentQueue.h"
#include "logging/Logger.h"


class CCCNetraid_client {
public:
    CCCNetraid_client(Logger *p_log, ServerManager *p_sm, serverid_t id, bool dsync);
    CCCNetraid_client(const CCCNetraid_client& orig);
    virtual ~CCCNetraid_client();        
    
    int perform(CCC_message *p_msg);    
    bool popqueue(void **p_res);
    //bool popqueueMSG(CCC_message **p_res);
    static void pushQueue(CCC_message *p_msg);
    
    int handle_CCC_cl_smallwrite(struct dstask_ccc_send_received *p_task);
    int handle_CCC_send_prepare(struct dstask_ccc_send_prepare *p_task);
    int handle_CCC_send_cancommit(struct operation_participant *p_part);
    int handle_CCC_send_docommit(struct operation_dstask_docommit *p_task);
    int handle_CCC_send_committed(struct operation_participant *p_part);
    int handle_CCC_send_result(struct dstask_ccc_send_result *p_task);
    int handle_CCC_stripewrite_cancommit(struct dstask_ccc_stripewrite_cancommit *p_task);
    
    int  handle_CCC_cl_pingping(struct dstask_pingpong *p_task);
private:
    //ConcurrentQueue<CCC_message*> *p_netmsg;    
    ServerManager *p_ds_manager;
    Libraid4 *p_raid;
    AsynClient<CCC_message> *p_asyn;
    uint32_t ds_count;
    uint32_t sequence_num;
    serverid_t id;
    bool dosync;
    
    Logger *log;
    pthread_mutex_t seqnum_mutex;
    uint32_t getSequenceNumber();
    int run();
};

#endif	/* CCCNETRAID_CLIENT_H */

