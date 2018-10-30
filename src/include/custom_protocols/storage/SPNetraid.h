/** 
 * @file   SPNetraid.h
 * @author markus
 *
 * Created on 8. Januar 2012, 10:03
 */

#ifndef SPNETRAID_H
#define	SPNETRAID_H

#include "SPdata.h"

#include "logging/Logger.h"
#include "custom_protocols/IProtocol.h"
#include "custom_protocols/global_protocol_data.h"
#include "server/DataServer/DataServer.h"
#include "coco/communication/ConcurrentQueue.h"
#include "components/raidlibs/Libraid5.h"
#include "components/network/asyn_tcp_server.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <cstring>
#include <pthread.h>


class SPNetraid : public IProtocol
{
public:
    SPNetraid(std::string basedir, serverid_t sid, void (*cb)(void *));
    SPNetraid(const SPNetraid& orig);
    virtual ~SPNetraid();
    
    void        register_dataserver(DataServer *p_ds);
    uint32_t    get_max_msg_len();
    PET_MINOR   handle_Message(SPN_message *p_msg);
    
    static void pushQueue(void *p);   
    
private:
    DataServer *p_ds;
    Logger *log;
    Libraid5 *p_raid;
    void (*cb)(void *);
    serverid_t serverid;
    ServerManager *p_sman_client;
    
    PET_MINOR handle_SPN_DirectWriteSU_req(struct SPN_DirectWrite_req  *p_msg);
    PET_MINOR handle_SPN_WriteS_req(struct SPN_Write_req *p_msg);
    PET_MINOR handle_SPN_WriteSU_req(struct SPN_Write_req *p_msg);
    PET_MINOR handle_SPN_read_req(struct SPN_Read_req *p_msg);
    PET_MINOR handle_SPN_Pingpong_req(struct SPN_Pingpong  *p_msg);
    
    PET_MINOR handle_create_participant_operation(struct SPN_Write_req *p_msg);
    PET_MINOR handle_create_primcoordinator_operation(struct SPN_Write_req *p_msg);
    PET_MINOR handle_create_seccoordinator_operation(struct SPN_Write_req *p_msg);
    void      handle_SPN_endpointreg(char *ip, serverid_t server);
};

#endif	/* SPNETRAID_H */

