/* 
 * File:   OpManager.cpp
 * Author: markus
 * 
 * Created on 8. Februar 2012, 16:30
 */

#include "components/OperationManager/OpManager.h"

bool timer_watchdog_killswitch=false;

void* timer_watchdog(void *p_in)
{
    std::map<InodeNumber, StripeManager*> *p_map = (std::map<InodeNumber, StripeManager*>*)p_in;
    time_t iteration_start;
    double end_diff;
    std::map<InodeNumber, StripeManager*>::iterator it;
    double interval = 2.0;
    
    while (!timer_watchdog_killswitch)
    {
        iteration_start = time(0);
        it = p_map->begin();
        for(it; it!=p_map->end(); it++)
        {
            it->second->timeout_check();
        }
        end_diff = interval-difftime(time(0),iteration_start);
        if (0<end_diff)
        {
            sleep(end_diff);
        }
    }    
}




/**
 * @param[in] p_op operation structure handled by the op manager
 * @return 
 * @Todo check existing file data.
 */
int calculate_parity_block(struct operation_participant *p_op)
{
    int rc=-1;
    std::map<StripeId,struct dataobject_collection*>::iterator it = p_op->datamap->begin();
    for(it; it!=p_op->datamap->end(); it++)
    {
        it->second->parity_data = malloc(it->second->recv_length);
        if (it->second->parity_data == NULL)
        {
            return -1;
        }           
        if (it->second->existing==NULL)
        {
            //it->second->parity_data=it->second->recv_data;
            memcpy(it->second->parity_data, it->second->recv_data, it->second->recv_length);
            rc=0;
        }
        else
        {
            calc_parity(it->second->recv_data,it->second->existing->data,it->second->parity_data,it->second->recv_length);
            rc=0;
        }
    }
    return rc;
}

/**
 * @param[in] p_op operation structure handled by the op manager
 * @return 
 * @Todo check existing file data.
 */
int calculate_parity_block(struct operation_primcoordinator *p_op)
{
    std::map<StripeId,struct datacollection_primco*>::iterator its = p_op->datamap_primco->begin();
    for (its; its!=p_op->datamap_primco->end(); its++)
    {        
        std::map<uint32_t,struct paritydata*>::iterator it = its->second->paritymap->begin();
        if (it==its->second->paritymap->end())
        {
            return -1;
        }
        its->second->finalparity = malloc(p_op->datalength);
        if (its->second->existing==NULL)
        {            
                memcpy(its->second->finalparity, it->second->data,p_op->datalength) ;
        }
        else
        {
                calc_parity(it->second->data ,its->second->existing->data,its->second->finalparity,p_op->datalength);
        }
        if (its->second->paritymap->size()>1)
        {
            it++;
            for(it; it!=its->second->paritymap->end(); it++)
            {
                calc_parity(it->second->data ,its->second->finalparity,its->second->finalparity,p_op->datalength);
            }    
        }
    }
    return 0;
}





void free_data_object(struct data_object *p)
{
    if (p->data!= NULL)
    {
        free(p->data);        
    }
    delete p;
}

void free_dstask_spn_recv(struct dstask_spn_recv *p)
{
    if (p->data!=NULL)
    {
        free(p->data);
    }
    delete p;
}

void free_dataobject_collection(struct dataobject_collection *p)
{
    if (p!=NULL)
    {
        //  if (p->existing!=NULL)free(p->existing); // freeed by docache
        //if (p->existing != p->recv_data) free(p->recv_data);
        free(p->parity_data);
        free(p);
    }
};


void free_operation_participant(operation_participant *p)
{
    if (p->datamap != NULL)
    {
        std::map<StripeId,struct dataobject_collection*>::iterator it = p->datamap->begin();
        for (it; it!=p->datamap->end(); it++)
        {
            free_dataobject_collection(it->second);
        }
        delete p->datamap;
    }
    delete p;
}

void free_parity_data(struct paritydata *p)
{
    if (p->data!=NULL)
    {
        free(p->data);
    }
    delete p;
}

void free_datacollection_primco(struct datacollection_primco *p)
{
    if (p->finalparity!=NULL)
    {
        free(p->finalparity);
    }
    if (p->existing!=NULL)
    {
        free_data_object(p->existing);
    }
    if (p->newobject!=NULL)
    {
        free_data_object(p->newobject);
    }
    if (p->paritymap!=NULL)
    {
        std::map<uint32_t,struct paritydata*>::iterator it = p->paritymap->begin();
        for (it; it!=p->paritymap->end(); it++)
        {
            free_parity_data(it->second);
        }
        free(p->paritymap);
    }
    delete p;
}

void free_operation_primcoordinator(struct operation_primcoordinator *p)
{
    if (p->datamap_primco!=NULL)
    {
        std::map<StripeId,struct datacollection_primco*>::iterator it = p->datamap_primco->begin();
        for(it;it!=p->datamap_primco->end(); it++)
        {
            free_datacollection_primco(it->second);
        }
        delete p->datamap_primco;
    }
    delete p;
}

void free_dstask_writetodisk(struct dstask_writetodisk *p_write)
{
    if (p_write->p_op!=NULL)
    {
        free_operation_primcoordinator(p_write->p_op);
    }
    delete p_write;
}

void print_dataobject_metadata(struct dataobject_metadata *p_obj, stringstream& ss)
{
    ss << "struct dataobject_metadata:\n";
    print_CCO_id(&p_obj->ccoid, ss);
    ss << "datalength:" << p_obj->datalength << "\noffset:" << p_obj->offset;
    ss << "\noperationlength:" << p_obj->operationlength  << "\n";
    for (int i=0; i<=default_groupsize; i++)
    {
       ss << "version" << i << ":" << p_obj->versionvector[i] << "\n";
    }
}

void print_dataobject(struct data_object *p_obj, stringstream& ss)
{
    ss << "struct data_object:\n";
    print_dataobject_metadata(&p_obj->metadata, ss);
    ss << "data...\nchecksum:" << p_obj->checksum;
}

void print_CCO_id(struct CCO_id *p_cco, stringstream& ss)
{
    ss<<"struct CCO_id.csid:" << p_cco->csid << "\nstruct CCO_id.sequence:" << p_cco->sequencenum <<".\n";
}

void print_operation_head(struct OPHead *p_head, stringstream& ss)
{
    ss << "---head start---\n";
    ss<<"InodeNumber:"<<p_head->inum<<".\n";
    //printf("printophead:inum=%llu.\n",p_head->inum);
    //printf("offset:%llu.\n",p_head->offset);
    ss<<"offset:" << p_head->offset << "\n";
    ss<<"length:" << p_head->length << "\n";
    print_CCO_id(&p_head->cco_id,ss);
    ss << "optype:" << p_head->type << "\n";
    ss << "stripecnt:" << p_head->stripecnt << "\n";
    ss << "file layout...\n";
    //char                filelayout[256];
    ss << "---head done---\n";
}

void print_op_client(struct operation_client *p_op, stringstream& ss)
{
    ss << "---Client operation start---\n";
    print_operation_head(&p_op->ophead,ss);
    ss << "recv timestamp:" << p_op->recv_timestamp << "\n";
    ss << "opstatus:" << p_op->ophead.status << "\n";
    print_struct_participant(&p_op->participants, ss);
    ss << "groups...\n---Client operation done---\n";
}

void print_op_participant(struct operation_participant *p_op, stringstream& ss)
{
    ss << "---participant start.---\n";
    print_operation_head(&p_op->ophead,ss);   
    ss << "recv_timestamp:" << p_op->recv_timestamp << "\n";
    /*s.append(string("old_block pointer:%p.\n",p_op->old_block));
    s.append(string("recv_block pointer:%p.\n",p_op->recv_block));
    s.append(string("paritydiff pointer:%p.\n",p_op->paritydiff));*/
    ss << "opstatus:" << p_op->ophead.status << "\n";
    print_struct_participant(&p_op->participants, ss);
    ss << "---participant done.---\n";
}

void print_op_primcoordinator(struct operation_primcoordinator *p_op, stringstream& ss)
{
    ss << "---primcoordinator start---.\n";
    print_operation_head(&p_op->ophead,ss);   
    ss << "recv_timestamp:" << p_op->recv_timestamp << "\n";
    /*s.append(string("old_block pointer:%p.\n",p_op->old_block));
    s.append(string("recv_block pointer:%p.\n",p_op->recv_block));
    s.append(string("paritydiff pointer:%p.\n",p_op->paritydiff));*/
    ss << "opstatus:" << p_op->ophead.status << "\n";
    ss << "participants \n";
    print_struct_participant(&p_op->participants, ss);
    ss << "participans: received msg \n";
    print_struct_participant(&p_op->received_from, ss);
    ss << "---primcoordinator done---.\n";
}

void print_operation(operation *p_op, stringstream& ss)
{
    struct OPHead *p_head = (struct OPHead*) p_op;
    switch (p_head->type)
    {
        case  operation_participant_type:
        {
            print_op_participant((struct operation_participant*)p_op,ss);
            break;
        }
        case  operation_primcoord_type:
        {
            print_op_primcoordinator((struct operation_primcoordinator*)p_op,ss);
            break;
        }
        case operation_client_type:
        {
            print_op_client((struct operation_client*)p_op,ss);
            break;
        }
        default:
        {
            ss << "Unknown operation type...\n";
            print_operation_head(p_head,ss);
        }
    }            
}


void print_struct_participant(struct Participants_bf *p, stringstream& ss)
{
    ss<<"Participants are:";
    if (p->su0) ss<<"00,";
    if (p->su1) ss<<"01,";
    if (p->su2) ss<<"02,";
    if (p->su3) ss<<"03,";
    if (p->su4) ss<<"04,";
    if (p->su5) ss<<"05,";
    if (p->su6) ss<<"06,";
    if (p->su7) ss<<"07,";
    if (p->su8) ss<<"08,";
    if (p->su9) ss<<"09,";
    if (p->su10) ss<<"10,";
    if (p->su11) ss<<"11,";
    ss<<"\n";
}

OpManager::OpManager(Logger *p_log, int (*p_cb)(queue_priorities, OPHead*), serverid_t myid,doCache *docache,Filestorage *fileio) {
    p_opmap = new std::map<InodeNumber, StripeManager*>();
    p_queuePush = p_cb;
    globallock_mutex = PTHREAD_MUTEX_INITIALIZER;
    opmap_getentry_mutex = PTHREAD_MUTEX_INITIALIZER;
    log = p_log;
    p_raid = new Libraid4(log);
    id = myid;
    p_docache = docache;
    p_fileio = fileio;
    sequence_num = 0;
    
    pthread_t timeout_thread;
    pthread_create(&timeout_thread, NULL, timer_watchdog , this->p_opmap );     
}


OpManager::OpManager(const OpManager& orig) {
}

OpManager::~OpManager() 
{
    log->debug_log("Shutting down");
    timer_watchdog_killswitch=true;
    pthread_mutex_destroy(&globallock_mutex);
    delete p_raid;
    std::map<InodeNumber, StripeManager*>::iterator it = p_opmap->begin();
    for(it;it!=p_opmap->end();it++)
    {
        delete it->second;
    }
    delete p_opmap;
}


int OpManager::insert(void *data)
{
    int rc = 0;
    log->debug_log("start insert.");
    StripeManager *p_sm;
    
    struct OPHead *ophead = (struct OPHead *) data;    
    pthread_mutex_lock(&globallock_mutex);
    std::map<InodeNumber,StripeManager*>::iterator it = this->p_opmap->find(ophead->inum);
    if (it==this->p_opmap->end())
    {
        p_sm = new StripeManager(log,p_queuePush,p_docache,this->id,p_fileio);
        this->p_opmap->insert(std::pair<InodeNumber, StripeManager*>(ophead->inum,p_sm));
        log->debug_log("new stripe manager inserted for inum:%llu inserted",ophead->inum);
    }
    else
    {
        
        log->debug_log("server manager found for %llu.",ophead->inum);
    }
    pthread_mutex_unlock(&globallock_mutex);
    log->debug_log("end:rc=%d.",rc);
    return rc;
}       

/**
 * @param csid
 * @param seq
 * @param p_einode
 * @param offset
 * @param length
 * @param newdata
 * @return 
 * @todo check for memory leaks. filestripe list etc.
 */
int OpManager::client_insert(struct operation_composite *op, void *newdata)
{
    int rc=0;
    log->debug_log("inum:%llu",op->ophead.inum);
    //op->groups = new std::map<uint32_t,struct operation_group*>;    
    rc = insert(op);
    if (!rc)
    {
        StripeManager *p_sm;
        get_entry(op->ophead.inum, &p_sm);
        if (p_sm!=NULL)
        {
            rc = p_sm->insert(op,newdata);
        }
        else
        {
            rc=-1;
        }
    }
    else
    {
        log->warning_log("could not insert operation");
    }
    log->debug_log("done.rc:%d",rc);
    return rc;
}

/**
 * @param csid
 * @param seq
 * @param p_einode
 * @param offset
 * @param length
 * @param newdata
 * @return 
 * @todo check for memory leaks. filestripe list etc.
 */
int OpManager::client_insert_read(struct operation_client_read *op, struct EInode *p_einode)
{
    int rc=0;
    log->debug_log("inum:%llu:offset:%llu,length:%llu",p_einode->inode.inode_number,op->ophead.offset,op->ophead.length);
    op->groups = new std::map<uint32_t,struct operation_group*>;
    
    filelayout_raid *p_fl = (filelayout_raid *) &p_einode->inode.layout_info;
    struct FileStripeList *p_fsl = p_raid->get_stripes(p_fl, op->ophead.offset, op->ophead.length);
    p_raid->printFileStripeList(p_fsl);
    std::map<StripeId,struct Stripe_layout*>::iterator it = p_fsl->stripemap->begin();
    
    size_t lengthdecr = op->ophead.length;
    //char *newdata_iter = (char*) newdata;
    for (it; it!=p_fsl->stripemap->end(); it++)
    {
        if (!lengthdecr) break;
        struct operation_group *p_gr = new struct operation_group;
        p_gr->sumap = new std::map<StripeUnitId,struct StripeUnit*>();
        p_gr->stripe_id = it->first;
        log->debug_log("StripeId:%u.",it->first);                
        std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->blockmap->begin();
        for (it2; it2!=it->second->blockmap->end(); it2++)
        {
            if (!lengthdecr) break;
            log->debug_log("Length left = %u",lengthdecr);
            log->debug_log("StripeUnitId:%u",it2->first);
            struct StripeUnit *p_su = new struct StripeUnit; //it2->second;
            p_su->start = it2->second->start;
            p_su->end = it2->second->end;
            p_su->assignedto = it2->second->assignedto;
            p_su->opsize = (lengthdecr > p_fl->raid4.stripeunitsize) ? p_fl->raid4.stripeunitsize : lengthdecr;
            p_su->newdata = NULL;
            //newdata_iter += p_su->opsize;
            p_gr->sumap->insert(std::pair<StripeUnitId,struct StripeUnit*>(it2->first,p_su));
            lengthdecr = (lengthdecr > p_fl->raid4.stripeunitsize) ? lengthdecr-p_fl->raid4.stripeunitsize : 0;
            
        }
        log->debug_log("Participants:%u",*(uint16_t*)&p_gr->participants);
        op->groups->insert(std::pair<uint32_t, struct operation_group*>(it->first,p_gr));
        op->ophead.stripecnt++;
    }    
    rc = insert(op);
    if (!rc)
    {
        StripeManager *p_sm;
        get_entry(op->ophead.inum, &p_sm);
        if (p_sm!=NULL)
        {
            rc = p_sm->insert(op);
        }
        else
        {
            rc=-1;
        }
    }
    else
    {
        log->warning_log("could not insert operation");
    }
    free_FileStripeList(p_fsl);
    log->debug_log("done.rc:%d",rc);
    return rc;
}

/**
 * @Todo check for existing operation...
 * @Todo is the received data block freed?
 * @param p_task_sp
 * @param iscoord
 * @return 
 */
int OpManager::dataserver_insert(struct dstask_spn_recv *p_task_sp)
{
    int rc = -1;
    filelayout_raid *fl = ( filelayout_raid*)&p_task_sp->dshead.ophead.filelayout[0];
    uint8_t coord = p_raid->is_coordinator(fl,p_task_sp->dshead.ophead.offset, this->id);
    log->debug_log("is coordinator:%u",coord);
    StripeManager *p_sm=NULL;
    get_entry(p_task_sp->dshead.ophead.inum, &p_sm);
    if (p_sm!=NULL)
    {
        if (coord==2)
        {
            rc = p_sm->add_spn_datablock(p_task_sp);
        }
        else
        {
            rc = p_sm->insert_paricipant_operation(p_task_sp);
        }
    }
    else
    {
        rc=-1;        
        //rc = insert(p_op);
        if (rc)
        {
            log->warning_log("could not insert new stripe manager instance");
        }
    }
    //p_op->recv_timestamp = p_task_sp->dshead.ophead.
    log->debug_log("rc:%d",rc);
    return rc;
}

/* delete...
int OpManager::dataserver_prim_operation(struct operation_primcoordinator *p_op)
{
    int rc=-1;
    log->debug_log("op:csid=%u, inum:%llu",p_op->ophead.cco_id.csid, p_op->ophead.inum);
    StripeManager *p_sm;
    rc = this->get_entry(p_op->ophead.inum, &p_sm);
    if (!rc)
    {
        switch (p_op->ophead.subtype)
        {
            case (MT_CCC_cancommit):
            {
                log->debug_log("Received can commit.");
                struct dstask_cancommit *p_t = (struct dstask_cancommit*)p_op;
                //rc = p_sm->handle_prim_cancommit(p_t);
                break;
            }
            case (MT_CCC_committed):
            {
                log->debug_log("Received committed.");
               // rc = p_sm->handle_prim_committed(p_op);
                break;
            }            
            default:
            {
                rc=1;
                log->debug_log("unknown operation.");
            }
        }
    }
    else
    {
        log->debug_log("No operation for that inode number.");
        p_sm = new StripeManager(log,p_queuePush,p_docache,this->id);
        opmap_lock();
        this->p_opmap->insert(std::pair<InodeNumber, StripeManager*>(p_op->ophead.inum,p_sm));
        opmap_unlock();
       // std::stringstream ss("");
       // print_op_primcoordinator(p_op, ss);
       // log->debug_log("inserting:%s.",ss.str().c_str());
        //rc = p_sm->insert_primcoord(p_op);
        log->debug_log("new stripe manager inserted for inum:%llu inserted",p_op->ophead.inum);
    }
    log->debug_log("rc:%d",rc);
    return rc;
}**/

int OpManager::get_entry(InodeNumber inum, StripeManager **p_sm)
{
    int rc=-1;
    log->debug_log("inum:%llu",inum);
    pthread_mutex_lock(&globallock_mutex);
    std::map<InodeNumber,StripeManager*>::iterator it = this->p_opmap->find(inum);
    if (it!=this->p_opmap->end())
    {
        *p_sm = it->second;
        rc=0;        
    }
    else
    {
        StripeManager *p_smnew = new StripeManager(log,p_queuePush,p_docache,this->id,p_fileio);
        this->p_opmap->insert(std::pair<InodeNumber, StripeManager*>(inum,p_smnew));
        *p_sm = p_smnew;
        rc=0;
        log->debug_log("No operation for that inode number. created it.");
    }
    pthread_mutex_unlock(&globallock_mutex);
    log->debug_log("rc:%d",rc);
    return rc;
}

int OpManager::get_operation_structure(struct OPHead *p_head, struct OPHead **p_op)
{
    int rc=-1;
    log->debug_log("inum:%llu",p_head->inum);
    StripeManager *p_sm = NULL;
    this->get_entry(p_head->inum, &p_sm);
    if (p_sm!=NULL)
    {
        *p_op = p_sm->get_op(p_head);
        if (*p_op!=NULL)
        {
            rc=0;
            log->debug_log("Operation found. csid:%u",(*p_op)->cco_id.csid);
        }
    }
    else
    {
        log->debug_log("no operation found");
    }
    log->debug_log("rc:%d",rc);
    return rc;

}
int OpManager::get_operation_structure(struct OPHead *p_head, struct operation_participant **p_op)
{
    int rc=-1;
    log->debug_log("inum:%llu",p_head->inum);
    StripeManager *p_sm = NULL;
    this->get_entry(p_head->inum, &p_sm);
    if (p_sm!=NULL)
    {
        rc = p_sm->get_op(p_head->cco_id, p_op);
        if (!rc)
        {
            rc=0;
            log->debug_log("Operation found. csid:%u",(*p_op)->ophead.cco_id.csid);
        }
        else
        {
            log->debug_log("No such Operation found csid:%u",(*p_op)->ophead.cco_id.csid);
        }
    }
    else
    {
        log->debug_log("no operation found");
    }
    log->debug_log("rc:%d",rc);
    return rc;

}

int OpManager::client_result(InodeNumber inum, struct CCO_id ccoid, opstatus result)
{
    int rc=0;
    log->debug_log("Inum:%llu, csid:%u, seq:%u, result:%u",inum,ccoid.csid, ccoid.sequencenum, result);
    StripeManager *p_sm = NULL;
    this->get_entry(inum, &p_sm);    
    if (p_sm!=NULL)
    {
        rc = p_sm->handle_client_result(ccoid,result);        
    }
    else
    {
        log->error_log("No operation found for inum:%llu!",inum);
        rc=-1;
    }
    log->debug_log("Rc:%u",rc);
    return rc;
}

int OpManager::handle_received_msg(struct dstask_ccc_received *p_task)
{
    int rc=-1;  
    log->debug_log("op:csid=%u, inum:%llu",p_task->dshead.ophead.cco_id.csid, p_task->dshead.ophead.inum);
    stringstream ss;
    print_struct_participant(&p_task->recv_from, ss);
    log->debug_log("received from stripe unit id:%s",ss.str().c_str());
    StripeManager *p_sm;
    rc = this->get_entry(p_task->dshead.ophead.inum, &p_sm);
    if (!rc)
    {
        rc = p_sm->handle_ccc_received(p_task);
    }
    log->debug_log("rc:%u",rc);
    return rc;
}

int OpManager::handle_cancommit_msg(struct dstask_cancommit *p_task)
{
    int rc=-1;  
    log->debug_log("op:csid=%u, inum:%llu",p_task->dshead.ophead.cco_id.csid, p_task->dshead.ophead.inum);
    StripeManager *p_sm;
    rc = this->get_entry(p_task->dshead.ophead.inum, &p_sm);
    if (!rc)
    {
        rc = p_sm->handle_ccc_cancommit(p_task);
    }
    log->debug_log("rc:%u",rc);
    return rc;
}

int OpManager::handle_committed_msg(struct dstask_ccc_recv_committed *p_task)
{
    int rc=-1;  
    log->debug_log("op:csid=%u, inum:%llu",p_task->dshead.ophead.cco_id.csid, p_task->dshead.ophead.inum);
    StripeManager *p_sm;
    rc = this->get_entry(p_task->dshead.ophead.inum, &p_sm);
    if (!rc)
    {
        rc = p_sm->handle_ccc_committed(p_task);
    }
    log->debug_log("rc:%u",rc);
    return rc;
}

int OpManager::cleanup(struct operation_client_read *op)
{    
    int rc=-1;  
    log->debug_log("op:csid=%u, inum:%llu",op->ophead.cco_id.csid, op->ophead.inum);
    StripeManager *p_sm;
    rc = this->get_entry(op->ophead.inum, &p_sm);
    if (!rc)
    {
        rc = p_sm->cleanup(op);
    }
    log->debug_log("rc:%u",rc);
    return rc;
}

int OpManager::cleanup(struct operation_client_write *op)
{    
    int rc=-1;  
    log->debug_log("op:csid=%u, inum:%llu",op->ophead.cco_id.csid, op->ophead.inum);
    StripeManager *p_sm;
    rc = this->get_entry(op->ophead.inum, &p_sm);
    if (!rc)
    {
        rc = p_sm->cleanup(op);
    }
    log->debug_log("rc:%u",rc);
    return rc;
}
int OpManager::cleanup(struct operation_composite *op)
{    
    log->debug_log("op:csid=%u, inum:%llu",op->ophead.cco_id.csid, op->ophead.inum);
    StripeManager *p_sm;
    int rc = this->get_entry(op->ophead.inum, &p_sm);
    if (!rc)
    {
        rc = p_sm->cleanup(op);
    }
    log->debug_log("rc:%u",rc);
    return rc;
}

void OpManager::cleanup(struct operation_primcoordinator *op)
{
    log->debug_log("op:csid=%u, inum:%llu",op->ophead.cco_id.csid, op->ophead.inum);
    StripeManager *p_sm;
    int rc = this->get_entry(op->ophead.inum, &p_sm);
    if (!rc)
    {
        p_sm->cleanup(op);
    }
}

void OpManager::cleanup(struct operation_participant *op)
{
    log->debug_log("op:csid=%u, inum:%llu",op->ophead.cco_id.csid, op->ophead.inum);
    StripeManager *p_sm;
    int rc = this->get_entry(op->ophead.inum, &p_sm);
    if (!rc)
    {
        p_sm->cleanup(op);
    }
}

int OpManager::client_handle_read_response(struct data_object *p_do, InodeNumber inum, StripeId stripeid, StripeUnitId stripeunitid, struct CCO_id ccoid)
{
    int rc=-1;
    log->debug_log("inum:%llu,sid:%u,suid:%u,csid:%u,seq:%u",inum,stripeid,stripeunitid,ccoid.csid,ccoid.sequencenum);
    StripeManager *p_sm;
    get_entry(inum, &p_sm);
    if (p_sm!=NULL)
    {
        rc = p_sm->handle_client_read_resp(stripeid,stripeunitid,p_do,ccoid);
    }
    else
    {
        log->debug_log("No stripe manager found for inum:%llu",inum);
        rc=-2;
    }
    log->debug_log("rc:%d",rc);
    return rc;
}

int OpManager::handle_prepare_msg(struct dstask_ccc_recv_prepare *p_task, struct operation_participant **part_out )
{
    struct operation_participant *p_part;        
    struct OPHead *p_head = &p_task->dshead.ophead;
    log->debug_log("prepare for %llu",p_head->inum);
    int rc = this->get_operation_structure(p_head, &p_part);
    if (!rc)
    {
        log->debug_log("found operation.");    
        std::map<StripeId,struct dataobject_collection*>::iterator it =p_part->datamap->begin();
        for (it; it!=p_part->datamap->end(); it++)
        {
            rc = p_docache->get_entry(p_head->inum, it->first, &it->second->existing);
            if (!rc)
            {
                filelayout_raid *fl = (filelayout_raid *) &it->second->existing->metadata.filelayout[0];
                StripeUnitId suid = p_raid->get_my_stripeunitid(fl,p_part->ophead.offset,this->id);
                it->second->mycurrentversion=it->second->existing->metadata.versionvector[suid];
                log->debug_log("Current version of existing block:%u",it->second->mycurrentversion);
            }
            else
            {
                log->debug_log("No existing block found");
                it->second->mycurrentversion = 0;
            }
            log->debug_log("cache op: inum:%llu, sid:%u,rc:%d",p_head->inum,it->first,rc);
        }
        rc=0;
        *part_out = p_part;
    }
    else
    {
        log->debug_log("No such operation found");
    }
    return rc;
}

int OpManager::handle_docommit_msg(struct operation_dstask_docommit *p_task, struct operation_participant **part_out)
{
    struct operation_participant *p_part;        
    struct OPHead *p_head = &p_task->dshead.ophead;
    log->debug_log("docommit for %llu, stripeid:%u",p_head->inum,p_task->vvmap.stripeid);
    int rc = this->get_operation_structure(p_head, &p_part);
        
    if (!rc)
    {
        log->debug_log("found operation.");
        std::map<StripeId,struct dataobject_collection*>::iterator it = p_part->datamap->find(p_task->vvmap.stripeid);
        if (it!=p_part->datamap->end())
        {
            memcpy(&it->second->versionvector,&p_task->vvmap.versionvector, sizeof(it->second->versionvector));
            log->debug_log("version vector copied");
        }
        else
        {
            log->debug_log("no such version vector availavle");
        }
        *part_out = p_part;
        //p_part->ophead.status = opstatus_docommit;
        //log->debug_log("STATUS:csid:%u,seq:%u,status:%u",p_part->ophead.cco_id.csid,p_part->ophead.cco_id.sequencenum, p_part->ophead.status);
    }
    log->debug_log("rc:%d",rc);
    return rc;
}

int OpManager::handle_result_msg(struct dstask_proccess_result *p_task, struct operation_participant **part_out)
{
    struct operation_participant *p_part;        
    struct OPHead *p_head = &p_task->dshead.ophead;
    log->debug_log("process result for %llu",p_head->inum);
    int rc = this->get_operation_structure(p_head, &p_part);
    if (!rc)
    {
        log->debug_log("found operation:new status:%u",p_task->dshead.ophead.status);
        p_part->ophead.status = p_task->dshead.ophead.status;
        *part_out=p_part;
    }
    log->debug_log("rc:%d",rc);
    return rc;
}

int OpManager::handle_stripewrite_cancommit(struct dstask_ccc_stripewrite_cancommit_received *p_task)
{
    int rc=-1;  
    log->debug_log("op:csid=%u, inum:%llu",p_task->dshead.ophead.cco_id.csid, p_task->dshead.ophead.inum);
    stringstream ss;
    print_struct_participant(&p_task->recv_from, ss);
    log->debug_log("received from stripe unit id:%s",ss.str().c_str());
    StripeManager *p_sm;
    rc = this->get_entry(p_task->dshead.ophead.inum, &p_sm);
    if (!rc)
    {
        rc = p_sm->handle_ccc_stripewrite_cancommit(p_task);
    }
    log->debug_log("rc:%u",rc);
    return rc;
}


int OpManager::handle_directwrite_msg(struct dstask_spn_recv *p_task)
{
    int rc=-1;
    log->debug_log("direct write msg");
    //log->debug_log("inum:%llu,sid:%u,suid:%u,csid:%u,seq:%u",inum,stripeid,stripeunitid,ccoid.csid,ccoid.sequencenum);
    StripeManager *p_sm;
    get_entry(p_task->dshead.ophead.inum, &p_sm);
    if (p_sm!=NULL)
    {
        rc = p_sm->handle_directwrite(p_task);
    }
    else
    {
        log->debug_log("No stripe manager found for inum:%llu",p_task->dshead.ophead.inum);
        rc=-2;
    }
    log->debug_log("rc:%d",rc);
    return rc;
}