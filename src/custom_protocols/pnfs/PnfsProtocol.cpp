/** 
 * @file   PnfsProtocol.cpp
 * @author Markus Maesker <maesker@gmx.net>
 * 
 * @date 6. Januar 2012, 11:08
 */

#include "custom_protocols/pnfs/PnfsProtocol.h"


PnfsProtocol::PnfsProtocol():IProtocol() 
{
    log = new Logger();
    log->set_log_location("/tmp/pnfsserverlib.log");
    maximal_message_length = 1024;
    p_mds = MetadataServer::get_instance();
}

PnfsProtocol::PnfsProtocol(const PnfsProtocol& orig) :IProtocol() {
}

PnfsProtocol::~PnfsProtocol() {
}

uint32_t PnfsProtocol::get_max_msg_len(){
    return maximal_message_length;
}


size_t   PnfsProtocol::handle_request(custom_protocol_reqhead_t *head, void *req, void *resp)
{
    log->debug_log("incomming...");
    PET_MAJOR rma = 0;
    PET_MINOR rmi = 0;
    size_t respsize = 0;
    struct custom_protocol_resphead_t *p_resp_head = (struct custom_protocol_resphead_t*) resp;
    switch (head->msg_type)
    {
        case PNFS_ByterangeLock_req:
        {
            log->debug_log("PNFS_ByterangeLock_req detected.");
            struct PNFS_ByterangeLock_req_t *p_req = (struct PNFS_ByterangeLock_req_t*) req;
            struct PNFS_ByterangeLock_resp_t *p_resp = (struct PNFS_ByterangeLock_resp_t *) resp;
            respsize = sizeof(struct PNFS_ByterangeLock_resp_t);
            //printf("recv:csid=%llu.%llu.%llu\n-------",p_req->csid,p_req->inum,p_req->start);
            log->debug_log("csid:%u, inum:%llu",p_req->csid, p_req->inum);
            rmi=p_mds->handle_pnfs_byterangelock(p_req->csid, p_req->inum, p_req->start, p_req->end ,&p_resp->expires);
            log->debug_log("expires:%u",p_resp->expires);
            p_resp->head.msg_type = PNFS_ByterangeLock_resp;
            break;
        }
        case PNFS_ReleaseLock_req:
        {
            log->debug_log("PNFS_ReleaseLock_req detected.");
            struct PNFS_ReleaseLock_req_t *p_req = (struct PNFS_ReleaseLock_req_t*) req;
            struct PNFS_ReleaseLock_resp_t *p_resp = (struct PNFS_ReleaseLock_resp_t *) resp;
            respsize = sizeof(struct PNFS_ReleaseLock_resp_t);
            //printf("recv:csid=%llu.%llu.%llu\n-------",p_req->csid,p_req->inum,p_req->start);
            log->debug_log("csid:%u, inum:%llu",p_req->csid, p_req->inum);
            rmi=p_mds->handle_pnfs_releaselock(p_req->csid, p_req->inum, p_req->start, p_req->end);
            log->debug_log("done");
            p_resp->head.msg_type = PNFS_ReleaseLock_resp;
            break;
        }
               
        case PNFS_DataserverLayout_req:
        {
            log->debug_log("PNFS_DataserverLayout_req detected.");
            struct PNFS_DataserverLayout_req_t *p_req = (struct PNFS_DataserverLayout_req_t*) req;
            struct PNFS_DataserverLayout_resp_t *p_resp = (struct PNFS_DataserverLayout_resp_t *) resp;
            respsize = sizeof(struct PNFS_DataserverLayout_resp_t);
            rmi=p_mds->handle_pnfs_dataserver_layout(&p_resp->count);
            p_resp->head.msg_type = PNFS_DataserverLayout_resp;
            break;
        }
              
        case PNFS_DataserverAddress_req:
        {
            log->debug_log("PNFS_DataserverAddress_req detected");
            struct PNFS_DataserverAddress_req_t *p_req = (struct PNFS_DataserverAddress_req_t*) req;
            struct PNFS_DataserverAddress_resp_t *p_resp = (struct PNFS_DataserverAddress_resp_t *) resp;
            respsize = sizeof(struct PNFS_DataserverAddress_resp_t);
            // ToDo port
            uint16_t port;
            rmi = p_mds->handle_pnfs_dataserver_address(&p_req->dsid, &p_resp->address,&port);
            p_resp->head.msg_type = PNFS_DataserverAddress_resp;
            break;
        }
        case PNFS_CreateSession_req:
        {
            log->debug_log("PNFS_CreateSession_req detected");
            struct PNFS_CreateSession_req_t *p_req = (struct PNFS_CreateSession_req_t*) req;
            struct PNFS_CreateSession_resp_t *p_resp = (struct PNFS_CreateSession_resp_t *) resp;
            respsize = sizeof(struct PNFS_CreateSession_resp_t);
            //printf("gid:%u.uid:%u\n",p_req->gid,p_req->uid);
            rmi=p_mds->handle_pnfs_create_session(p_req->gid, p_req->uid, &p_resp->csid);
            //printf("Return csid:%llu.uid:%u\n",p_resp->csid,p_req->uid);
            p_resp->head.msg_type = PNFS_CreateSession_resp;
            break;
        }
        
        case MT_helloworld_req:
        {
            struct PNFS_HelloWorld_req_t *p_req = (struct PNFS_HelloWorld_req_t*) req;
            struct PNFS_HelloWorld_resp_t *p_resp = (struct PNFS_HelloWorld_resp_t *) resp;
            respsize = sizeof(struct PNFS_HelloWorld_resp_t);
            handle_helloworld(p_req, p_resp);
            p_resp->head.msg_type = MT_helloworld_resp;
            break;
        }
    }
    p_resp_head->protocol_id = pnfs_id;
    p_resp_head->sequence_number = head->sequence_number;
    p_resp_head->error.major_id = rma;
    p_resp_head->error.minor_id = rmi;
    log->debug_log("rmi=%u, rma=%u, respsize:%u",rmi,rma,respsize);
    return respsize;
}



PET_MINOR PnfsProtocol::handle_helloworld(struct PNFS_HelloWorld_req_t *p_req, struct PNFS_HelloWorld_resp_t *p_resp )
{
    strncpy(&p_resp->data[0], "World\0", 6);
    return 0;
}
