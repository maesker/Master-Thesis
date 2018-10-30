/** 
 * File:   SPNetraid_client.cpp
 * Author: markus
 * 
 * Created on 8. Januar 2012, 17:19
 */

#include "custom_protocols/storage/SPNetraid_client.h"

static bool killthreads;

static ConcurrentQueue<void*>  *p_queue = new ConcurrentQueue<void*>();
static ConcurrentQueue<void*>  *p_queue_bch = new ConcurrentQueue<void*>();

/**
 * @Todo change sleep time
 */
void* networker(void *data)
{
    SPNetraid_client *p_cl = (SPNetraid_client*)data;
    operation *p_op;
    struct OPHead *p_ophead;
    float backoff = THREADING_TIMEOUT_BACKUOFF;
    uint32_t sleeptime = THREADING_TIMEOUT_USLEEP;
    uint8_t  i = 0;
 
    while (!killthreads)
    {
        if (p_cl->popqueue((void**)&p_op))      
        {
            sleeptime = THREADING_TIMEOUT_USLEEP;
            i=0;
            p_ophead = (struct OPHead *) p_op;
            if (p_ophead->type == operation_client_type)
            {
                
                /*
                struct operation_client  *p_clop = (struct operation_client *) p_op;
                //printf("poped.:inum:%llu.\n",p_clop->ophead.inum);
                //printf("stripecnt:%llu\n",p_clop->ophead.stripecnt);
                if (p_clop->ophead.stripecnt=1)
                {
                    //p_cl->analyseParticipants(p_clop);
                    p_cl->perform_operation(p_clop);
                }**/
            }
            else if (p_ophead->type==cl_task_type)
            {
                switch (p_ophead->subtype)
                {
                    case (send_spn_read):
                    {
                        struct dstask_spn_send_read *p_clop = (struct dstask_spn_send_read *) p_op;
                        p_cl->perform_operation(p_clop);
                        
                    }
                }
            }
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
    pthread_exit(0);
}


void* networkIO(void *data)
{
    SPNetraid_client *p_cl = (SPNetraid_client*)data;
    struct spn_task *p_task;
    //SPN_message *p_msg;
    float backoff = THREADING_TIMEOUT_BACKUOFF;
    uint32_t sleeptime = THREADING_TIMEOUT_USLEEP;
    uint8_t  i = 0;
 
    while (!killthreads)
    {
        try
        {
            //printf("networkIO:pop.\n");
            if (p_cl->popqueueMSG(&p_task))
            {
                sleeptime = THREADING_TIMEOUT_USLEEP;
                i=0;
                  p_cl->perform_network_send(p_task);
                  //free(p_task->p_msg);
                  //free(p_task);
               // struct SPHead *p_sphead = (struct SPHead*) p_msg;
                //printf("popqueueMsg:poped.:inum:%llu.\n",p_sphead->ophead.inum);
                //printf("popqueueMsg:stripecnt:%llu\n",p_sphead->ophead.stripecnt); 
                /*if (p_sphead->ophead.stripecnt=1)
                {

                }
                else
                {
                    printf("Invalid stripe count.\n");
                }*/
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
        catch(...)
        {
            printf("networkIO:Error occured.\n");
            if (i%5==0) usleep(THREADING_TIMEOUT_USLEEP);
        }
    }
    pthread_exit(0);
}


/**
 * @Todo change sleep time
 */
void* networkerBCH(void *data)
{
    SPNetraid_client *p_cl = (SPNetraid_client*)data;
    float backoff = THREADING_TIMEOUT_BACKUOFF;
    uint32_t sleeptime = THREADING_TIMEOUT_USLEEP;
    uint8_t  i = 0;
  
    SPNBC_head *p_head;
    while (!killthreads)
    {
        if (p_cl->popqueueBCH((void**) &p_head))
        {
             sleeptime = THREADING_TIMEOUT_USLEEP;
             i=0;
             p_cl->perform_operationBCH(p_head);
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
    pthread_exit(0);
}


SPNetraid_client::SPNetraid_client(Logger *p_log) {
    log = p_log;
    seqnum_mutex = PTHREAD_MUTEX_INITIALIZER;
    killthreads = false;
    sequence_num = 1;
    p_netmsg = new ConcurrentQueue<struct spn_task*>();
    p_ds_manager = new ServerManager(log, SPN_ASIO_BASEPORT);
    p_opman = new OpManager(log,&pushQueue,0,NULL,NULL);
    p_asyn = new AsynClient<SPN_message>(log,p_ds_manager);
    //p_raid = new Libraid5(log);
    p_raid4 = new Libraid4(log);
    p_raid4->register_server_manager(p_ds_manager);   
    
    io_service = new boost::asio::io_service();
    uint16_t port = SPN_ASIO_CLIENT_BACKCHANNEL;
    log->debug_log("port:%u",port);
    p_bch = new asyn_tcp_server<SPNBC_head> (*io_service , port, &pushQueueBCH,log);

    log->debug_log("Components created");
    this->pingpongflag=false;
    this->run();
}

SPNetraid_client::SPNetraid_client(const SPNetraid_client& orig) 
{   
}

SPNetraid_client::~SPNetraid_client() 
{
    log->debug_log("Shutting down");
    killthreads=true;
    delete p_ds_manager;
    delete p_asyn;
    delete p_raid4;    
    //delete p_raid4;    
    delete p_bch;
    log->debug_log("check1");
    delete p_opman;
    pthread_mutex_destroy(&seqnum_mutex);
    //delete p_queue;
    //delete p_queue_bch;
    std::vector<pthread_t>::iterator it = thread_vec.begin();
    for (it;it!=thread_vec.end();it++)
    {
        pthread_join(*it,NULL);
    }
    //io_service->stop();
    delete io_service;
    delete p_netmsg;
    log->debug_log("done");
}

int SPNetraid_client::run()
{
    int i;
    int rc;
    int threadcnt1=BASE_THREADNUMBER;
    int threadcnt2=BASE_THREADNUMBER;
    int threadcnt3=BASE_THREADNUMBER;
    
    pthread_t wthread;
    rc = pthread_create(&wthread, NULL, spn_ioservicerun, io_service );
    thread_vec.push_back(wthread);
    
    for (i=0 ; i < threadcnt1 ; i++)
    {
        pthread_t worker_thread;
        rc = pthread_create(&worker_thread, NULL,networker , this );
        if (rc)
        {
            log->error_log("thread creation failed.");
        }
        thread_vec.push_back(worker_thread);
    }
    log->debug_log("networker threads created");
    for (i=0 ; i < threadcnt2 ; i++)
    {
        pthread_t worker_thread;
        rc = pthread_create(&worker_thread, NULL,networkIO , this );
        if (rc)
        {
            log->error_log("thread creation failed.");
        }
        thread_vec.push_back(worker_thread);
    }        
    log->debug_log("network IO threads created");
    for (i=0 ; i < threadcnt3 ; i++)
    {
        pthread_t worker_thread;
        rc = pthread_create(&worker_thread, NULL,networkerBCH , this );
        if (rc)
        {
            log->error_log("thread creation failed.");
        }
        thread_vec.push_back(worker_thread);
    }
    log->debug_log("done:rc=%d",rc);
    return rc;
}
/*/
uint32_t SPNetraid_client::getSequenceNumber(){
    uint32_t seq;
    pthread_mutex_lock(&seqnum_mutex);
    seq = sequence_num++;
    pthread_mutex_unlock(&seqnum_mutex);
    return seq;
}**/

bool SPNetraid_client::popqueue(void **p_res)
{
    p_queue->wait_and_pop(*p_res);
    //return p_queue->try_pop(*p_res);
    if (*p_res==NULL) return false;
    return true;
}

bool SPNetraid_client::popqueueBCH(void **p_res)
{
    p_queue_bch->wait_and_pop(*p_res);
    if (*p_res==NULL) return false;
    return true;
}

bool SPNetraid_client::popqueueMSG(struct spn_task **p_res)
{
    p_netmsg->wait_and_pop(*p_res);
    if (*p_res==NULL) return false;
    return true;
}

int SPNetraid_client::pushQueue(queue_priorities priority, OPHead *p_head)
{
    int rc=0;
    p_queue->push(p_head);
    return rc;
}

void SPNetraid_client::pushQueueBCH(void *msg)
{
    p_queue_bch->push(msg);
}

int SPNetraid_client::perform_operation(struct operation_composite *p_op)
{
    int rc=-1;
    log->debug_log("start");
    struct OPHead *p_head;
    std::map<uint32_t,operation*>::iterator it = p_op->ops->begin();
    for (it; it!=p_op->ops->end();it++)
    {
        p_head=(struct OPHead*)it->second;
        switch(p_head->type)
        {
            case operation_client_s_write:
            {
                rc = perform_stripewrite((struct operation_client_write*) it->second);
                break;
            }
            case operation_client_su_write:
            {
                rc = perform_stripeunitwrite((struct operation_client_write*) it->second);
                break;
            }
            case operation_client_su_write_direct:
            {
                rc = perform_Direct_write((struct operation_client_write*) it->second);
                break;
            }
        }/*
        
        std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->sumap.begin();
        for (it2;it2!=it->second->sumap.end();it2++)
        {
            log->debug_log("stripeid:%u, stripeunit:%u",it->first,it2->first);
            struct spn_task *task = new struct spn_task;
            struct SPN_Write_req *req = new struct SPN_Write_req;
            //struct SPN_Write_req *req = new struct SPN_Write_req;
            req->head.ophead = p_op->ophead;
            
            req->head.customhead.creation_time = time(0);
            req->head.customhead.msg_type = SP_SmallWrite_req;
            req->head.customhead.protocol_id = spn_id;
            //req->head.customhead.sequence_number = 0;
            req->stripeid = it->second->stripe_id;
            //req->stripeunitid = it2->first;
            req->head.customhead.datalength = it2->second->opsize;
            req->head.participants = it->second->participants;
            req->head.customhead.datablock = malloc(it2->second->opsize);
            memcpy(req->head.customhead.datablock, it2->second->newdata,req->head.customhead.datalength);
            //log->debug_log("text:%s",(char*)req->head.customhead.datablock);
            task->receiver= get_server_id((filelayout_raid*)&req->head.ophead.filelayout[0],req->stripeid, it2->first);
            log->debug_log("send to id:%u, size:%u, text:%s",task->receiver, req->head.customhead.datalength,(char*)req->head.customhead.datablock);
            task->p_msg = (SPN_message*)req;
            log->debug_log("pointer:%p",task->p_msg);
            p_netmsg->push(task);
        }*/
    }
    log->debug_log("done");
    return rc;
}

int SPNetraid_client::perform_operation(struct dstask_spn_send_read  *p_t)
{
    struct spn_task *p_task = new struct spn_task;
    struct SPN_Read_req *req = new struct SPN_Read_req;
    p_task->p_msg = (SPN_message*)req;
    memcpy(&p_task->p_msg->read_request.head.ophead, &p_t->dshead.ophead, sizeof(struct OPHead));
    p_task->p_msg->read_request.head.customhead.creation_time = time(0);
    p_task->p_msg->read_request.head.customhead.msg_type = SP_Read_req;
    p_task->p_msg->read_request.head.customhead.protocol_id = spn_id;
    p_task->p_msg->read_request.head.customhead.datablock = NULL;  
    p_task->p_msg->read_request.head.customhead.datalength = 0;
    p_task->p_msg->read_request.stripeid = p_t->stripeid;
    p_task->p_msg->read_request.stripeunitid = p_t->stripeunitid;
    p_task->p_msg->read_request.head.customhead.datalength = 0;
    p_task->p_msg->read_request.start = p_t->offset;
    p_task->p_msg->read_request.end = p_t->end;
    p_task->receiver= p_t->receiver;    
    p_netmsg->push(p_task);
    log->debug_log("done");
    //delete p_t;
    return 0;
}


int SPNetraid_client::perform_network_send(struct spn_task *p_task)
{
    int rc=0;
    //stringstream ss("");
    struct SPHead *p_head = (struct SPHead*) p_task->p_msg;    
    p_head->customhead.creation_time = time(0);
    p_head->customhead.protocol_id = spn_id;    
    //print_SPN_Message(p_msg,ss);
    //log->debug_log("msg:%s",ss.str().c_str());
    //log->debug_log("val msg pointer:%p.",p_task->p_msg);
    rc = p_asyn->send(p_task->p_msg, p_task->receiver);
    log->debug_log("send returned %u.",rc);
/*    ipaddress_t  addr;
    uint8_t proc = 1;
    p_ds_manager->get_server_address(&server, &addr);
    boost::asio::ip::tcp::socket *p_sock = p_ds_manager->get_socket_entry(&server, &proc, &proc);
    
    log->debug_log(ss.str().c_str());*/
    if (rc==0)
    {
        log->debug_log("csid:%u, seq:%u",p_task->p_msg->read_request.head.ophead.cco_id.csid,p_task->p_msg->read_request.head.ophead.cco_id.sequencenum);
        if (p_task->p_msg!=NULL)
        {
            if (p_head->customhead.datablock!=NULL)
            {
                free(p_head->customhead.datablock);
            }
            free(p_task->p_msg);
        }
        delete p_task;
    }
    return rc;
}

int SPNetraid_client::perform_operationBCH(SPNBC_head* p_head)
{
    int rc=-1;
    log->debug_log("Received operation:%u.%u",p_head->ccoid.csid, p_head->ccoid.sequencenum);
    switch (p_head->customhead.msg_type)
    {
        case (result_msg):
        {
            opstatus result = *(opstatus*)p_head->customhead.datablock;
            log->debug_log("received result msg:result=%u.",result);
            rc = p_opman->client_result(p_head->inum, p_head->ccoid, result);
            delete p_head;
            break;
        }
        case (read_response):
        {
            log->debug_log("received read response msg.:length:%u",p_head->customhead.datalength);
            struct data_object *p_do = new struct data_object;
            p_do->data = malloc(p_head->customhead.datalength);
            if (p_head->customhead.datalength>0)
            {                
                memcpy(&p_do->metadata,(char*)p_head->customhead.datablock, sizeof(p_do->metadata));
                memcpy( p_do->data,    (char*)p_head->customhead.datablock+sizeof(p_do->metadata), p_do->metadata.datalength);
                memcpy(&p_do->checksum,(char*)p_head->customhead.datablock+sizeof(p_do->metadata)+p_do->metadata.datalength, sizeof(p_do->checksum));
                log->debug_log("receivede checksum.:%u",p_do->checksum);
            }           
            free(p_head->customhead.datablock);
            rc = p_opman->client_handle_read_response(p_do, p_head->inum, p_head->stripeid, p_head->stripeunitid, p_head->ccoid);
            delete p_head;
            break;            
        }
        case (spn_pingpong):
        {
            this->pingpongflag=true;
            delete p_head;
            break;
        }
        default:
        {
            log->debug_log("received unknown message type:%u",p_head->customhead.msg_type);
            rc=-2;
            break;
        }
    }/*
    if (p_head->customhead.datablock!=NULL)
    {
        free(p_head->customhead.datablock);
    }
    if (p_head->customhead.ip!=NULL)
    {
        free(p_head->customhead.ip);
    }*/
    //delete p_head;
    log->debug_log("rc:%d",rc);
    return rc;
}


int SPNetraid_client::handle_write_lock(ClientSessionId csid, struct EInode *p_einode, size_t offset, size_t length, void *data)
{
    log->debug_log("inum:%llu,csid:%u,offset:%llu,length:%llu",p_einode->inode.inode_number,csid,offset,length);
    int rc = -1;
    time_t  starttime = time(0);
    
    struct operation_composite *op = new struct operation_composite;
    op->ops = new std::map<uint32_t,operation*>();
    
    op->ophead.cco_id.csid = csid;
    op->ophead.cco_id.sequencenum = 0;// this->getSequenceNumber();    
    op->ophead.inum = p_einode->inode.inode_number;
    op->ophead.offset = offset;
    op->ophead.length = length;
    op->ophead.status = opstatus_init;
    op->ophead.type = operation_client_composite_directwrite_type;
    op->ophead.subtype = 1;
    memcpy(&op->ophead.filelayout, &p_einode->inode.layout_info[0], 256);    
    rc = p_opman->client_insert(op, data);
    if (!rc)
    {
        rc = perform_operation(op);
    }
    else
    {
        log->debug_log("opmanager reported error.");
        op->ophead.status=opstatus_failure;
    }
    bool _continue=true;
    try
    {
        while (_continue)
        {
            switch (op->ophead.status)
            {
                case (opstatus_success):
                {
                    log->debug_log("Operation successful:inum:%llu,csid:%u,offset:%llu,length:%llu",p_einode->inode.inode_number,csid,offset,length);
                    _continue=false;
                    rc=0;
                    break;
                }
                case (opstatus_failure):
                {
                    log->debug_log("Operation failed:inum:%llu,csid:%u,offset:%llu,length:%llu",p_einode->inode.inode_number,csid,offset,length);
                    _continue=false;
                    rc=-1;
                    break;
                }
                default:
                {
                    if (difftime(time(0),starttime)>SPN_OPERATION_TIMEOUT )
                    {
                        log->debug_log("Operation failed:TIMEOUT:inum:%llu,csid:%u,offset:%llu,length:%llu",p_einode->inode.inode_number,csid,offset,length);
                        rc=-1;
                        _continue=false;
                    }
                    else
                    {
                        usleep(THREADING_TIMEOUT_USLEEP);
                    }
                }
            }
            //log->debug_log("status:%u",op->ophead.status);
        }    
    }
    catch(...)
    {
        log->debug_log("spnclient.Error occured...");
    }
    
    p_opman->cleanup(op);
    double timetaken = difftime(time(0),starttime);
    log->debug_log("end, rc=%d, time:%f",rc,timetaken);
    return rc;
}

int SPNetraid_client::handle_write(ClientSessionId csid, struct EInode *p_einode, size_t offset, size_t length, void *data)
{
    log->debug_log("inum:%llu,csid:%u,offset:%llu,length:%llu",p_einode->inode.inode_number,csid,offset,length);
    int rc = -1;
    time_t  starttime = time(0);
    
    struct operation_composite *op = new struct operation_composite;
    op->ops = new std::map<uint32_t,operation*>();
    
    op->ophead.cco_id.csid = csid;
    op->ophead.cco_id.sequencenum = 0;// this->getSequenceNumber();    
    op->ophead.inum = p_einode->inode.inode_number;
    op->ophead.offset = offset;
    op->ophead.length = length;
    op->ophead.status = opstatus_init;
    op->ophead.subtype=0;
    op->ophead.type = operation_client_composite_write_type;
    
    memcpy(&op->ophead.filelayout, &p_einode->inode.layout_info[0], 256);    
    rc = p_opman->client_insert(op, data);
    if (!rc)
    {
        rc = perform_operation(op);
    }
    else
    {
        log->debug_log("opmanager reported error.");
        op->ophead.status=opstatus_failure;
    }
    bool _continue=true;
    try
    {
        while (_continue)
        {
            switch (op->ophead.status)
            {
                case (opstatus_success):
                {
                    log->debug_log("Operation successful:inum:%llu,csid:%u,offset:%llu,length:%llu",p_einode->inode.inode_number,csid,offset,length);
                    _continue=false;
                    rc=0;
                    break;
                }
                case (opstatus_failure):
                {
                    log->debug_log("Operation failed:inum:%llu,csid:%u,offset:%llu,length:%llu",p_einode->inode.inode_number,csid,offset,length);
                    _continue=false;
                    rc=-1;
                    break;
                }
                default:
                {
                    if (difftime(time(0),starttime)>SPN_OPERATION_TIMEOUT )
                    {
                        log->debug_log("Operation failed:TIMEOUT:inum:%llu,csid:%u,offset:%llu,length:%llu",p_einode->inode.inode_number,csid,offset,length);
                        rc=-1;
                        _continue=false;
                    }
                    else
                    {
                        usleep(THREADING_TIMEOUT_USLEEP);
                    }
                }
            }
            //log->debug_log("status:%u",op->ophead.status);
        }    
    }
    catch(...)
    {
        log->debug_log("spnclient.Error occured...");
    }
    
    p_opman->cleanup(op);
    double timetaken = difftime(time(0),starttime);
    log->debug_log("end, rc=%d, time:%f",rc,timetaken);
    return rc;
}

int SPNetraid_client::register_ds_address(serverid_t id, char *ip)
{
    return p_ds_manager->register_server(id,ip);
}

int SPNetraid_client::getDeviceLayouts(std::vector<serverid_t> *ds)
{
    int rc=-1;
    log->debug_log("start");
    for (std::vector<serverid_t>::iterator it = ds->begin(); it!=ds->end(); it++)
    {
        serverid_t i = *it;
        //handle_DeviceLayout(&i);
    }
    log->debug_log("done");
    return rc;
}

int SPNetraid_client::handle_read(ClientSessionId csid, struct EInode *p_einode, size_t offset, size_t length, void **data)
{
    log->debug_log("inum:%llu,csid:%u,offset:%llu,length:%llu",p_einode->inode.inode_number,csid,offset,length);
    int rc = -1;
    time_t  starttime = time(0);
    struct operation_client_read *op = new struct operation_client_read;
    op->ophead.status = opstatus_init;
    op->ophead.cco_id.csid = csid;
    op->ophead.cco_id.sequencenum = 0;//this->getSequenceNumber();    
    op->ophead.inum = p_einode->inode.inode_number;
    op->ophead.offset = offset;
    op->ophead.length = length;
    op->ophead.type = operation_client_read;
    memcpy(&op->ophead.filelayout, &p_einode->inode.layout_info[0], 256);
    op->groups = NULL;    
    op->resultdata = data;
    rc = p_opman->client_insert_read(op,p_einode);
    bool _continue=true;
    try
    {
        while (_continue)
        {
            switch (op->ophead.status)
            {
                case (opstatus_success):
                {
                    log->debug_log("Operation successful");
                    _continue=false;
                    break;
                }
                case (opstatus_failure):
                {
                    log->debug_log("Operation failed.");
                    _continue=false;
                    break;
                }
                default:
                {
                    if (difftime(time(0),starttime)>SPN_OPERATION_TIMEOUT ) 
                    {
                        log->debug_log("Operation failed:TIMEOUT:inum:%llu,csid:%u,offset:%llu,length:%llu",p_einode->inode.inode_number,csid,offset,length);
                        rc=-1;
                        _continue=false;
                    }
                    else
                    {
                        usleep(THREADING_TIMEOUT_USLEEP);
                    }

                }
            }
            //log->debug_log("status:%u",op->ophead.status);
        }    
    }
    catch(...)
    {
        log->debug_log("spnclient.Error occured...");
    }
    log->debug_log("end, rc=%d",rc);
    p_opman->cleanup(op);
    return rc;
}

int SPNetraid_client::perform_stripewrite(struct operation_client_write *p_op)
{
    int rc=-1;
    log->debug_log("inum:%llu",p_op->ophead.inum);
    filelayout_raid *p_fl = (filelayout_raid *) &p_op->ophead.filelayout[0];
   // Stripe_layout *p_sl = p_raid4->get_stripe(p_fl,p_op->ophead.offset);
    
    
    struct spn_task *paritytask = new struct spn_task;
    struct SPN_Write_req *parityreq = new struct SPN_Write_req;
    parityreq->head.ophead = p_op->ophead;
    parityreq->head.ophead.type = operation_client_s_write;        
    parityreq->head.customhead.creation_time = time(0);
    parityreq->head.customhead.msg_type = SP_Write_S_req;
    parityreq->head.customhead.protocol_id = spn_id;
    //parityreq->head.customhead.sequence_number = 0;
    parityreq->stripeid = p_op->group->stripe_id;
    //req->stripeunitid = it2->first;
    
    parityreq->head.customhead.datalength = p_fl->raid4.stripeunitsize;
    parityreq->head.participants = p_op->participants;
    parityreq->head.customhead.datablock = malloc(parityreq->head.customhead.datalength);
    memset(parityreq->head.customhead.datablock,0,parityreq->head.customhead.datalength);
    //log->debug_log("text:%s",(char*)req->head.customhead.datablock);
    paritytask->receiver = get_coordinator((filelayout_raid*)&parityreq->head.ophead.filelayout[0],p_op->ophead.offset);
    //log->debug_log("send to id:%u, size:%u, text:%s",paritytask->receiver, parityreq->head.customhead.datalength,(char*)parityreq->head.customhead.datablock);
    paritytask->p_msg = (SPN_message*)parityreq;
    //log->debug_log("pointer:%p",paritytask->p_msg);    
    
    std::map<StripeUnitId,struct StripeUnit*>::iterator it = p_op->group->sumap->begin();
    for (it; it!=p_op->group->sumap->end(); it++)
    {
        log->debug_log("csid:%u, seq:%u,sid:%u,suid:%u",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum,p_op->group->stripe_id, it->first);
        
        struct spn_task *task = new struct spn_task;
        struct SPN_Write_req *req = new struct SPN_Write_req;
        req->head.ophead = p_op->ophead;
        req->head.ophead.type = operation_client_s_write;  

        req->head.customhead.creation_time = time(0);
        req->head.customhead.msg_type = SP_Write_S_req;
        req->head.customhead.protocol_id = spn_id;
        //req->head.customhead.sequence_number = 0;
        req->stripeid = p_op->group->stripe_id;
        //req->stripeunitid = it2->first;
        req->head.customhead.datalength = it->second->opsize;
        req->head.participants = p_op->participants;
        req->head.customhead.ip = NULL;
        req->head.customhead.datablock = malloc(it->second->opsize);
        memcpy(req->head.customhead.datablock, it->second->newdata,req->head.customhead.datalength);
        calc_parity(req->head.customhead.datablock,parityreq->head.customhead.datablock,parityreq->head.customhead.datablock,parityreq->head.customhead.datalength);
        //log->debug_log("text:%s",(char*)req->head.customhead.datablock);
        task->receiver= get_server_id((filelayout_raid*)&req->head.ophead.filelayout[0],req->stripeid, it->first);
        //log->debug_log("send to id:%u, size:%u, text:%s",task->receiver, req->head.customhead.datalength,(char*)req->head.customhead.datablock);
        task->p_msg = (SPN_message*)req;
        log->debug_log("pointer:%p",task->p_msg);
        p_netmsg->push(task);
    }
    p_netmsg->push(paritytask);
    return rc;
}

int SPNetraid_client::perform_stripeunitwrite(struct operation_client_write *p_op)
{
    int rc=0;
    log->debug_log("inum:%llu",p_op->ophead.inum);
    std::map<StripeUnitId,struct StripeUnit*>::iterator it = p_op->group->sumap->begin();
    for (it; it!=p_op->group->sumap->end(); it++)
    {
        log->debug_log("csid:%u,seq:%u,sid:%u,suid:%u",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum,p_op->group->stripe_id, it->first);    
        struct spn_task *task = new struct spn_task;
        struct SPN_Write_req *req = new struct SPN_Write_req;
        memcpy(&req->head.ophead, &p_op->ophead, sizeof(struct OPHead));
        req->head.ophead.type = operation_client_su_write;        

        req->head.customhead.creation_time = time(0);
        req->head.customhead.msg_type = SP_Write_SU_req;
        req->head.customhead.protocol_id = spn_id;
        req->head.customhead.sequence_number = 0;
        req->stripeid = p_op->group->stripe_id;
        //req->stripeunitid = it2->first;
        req->head.customhead.datalength = it->second->opsize;
        req->head.participants = p_op->participants;
        req->head.customhead.ip = NULL;
        req->head.customhead.datablock = malloc(it->second->opsize);
        memcpy(req->head.customhead.datablock, it->second->newdata,req->head.customhead.datalength);
        //log->debug_log("text:%s",(char*)req->head.customhead.datablock);
        task->receiver= get_server_id((filelayout_raid*)&req->head.ophead.filelayout[0],req->stripeid, it->first);
        //log->debug_log("send to id:%u, size:%u, offset:%llu, text:%s",task->receiver, req->head.customhead.datalength,req->head.ophead.offset,(char*)req->head.customhead.datablock);
        task->p_msg = (SPN_message*)req;
        //log->debug_log("pointer:%p",task->p_msg);
        p_netmsg->push(task);
    }
    return rc;
}

int SPNetraid_client::perform_Direct_write(struct operation_client_write *p_op)
{
    int rc=-1;
    log->debug_log("inum:%llu",p_op->ophead.inum);
    filelayout_raid *p_fl = (filelayout_raid *) &p_op->ophead.filelayout[0];
   // Stripe_layout *p_sl = p_raid4->get_stripe(p_fl,p_op->ophead.offset);
    
    struct spn_task *paritytask = new struct spn_task;
    struct SPN_DirectWrite_req *parityreq = new struct SPN_DirectWrite_req;
    parityreq->head.ophead = p_op->ophead;
    parityreq->head.ophead.type = operation_client_s_write_direct;        
    parityreq->head.customhead.creation_time = time(0);
    parityreq->head.customhead.msg_type = SP_DIRECT_Write_SU_req;
    parityreq->head.customhead.protocol_id = spn_id;
    //parityreq->head.customhead.sequence_number = 0;
    parityreq->stripeid = p_op->group->stripe_id;
    //req->stripeunitid = it2->first;
    
    parityreq->head.customhead.datalength = p_fl->raid4.stripeunitsize;
    parityreq->head.participants = p_op->participants;
    parityreq->head.customhead.datablock = malloc(parityreq->head.customhead.datalength);
    memset(parityreq->head.customhead.datablock,0,parityreq->head.customhead.datalength);
    //log->debug_log("text:%s",(char*)req->head.customhead.datablock);
    paritytask->receiver = get_coordinator((filelayout_raid*)&parityreq->head.ophead.filelayout[0],p_op->ophead.offset);
    //log->debug_log("send to id:%u, size:%u, text:%s",paritytask->receiver, parityreq->head.customhead.datalength,(char*)parityreq->head.customhead.datablock);
    paritytask->p_msg = (SPN_message*)parityreq;
    //log->debug_log("pointer:%p",paritytask->p_msg);    
    p_netmsg->push(paritytask);
    std::map<StripeUnitId,struct StripeUnit*>::iterator it = p_op->group->sumap->begin();
    for (it; it!=p_op->group->sumap->end(); it++)
    {
        log->debug_log("csid:%u, seq:%u,sid:%u,suid:%u",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum,p_op->group->stripe_id, it->first);
        
        struct spn_task *task = new struct spn_task;
        struct SPN_DirectWrite_req *req = new struct SPN_DirectWrite_req;
        req->head.ophead = p_op->ophead;
        req->head.ophead.type = operation_client_s_write;  

        req->head.customhead.creation_time = time(0);
        req->head.customhead.msg_type = SP_DIRECT_Write_SU_req;
        req->head.customhead.protocol_id = spn_id;
        //req->head.customhead.sequence_number = 0;
        req->stripeid = p_op->group->stripe_id;
        //req->stripeunitid = it2->first;
        req->head.customhead.datalength = it->second->opsize;
        req->head.participants = p_op->participants;
        req->head.customhead.ip = NULL;
        req->head.customhead.datablock = malloc(it->second->opsize);
        memcpy(req->head.customhead.datablock, it->second->newdata,req->head.customhead.datalength);
        //calc_parity(req->head.customhead.datablock,parityreq->head.customhead.datablock,parityreq->head.customhead.datablock,parityreq->head.customhead.datalength);
        //log->debug_log("text:%s",(char*)req->head.customhead.datablock);
        task->receiver= get_server_id((filelayout_raid*)&req->head.ophead.filelayout[0],req->stripeid, it->first);
        //log->debug_log("send to id:%u, size:%u, text:%s",task->receiver, req->head.customhead.datalength,(char*)req->head.customhead.datablock);
        task->p_msg = (SPN_message*)req;
        log->debug_log("pointer:%p",task->p_msg);
        p_netmsg->push(task);
    }
    return rc;
}

int SPNetraid_client::handle_pingpong(ClientSessionId csid)
{
    log->debug_log("ping pong");
    struct spn_task *p_task = new struct spn_task;
    struct SPN_Pingpong *req = new struct SPN_Pingpong;

    req->head.ophead.cco_id.csid=csid;
    req->a=0;
    req->b=1;
    p_task->p_msg = (SPN_message*)req;
    
    p_task->p_msg->read_request.head.customhead.creation_time = time(0);
    p_task->p_msg->read_request.head.customhead.msg_type = SP_Pingpong_req;
    p_task->p_msg->read_request.head.customhead.protocol_id = spn_id;
    p_task->p_msg->read_request.head.customhead.datablock = NULL;  
    p_task->p_msg->read_request.head.customhead.datalength = 0;
    p_task->receiver= req->a; 
    p_netmsg->push(p_task);
    
    this->pingpongflag=false;
    try
    {
        while (!this->pingpongflag)
        {
            usleep(THREADING_TIMEOUT_USLEEP);
        }    
    }
    catch(...)
    {
        log->debug_log("spnclient.Error occured...");
    }
    log->debug_log("end.");
    return 0;
}
