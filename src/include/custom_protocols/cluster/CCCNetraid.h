/** 
 * @file   CCCNetraid.h
 * @author markus
 *
 * Created on 8. Januar 2012, 10:03
 */

#ifndef CCCNETRAID_H
#define	CCCNETRAID_H

#include "CCCdata.h"

#include "logging/Logger.h"
#include "custom_protocols/IProtocol.h"
#include "custom_protocols/global_protocol_data.h"
//#include "server/DataServer/DataServer.h"
#include "coco/communication/ConcurrentQueue.h"
#include "components/raidlibs/Libraid5.h"
#include "components/OperationManager/OpManager.h"
#include "components/network/asyn_tcp_server.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <cstring>
#include <pthread.h>


class CCCNetraid : public IProtocol
{
public:
    CCCNetraid(std::string logdir, serverid_t sid, void (*cb)(void *));
    CCCNetraid(const CCCNetraid& orig);
    virtual ~CCCNetraid();
    
    //void        register_dataserver(DataServer *p_ds);
    uint8_t     get_protocol_id();
    uint32_t    get_max_msg_len();
    int         handle_Message(CCC_message *p_msg);
    int         handle_CCC_cl_smallwrite(struct CCC_cl_smallwrite *p_msg);
    int         handle_CCC_prepare(struct CCC_prepare *p_msg);
    int         handle_CCC_cancommit(struct CCC_cancommit *p_msg);
    int         handle_CCC_docommit(struct CCC_docommit *p_msg);
    int         handle_CCC_committed(struct CCC_committed *p_msg);
    int         handle_CCC_result(struct CCC_result *p_msg);
    int         handle_CCC_stripewrite_cancommit(struct CCC_cl_stripewrite_cancommit *p_msg);
    int         handle_CCC_pingpong(struct CCC_cl_pingpong *p_msg);

    static void pushQueue(void *p);   
    
private:
    //DataServer *p_ds;
    Logger *log;
    void (*cb)(void *);
    serverid_t serverid;
    
    
    //PET_MINOR handle_create_participant_operation(struct SPN_Write_req *p_msg);
    //PET_MINOR handle_create_primcoordinator_operation(struct SPN_Write_req *p_msg);
    //PET_MINOR handle_create_seccoordinator_operation(struct SPN_Write_req *p_msg);
};

#endif	/* SPNETRAID_H */

