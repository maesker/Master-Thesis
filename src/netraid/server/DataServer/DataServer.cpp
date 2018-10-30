/**
 * @file DataServer.cpp
 * @class DataServer
 * @author Markus Maesker, maesker@gmx.net
 * @date 20.12.11
 *
 * @brief Dataserver source, doing the actual work.
 *
 * */

#include "server/DataServer/DataServer.h"

//#define CONFFILE "../conf/main_ds.conf"

pthread_mutex_t queue_0_trylockmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_cccin_trylockmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_spnin_trylockmutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t queue0_trylockmutex = PTHREAD_MUTEX_INITIALIZER;

void* ds_tasks_worker(void *obj);

static ConcurrentQueue<void*>  *p_queue_spn_in = new ConcurrentQueue<void*>();
static ConcurrentQueue<void*>  *p_queue_ccc_in = new ConcurrentQueue<void*>();
static ConcurrentQueue<void*>  *p_queue_0 = new ConcurrentQueue<void*>();
static ConcurrentQueue<void*>  *p_queue_1 = new ConcurrentQueue<void*>();
static ConcurrentQueue<void*>  *p_queue_2 = new ConcurrentQueue<void*>();
static ConcurrentQueue<void*>  *p_queue_3 = new ConcurrentQueue<void*>();
static ConcurrentQueue<void*>  *p_queue_maintenance = new ConcurrentQueue<void*>();

/** 
 * @brief worker thread of 
 */
void* ds_storage_ops(void *obj)
{
    DataServer *p_ds = (DataServer *) obj;
    //printf("ds_storage_ops: startet.\n");
    struct OPHead *p_head;
    void *p;
    float backoff = THREADING_TIMEOUT_BACKUOFF;
    uint32_t sleeptime = THREADING_TIMEOUT_USLEEP;
    uint8_t  i = 0;
    uint32_t rc=0;
    while (1)
    {
        p = p_ds->popOperation();
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
            rc=-4;
            p_head = (struct OPHead *)p;
            switch (p_head->type)
            {
                case (ds_task_type):
                {
                    rc=p_ds->handle_ds_task((struct dstask_head *) p_head);
                    break;
                }
                case (operation_primcoord_type):
                {
                    //rc = p_ds->handle_primcoordinator_task(p_head);
                    break;
                }
                case (operation_participant_type):
                {
                    //rc = p_ds->handle_participator_task(p_head);
                    break;
                }
                case (operation_client_type):
                {
                    //rc=p_ds->handle_incomming_storage(p_head);
                    printf("received client type...");
                    break;
                } 
                default:
                {
                    printf("Unknown operation type:%u.\n",p_head->type);
                    rc=3;
                }
            }
            if (rc)
            {
                printf("Error occured:rc=%u\n",rc);
                //exit(0);
            }
            sleeptime = THREADING_TIMEOUT_USLEEP;
            i=0;
        }
    }
}

static int global_gcinterval = 5;

static void pushtask_garbage_collector()
{
       struct dstask_maintenance_gc *p_task = new struct dstask_maintenance_gc;
       p_task->dshead.ophead.type = ds_task_type;    
       p_task->dshead.ophead.subtype = maintenance_garbagecollection;
       p_queue_maintenance->push((void*) p_task);
}

void* maintenence_feeder(void *p)
{
    while (1)
    {
        pushtask_garbage_collector();
        sleep(global_gcinterval);
    }
}


/**
 * @brief Return the singleton instance. Creates the instance if called
 * for the first time.
 * */
DataServer *DataServer::p_instance=NULL;

DataServer *DataServer::create_instance(std::string logdir,std::string basedir, serverid_t id)
{
    if(p_instance == NULL)
    {
        // if no instance existing, create one
        std::string conf = string(DS_CONFIG_FILE);
        basedir = std::string("");
        p_instance = new DataServer(basedir,conf,logdir, id);
    }
    // return MetadataServer instance
    return p_instance;
} // end of get_instance


/** 
 * @brief returns the DataServer instance
 * @return DataServer instance
 */
DataServer *DataServer::get_instance()
{
    return p_instance;
}


/**
 * @brief DS Constructor
 * 
 * */
DataServer::DataServer(
    std::string basedir,
    std::string conffile,
    std::string logdir,
    serverid_t id): AbstractComponent(basedir,conffile)
{
    int rc;
    p_profiler->function_start();
    p_profiler->function_sleep();
    p_profiler->function_wakeup();
    p_profiler->function_sleep();
    p_profiler->function_wakeup();
    p_cm->register_option("mds","Metadata Server ip address");
    p_cm->register_option("storage","Storage directory");
    p_cm->register_option("fsync", "Force fsync after write");
    p_cm->register_option("gcinterval", "Garbage collecter interval in seconds");
    p_cm->parse();
    bool dosync =  (p_cm->get_value("fsync").compare("1")==0) ? true : false;
    gcinterval = atoi(p_cm->get_value("gcinterval").c_str());
    global_gcinterval = gcinterval;
    std::string logfile = logdir+"Dataserver.log";
    
    worker_threads_storage = BASE_THREADNUMBER*2;
    log->set_log_location(logfile);
    
    this->id = id;
    std::string storagedir = p_cm->get_value("storage");
    log->debug_log("storage dir:%s.",storagedir.c_str());
    p_fileio = new Filestorage(log,storagedir.c_str(), id,dosync);
    p_docache = new doCache(log,p_fileio,id);
    mdsid = 0;
    
    p_brlman = new ByterangeLockManager(log);
    p_raid = new Libraid4(log);
    p_opman = new OpManager(log,&pushOperation,id,p_docache, p_fileio);
    p_sm = new ServerManager(log,CCC_ASIO_BASEPORT);
    p_spnbc = new SPNBC_client(log);
    p_ccc = new CCCNetraid_client(log, p_sm, id,dosync);
    p_pnfs_cl   = new Pnfsdummy_client(log);    

    serverid_t a = 0;
    rc = p_pnfs_cl->addMDS(a,p_cm->get_value("mds"),DEF_MDS_PORT+a);
    if (rc)
    {
        log->error_log("could not add mds: rc=%d",rc);
    }
    log->debug_log("done.");
    p_profiler->function_end();      
} // end constructor

/**
 * @brief Deconstructor of the MDS. Subtreemanager, journal manager and
 * mlt handler singleton instances are not deleted.
 * */
DataServer::~DataServer()
{
    log->debug_log(" Stopping metadata server" );
    delete log;
    delete p_cm;
    delete p_profiler;
    delete p_fileio;
    delete p_docache;
    delete p_brlman;
    delete p_raid;
    delete p_opman;
    delete p_sm;
    delete p_spnbc;
    delete p_ccc;
    delete p_pnfs_cl;
} // end if deconstructor


/** 
 * @brief stops the MetadataServer
 * @return 0 on success
 */
int32_t DataServer::stop()
{
    int32_t rc = 0;
    p_profiler->function_start();
    set_status(stopped);
    log->debug_log("stopping metadata server");
    p_profiler->function_end();
    return rc;
}

/**
 * @brief Start the fsal-zmq worker threads. The number of threads is
 * defined by the 'threads' object variable.
 * @return integer representing pthread_create rc.
 * */
int32_t DataServer::run()
{
    p_profiler->function_start();
    log->debug_log( "Starting metadata server threads." );
    int32_t rc = -1;
    try
    {
        int i;
        if (get_status()!=started)
        {
            for(i = 0; i < worker_threads_storage; i++)
            {
                pthread_t worker_thread;
                rc = pthread_create(&worker_thread, NULL, ds_storage_ops, this );
                if (rc)
                {
                    // throw an exception on fail
                    log->error_log("ds thread not created:rc=%d.",rc);
                    throw DataServerException("storage worker thread not created.");
                }
                log->debug_log("thread created:%d",rc);
            }
            pthread_t worker_thread;
            rc = pthread_create(&worker_thread, NULL, maintenence_feeder, NULL );
            
        }
    }
    catch (DataServerException e)
    {
        log->error_log("Exceptions raised: %s\n",e.what());
    }
    set_status(started);
    log->debug_log("Created garbage collector maintanence task");
    p_profiler->function_end();
    return rc;
} // end MetadataServer::run()


serverid_t DataServer::getId() 
{
    return this->id;
}


/** 
 * @brief Sanity check for the needed components
 * 
 * @details Call the Inherited verify method to test the inherited components
 * and performs sanity check on the instantiated components.
 * 
 * @return 0 if one check fails, 1 on success
 * */
int16_t DataServer::verify()
{
    p_profiler->function_start();
    int rc = 1;
    if (!AbstractComponent::verify())
    {
        log->error_log("AbstractComponent verify failed.");
        rc = 0;
    }
    if (get_status()!=started)
    {
        log->error_log("Invalid status detected.");
        rc = 0;
    }
    p_profiler->function_end();
    return rc;
}


int DataServer::getDataserverAddress(serverid_t mds,serverid_t dsid)
{
    log->debug_log("from mds:%u for %u",mds,dsid);
    int rc;
    ipaddress_t address;    
    rc = p_pnfs_cl->handle_pnfs_send_dataserver_address(&mds, &dsid, &address);
    if (!rc)
    {
        log->debug_log("getAddress:%s.",&address[0]);
        rc = p_sm->register_server(dsid, &address[0]);        
        if (rc)
        {
            log->error_log("could not register dataserver: rc=%d",rc);
        }
    }
    else
    {
        log->error_log("Error occured:rc=%d",rc);
        rc=-1;
    }
    log->debug_log("rc:%u",rc);
    return rc;
}
        
PET_MINOR DataServer::handle_DeviceLayout(serverid_t *id)
{
    PET_MINOR rc = 0;
    *id = getId();
    log->debug_log("handle_DeviceLayout:id:%u, rc:%u",*id,rc);
    return rc;
}

/** */
int32_t DataServer::register_lock_object(struct lock_exchange_struct *p_lock)
{
    int32_t rc=-1;
    try
    {
        rc = this->p_brlman->importLock(p_lock);
        log->debug_log("Registered lock:%d\n",rc);
    }
    catch(...)
    {
        log->error_log("Exception in lock register\n"); 
    }
    log->debug_log("rc=%d",rc);
    return rc;
}


int DataServer::pushOperation(queue_priorities priority, OPHead *op)
{
    int rc = 0;
    try
    {
        switch (priority)
        {
            case realtimetask:
            {
                p_queue_0->push((void*)op);
                break;
            }
            case prim_recv:
            {
                p_queue_1->push((void*)op);
                break;
            }
            case sec_recv:
            {
                p_queue_2->push(op);
                break;
            }
            case part_recv:
            {
                p_queue_3->push(op);
                break;
            }
            case maintenance:
            {
                p_queue_maintenance->push(op);
                break;
            }
            default:
            {
                rc = -2;
            }            
        }            
    }
    catch (int)
    {
        rc=-3;
    }
    return rc;
}

void DataServer::push_spn_in(void *op)
{
    p_queue_spn_in->push(op);
}

void DataServer::push_ccc_in(void *op)
{
    p_queue_ccc_in->push(op);
}

void* DataServer::popOperation()
{
    void *p = NULL;
    if (pthread_mutex_trylock(&queue_0_trylockmutex)==0)
    {
        p_queue_0->wait_and_pop(p);
        pthread_mutex_unlock(&queue_0_trylockmutex);
        return p;        
    }
    if (pthread_mutex_trylock(&queue_cccin_trylockmutex)==0)
    {
        p_queue_ccc_in->wait_and_pop(p);
        pthread_mutex_unlock(&queue_cccin_trylockmutex);
        return p;
    } 
    if (p_queue_1->try_pop(p))
    {
        return p;
    }
    if (p_queue_2->try_pop(p))
    {
        return p;
    }
    if (p_queue_3->try_pop(p))
    {
        return p;
    }
    if (pthread_mutex_trylock(&queue_spnin_trylockmutex)==0)
    {
        p_queue_spn_in->wait_and_pop(p);
        pthread_mutex_unlock(&queue_spnin_trylockmutex);
        return p;
    }    
    if (p_queue_maintenance->try_pop(p))
    {
        return p;
    }
    return p;
}

/*int  DataServer::handle_primcoordinator_task(struct OPHead *p_head)
{
    int rc = -1;
    log->debug_log("start:inum=%llu",p_head->inum);
    struct operation_primcoordinator *p_op = (struct operation_primcoordinator*)p_head;
    rc = p_opman->dataserver_prim_operation(p_op);
    log->debug_log("rc:%d",rc);
    return rc;
}

int  DataServer::handle_participator_task(struct OPHead *p_head)
{
    int rc = -1;
    log->debug_log("start:inum=%llu",p_head->inum);
    switch (p_head->subtype)
    {
        case (MT_CCC_prepare):
        {
            break;
        }
        case (MT_CCC_docommit):
        {
            //rc = handle_CCC_docommit(p_head);
            break;
        }
        case (MT_CCC_result):
        {
            rc = handle_CCC_result(p_head);
            break;
        }
        default:
        {
            rc=-2;
            log->debug_log("unknown participator task.");
        }
    }
//    struct operation_primcoordinator *p_op = (struct operation_primcoordinator*)p_head;
//    struct CCO_id *opid = &p_op->ophead.cco_id;
  //  rc = p_opman->dataserver_prim_operation(p_op);
    log->debug_log("rc:%d",rc);
    return rc;
}*/

int DataServer::get_primcoordinator(struct OPHead *p_head, serverid_t *server)
{
    int rc=-1;
    filelayout_raid *fl = (filelayout_raid*) &p_head->filelayout;
    *server = get_coordinator(fl,p_head->offset);
    if (!p_sm->exists(*server))
    {
        rc = getDataserverAddress(mdsid,*server);
    }
    else
    {
        rc = 0;
    }
    log->debug_log("primary coordinator is :%u",*server);
    return rc;    
}

int DataServer::perform_CCC_cl_smallwrite(struct dstask_ccc_send_received *p_task)
{
    int rc=-1;
    log->debug_log("start:sending received.");
    serverid_t primcoord;
    rc = get_primcoordinator(&p_task->dshead.ophead, &primcoord);
    p_task->receiver=primcoord;
    rc = p_ccc->handle_CCC_cl_smallwrite(p_task);
    log->debug_log("rc:%d",rc);
    return rc;
}

int DataServer::perform_CCC_stripewrite_cancommit(struct dstask_ccc_stripewrite_cancommit *p_task)
{
    int rc=-1;
    log->debug_log("start:sending received.");
    serverid_t primcoord;
    rc = get_primcoordinator(&p_task->dshead.ophead, &primcoord);
    p_task->receiver=primcoord;
    rc = p_ccc->handle_CCC_stripewrite_cancommit(p_task);
    log->debug_log("rc:%d",rc);
    return rc;
}

int DataServer::perform_CCC_send_prepare(struct dstask_ccc_send_prepare *p_task)
{
    int rc=-1;
    log->debug_log("start:sending prepare to %u.",p_task->receiver);
    if (!p_sm->exists(p_task->receiver))
    {
        rc = getDataserverAddress(mdsid,p_task->receiver);
    }
    rc = p_ccc->handle_CCC_send_prepare(p_task);
    log->debug_log("rc:%d",rc);
    return rc;
}

int DataServer::perform_CCC_send_docommit(struct operation_dstask_docommit *p_task)
{
    int rc=-1;
    log->debug_log("start:sending docommit to %u.",p_task->receiver);
    if (!p_sm->exists(p_task->receiver))
    {
        rc = getDataserverAddress(mdsid,p_task->receiver);
    }
    rc = p_ccc->handle_CCC_send_docommit(p_task);
    log->debug_log("rc:%d",rc);
    return rc;
}

int DataServer::perform_CCC_send_result(struct dstask_ccc_send_result *p_task)
{
    int rc=-1;
    log->debug_log("start:sending result to %u.",p_task->receiver);
    if (!p_sm->exists(p_task->receiver))
    {
        rc = getDataserverAddress(mdsid,p_task->receiver);
    }
    rc = p_ccc->handle_CCC_send_result(p_task);
    log->debug_log("rc:%d",rc);
    return rc;
}

int DataServer::handle_CCC_prepare(struct dstask_ccc_recv_prepare *p_task)
{
    log->debug_log("recv prepare for %u",p_task->dshead.ophead.cco_id.csid);
    struct operation_participant *p_part = NULL;
    int rc = p_opman->handle_prepare_msg(p_task,&p_part);
    if (!rc)
    {
        rc = calculate_parity_block(p_part);
        if (!rc)
        {
            log->debug_log("parity ok.");
            rc = get_primcoordinator(&p_part->ophead, &p_part->primcoordinator);
            if (!rc)
            {
                    rc = p_ccc->handle_CCC_send_cancommit(p_part);
                    log->debug_log("handle cancommit done:%d",rc);
            }
            else
            {
                log->debug_log("Error occured while retriving primcoord.");
            }
        }
        else
        {
            log->warning_log("Error occured.");
        }
        p_part->ophead.status=opstatus_prepare;
        log->debug_log("STATUS:csid:%u,seq:%u,status:%u",p_part->ophead.cco_id.csid,p_part->ophead.cco_id.sequencenum, p_part->ophead.status);

    }
    else
    {
        log->debug_log("opmanager reported error:%d",rc);
    }
    log->debug_log("rc:%d",rc);
    return rc;
}

int DataServer::handle_CCC_docommit(struct operation_dstask_docommit *p_task)
{
    struct operation_participant *part_out=NULL;
    int rc = p_opman->handle_docommit_msg(p_task, &part_out);
    if (!rc)
    {
        log->debug_log("write to disk");
        rc = p_fileio->write_file(part_out);
        if (!rc)
        {
            log->debug_log("write ok");
            part_out->ophead.status = opstatus_committed;
            rc = get_primcoordinator(&part_out->ophead, &part_out->primcoordinator);            
            if (!rc)
            {
                rc = p_ccc->handle_CCC_send_committed(part_out);
            }
            else
            {
                log->debug_log("Error occured while retriving primcoord.");
            }
        }
        else
        {
            log->warning_log("Error occured while writing file.");
        }
    }
    else
    {
        log->debug_log("Operation manager reported error.");
    }
    part_out->ophead.status=opstatus_docommit;
    log->debug_log("STATUS:csid:%u,seq:%u,status:%u",part_out->ophead.cco_id.csid,part_out->ophead.cco_id.sequencenum, part_out->ophead.status);
    return rc;
}

int DataServer::handle_CCC_result(struct dstask_proccess_result *p_task)
{
    log->debug_log("result for inum:%llu, csid:%u, seq:%u",p_task->dshead.ophead.inum,p_task->dshead.ophead.cco_id.csid,p_task->dshead.ophead.cco_id.sequencenum);
    struct operation_participant *part_out=NULL;
    int rc = p_opman->handle_result_msg(p_task, &part_out);
    if (!rc)
    {
        log->debug_log("Result of operation:%u",p_task->dshead.ophead.status);
        if (p_task->dshead.ophead.status == opstatus_success)
        {
            rc=handle_success(part_out);
        }
        else if (p_task->dshead.ophead.status == opstatus_failure)
        {
            rc=handle_failure(part_out);
        }
        else
        {
            log->warning_log("Error occured.");
            rc=-1;
        }
        log->debug_log("STATUS:csid:%u,seq:%u,status:%u",part_out->ophead.cco_id.csid,part_out->ophead.cco_id.sequencenum, part_out->ophead.status);
        //p_opman->cleanup(part_out);
    }
    else
    {
        log->debug_log("no operation found");
    }
    log->debug_log("rc:%d",rc);
    return rc;
}

int DataServer::handle_ds_task(struct dstask_head *p_taskhead)
{
    int rc = -1;
    log->debug_log("Received ds task operation:%u:%p,",p_taskhead->ophead.subtype, p_taskhead);   
    switch (p_taskhead->ophead.subtype)
    {
        case(received_received):
        {
            rc = p_opman->handle_received_msg( (struct dstask_ccc_received*) p_taskhead);
            delete p_taskhead;
            break;
        }               
        case(received_cancommit):
        {
            rc = p_opman->handle_cancommit_msg( (struct dstask_cancommit*) p_taskhead);
            delete p_taskhead;
            break;
        }               
        case (received_committed):
        {
            rc = p_opman->handle_committed_msg( (struct dstask_ccc_recv_committed*) p_taskhead);
            delete p_taskhead;
            break;
        }
        case (send_prepare):
        {
            log->debug_log("send prepare...");
            rc = perform_CCC_send_prepare((dstask_ccc_send_prepare*)p_taskhead); 
            delete p_taskhead;
            break;
        }
        case (send_docommit):
        {
            log->debug_log("send docommit...");
            rc = perform_CCC_send_docommit((operation_dstask_docommit*)p_taskhead);
            delete p_taskhead;
            break;
        }
        case (send_result):
        {
            log->debug_log("send result...");
            rc = perform_CCC_send_result((dstask_ccc_send_result*)p_taskhead); 
            delete p_taskhead;
            break;
        }

        
        case(received_prepare):
        {
            rc = handle_CCC_prepare( (struct dstask_ccc_recv_prepare*) p_taskhead);
            delete p_taskhead;
            break;
        }               
        case (received_docommit):
        {
            rc = handle_CCC_docommit( (struct operation_dstask_docommit*) p_taskhead);
            delete p_taskhead;
            break;
        }        
        case (received_result):
        {
            rc = handle_CCC_result((struct dstask_proccess_result*)p_taskhead);
            delete p_taskhead;
            break;
        }                
        case (send_received):
        {
            log->debug_log("send received...");
            rc = perform_CCC_cl_smallwrite((struct dstask_ccc_send_received*)p_taskhead); 
            delete p_taskhead;
            break;
        }
        
        case (received_spn_directwrite_su):
        {
            // quick n dirty
            rc = p_opman->handle_directwrite_msg( (struct dstask_spn_recv *) p_taskhead);
            rc=p_spnbc->handle_result(p_taskhead->ophead.inum,p_taskhead->ophead.cco_id, opstatus_success);
            delete p_taskhead;
            break;
        }
        case (send_ccc_stripewrite_cancommit):
        {
            log->debug_log("send_ccc_stripewrite_docommit.");
            rc = perform_CCC_stripewrite_cancommit((struct dstask_ccc_stripewrite_cancommit*)p_taskhead); 
            delete p_taskhead;
            break;
        }
        case (received_ccc_stripewrite_cancommit):
        {
            log->debug_log("received_ccc_stripewrite_cancommit");
            rc = p_opman->handle_stripewrite_cancommit((struct dstask_ccc_stripewrite_cancommit_received *)p_taskhead);
            delete p_taskhead;
            break;
        }
        case (received_spn_write_su):      //opman ok
        {
            struct dstask_spn_recv *p_task_sp = (struct dstask_spn_recv *) p_taskhead;
            log->debug_log("%p:",p_task_sp->data);
            log->debug_log("Received spn write stripe unit operation");
            //log->debug_log("seq:%u,text:%s",p_task_sp->dshead.ophead.cco_id.sequencenum,(char*) p_task_sp->data);
            rc = p_opman->dataserver_insert(p_task_sp);
            //free_dstask_spn_recv(p_task_sp);
            p_task_sp->data=NULL;
            delete p_taskhead; // the data block received must not be freed
            break;
        }
        case (received_spn_write_s):      //opman ok
        {
            struct dstask_spn_recv *p_task_sp = (struct dstask_spn_recv *) p_taskhead;
            log->debug_log("%p:",p_task_sp->data);
            log->debug_log("Received spn write stripe operation");
      //      log->debug_log("text:%s",(char*) p_task_sp->data);
            rc = p_opman->dataserver_insert(p_task_sp);
            //free_dstask_spn_recv(p_task_sp);
            p_task_sp->data=NULL;
            delete p_taskhead; // the data block received must not be freed
            break;
        }
        case (received_spn_read):
        {
            struct dstask_spn_recv *p_task_sp = (struct dstask_spn_recv *) p_taskhead;
            log->debug_log("Received spn read operation");
            rc = handle_read_req(p_task_sp);
            delete p_task_sp;
            break;
        }
        case (regendpoint):
        {
            struct dstask_endpointreg *p_t = (struct dstask_endpointreg*) p_taskhead;
            log->debug_log("IP:%s",p_t->ip);
            p_spnbc->register_cl_address(p_t->dshead.ophead.cco_id.csid,p_t->ip);
            free(p_t->ip);
            free(p_t);
            rc=0;
            break;
        }
        case (dstask_write_fo):
        {
            struct dstask_write_fileobject *p_dstask = (struct dstask_write_fileobject *)p_taskhead; 
            rc = p_fileio->write_file(&p_taskhead->ophead, p_dstask->data_col); 
            delete p_dstask;
            break;
        }
        case (dspingpong):
        {
            rc = handle_ping_msg((struct dstask_pingpong *)p_taskhead);
            break;
        }
        case (maintenance_garbagecollection):
        {
            struct dstask_maintenance_gc *p_task = (struct dstask_maintenance_gc*) p_taskhead;
            rc = maintenance_garbage_collection(p_task);
            break;
        }
        default:
        {
            log->debug_log("unknown message received:%u",p_taskhead->ophead.subtype);
            rc = -2;
            delete p_taskhead;
        }
    }
    log->debug_log("rc:%d",rc);
    return rc;
}


/**
 * @param[in] p_op operation structure handled by the op manager
 * @return 
 * @Todo check existing file data.
 *
int DataServer::calculate_parity_block(struct operation_participant *p_op)
{
    int rc=-1;
    log->debug_log("calc parity");
    std::map<StripeId,struct dataobject_collection*>::iterator it = p_op->datamap->begin();
    for(it; it!=p_op->datamap->end(); it++)
    {
        log->debug_log("stripeid:%u",it->first);
        it->second->parity_data = malloc(it->second->recv_length);
        if (it->second->parity_data == NULL)
        {
            log->debug_log("Failed to allocate memory.");
            return -1;
        }           
        if (it->second->existing==NULL)
        {
            log->debug_log("Do copy.");
            //it->second->parity_data=it->second->recv_data;
            memcpy(it->second->parity_data, it->second->recv_data, it->second->recv_length);
            rc=0;
            log->debug_log("existing block empty. parity equals recv data");
        }
        else
        {
            log->debug_log("do calc.");
            calc_parity(it->second->recv_data,it->second->existing->data,it->second->parity_data,it->second->recv_length);
            rc=0;
        }
        log->debug_log("calculated for %llu bytes, rc:%d",it->second->recv_length,rc);
    }
    return rc;
}
*/
/**
 * @param[in] p_op operation structure handled by the op manager
 * @return 
 * @Todo check existing file data.
 *
int DataServer::calculate_parity_block(struct operation_primcoordinator *p_op)
{
    log->debug_log("calc parity");
    std::map<StripeId,struct datacollection_primco*>::iterator its = p_op->datamap_primco->begin();
    for (its; its!=p_op->datamap_primco->end(); its++)
    {        
        
        std::map<uint32_t,struct paritydata*>::iterator it = its->second->paritymap->begin();
        if (it==its->second->paritymap->end())
        {
            log->error_log("no parity data available.");
            return -1;
        }
        its->second->finalparity = malloc(p_op->datalength);
        if (its->second->existing==NULL)
        {            
                its->second->finalparity = it->second;
                log->debug_log("initial parity block created.");
        }
        else
        {
                calc_parity(it->second->data ,its->second->existing->data,its->second->finalparity,p_op->datalength);
                log->debug_log("old block processed");
        }
        it++;
        for(it; it!=its->second->paritymap->end(); it++)
        {
            log->debug_log("stripeunitid:%u",it->first);
            calc_parity(it->second->data ,its->second->finalparity,its->second->finalparity,p_op->datalength);
            log->debug_log("calculated for %llu bytes,",p_op->datalength);        
        }    
    }
    log->debug_log("done");
    return 0;
}*/


int DataServer::handle_success(struct operation_participant *p_part)
{
    int rc=0;
    log->debug_log("got operation:csid:%u,seq:%u",p_part->ophead.cco_id.csid,p_part->ophead.cco_id.sequencenum);
    std::map<StripeId,struct dataobject_collection*>::iterator it = p_part->datamap->begin();
    for (it; it!=p_part->datamap->end(); it++)
    {
        p_docache->set_entry(p_part->ophead.inum, it->first, it->second->newblock);
        it->second->newblock=NULL;
        free(it->second->parity_data);
        //free(it->second->recv_data);
        it->second->recv_data=NULL;
        delete it->second;
    }
    delete p_part->datamap;
    if (p_part->iamsecco)
    {
        rc=p_spnbc->handle_result(p_part->ophead.inum,p_part->ophead.cco_id, opstatus_success);
    }
    return rc;
}

int DataServer::handle_failure(struct operation_participant *p_part)
{
    int rc=0;
    if (p_part->iamsecco)
    {
        rc=p_spnbc->handle_result(p_part->ophead.inum,p_part->ophead.cco_id, opstatus_failure);
    }
    return rc;
}

/*
int DataServer::perform_primcoord_docommit(struct dstask_writetodisk *p_task)
{
    int rc=0;
    log->debug_log("inum:%llu",p_task->dshead.ophead.inum);
    rc = calculate_parity_block(p_task->p_op);
    if (rc)
    {
        log->debug_log("error occured while creating parity block.");
    }
    else
    {
        std::map<StripeId,struct datacollection_primco*>::iterator its = p_task->p_op->datamap_primco->begin();
        for (its; its!=p_task->p_op->datamap_primco->end(); its++)
        {
            its->second->newobject = new struct data_object;;
            its->second->newobject->data = its->second->finalparity;
            memcpy(&its->second->newobject->metadata.ccoid, &p_task->dshead.ophead.cco_id, sizeof(struct CCO_id));
            its->second->newobject->metadata.datalength = p_task->p_op->datalength;
            its->second->newobject->metadata.offset  = p_task->dshead.ophead.offset;

            memcpy(&its->second->newobject->metadata.filelayout[0], &p_task->dshead.ophead.filelayout[0], sizeof(p_task->dshead.ophead.filelayout));
                    its->second->newobject->metadata.operationlength = p_task->dshead.ophead.length;
            memcpy(&its->second->newobject->metadata.versionvector[0], &(its->second->versionvec[0]), sizeof(its->second->versionvec));

            filelayout_raid *p_fl = ( filelayout_raid*) &p_task->p_op->ophead.filelayout[0];
            StripeId sid = p_raid->getStripeId(p_fl,p_task->p_op->ophead.offset);

            rc = p_docache->get_entry(p_task->p_op->ophead.inum,sid,&its->second->existing);
            if (rc)
            {
                log->debug_log("request for existing  rc:%d",rc);
                its->second->newobject->metadata.versionvector[default_groupsize]=1;
            }
            else
            {
                its->second->newobject->metadata.versionvector[default_groupsize]=its->second->existing->metadata.versionvector[default_groupsize]+1;
            }
            stringstream ss;
            print_dataobject_metadata(&its->second->newobject->metadata,ss);
            log->debug_log("%s",ss.str().c_str());
            log->debug_log("write to disk");
            rc = p_fileio->write_file(&p_task->p_op->ophead, its->second); 
            log->debug_log("Object written to disk:rc=%d",rc);
            if (!rc)
            {
                rc = p_docache->parityUnconfirmed(p_task->p_op->ophead.inum, its->first, its->second->newobject, default_groupsize);
                log->debug_log("Cache updated...rc=%d",rc);
            }
        }
        
    }
    log->debug_log("rc:%d",rc);
    return rc;
}**/


int DataServer::handle_read_req(struct dstask_spn_recv *p_task)
{
    int rc=-1;
    log->debug_log("got read request for inum:%llu, stripe id:%u",p_task->dshead.ophead.inum, p_task->stripeid);
    StripeUnitId suid = p_raid->get_my_stripeunitid((filelayout_raid*)&p_task->dshead.ophead.filelayout[0], p_task->dshead.ophead.offset, this->id);
    struct data_object *p_obj;// = new struct data_object;
    rc = this->p_docache->get_entry(p_task->dshead.ophead.inum, p_task->stripeid,  &p_obj);
    log->debug_log("cache read:rc:%d",rc);
    //log->debug_log("text:%s",(char*)p_obj->data);
    rc = p_spnbc->handle_read_response(p_task->dshead.ophead.inum, p_task->stripeid, suid, p_task->dshead.ophead.cco_id, p_obj);
    
    log->debug_log("rc:%d",rc);
    return rc;
}

int DataServer::maintenance_garbage_collection(struct dstask_maintenance_gc *p_task)
{
    int rc=0;
    log->debug_log("start garbage collector");
    rc = p_docache->garbage_collection();
    delete p_task;
    log->debug_log("End garbage collector");
    return rc;
}


int DataServer::handle_ping_msg(struct dstask_pingpong *p_task)
{
    int rc;
    if (p_task->counter>=DS_PINGPONG_INTERVAL)
    {
        rc = p_spnbc->handle_pingpong(p_task->dshead.ophead.cco_id);
    }
    else
    {
        if (!p_sm->exists(p_task->sender))
        {
            rc = getDataserverAddress(mdsid,p_task->sender);
        }

        rc = p_ccc->handle_CCC_cl_pingping(p_task);
    }
    log->debug_log("done");
    return rc;
}