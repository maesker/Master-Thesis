/**
 * @file SPNBC_client.h
 * @author Markus Maesker, <maesker@gmx.net>
 *
 * @data 02.05.2012
 * @brief Client end of the data servers backchannel to the client.
 * 
 */

#ifndef SPNBC_CLIENT_H
#define	SPNBC_CLIENT_H

#include "stdint.h"
#include <pthread.h>
#include <string>
#include <zmq.h>
#include <zmq.hpp>

#include "EmbeddedInode.h"
#include "components/network/AsynClient.h"
#include "custom_protocols/storage/SPdata.h"
#include "custom_protocols/SocketManager/SocketManager.h"
#include "components/OperationManager/OpManager.h"
#include "components/raidlibs/Libraid5.h"
#include "components/network/ServerManager.h"
#include "coco/communication/ConcurrentQueue.h"
#include "logging/Logger.h"
#include "components/network/asyn_tcp_server.h"

/**
 * @class SPNBC_client class.
 * @param p_log pointer to a Logger instance.
 */
class SPNBC_client {
public:
    SPNBC_client(Logger *p_log);
    SPNBC_client(const SPNBC_client& orig);
    virtual ~SPNBC_client();
    
    bool popqueue(SPNBC_head **p_res);    
    int perform_network_send(struct SPNBC_head  *p_head);
    int register_cl_address(serverid_t id, char *ip);
    
    int handle_result(InodeNumber inum,CCO_id ccoid, opstatus result);
    int handle_read_response(InodeNumber inum, StripeId sid, StripeUnitId suid, CCO_id ccoid,struct data_object *obj);
    
    int handle_pingpong(CCO_id ccoid);
private:
    ServerManager *p_cl_manager;
    AsynClient<SPNBC_head> *p_asyn;
        
    Logger *log;
    int run();
};

#endif	/* SPNBC_CLIENT_H */

