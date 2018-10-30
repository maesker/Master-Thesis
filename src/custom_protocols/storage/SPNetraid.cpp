/* 
 * File:   SPNetraid.cpp
 * Author: markus
 * 
 * Created on 8. Januar 2012, 10:03
 */

#include "custom_protocols/storage/SPNetraid.h"

/**
 * @brief 
 */
static ConcurrentQueue<void*>  *p_queue = new ConcurrentQueue<void*>();

void* worker_threads(void *obj)
{
    SPNetraid *p_obj = (SPNetraid *) obj;
   // printf("worker thread: startet.\n");
    void *p;
    struct custom_protocol_reqhead_t *p_custom;
    PET_MINOR rc;
    
    float backoff = THREADING_TIMEOUT_BACKUOFF;
    uint32_t sleeptime = THREADING_TIMEOUT_USLEEP;
    uint8_t  i = 0;
    while (1)
    {
        p=NULL;
        p_queue->wait_and_pop(p);
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
            usleep(sleeptime);
        }
        else
        {
            sleeptime = THREADING_TIMEOUT_USLEEP;
            i=0;
            p_custom = (struct custom_protocol_reqhead_t *)p;
            switch (p_custom->protocol_id)
            {
                case (spn_id):
                {
                    rc = p_obj->handle_Message((SPN_message*)p);
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
            //free_custom_protocol_reqhead_t(p_custom);
        }
    }
}

/*void* spn_ioservicerun(void *ios)
{
    boost::asio::io_service *io_service = (boost::asio::io_service *) ios;
    io_service->run();
}**/

SPNetraid::SPNetraid(std::string basedir, serverid_t sid, void (*p_cb)(void *)) {
    log = new Logger();
    basedir += "SP_server.log";
    log->set_log_location(basedir.c_str());
    serverid = sid;
    p_raid = new Libraid5(log);
    cb = p_cb;
            
    boost::asio::io_service *io_service = new boost::asio::io_service();
    uint16_t port = SPN_ASIO_BASEPORT+serverid;
    log->debug_log("port:%u",port);
    asyn_tcp_server<SPN_message> *p_asyn = new asyn_tcp_server<SPN_message> (*io_service , port, &pushQueue,log);
    
    int rc;
    for(int i = 0; i < BASE_THREADNUMBER ; i++)
    {
        pthread_t wthread;
        rc = pthread_create(&wthread, NULL, worker_threads, this );
        if (rc)
        {
            // throw an exception on fail
            log->error_log("thread not created:rc=%d.",rc);
        }
        log->debug_log("thread created:%d",rc);
    }
    
    pthread_t wthread;
    rc = pthread_create(&wthread, NULL, spn_ioservicerun, io_service );
    log->debug_log("done on port:%u.",port);
}

SPNetraid::SPNetraid(const SPNetraid& orig) {
}

SPNetraid::~SPNetraid() {
}

void  SPNetraid::register_dataserver(DataServer *p_ds)
{
    this->p_ds = p_ds;
}

uint32_t SPNetraid::get_max_msg_len(){
    return maximal_message_length;
}

void SPNetraid::pushQueue(void *op)
{    
    p_queue->push(op);
}

PET_MINOR SPNetraid::handle_SPN_WriteSU_req(struct SPN_Write_req *p_msg)
{
    log->debug_log("Received msg: inum=%llu, offset:%llu",p_msg->head.ophead.inum,p_msg->head.ophead.offset);
    //std::stringstream ss;
    //print_SPN_Message((SPN_message*)p_msg,ss);
    //std::cout<<ss.str();
    
    struct dstask_spn_recv *p_task = new struct dstask_spn_recv;
    p_task->dshead.ophead = p_msg->head.ophead;
    p_task->dshead.ophead.subtype = received_spn_write_su;
    p_task->participants = p_msg->head.participants;    
    p_task->stripeid = p_msg->stripeid;
    p_task->data = p_msg->head.customhead.datablock;
    p_task->length = p_msg->head.customhead.datalength;
    log->debug_log("%p:%p",p_task->data,p_msg->head.customhead.datablock);
    //log->debug_log("text:%s",(char*) p_task->data);
    
    cb((void*) p_task);
    log->debug_log("done");
    return 0;
}

PET_MINOR SPNetraid::handle_SPN_DirectWriteSU_req(struct SPN_DirectWrite_req *p_msg)
{
    log->debug_log("Received msg: inum=%llu, offset:%llu",p_msg->head.ophead.inum,p_msg->head.ophead.offset);
    //std::stringstream ss;
    //print_SPN_Message((SPN_message*)p_msg,ss);
    //std::cout<<ss.str();
    
    struct dstask_spn_recv *p_task = new struct dstask_spn_recv;
    p_task->dshead.ophead = p_msg->head.ophead;
    p_task->dshead.ophead.subtype = received_spn_directwrite_su;
    p_task->participants = p_msg->head.participants;    
    p_task->stripeid = p_msg->stripeid;
    p_task->data = p_msg->head.customhead.datablock;
    p_task->length = p_msg->head.customhead.datalength;
    log->debug_log("%p:%p",p_task->data,p_msg->head.customhead.datablock);
    //log->debug_log("text:%s",(char*) p_task->data);
    
    cb((void*) p_task);
    log->debug_log("done");
    return 0;
}

PET_MINOR SPNetraid::handle_SPN_WriteS_req(struct SPN_Write_req *p_msg)
{
    log->debug_log("Received msg: inum=%llu, offset:%llu",p_msg->head.ophead.inum,p_msg->head.ophead.offset);
    //std::stringstream ss;
    //print_SPN_Message((SPN_message*)p_msg,ss);
    //std::cout<<ss.str();
    
    struct dstask_spn_recv *p_task = new struct dstask_spn_recv;
    p_task->dshead.ophead = p_msg->head.ophead;
    p_task->dshead.ophead.subtype = received_spn_write_s;
    p_task->participants = p_msg->head.participants;    
    p_task->stripeid = p_msg->stripeid;
    p_task->data = p_msg->head.customhead.datablock;
    p_task->length = p_msg->head.customhead.datalength;
    log->debug_log("%p:%p",p_task->data,p_msg->head.customhead.datablock);
    log->debug_log("Datalength:%u",p_task->length);
    //log->debug_log("text:%s",(char*) p_task->data);
    
    cb((void*) p_task);
    log->debug_log("done");
    return 0;
}

PET_MINOR SPNetraid::handle_SPN_read_req(struct SPN_Read_req *p_msg)
{
    log->debug_log("Received msg: inum=%llu, offset:%llu",p_msg->head.ophead.inum,p_msg->head.ophead.offset);
    //std::stringstream ss;
    //print_SPN_Message((SPN_message*)p_msg,ss);
    //std::cout<<ss.str();
    
    struct dstask_spn_recv *p_task = new struct dstask_spn_recv;
    p_task->dshead.ophead = p_msg->head.ophead;
    p_task->dshead.ophead.subtype = received_spn_read;
    //p_task->participants = p_msg->head.participants;    
    p_task->stripeid = p_msg->stripeid;
    p_task->data = NULL;
    //p_task->length = p_msg->head.customhead.datalength;   
    cb((void*) p_task);
    return 0;
}


/*
PET_MINOR SPNetraid::handle_create_participant_operation(struct SPN_Write_req *p_msg)
{
    log->debug_log("start.");
}

PET_MINOR SPNetraid::handle_create_primcoordinator_operation(struct SPN_Write_req *p_msg)
{
    log->debug_log("start.");
}

PET_MINOR SPNetraid::handle_create_seccoordinator_operation(struct SPN_Write_req *p_msg)
{
    PET_MINOR rc=0;
    log->debug_log("start.");
    //log->debug_log("Received:%p.\n",p_msg->datablock);
    struct operation_seccoordinator  *p_secop = new struct operation_seccoordinator;
    memcpy (&p_secop->ophead,&p_msg->head.ophead,sizeof(struct OPHead));
    p_secop->recv_timestamp = p_msg->head.customhead.creation_time;
    p_secop->old_block = NULL; //check cache
    p_secop->recv_block = p_msg->head.customhead.datablock;
    p_secop->paritydiff = NULL;
    p_secop->status = opstatus_init;
    p_secop->datalength = p_msg->datalength;
    p_secop->participants = p_msg->head.participants;    
    p_secop->ophead.type = operation_seccoord_type;
    
    //std::stringstream ss;
    //print_op_seccoordinator(p_secop,ss);
    //std::cout<<ss.str();
    //log->debug_log("%s.",ss.str());
    cb((void*) p_secop);
    log->debug_log("rC=%d,%s:::",rc,(char *)p_secop->recv_block);
    return rc;
}*/

PET_MINOR SPNetraid::handle_Message(SPN_message *p_msg)
{
    PET_MINOR rc=-1;
    log->debug_log("start");
    try
    {     
        struct SPHead *p_head = (struct SPHead*) p_msg;
        p_head->ophead.type = ds_task_type;
        if (p_head->customhead.ip!=NULL)
        {
            handle_SPN_endpointreg(p_head->customhead.ip, p_head->ophead.cco_id.csid);
            log->debug_log("IP:%s registered for %u",p_head->customhead.ip,p_head->ophead.cco_id.csid);
        }
        switch(p_head->customhead.msg_type)
        {
            case (SP_Write_SU_req):
            {
                rc = handle_SPN_WriteSU_req((struct SPN_Write_req*)p_msg);
                break;
            }
            case (SP_Write_S_req):
            {            
                rc = handle_SPN_WriteS_req((struct SPN_Write_req*)p_msg);
                break;
            }
            case (SP_Read_req):
            {
                rc = handle_SPN_read_req((SPN_Read_req*) p_msg);
                break;
            }
            case (SP_DIRECT_Write_SU_req):
            {
                rc = handle_SPN_DirectWriteSU_req((struct SPN_DirectWrite_req*)p_msg);
                break;
            }
            case (SP_Pingpong_req):
            {
                rc = handle_SPN_Pingpong_req((struct SPN_Pingpong*)p_msg);
                break;
            }
            default:
            {
                log->debug_log("Unknown message type.:%u",p_head->customhead.msg_type);
                rc = -2;
            }
        }   
    }
    catch (...)
    {
        log->error_log("Error occured during operation...");
        rc=-3;
    }
    return rc;
}


void SPNetraid::handle_SPN_endpointreg(char *ip, serverid_t server)
{    
    log->debug_log("is this used.");
    struct dstask_endpointreg *p_task = new struct dstask_endpointreg;
    p_task->dshead.ophead.cco_id.csid = server;
    p_task->dshead.ophead.subtype = regendpoint;
    p_task->dshead.ophead.type = ds_task_type;
    p_task->ip = ip;    
    cb((void*) p_task);
}



PET_MINOR SPNetraid::handle_SPN_Pingpong_req(struct SPN_Pingpong  *p_msg)
{
    log->debug_log("Received pingpong");
    struct dstask_pingpong *p_task = new struct dstask_pingpong;
    p_task->dshead.ophead = p_msg->head.ophead;
    p_task->dshead.ophead.subtype = dspingpong;
    p_task->counter=0;
    p_task->sender=p_msg->b;
        
    cb((void*) p_task);
    log->debug_log("done");
    return 0;
}