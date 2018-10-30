/* 
 * File:   CCCNetraid.cpp
 * Author: markus
 * 
 * Created on 8. Januar 2012, 10:03
 */

#include "custom_protocols/cluster/CCCNetraid.h"

static ConcurrentQueue<void*>  *p_queue = new ConcurrentQueue<void*>();

void* cccworker_threads(void *obj)
{
    CCCNetraid *p_obj = (CCCNetraid *) obj;
   // printf("worker thread: startet.\n");
    void *p;
    
    float backoff = THREADING_TIMEOUT_BACKUOFF;
    uint32_t sleeptime = THREADING_TIMEOUT_USLEEP;
    uint8_t  i = 0;
    struct custom_protocol_reqhead_t *p_custom;
    PET_MINOR rc;
    while (1)
    {
        p=NULL;
        p_queue->wait_and_pop(p);
        
        //if (!p_queue->try_pop(p))
        if (p==NULL)
        {
            if (i<THREADING_TIMEOUT_MAXITER)
            {
                sleeptime = sleeptime*backoff;
                i++;
            }
            else
            {
                sleeptime = THREADING_TIMEOUT_USLEEP;
                i=0;
            }            
            
            if (i%5==0) usleep(sleeptime);
        }
        else
        {
            p_custom = (struct custom_protocol_reqhead_t *)p;
            //std::stringstream ss;
            //print_CCC_Message((CCC_message)p_custom,ss);
            switch (p_custom->protocol_id)
            {
                case (ccc_id):
                {
                  //  printf("XX:%u\n",p_custom->msg_type);
                   // printf("XX:cccid:%u\n",p_custom->protocol_id);                    
                    rc = p_obj->handle_Message((CCC_message*)p);
                    break;
                }                
                case (dsc_id):
                {
                    break;
                }
                case (cpn_id):
                {
                    break;
                }
                case (pnfs_id):
                {
                    break;
                }
                default:
                {
                    printf("Unknown operation type.\n");
                }
            }
            sleeptime = THREADING_TIMEOUT_USLEEP;
            i=0;
        }
    }
}

void* ccc_ioservicerun(void *ios)
{
    boost::asio::io_service *io_service = (boost::asio::io_service *) ios;
    io_service->run();
}

CCCNetraid::CCCNetraid(std::string logdir, serverid_t sid, void (*p_cb)(void *)) 
{
    log = new Logger();
    std::string logfile = logdir + "CCC_server.log";
    log->set_log_location(logfile);
    serverid = sid;
    cb = p_cb;
            
    boost::asio::io_service *io_service = new boost::asio::io_service();
    uint16_t port = CCC_ASIO_BASEPORT+serverid;
    log->debug_log("port:%u",port);
    asyn_tcp_server<CCC_message> *p_asyn = new asyn_tcp_server<CCC_message> (*io_service , port, &pushQueue,log);
    
    int rc;
       pthread_t wthread;
    for(int i = 0; i < BASE_THREADNUMBER ; i++)
    {
     
        rc = pthread_create(&wthread, NULL, cccworker_threads, this );
        if (rc)
        {
            // throw an exception on fail
            log->error_log("thread not created:rc=%d.",rc);
        }
        log->debug_log("thread created:%d",rc);
    }
    
    rc = pthread_create(&wthread, NULL, ccc_ioservicerun, io_service );
 
    //io_service.run();
    log->debug_log("done.");
}

CCCNetraid::CCCNetraid(const CCCNetraid& orig) {
}

CCCNetraid::~CCCNetraid() {
}

uint32_t CCCNetraid::get_max_msg_len(){
    return maximal_message_length;
}

void CCCNetraid::pushQueue(void *op)
{    
    p_queue->push(op);
}

int CCCNetraid::handle_CCC_cl_smallwrite(struct CCC_cl_smallwrite *p_msg)
{
    int rc=-1;
    log->debug_log("got request for inum:%llu, from ds:%u.",p_msg->ophead.inum, p_msg->sender);
   // std::stringstream ss("");
   // print_struct_participant(&p_msg->head.sender_bf,ss);
    //log->debug_log("participant:%s",ss.str().c_str());
    
    /*struct operation_primcoordinator *p_prim = new struct operation_primcoordinator;
    p_prim->datamap_primco = new std::map<StripeId,struct datacollection_primco*>;
    
    struct datacollection_primco *dc = new struct datacollection_primco;
    dc->paritymap = new std::map<uint32_t,struct paritydata*>;
    dc->existing = NULL;
    dc->newobject = NULL;
    dc->finalparity = NULL;
    memset(&dc->versionvec,0,sizeof(dc->versionvec));   
    p_prim->datamap_primco->insert(std::pair<StripeId,struct datacollection_primco*>(p_msg->stripeid,dc));
    p_prim->ophead=p_msg->ophead;
    p_prim->ophead.type = operation_primcoord_type;
    p_prim->recv_timestamp = time(0);
    p_prim->participants = p_msg->participants;
    p_prim->received_from = p_msg->head.sender_bf;
    p_prim->datalength = 0;
    p_prim->ophead.status = opstatus_init; 
     **/
    struct dstask_ccc_received *p_task = new struct dstask_ccc_received;
    p_task->dshead.ophead       = p_msg->ophead;
    p_task->dshead.ophead.subtype = received_received;
    p_task->dshead.ophead.type  = ds_task_type;
    p_task->participants        = p_msg->participants;
    p_task->recv_from           = p_msg->head.sender_bf;
    //p_task->sender              = p_msg->sender;
    p_task->stripeid            = p_msg->stripeid;
    cb(p_task);
    log->debug_log("done.");
    return 0;
}

int CCCNetraid::handle_CCC_stripewrite_cancommit(struct CCC_cl_stripewrite_cancommit *p_msg)
{
    int rc=-1;
    log->debug_log("got request for inum:%llu, csid:%u,seq:%u",p_msg->ophead.inum,p_msg->ophead.cco_id.csid,p_msg->ophead.cco_id.sequencenum);
    log->debug_log("got request for inum:%llu, from ds:%u.",p_msg->ophead.inum, p_msg->sender);
   // std::stringstream ss("");
   // print_struct_participant(&p_msg->head.sender_bf,ss);
    //log->debug_log("participant:%s",ss.str().c_str());
 
    struct dstask_ccc_stripewrite_cancommit_received *p_task = new struct dstask_ccc_stripewrite_cancommit_received;
    p_task->dshead.ophead       = p_msg->ophead;
    p_task->dshead.ophead.subtype = received_ccc_stripewrite_cancommit;
    p_task->dshead.ophead.type  = ds_task_type;
    p_task->recv_from           = p_msg->head.sender_bf;
    p_task->version             = p_msg->version;
    cb(p_task);
    log->debug_log("done.");
    return 0;
}

int CCCNetraid::handle_CCC_prepare(struct CCC_prepare *p_msg)
{
    log->debug_log("got request for inum:%llu, csid:%u,seq:%u",p_msg->ophead.inum,p_msg->ophead.cco_id.csid,p_msg->ophead.cco_id.sequencenum);
    struct dstask_ccc_recv_prepare *p_task = new struct dstask_ccc_recv_prepare;
    p_task->dshead.ophead = p_msg->ophead;
    p_task->dshead.ophead.type = ds_task_type;
    p_task->dshead.ophead.subtype = received_prepare;    
    
    cb(p_task);
    log->debug_log("done.");
    return 0;
}

int CCCNetraid::handle_CCC_docommit(struct CCC_docommit *p_msg)
{
    log->debug_log("got request for inum:%llu, csid:%u,seq:%u",p_msg->ophead.inum,p_msg->ophead.cco_id.csid,p_msg->ophead.cco_id.sequencenum);
    log->debug_log("got request for inum:%llu, datalength:%u",p_msg->ophead.inum,p_msg->head.customhead.datalength);
    struct operation_dstask_docommit *p_part = new struct operation_dstask_docommit;
    memcpy(&p_part->dshead.ophead,&p_msg->ophead,sizeof(struct OPHead));
    p_part->dshead.ophead.type = ds_task_type;
    p_part->dshead.ophead.subtype = received_docommit;
    memcpy(&p_part->vvmap,p_msg->head.customhead.datablock, p_msg->head.customhead.datalength);
    uint32_t *vv;
    vv=(uint32_t*)p_msg->head.customhead.datablock;
    log->debug_log("sid:%u.",*vv);
    
    log->debug_log("vvmap:stripeid:%u,v0:%u,v1:%u,v2:%u,v3:%u",p_part->vvmap.stripeid,p_part->vvmap.versionvector[0],p_part->vvmap.versionvector[1],p_part->vvmap.versionvector[2],p_part->vvmap.versionvector[3]);
    cb(p_part);
    log->debug_log("done.");
    return 0;
}

int CCCNetraid::handle_CCC_result(struct CCC_result *p_msg)
{
    log->debug_log("got request for inum:%llu, csid:%u,seq:%u",p_msg->ophead.inum,p_msg->ophead.cco_id.csid,p_msg->ophead.cco_id.sequencenum);
    struct dstask_proccess_result  *p_part = new struct dstask_proccess_result;
    p_part->dshead.ophead = p_msg->ophead;
    p_part->dshead.ophead.type = ds_task_type;
    p_part->dshead.ophead.subtype = received_result;
    cb(p_part);
    log->debug_log("done.");
    return 0;
}

int CCCNetraid::handle_CCC_committed(struct CCC_committed *p_msg)
{
    log->debug_log("got request for inum:%llu, csid:%u,seq:%u",p_msg->ophead.inum,p_msg->ophead.cco_id.csid,p_msg->ophead.cco_id.sequencenum);
    std::stringstream ss("");
    print_struct_participant(&p_msg->head.sender_bf,ss);
    log->debug_log("received from:%s",ss.str().c_str());
    struct dstask_ccc_recv_committed *p_canc = new struct dstask_ccc_recv_committed;
    p_canc->dshead.ophead=p_msg->ophead;
    p_canc->dshead.ophead.type = ds_task_type;
    p_canc->dshead.ophead.subtype = received_committed;
    p_canc->recv_from = p_msg->head.sender_bf;
    p_canc->recv_ts = time(0);
    p_canc->dshead.ophead.status = opstatus_committed;
    cb(p_canc);
    free(p_msg);
    log->debug_log("done.");
    return 0;
}

int CCCNetraid::handle_CCC_cancommit(struct CCC_cancommit *p_msg)
{
    int rc=-1;
    log->debug_log("got request for inum:%llu, csid:%u,seq:%u",p_msg->ophead.inum,p_msg->ophead.cco_id.csid,p_msg->ophead.cco_id.sequencenum);
    //std::stringstream ss("");
    //print_struct_participant(&p_msg->head.sender_bf,ss);
    //log->debug_log("received from:%s",ss.str().c_str());
    struct dstask_cancommit *p_t = new struct dstask_cancommit;
    p_t->dshead.ophead=p_msg->ophead;
    p_t->length = p_msg->head.customhead.datalength;
    p_t->dshead.ophead.type = ds_task_type;
    p_t->dshead.ophead.subtype = received_cancommit;
    p_t->recv_ts = time(0);
    p_t->recv = p_msg->head.customhead.datablock;
    p_t->recv_from = p_msg->head.sender_bf;
    p_t->stripeid = p_msg->stripeip;
    p_t->version = p_msg->nextversion;
    log->debug_log("next version %u",p_t->version);
    cb(p_t);
    log->debug_log("done.");
    return 0;
}


int CCCNetraid::handle_CCC_pingpong(struct CCC_cl_pingpong *p_msg)
{
    int rc=-1;
    log->debug_log("got request for csid:%u,seq:%u",p_msg->ophead.cco_id.csid,p_msg->ophead.cco_id.sequencenum);
    //std::stringstream ss("");
    //print_struct_participant(&p_msg->head.sender_bf,ss);
    //log->debug_log("received from:%s",ss.str().c_str());
    
    struct dstask_pingpong *p_t = new struct dstask_pingpong ;    
    p_t->sender = p_msg->sender;
    p_t->counter = p_msg->counter;
    p_t->dshead.ophead=p_msg->ophead;

    p_t->dshead.ophead.type = ds_task_type;
    p_t->dshead.ophead.subtype = dspingpong;
    
    cb(p_t);
    log->debug_log("done.");
    return 0;
}

int CCCNetraid::handle_Message(CCC_message *p_msg)
{
    int rc=-1;
    log->debug_log("start");
    struct CCCHead *p_head = (struct CCCHead*) p_msg;
    switch(p_head->customhead.msg_type)
    {
        log->debug_log("type:%u",p_head->customhead.msg_type);
        
        case (MT_CCC_cl_smallwrite) :
        {
            rc = this->handle_CCC_cl_smallwrite((struct CCC_cl_smallwrite*)p_msg);
            break;
        }
        case (MT_CCC_prepare):
        {
            rc = this->handle_CCC_prepare((struct CCC_prepare *)p_msg);
            break;
        }
        case (MT_CCC_cancommit):
        {
            rc = this->handle_CCC_cancommit((struct CCC_cancommit *)p_msg);
            break;
        }
        case (MT_CCC_docommit):
        {
            rc = this->handle_CCC_docommit((struct CCC_docommit *)p_msg);
            break;
        }
        case (MT_CCC_committed):
        {
            rc = this->handle_CCC_committed((struct CCC_committed *)p_msg);
            break;
        }
        case (MT_CCC_result):
        {
            rc = this->handle_CCC_result((struct CCC_result *)p_msg);
            break;
        }
        case (MT_CCC_stripewrite_cancommit):
        {
            rc = this->handle_CCC_stripewrite_cancommit((struct CCC_cl_stripewrite_cancommit*)p_msg);
            break;
        }
        case (MT_CCC_pingpong):
        {
            rc = this->handle_CCC_pingpong((struct CCC_cl_pingpong *)p_msg);
            break;
        }
        default:
        {
            log->debug_log("Unknown message type:%u.",p_head->customhead.msg_type);
            rc = -2;
        }
    }   
    log->debug_log("rc:%d",rc);
    return rc;
}
