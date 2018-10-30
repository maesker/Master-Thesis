/** 
 * File:   SPNBC_client.cpp
 * Author: markus
 * 
 * Created on 8. Januar 2012, 17:19
 */

#include "custom_protocols/storage/SPNBC_client.h"


static ConcurrentQueue<SPNBC_head*>  *p_queue = new ConcurrentQueue<SPNBC_head*>();

/**
 * @Todo change sleep time
 */
static void* networker(void *data)
{
    float backoff = THREADING_TIMEOUT_BACKUOFF;
    uint32_t sleeptime = THREADING_TIMEOUT_USLEEP;
    uint8_t  i = 0;
  
    SPNBC_client *p_cl = (SPNBC_client*)data;
    struct SPNBC_head *p_head;
    while (true)
    {
        if (p_cl->popqueue(&p_head))
        {
            p_cl->perform_network_send(p_head);
            sleeptime = THREADING_TIMEOUT_USLEEP;
            i=0;
        }
        else
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
    }
}


SPNBC_client::SPNBC_client(Logger *p_log) {
    log = p_log;
    p_cl_manager = new ServerManager(log, SPN_ASIO_CLIENT_BACKCHANNEL, false);
    p_asyn = new AsynClient<SPNBC_head>(log,p_cl_manager);
    this->run();
}

SPNBC_client::SPNBC_client(const SPNBC_client& orig) {
}

SPNBC_client::~SPNBC_client() {
}

int SPNBC_client::run()
{
    int i;
    int rc;
    int threadcnt1=BASE_THREADNUMBER;
    for (i=0 ; i < threadcnt1 ; i++)
    {
        pthread_t worker_thread;
        rc = pthread_create(&worker_thread, NULL,networker , this );
        if (rc)
        {
            log->error_log("thread creation failed.");
        }
    }    
    return rc;
}

bool SPNBC_client::popqueue(SPNBC_head **p_res)
{
    p_queue->wait_and_pop(*p_res);
    if (*p_res==NULL) return false;
    return true;
}


int SPNBC_client::perform_network_send(struct SPNBC_head *p_head)
{
    p_head->customhead.protocol_id = spnbc_id;    
    int rc=p_asyn->send(p_head,p_head->ccoid.csid);
    /*
    //stringstream ss("");
    struct SPHead *p_head = (struct SPHead*) &p_task->p_msg;
    
    p_head->customhead.creation_time = time(0);
    p_head->customhead.protocol_id = spn_id;
    
    //print_SPN_Message(p_msg,ss);
    //log->debug_log("msg:%s",ss.str().c_str());

    rc = p_asyn->send(&p_task->p_msg, p_task->receiver);*/
    log->debug_log("send returned %u.",rc);
    
/*    ipaddress_t  addr;
    uint8_t proc = 1;
    p_ds_manager->get_server_address(&server, &addr);
    boost::asio::ip::tcp::socket *p_sock = p_ds_manager->get_socket_entry(&server, &proc, &proc);
    log->debug_log("server:%u=%s",server,addr);
    log->debug_log(ss.str().c_str());*/
    if (rc==0)
    {
        log->debug_log("Freeing msg csid:%u,seq:%u",p_head->ccoid.csid,p_head->ccoid.sequencenum);
        if (p_head->customhead.datablock!=NULL) free(p_head->customhead.datablock);
        delete p_head;
    }
    return rc;
}

int SPNBC_client::handle_result(InodeNumber inum, CCO_id ccoid, opstatus result)
{
    log->debug_log("csid:%u, seq:%u,result:%u",ccoid.csid,ccoid.sequencenum,result);
    int rc = 0;
    struct SPNBC_head *p_msg = new struct SPNBC_head;
    memcpy(&p_msg->ccoid,&ccoid,sizeof(struct CCO_id));
    p_msg->inum                    = inum;
    p_msg->customhead.msg_type     = result_msg;
    p_msg->customhead.datalength   = sizeof(opstatus);
    opstatus *p_res                = (opstatus *) malloc(sizeof(opstatus));
    *p_res                         = result;
    p_msg->customhead.datablock    = p_res;
    p_queue->push(p_msg);
    log->debug_log("end, rc=%d, inum:%llu",rc,p_msg->inum);
    return rc;
}


int SPNBC_client::handle_read_response(InodeNumber inum, StripeId sid, StripeUnitId suid, CCO_id ccoid, struct data_object *obj)
{
    log->debug_log("csid:%u",ccoid.csid);
    struct SPNBC_head *p_msg = new struct SPNBC_head;
    if (obj!=NULL)
    {
        size_t fullsize = sizeof(obj->checksum)+sizeof(obj->metadata)+obj->metadata.datalength;
        void *fullblock = malloc(fullsize);
        memcpy(fullblock, &obj->metadata, sizeof(obj->metadata));
        memcpy((char*)fullblock+sizeof(obj->metadata), obj->data, obj->metadata.datalength);
        memcpy((char*)fullblock+sizeof(obj->metadata)+obj->metadata.datalength, &obj->checksum, sizeof(obj->checksum));
        log->debug_log("sending with checksum:%u",*(uint32_t*)((char*)fullblock+sizeof(obj->metadata)+obj->metadata.datalength));
        
        p_msg->customhead.datalength   = fullsize;
        p_msg->customhead.datablock    = fullblock;
    }
    else
    {
        p_msg->customhead.datalength   = 0;
        p_msg->customhead.datablock    = malloc(1);
    }
    
    memcpy(&p_msg->ccoid,&ccoid,sizeof(struct CCO_id));
    p_msg->inum                    = inum;
    p_msg->customhead.msg_type     = read_response;
    p_msg->stripeid                = sid;
    p_msg->stripeunitid            = suid;
    //log->debug_log("seq:%u,data:%s",ccoid.sequencenum,obj->data);
    p_queue->push(p_msg);
    log->debug_log("end, sid:%u,suid:%u",sid,suid);
    return 0;
}



int SPNBC_client::register_cl_address(serverid_t id, char *ip)
{
    return p_cl_manager->register_server(id,ip);
}


int SPNBC_client::handle_pingpong(CCO_id ccoid)
{
    log->debug_log("csid:%u, seq:%u",ccoid.csid,ccoid.sequencenum);
    int rc = 0;
    struct SPNBC_head *p_msg = new struct SPNBC_head;
    memcpy(&p_msg->ccoid,&ccoid,sizeof(struct CCO_id));
    p_msg->customhead.msg_type     = spn_pingpong;
    p_msg->customhead.datalength   = 0;
    p_msg->customhead.datablock    = NULL;
    p_queue->push(p_msg);
    log->debug_log("end");
    return rc;
}

