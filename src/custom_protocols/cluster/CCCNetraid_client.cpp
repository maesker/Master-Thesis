/** 
 * File:   CCCNetraid_client.cpp
 * Author: markus
 * 
 * Created on 8. Januar 2012, 17:19
 */

#include "custom_protocols/cluster/CCCNetraid_client.h"


static ConcurrentQueue<void*>  *p_queue = new ConcurrentQueue<void*>();

/**
 * @Todo change sleep time
 */
void* cccnetworker(void *data)
{
    CCCNetraid_client *p_cl = (CCCNetraid_client*)data;
    CCC_message *p_msg;
    
    float backoff = THREADING_TIMEOUT_BACKUOFF;
    uint32_t sleeptime = THREADING_TIMEOUT_USLEEP;
    uint8_t  i = 0;
    while (true)
    {
        try
        {
            p_msg=NULL;
            if (p_cl->popqueue((void**) &p_msg))
            {
                p_cl->perform(p_msg);
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
        catch (...)
        {
             printf("ERROR:::CCC::: cought...");
        }
    }
}


/*void* cccnetworkIO(void *data)
{
    CCCNetraid_client *p_cl = (CCCNetraid_client*)data;
    CCC_message *p_msg;
    while (true)
    {
        if (p_cl->popqueueMSG(&p_msg))
        {
            struct CCCHead *p_ccchead = (struct CCCHead*) p_msg;
            //printf("popqueueMsg:poped.:inum:%llu.\n",p_sphead->ophead.inum);
            //printf("popqueueMsg:stripecnt:%llu\n",p_sphead->ophead.stripecnt); 
            if (p_ccchead->ophead.stripecnt=1)
            {
                p_cl->perform_network_send(p_msg);
            }
            else
            {
                printf("Invalid stripe count.\n");
            }
        }
        else
        {
            //printf("networkIO:sleep.\n");
            usleep(THREADING_TIMEOUT_USLEEP);
        }
    }
}
**/

CCCNetraid_client::CCCNetraid_client(Logger *p_log, ServerManager *p_sm, serverid_t id, bool dsync) {
    log = p_log;
    this->id = id;
    dosync = dsync;
    seqnum_mutex = PTHREAD_MUTEX_INITIALIZER;
    sequence_num = 1;
    //p_netmsg = new ConcurrentQueue<CCC_message*>;
    if (p_sm==NULL)
    {        
        log->error_log("Server manager needed.");
    }
    else
    {
        p_ds_manager = p_sm;
    }
    p_raid = new Libraid4(log);
    p_asyn = new AsynClient<CCC_message>(log,p_ds_manager);
    this->run();
}

CCCNetraid_client::CCCNetraid_client(const CCCNetraid_client& orig) {
}

CCCNetraid_client::~CCCNetraid_client() {
}

int CCCNetraid_client::run()
{
    int i;
    int rc;
    int threadcnt1=2;
    
    for (i=0 ; i < threadcnt1 ; i++)
    {
        pthread_t worker_thread;
        rc = pthread_create(&worker_thread, NULL,cccnetworker , this );
        if (rc)
        {
            log->error_log("thread creation failed.");
        }
    }
    return rc;
}

uint32_t CCCNetraid_client::getSequenceNumber(){
    uint32_t seq;
    pthread_mutex_lock(&seqnum_mutex);
    seq = sequence_num++;
    pthread_mutex_unlock(&seqnum_mutex);
    return seq;
}

bool CCCNetraid_client::popqueue(void **p_res)
{
    p_queue->wait_and_pop(*p_res);
    if (*p_res==NULL) return false;
    return true;
}

/*bool CCCNetraid_client::popqueueMSG(CCC_message **p_res)
{
    return this->p_netmsg->try_pop(*p_res);
}*/

void CCCNetraid_client::pushQueue(CCC_message *p_msg)
{
    p_queue->push(p_msg);
}



int CCCNetraid_client::perform(CCC_message *p_msg)
{
    int rc=-1;
    //stringstream ss("");
    struct CCCHead *p_head = (struct CCCHead*) p_msg;
    p_head->customhead.protocol_id = ccc_id;
    p_head->customhead.sequence_number = getSequenceNumber();
    if (p_head->customhead.msg_type==MT_CCC_committed)
    {
        struct CCC_committed *p_m = (struct CCC_committed *) p_msg;
        std::map<StripeId,struct dataobject_collection*>::iterator it = p_m->partop->datamap->begin();
        for (it;it!=p_m->partop->datamap->end(); it++)
        {
            while (!it->second->fhvalid) usleep(100);
            if(dosync)
            {
                fsync(it->second->filehandle);
                log->debug_log("flushed...");
            }
            close(it->second->filehandle);
            log->debug_log("File closed:inum:%llu",p_m->ophead.inum);
        }
    }
    log->debug_log("msg_type=%u for %u",p_head->customhead.msg_type,p_head->receiver);
    rc = p_asyn->send(p_msg, p_head->receiver);
    log->debug_log("send returned %u",rc);
    
/*    ipaddress_t  addr;
    uint8_t proc = 1;
    p_ds_manager->get_server_address(&server, &addr);
    boost::asio::ip::tcp::socket *p_sock = p_ds_manager->get_socket_entry(&server, &proc, &proc);
    log->debug_log("server:%u=%s",server,addr);
    log->debug_log(ss.str().c_str());*/
    if (rc==0)
    {
        if (p_head->customhead.datablock!=NULL)
        {
            free(p_head->customhead.datablock);
        }
        delete p_msg;
    }
    log->debug_log("rc:%d",rc);
    return rc;
}


int  CCCNetraid_client::handle_CCC_cl_smallwrite(struct dstask_ccc_send_received *p_task)
{
    int rc=0;
    struct CCC_cl_smallwrite *p_msg = new struct CCC_cl_smallwrite;
    memcpy(&p_msg->ophead, &p_task->dshead.ophead, sizeof(struct OPHead));
    p_msg->sender = this->id;
    p_msg->head.receiver = p_task->receiver;
    p_msg->participants = p_task->participants;
    p_msg->head.customhead.datablock=NULL;
    p_msg->head.customhead.datalength=0;
    p_msg->head.customhead.msg_type=MT_CCC_cl_smallwrite;
    memset(&p_msg->head.sender_bf,0,sizeof(struct Participants_bf));
    filelayout_raid *fl = (filelayout_raid*) &p_msg->ophead.filelayout;
    StripeUnitId suid = p_raid->get_my_stripeunitid(fl,p_msg->ophead.offset, this->id);
    participant_settrue(&p_msg->head.sender_bf,suid);
    p_msg->stripeid = p_raid->getStripeId(fl,p_msg->ophead.offset);
    //std::stringstream ss("");
    //print_struct_participant(&p_msg->head.sender_bf,ss);
    //log->debug_log("participant:%s",ss.str().c_str());
    log->debug_log("suid:%u",suid);
    log->debug_log("got request for inum:%llu, from ds:%u for %u.",p_msg->ophead.inum, p_msg->sender,p_msg->head.receiver);
    pushQueue((CCC_message*)p_msg);
    return rc;
}


int  CCCNetraid_client::handle_CCC_stripewrite_cancommit(struct dstask_ccc_stripewrite_cancommit *p_task)
{
    int rc=0;    
    struct CCC_cl_stripewrite_cancommit *p_msg = new struct CCC_cl_stripewrite_cancommit;
    memcpy(&p_msg->ophead, &p_task->dshead.ophead, sizeof(struct OPHead));
    p_msg->sender = this->id;
    p_msg->head.receiver = p_task->receiver;
    p_msg->head.customhead.datablock=NULL;
    p_msg->head.customhead.datalength=0;
    p_msg->head.customhead.msg_type=MT_CCC_stripewrite_cancommit;
    memset(&p_msg->head.sender_bf,0,sizeof(struct Participants_bf));
    filelayout_raid *fl = (filelayout_raid*) &p_msg->ophead.filelayout;
    StripeUnitId suid = p_raid->get_my_stripeunitid(fl,p_msg->ophead.offset, this->id);
    participant_settrue(&p_msg->head.sender_bf,suid);
    p_msg->stripeid = p_raid->getStripeId(fl,p_msg->ophead.offset);
    p_msg->version = p_task->version;
    //std::stringstream ss("");
    //print_struct_participant(&p_msg->head.sender_bf,ss);
    //log->debug_log("participant:%s",ss.str().c_str());
    log->debug_log("suid:%u",suid);
    log->debug_log("got request for inum:%llu, from ds:%u for %u.",p_msg->ophead.inum, p_msg->sender,p_msg->head.receiver);
    pushQueue((CCC_message*)p_msg);
    return rc;
}

int  CCCNetraid_client::handle_CCC_send_prepare(struct dstask_ccc_send_prepare *p_task)
{
    int rc=0;
    struct CCC_prepare *p_msg = new struct CCC_prepare;
    memcpy(&p_msg->ophead, &p_task->dshead.ophead, sizeof(struct OPHead));
    p_msg->head.receiver = p_task->receiver;
    p_msg->ophead.type = operation_participant_type;
    p_msg->ophead.subtype = MT_CCC_prepare;
    p_msg->head.customhead.datablock=NULL;
    p_msg->head.customhead.datalength=0;
    p_msg->head.customhead.msg_type=MT_CCC_prepare;

    log->debug_log("got request for inum:%llu.",p_msg->ophead.inum);
    pushQueue((CCC_message*)p_msg);
    return rc;
}

int  CCCNetraid_client::handle_CCC_send_docommit(struct operation_dstask_docommit *p_task)
{
    int rc=0;
    struct CCC_docommit *p_msg = new struct CCC_docommit;
    memcpy(&p_msg->ophead, &p_task->dshead.ophead, sizeof(struct OPHead));
    p_msg->head.receiver = p_task->receiver;
    p_msg->ophead.type = operation_participant_type;
    p_msg->ophead.subtype = send_docommit;
    p_msg->head.customhead.msg_type=MT_CCC_docommit;
    p_msg->head.customhead.datablock = malloc(sizeof(struct vvecmap));
    memcpy(p_msg->head.customhead.datablock,&p_task->vvmap,sizeof(struct vvecmap));
    p_msg->head.customhead.datalength=sizeof(struct vvecmap);    
    log->debug_log("got request for inum:%llu, sid:%u:v1:%u, length:%u.",p_msg->ophead.inum,p_task->vvmap.stripeid, p_task->vvmap.versionvector[1],p_msg->head.customhead.datalength);
    uint32_t *vv;
    vv=(uint32_t*)p_msg->head.customhead.datablock;
    log->debug_log("sid:%u.",*vv);
    log->debug_log("data pointer:%p",p_msg->head.customhead.datablock);
    pushQueue((CCC_message*)p_msg);
    return rc;
}
int  CCCNetraid_client::handle_CCC_send_result(struct dstask_ccc_send_result *p_task)
{
    int rc=0;
    log->debug_log("send result to %u",p_task->receiver);
    struct CCC_result *p_msg = new struct CCC_result;
    memcpy(&p_msg->ophead, &p_task->dshead.ophead, sizeof(struct OPHead));
    p_msg->head.receiver = p_task->receiver;
    p_msg->ophead.type = operation_participant_type;
    p_msg->ophead.subtype = send_result;
    p_msg->head.customhead.datablock=NULL;
    p_msg->head.customhead.datalength=0;
    p_msg->head.customhead.msg_type=MT_CCC_result;
    memset(&p_msg->head.sender_bf,0,sizeof(struct Participants_bf));
    filelayout_raid *fl = (filelayout_raid*) &p_msg->ophead.filelayout;
    StripeUnitId suid = p_raid->get_my_stripeunitid(fl,p_msg->ophead.offset, this->id);
    participant_settrue(&p_msg->head.sender_bf,suid);

    log->debug_log("got request for inum:%llu.",p_msg->ophead.inum);
    pushQueue((CCC_message*)p_msg);
    return rc;
}

int CCCNetraid_client::handle_CCC_send_cancommit(struct operation_participant *p_part)
{
    log->debug_log("start for csid:%u",p_part->ophead.cco_id.csid);
    std::map<StripeId,struct dataobject_collection*>::iterator it = p_part->datamap->begin();
    for (it; it!=p_part->datamap->end(); it++)
    {
        struct CCC_cancommit *p_msg = new struct CCC_cancommit;
        memcpy(&p_msg->ophead, &p_part->ophead, sizeof(struct OPHead));
        p_msg->head.receiver = p_part->primcoordinator;
        p_msg->ophead.type = operation_participant_type;
        p_msg->ophead.subtype = MT_CCC_cancommit;
        p_msg->head.customhead.datablock=malloc(it->second->recv_length);
        memcpy(p_msg->head.customhead.datablock,it->second->parity_data,it->second->recv_length);
        log->debug_log("datablock:%p.",p_msg->head.customhead.datablock);
        p_msg->head.customhead.datalength=it->second->recv_length;
        p_msg->head.customhead.msg_type=MT_CCC_cancommit;
        p_msg->nextversion = it->second->mycurrentversion+1;
        log->debug_log("nextversion:%u",p_msg->nextversion);
        p_msg->stripeip = it->first;
        memset(&p_msg->head.sender_bf,0,sizeof(struct Participants_bf));
        filelayout_raid *fl = (filelayout_raid*) &p_msg->ophead.filelayout;
        StripeUnitId suid = p_raid->get_my_stripeunitid(fl,p_msg->ophead.offset, this->id);
        participant_settrue(&p_msg->head.sender_bf,suid);
        log->debug_log("send for inum:%llu, suid:%u",p_msg->ophead.inum,suid);
        pushQueue((CCC_message*)p_msg);
    }
    log->debug_log("done");
    return 0;
}

int  CCCNetraid_client::handle_CCC_send_committed(struct operation_participant *p_part)
{
    int rc=0;
    struct CCC_committed *p_msg = new struct CCC_committed;
    memcpy(&p_msg->ophead, &p_part->ophead, sizeof(struct OPHead));
    p_msg->head.receiver = p_part->primcoordinator;
    p_msg->ophead.type = operation_participant_type;
    p_msg->ophead.subtype = MT_CCC_committed;
    p_msg->head.customhead.datablock=NULL;
    p_msg->head.customhead.datalength=0;
    p_msg->head.customhead.msg_type=MT_CCC_committed;
    p_msg->partop = p_part;
    
    memset(&p_msg->head.sender_bf,0,sizeof(struct Participants_bf));
    filelayout_raid *fl = (filelayout_raid*) &p_msg->ophead.filelayout;
    StripeUnitId suid = p_raid->get_my_stripeunitid(fl,p_msg->ophead.offset, this->id);
    participant_settrue(&p_msg->head.sender_bf,suid);
    log->debug_log("got request for inum:%llu,suid:%u.",p_msg->ophead.inum,suid);
    pushQueue((CCC_message*)p_msg);
    return rc;
}

int  CCCNetraid_client::handle_CCC_cl_pingping(struct dstask_pingpong *p_task)
{
    int rc=0;
    struct CCC_cl_pingpong *p_msg = new struct CCC_cl_pingpong;
    memcpy(&p_msg->ophead, &p_task->dshead.ophead, sizeof(struct OPHead));
    p_msg->sender = this->id;
    p_msg->head.receiver = p_task->sender;
    p_msg->head.customhead.datablock=NULL;
    p_msg->head.customhead.datalength=0;
    p_msg->head.customhead.msg_type=MT_CCC_pingpong;
    p_msg->counter = p_task->counter+1;
    
    log->debug_log("ping cnt:%u",p_msg->counter);
    pushQueue((CCC_message*)p_msg);
    delete p_task;
    return rc;
}
