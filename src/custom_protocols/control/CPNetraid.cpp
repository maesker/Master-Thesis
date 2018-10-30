/* 
 * File:   CPNetraid.cpp
 * Author: markus
 * 
 * Created on 10. Januar 2012, 18:51
 */

#include "custom_protocols/control/CPNetraid.h"

CPNetraid::CPNetraid() : IProtocol() {
}

CPNetraid::CPNetraid(const CPNetraid& orig):IProtocol() {
}

CPNetraid::~CPNetraid() {
}

uint32_t CPNetraid::get_max_msg_len(){
    return maximal_message_length;
}

void    CPNetraid::register_dataserver(DataServer *p_ds)
{
    this->p_ds = p_ds;
}


size_t  CPNetraid::handle_request(custom_protocol_reqhead_t *head, void *req, void *resp)
{
    PET_MAJOR rma = 0;
    PET_MINOR rmi = 0;
    size_t respsize = 0;
    struct custom_protocol_resphead_t *p_resp_head = (struct custom_protocol_resphead_t*) resp;
    //printf("%lu:Current\n%lu:Msg.\n",time(NULL),head->creation_time);
    if ((head->creation_time + MSG_TIMEOUT_CONTROL_SEC) >= time(NULL))
    {
        switch (head->msg_type)
        {
            case CP_DistricuteLock_req:
            {
                printf("distribute lock detectd.\n");
                struct CPN_LockDistribution_req_t *p_req = (struct CPN_LockDistribution_req_t*) req;
                struct CPN_LockDistribution_resp_t *p_resp = (struct CPN_LockDistribution_resp_t *) resp;
                respsize = sizeof( struct CPN_LockDistribution_resp_t);
                rmi = this->p_ds->register_lock_object(&p_req->lock);
                p_resp->head.msg_type = CP_DistricuteLock_resp;
                break;
            }
            case MT_helloworld_req:
            {
                printf("hello world detected.\n");
                struct CPN_HelloWorld_req_t *p_req = (struct CPN_HelloWorld_req_t*) req;
                struct CPN_HelloWorld_resp_t *p_resp = (struct CPN_HelloWorld_resp_t *) resp;
                respsize = sizeof(struct CPN_HelloWorld_resp_t);
                //handle_helloworld(p_req, p_resp);
                p_resp_head->msg_type = MT_helloworld_resp;
                break;
            }
            default:
            {
                rma = MAJOR_UNKNOWN_MSG;
                p_resp_head->msg_type = MT_unknownmsg_resp;
            }
        }
    }
    else
    {
        rma = MAJOR_TIMEDOUT;
    }
    p_resp_head->sequence_number = head->sequence_number;
    p_resp_head->error.major_id = rma;
    p_resp_head->error.minor_id = rmi;
    return respsize;
}

PET_MINOR CPNetraid::handle_helloworld(struct CPN_HelloWorld_req_t *p_req, struct CPN_HelloWorld_resp_t *p_resp )
{
    strncpy(&p_resp->data[0], "World\0", 6);
    return 0;
}
