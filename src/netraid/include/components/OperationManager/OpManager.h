/* 
 * File:   OpManager.h
 * Author: markus
 *
 * Created on 8. Februar 2012, 16:30
 */

#ifndef OPMANAGER_H
#define	OPMANAGER_H

#include <time.h>

#include "OpData.h"
#include "coco/communication/ConcurrentQueue.h"
#include "logging/Logger.h"
#include "components/raidlibs/Libraid4.h"
#include "components/DataObjectCache/doCache.h"

static pthread_mutex_t segnum_mutex;
static uint32_t sequence_num=1;    

static uint32_t getSequenceNumber(){
    uint32_t seq;
    pthread_mutex_lock(&segnum_mutex);
    seq = sequence_num++;
    pthread_mutex_unlock(&segnum_mutex);
    return seq;
}


static uint64_t ccoid_to_uint64(struct CCO_id ccoid)
{
    uint64_t *res = (uint64_t*) &ccoid;
    return *res;
}


static void participant_settrue(struct Participants_bf *p, StripeUnitId id)
{
    switch (id)
    {
        case 0:
        {
            p->su0 = 1;
            break;
        }
        case 1:
        {
            p->su1 = 1;
            break;
        }
        case 2:
        {
            p->su2 = 1;
            break;
        }
        case 3:
        {
            p->su3 = 1;
            break;
        }
        case 4:
        {
            p->su4 = 1;
            break;
        }
        case 5:
        {
            p->su5 = 1;
            break;
        }
        case 6:
        {
            p->su6 = 1;
            break;
        }
        case 7:
        {
            p->su7 = 1;
            break;
        }
        case 8:
        {
            p->su8 = 1;
            break;
        }
        case 9:
        {
            p->su9 = 1;
            break;
        }
        case 10:
        {
            p->su10 = 1;
            break;
        }
        case 11:
        {
            p->su11 = 1;
            break;
        }
    }
}

static inline void participant_settrue(struct Participants_bf *p, struct Participants_bf *r)
{
    p->su0 |= r->su0;
    p->su1 |= r->su1;
    p->su2 |= r->su2;
    p->su3 |= r->su3;
    p->su4 |= r->su4;
    p->su5 |= r->su5;
    p->su6 |= r->su6;
    p->su7 |= r->su7;
    p->su8 |= r->su8;
    p->su9 |= r->su9;
    p->su10 |= r->su10;
    p->su11 |= r->su11;
}


static void fullParticipants(struct Participants_bf *p, uint8_t servercnt)
{
    p->start=3;
    for (StripeUnitId i =0; i<servercnt; i++)
    {
        participant_settrue(p,i);
    }
}


static bool bitfield_equal(struct Participants_bf *p_bfa, struct Participants_bf *p_bfb)
{
    return p_bfa->su0==p_bfb->su0 && \
    p_bfa->su1==p_bfb->su1 && \
    p_bfa->su2==p_bfb->su2 && \
    p_bfa->su3==p_bfb->su3 && \
    p_bfa->su4==p_bfb->su4 && \
    p_bfa->su5==p_bfb->su5 && \
    p_bfa->su6==p_bfb->su6 && \
    p_bfa->su7==p_bfb->su7 && \
    p_bfa->su8==p_bfb->su8 && \
    p_bfa->su9==p_bfb->su9 && \
    p_bfa->su10==p_bfb->su10 && \
    p_bfa->su11==p_bfb->su11 ;
}

static bool get_received_from_id(struct Participants_bf *p, uint32_t *res)
{
    bool found=false;
    if (p->su0)
    {
        *res=0;
        found=true;
    }
    if (p->su1)
    {
        if (found) return false;            
        *res=1;
        found=true;
    }
    if (p->su2)
    {
        if (found) return false;            
        *res=2;
        found=true;
    }
    if (p->su3)
    {
        if (found) return false;            
        *res=3;
        found=true;
    }
    if (p->su4)
    {
        if (found) return false;            
        *res=4;
        found=true;
    }
    if (p->su5)
    {
        if (found) return false;            
        *res=5;
        found=true;
    }
    if (p->su6)
    {
        if (found) return false;            
        *res=6;
        found=true;
    }
    if (p->su7)
    {
        if (found) return false;            
        *res=7;
        found=true;
    }
    if (p->su8)
    {
        if (found) return false;            
        *res=8;
        found=true;
    }
    if (p->su9)
    {
        if (found) return false;            
        *res=9;
        found=true;
    }
    if (p->su10)
    {
        if (found) return false;            
        *res=10;
        found=true;
    }
    if (p->su11)
    {
        if (found) return false;            
        *res=11;
        found=true;
    }
    return found;
}

int calculate_parity_block(struct operation_primcoordinator *p_op);
int calculate_parity_block(struct operation_participant *p_op);


static struct operation_dstask_docommit* create_dstask_send_docommit(operation* p_op)
{
    struct OPHead *p_head = (struct OPHead*) p_op;
    struct operation_dstask_docommit *p_task = new struct operation_dstask_docommit;
    p_task->dshead.ophead = *p_head;
    p_task->dshead.ophead.subtype = send_docommit;
    p_task->dshead.ophead.type = ds_task_type;
    return p_task;
}

void print_CCO_id(struct CCO_id *p_cco, stringstream& ss);
void print_operation_head(struct OPHead *p_head, stringstream& ss);
void print_op_client(struct operation_client *p_op, stringstream& ss);
void print_op_participant(struct operation_participant *p_op, stringstream& ss);
void print_op_primcoordinator(struct operation_primcoordinator *p_op, stringstream& ss);
//void print_op_seccoordinator(struct operation_seccoordinator *p_op, stringstream& ss);
void print_operation(operation *p_op, stringstream& ss);
void print_struct_participant(struct Participants_bf *p, stringstream& ss);
void print_dataobject(struct data_object *p_obj, stringstream& ss);
void print_dataobject_metadata(struct dataobject_metadata *p_obj, stringstream& ss);


void free_dataobject_collection(struct dataobject_collection *p);
void free_operation_participant(operation_participant *p);
void free_dstask_writetodisk(dstask_writetodisk *p_write);
void free_dstask_spn_recv(struct dstask_spn_recv *p);
void free_operation_primcoordinator(struct operation_primcoordinator *p);
void free_datacollection_primco(struct datacollection_primco *p);
void free_data_object(struct data_object *p);
void free_parity_data(struct paritydata *p);


class StripeManager{
public:
    StripeManager(Logger *p_log, int (*p_cb)(queue_priorities, OPHead*), doCache *docache, serverid_t servid,Filestorage *fileio){
        log = p_log;
        globallock_mutex = PTHREAD_MUTEX_INITIALIZER;
        ops_mutex = PTHREAD_MUTEX_INITIALIZER;
        partops_mutex = PTHREAD_MUTEX_INITIALIZER;
        writeops_mutex = PTHREAD_MUTEX_INITIALIZER;
        readops_mutex = PTHREAD_MUTEX_INITIALIZER;
        segnum_mutex =  PTHREAD_MUTEX_INITIALIZER;
        switchmap_mutex = PTHREAD_MUTEX_INITIALIZER;
        id = servid;
        p_queuePush = p_cb;
        p_ops           = new std::map<uint64_t, std::vector<struct OPHead*>* >;    
        p_readops       = new std::map<uint64_t, struct operation_client_read*>;
        p_writeops      = new std::map<uint64_t, struct operation_composite*>;
        p_partops       = new std::map<uint64_t, struct operation_participant*>; 
        operation_active=false;
        p_raid = new Libraid4(log);
        p_docache = docache;
        p_fileio = fileio;
    }
    
    ~StripeManager()
    {
        pthread_mutex_destroy(&globallock_mutex);
        delete p_raid;
    }
    
    /*
    bool mapinsert_readops(struct CCO_id ccoid , struct operation_client_read *p_op)
    {
        pthread_mutex_lock(&readops_mutex);
        std::pair<std::map<uint64_t, struct operation_client_read*>::iterator,bool> ret;
        try 
        {
                ret = p_readops->insert(std::pair<uint64_t, struct operation_client_read*>(ccoid_to_uint64(ccoid), p_op));
        }
        catch (...)
        {
            log->error_log("Error occured during insert:%u.%u",ccoid.csid,ccoid.sequencenum);
        }
        pthread_mutex_unlock(&readops_mutex);
        return ret->second;
    }**/
    
    int insert(struct operation_participant *p_op)
    {
        int rc=-1;
        std::map<uint64_t, struct operation_participant*>::iterator it = p_partops->find(ccoid_to_uint64(p_op->ophead.cco_id));
        if (it!= p_partops->end())
        {
            log->debug_log("such an operation alredy exists.");
            rc=-2;
        }
        else
        {
            log->debug_log("inserting.");
            p_op->isready=false;
            p_partops->insert(std::pair<uint64_t, struct operation_participant*>(ccoid_to_uint64(p_op->ophead.cco_id), p_op));
            /*struct dstask_ccc_send_received *p_task = new struct dstask_ccc_send_received;
            p_task->dshead.ophead = p_op->ophead;
            p_task->dshead.ophead.type = ds_task_type;
            p_task->dshead.ophead.subtype = send_received;
            p_task->participants = p_op->participants;
            p_task->receiver = p_op->primcoordinator;
            p_queuePush(prim_recv,(struct OPHead*)p_task);*/
            rc=0;            
        }
        log->debug_log("rc:%u",rc);
        return rc;
    }
    
    int insert(struct operation_client_read *p_op)
    {
        int rc=-1;
        std::pair<std::map<uint64_t, struct operation_client_read*>::iterator,bool> ret;
        bool locked=true;
        pthread_mutex_lock(&readops_mutex);
        p_op->ophead.cco_id.sequencenum = getSequenceNumber();
        uint64_t ccoid_combined = ccoid_to_uint64(p_op->ophead.cco_id);
        std::map<uint64_t, struct operation_client_read*>::iterator it = p_readops->find(ccoid_combined);
        if (it!= p_readops->end())
        {
            log->debug_log("the client already has an read request for that operation.");
            rc=-2;
        }
        else
        {
            log->debug_log("inserting:csid:%u,seq:%u",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum);   
            p_op->isready=false;
            //mapinsert_readops(p_op->ophead.cco_id,p_op);
            ret = p_readops->insert(std::pair<uint64_t, struct operation_client_read*>(ccoid_combined, p_op));
            pthread_mutex_unlock(&readops_mutex);
            locked=false;
            if (ret.second)
            {
                std::map<uint32_t,struct operation_group *>::iterator it2 = p_op->groups->begin();
                for (it2; it2!= p_op->groups->end(); it2++)
                {
                    it2->second->isready=false;
                    std::map<StripeUnitId,struct StripeUnit*>::iterator it3 = it2->second->sumap->begin();
                    for (it3; it3!=it2->second->sumap->end(); it3++)
                    {
                        log->debug_log("stripeunit id :%u",it3->first);
                        struct dstask_spn_send_read *p_t = new struct dstask_spn_send_read;
                        p_t->dshead.ophead=p_op->ophead;
                        p_t->dshead.ophead.type = cl_task_type;
                        p_t->dshead.ophead.subtype = send_spn_read;
                        p_t->end = it3->second->end;
                        p_t->receiver = it3->second->assignedto;
                        p_t->stripeid = it2->first;
                        p_t->stripeunitid = it3->first;
                        p_t->offset = it3->second->start;
                        log->debug_log("XXXpushed read request:sid:%u,suid:%u,server:%u",p_t->stripeid, p_t->stripeunitid,p_t->receiver);
                        p_queuePush(client_prim,(struct OPHead*)p_t);
                    }
                }
                rc=0;            
            }
            else
            {
                log->debug_log("Error occured while inserting map entry:csid:%u,seq:%u",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum);
            }
        }   
        if (locked)pthread_mutex_unlock(&readops_mutex);
        return rc;
    }
        
    int insert(struct operation_composite *p_op, void *data)
    {
        int rc=0;
        int i;
        bool locked=true;
        pthread_mutex_lock(&writeops_mutex);
        p_op->ophead.cco_id.sequencenum = getSequenceNumber();
        uint64_t ccocomb = ccoid_to_uint64(p_op->ophead.cco_id);
        log->debug_log("insert:inum:%u,csid:%u,seq:%u",p_op->ophead.inum,p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum);
        std::map<uint64_t, struct operation_composite*>::iterator itsm = p_writeops->find(ccocomb);
        if (itsm!= p_writeops->end())
        {
            log->debug_log("the client already has an write request for that operation.");
            rc=-2;
        }
        else
        {
            log->debug_log("inserted.");
            p_writeops->insert(std::pair<uint64_t, struct operation_composite*>(ccocomb, p_op));
            pthread_mutex_unlock(&writeops_mutex);
            locked=false;
            filelayout_raid *p_fl = (filelayout_raid *) &p_op->ophead.filelayout[0];
            struct FileStripeList *p_fsl = p_raid->get_stripes(p_fl, p_op->ophead.offset, p_op->ophead.length);
            std::map<StripeId,struct Stripe_layout*>::iterator it = p_fsl->stripemap->begin();
            
            size_t lengthdecr = p_op->ophead.length;
            size_t tmpoffset = p_op->ophead.offset;
            char *data_iter = (char*) data;
            for (it; it!=p_fsl->stripemap->end(); it++)
            {
                struct operation_client_write *p_wrop = new struct operation_client_write;
                p_wrop->group = new struct operation_group;
                p_wrop->group->sumap = new std::map<StripeUnitId,struct StripeUnit*>();
                memcpy(&p_wrop->ophead,&p_op->ophead, sizeof(struct OPHead));
                p_wrop->ophead.cco_id.sequencenum = getSequenceNumber();
                p_wrop->ophead.offset = tmpoffset;
                p_wrop->ophead.length = get_stripe_size(p_fl)<lengthdecr ? get_stripe_size(p_fl) :lengthdecr ;
                tmpoffset+=p_wrop->ophead.length;
                log->debug_log("stripeid:%u, seq:%u, oplength:%u",it->first,p_wrop->ophead.cco_id.sequencenum,p_wrop->ophead.length);
                if (!lengthdecr) break;
                
                memset(&p_wrop->participants,0,sizeof(struct Participants_bf));
                p_wrop->participants.end=0;
                p_wrop->participants.start=(uint8_t)3;
                p_wrop->group->stripe_id = it->first;         
                if (p_op->ophead.subtype==0)
                {
                    p_wrop->ophead.type = it->second->isfull ?  operation_client_s_write : operation_client_su_write;
                }
                else
                {
                    p_wrop->ophead.type = it->second->isfull ?  operation_client_s_write_direct : operation_client_su_write_direct;
                }
                
                std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->blockmap->begin();
                for (it2; it2!=it->second->blockmap->end(); it2++)
                {
                    log->debug_log("Length left = %u",lengthdecr);
                    log->debug_log("StripeUnitId:%u",it2->first);
                    if (!lengthdecr) break;
                    struct StripeUnit *p_su = new struct StripeUnit; //it2->second;
                    p_su->start = it2->second->start;
                    p_su->end = it2->second->end;
                    p_su->assignedto = it2->second->assignedto;
                    p_su->opsize = (lengthdecr > p_fl->raid4.stripeunitsize) ? p_fl->raid4.stripeunitsize : lengthdecr;
                    p_su->newdata = data_iter;
                    data_iter += p_su->opsize;
                    p_wrop->group->sumap->insert(std::pair<StripeUnitId,struct StripeUnit*>(it2->first,p_su));
                    if (it->second->isfull)
                    {
                        for (i=0; i<p_fsl->size; i++)
                        {
                            participant_settrue(&p_wrop->participants, i);
                        }
                        log->debug_log("Participants set true for %d.",i);
                    }
                    else
                    {
                        participant_settrue(&p_wrop->participants, it2->first);
                        p_wrop->ophead.cco_id.sequencenum = getSequenceNumber();
                    }                    
                    lengthdecr = (lengthdecr > p_fl->raid4.stripeunitsize) ? lengthdecr-p_fl->raid4.stripeunitsize : 0;
                    //log->debug_log("%s.",p_su->newdata);
                }
                log->debug_log("Participants:%u",*(uint16_t*)&p_wrop->participants);
                p_op->ops->insert(std::pair<uint32_t, operation*>(p_wrop->ophead.cco_id.sequencenum,(operation*)p_wrop));
                p_op->ophead.stripecnt++;
            } 
            free_FileStripeList(p_fsl);
        }        
        if (locked) pthread_mutex_unlock(&readops_mutex);
        log->debug_log("rc:%d",rc);
        return rc;
    }
    
    int handle_client_result(struct CCO_id ccoid, opstatus result)
    {
        int rc;
        bool success=true;
        log->debug_log("csid:%u, sequence:%u",ccoid.csid,ccoid.sequencenum);
        struct operation_composite *p_op;
        pthread_mutex_lock(&writeops_mutex);
        rc = get_op(ccoid, &p_op);
        pthread_mutex_unlock(&writeops_mutex);
        if (!rc)
        {
            log->debug_log("Result:%u.",result);
            pthread_mutex_lock(&ops_mutex);
            std::map<uint32_t,operation*>::iterator it2 = p_op->ops->begin();//find(ccoid.sequencenum);
            for (it2;it2!=p_op->ops->end();it2++)
            {                
                if (it2->first==ccoid.sequencenum)
                {
                    if (p_op->ophead.type == operation_client_composite_directwrite_type)
                    {
                        if (it2->second->client_write_op.ophead.status==clop_intermediate_alpha)
                        {
                            it2->second->client_write_op.ophead.status=result;
                        }
                        else 
                        {
                            it2->second->client_write_op.ophead.status=clop_intermediate_alpha;
                        }
                        rc=0;
                    }
                    else
                    {                       
                        it2->second->client_write_op.ophead.status=result;
                        log->debug_log("registered result:%u for seq:%u",result,it2->first);
                        rc=0;
                    }
                }
                if (opstatus_success!=it2->second->client_write_op.ophead.status)
                {
                    log->debug_log("Operation seq %u not successful yet %u!=%u.",it2->first,opstatus_success,it2->second->client_write_op.ophead.status);
                    success=false;
                }
            }
            pthread_mutex_unlock(&ops_mutex);
            if (success)
            {
                p_op->ophead.status=opstatus_success;
                log->debug_log("Operation csid:%u,seq:%u successful.",ccoid.csid,ccoid.sequencenum);
                rc=0;
            }
            else
            {
                log->debug_log("sequence num:%u not successful yet.",ccoid.sequencenum);
                rc=-1;
            }
        }
        else
        {
            struct operation_client_read *p_op;
            rc = get_op(ccoid, &p_op);
            if (!rc)
            {
                log->debug_log("Result:%u.",result);
                p_op->ophead.status=result;
                rc=0;
            }
            else
            {
                log->debug_log("no such operation found.");
                rc=-1;
            }
        }
        return rc;
    }
    
    int get_op(struct CCO_id ccoid, struct operation_client_read **p_op)
    {
        int rc=-1;
        uint64_t ccoid_combined = ccoid_to_uint64(ccoid);
        log->debug_log("find:%llu, csid:%u,seq:%u",ccoid_combined,ccoid.csid, ccoid.sequencenum);   
       // pthread_mutex_lock(&readops_mutex);
        std::map<uint64_t, struct operation_client_read*>::iterator it = p_readops->find(ccoid_combined);
       // pthread_mutex_unlock(&readops_mutex);
        if (it!= p_readops->end())
        {
            *p_op=it->second;
            rc=0;
        }
        else
        {
            log->debug_log("No such operation with csid:%u, seq:%u",ccoid.csid,ccoid.sequencenum);
            rc=-2;
        }
        return rc;
    }
    
    int get_op(struct CCO_id ccoid, uint64_t offset, struct operation_primcoordinator **p_out)
    {
        log->debug_log("start:csid:%u,seq:%u,offset:%llu",ccoid.csid,ccoid.sequencenum,offset);
        int rc=-3;
        //pthread_mutex_lock(&ops_mutex);
        std::map<uint64_t, std::vector<struct OPHead*>* >::iterator it = this->p_ops->find(offset);
        if (it!=this->p_ops->end())
        {
            struct OPHead *p_existing;            
            std::vector<struct OPHead*>::iterator it2 = it->second->begin();
            while (it2 != it->second->end())
            {
                p_existing = *it2;
                log->debug_log("csid:%u,seq:%u",p_existing->cco_id.csid,p_existing->cco_id.sequencenum);
                if (p_existing->cco_id.csid==ccoid.csid && p_existing->cco_id.sequencenum==ccoid.sequencenum)
                {
                    log->debug_log("Found operation");
                    //if (p_existing->type==operation_primcoord_type)
                    //{
                        *p_out = (struct operation_primcoordinator*)p_existing;
                        //log->debug_log("return:%p",p_existing);
                        rc=0;
                        break;
                    /*}
                    else
                    {
                        log->debug_log("Invalid type:%u",p_existing->type);
                    }*/
                }
                it2++;
            }
        }
        else
        {
            log->debug_log("no vector found for offset:%u.",offset);
            rc=-1;
        }
        //pthread_mutex_unlock(&ops_mutex);
        log->debug_log("operation found:rc=%d",rc);
        return rc;
    }
    
    int get_op(struct CCO_id ccoid, struct operation_participant **p_out)
    {
        int rc=-1;
        log->debug_log("start: look for csid:%u, seq:%u.",ccoid.csid, ccoid.sequencenum);
        std::map<uint64_t, struct operation_participant*>::iterator it = p_partops->find(ccoid_to_uint64(ccoid));
        if (it!=this->p_partops->end())
        {
            *p_out = it->second;
            log->debug_log("return:%p",*p_out);
            rc=0;
        }
        else
        {
            log->debug_log("No operation found");
        }
        return rc;
    }
    
    int get_op(struct CCO_id ccoid, struct operation_composite **p_out)
    {
        int rc=-1;
        log->debug_log("start: csid:%u,seq:%u",ccoid.csid,ccoid.sequencenum);
        //pthread_mutex_lock(&writeops_mutex);
        std::map<uint64_t,struct operation_composite*>::iterator it = p_writeops->begin();
        for (it;it!= p_writeops->end();it++)
        {
            //log->debug_log("csid:%u,seq:%u",it->)
            std::map<uint32_t,operation*>::iterator it2 = it->second->ops->find(ccoid.sequencenum);
            if (it2!=it->second->ops->end())
            {                
                *p_out = it->second;
                rc=0;
                break;
            }
        }
        //pthread_mutex_unlock(&writeops_mutex);
        log->debug_log("operation found=rc:%d",rc);
        return rc;
    }
        
    struct OPHead* get_op(struct OPHead *p_head)
    {
        log->debug_log("start.");
        //pthread_mutex_lock(&ops_mutex);
        std::map<uint64_t, std::vector<struct OPHead*>* >::iterator it = this->p_ops->find(p_head->offset);
        if (it!=this->p_ops->end())
        {
            struct OPHead *p_existing;
            std::vector<struct OPHead*>::iterator it2 = it->second->begin();
            while (it2 != it->second->end())
            {
                p_existing = *it2;
                if (p_existing->cco_id.csid==p_head->cco_id.csid && p_existing->cco_id.sequencenum==p_head->cco_id.sequencenum)
                {
                    pthread_mutex_unlock(&ops_mutex);
                    return p_existing;
                }
            }
        }
       // pthread_mutex_unlock(&ops_mutex);
        return NULL;
    }
    
    int insert(void *data)
    {
        int rc = -1;
        bool locked=true;
        bool conflict_detected = false;
        struct OPHead *p_ophead = (struct OPHead *) data;
        log->debug_log("head inum:%llu, csid:%u.seq:%u",p_ophead->inum, p_ophead->cco_id.csid, p_ophead->cco_id.sequencenum);
        pthread_mutex_lock(&ops_mutex);
        std::map<uint64_t, std::vector<struct OPHead*>* >::iterator it = this->p_ops->lower_bound(p_ophead->offset);
        if (it==this->p_ops->end())
        {
            std::vector<struct OPHead*> *opvec = new std::vector<struct OPHead*>;
            opvec->push_back(p_ophead);
            this->p_ops->insert(std::pair<uint64_t,std::vector<struct OPHead*>*>(p_ophead->offset,opvec));
            pthread_mutex_unlock(&ops_mutex);
            locked=false;
            rc = 0;
            log->debug_log("pushed back. no lower bound found.");
        }
        else
        {
            bool keepgoing=true;
            while(keepgoing)
            {   // it start,ophead
                log->debug_log("offset first:%llu.",it->first);
                if (it->first > p_ophead->offset+p_ophead->length) 
                {
                    keepgoing=false;
                }
                else
                {   
                    std::vector<struct OPHead*> *opvec = it->second;
                    std::vector<struct OPHead*>::iterator it2 = opvec->begin();
                    while (it2 != opvec->end())
                    {
                        struct OPHead *head = *it2;
                        log->debug_log("it2: offset:%u",head->offset);
                        if (conflict_check(it->first,head->offset + head->length,p_ophead))
                        {
                             conflict_detected = true;
                             log->debug_log("Conflict detected.");
                        }
                        it2++;
                    }
                }
                if (it==this->p_ops->end())
                {
                    keepgoing=false;
                }
                else
                {
                    it++;
                }
            }
        }  
        if (locked) pthread_mutex_lock(&ops_mutex);
        if (!conflict_detected)
        {
            rc = handle_ready_phase1(p_ophead);
            log->debug_log("no conflict detected...");
        }
        log->debug_log("rc:%u",rc);
        return rc;
    }

    int handle_ccc_received(struct dstask_ccc_received *p_task)
    {
        int rc = -1;
        struct OPHead *p_ophead = (struct OPHead *) &p_task->dshead.ophead;
        stringstream ss;
        print_operation_head(&p_task->dshead.ophead, ss);
        log->debug_log("%s.\n",ss.str().c_str());        
        log->debug_log("head inum:%llu, csid:%u.seq:%u",p_ophead->inum, p_ophead->cco_id.csid, p_ophead->cco_id.sequencenum);
        struct operation_primcoordinator *p_primop;
        pthread_mutex_lock(&ops_mutex);
        rc = get_op(p_task->dshead.ophead.cco_id, p_task->dshead.ophead.offset,&p_primop);

        if (!rc)
        {
            pthread_mutex_lock(&p_primop->mutex_private);
            log->debug_log("head inum:%llu, csid:%u.seq:%u",p_primop->ophead.inum, p_primop->ophead.cco_id.csid, p_primop->ophead.cco_id.sequencenum);
            participant_settrue(&p_primop->received_from, &p_task->recv_from);
            stringstream ss;
            print_struct_participant(&p_primop->received_from, ss);
            log->debug_log("%s",ss.str().c_str());        
        }
        else
        {
            p_primop = new struct operation_primcoordinator;
            memcpy(&p_primop->ophead, &p_task->dshead.ophead, sizeof(struct OPHead));
            memset(&p_primop->received_from, 0, sizeof(struct Participants_bf));
            p_primop->received_from.start = 3;            
            memcpy(&p_primop->participants ,& p_task->participants, sizeof(struct Participants_bf));
            p_primop->ophead.status = opstatus_init;
            p_primop->mutex_private = PTHREAD_MUTEX_INITIALIZER;
            pthread_mutex_lock(&p_primop->mutex_private);
                        
            stringstream ss;
            print_struct_participant(&p_primop->participants, ss);
            log->debug_log("received participants:%s.\n",ss.str().c_str());        
            
            struct filelayout_raid4 *fl = (struct filelayout_raid4*) &p_primop->ophead.filelayout[0];
            participant_settrue(&p_primop->received_from, &p_task->recv_from);
            p_primop->recv_timestamp = time(0);
            p_primop->datalength = fl->stripeunitsize;
            p_primop->datamap_primco = new std::map<StripeId,struct datacollection_primco*>;
            struct datacollection_primco *p_dc = new struct datacollection_primco;
            memset(&p_dc->versionvec,0,sizeof(p_dc->versionvec));
            p_dc->existing=NULL;
            p_dc->fhvalid=false;
            p_dc->finalparity=NULL;
            p_dc->paritymap= new std::map<uint32_t,struct paritydata*>;
            p_dc->newobject=NULL;
            p_primop->datamap_primco->insert(std::pair<StripeId,struct datacollection_primco*>(p_task->stripeid,p_dc));
            log->debug_log("created primco datamap:%p",p_primop->datamap_primco);
        
            std::vector<struct OPHead*> *opvec;
            if (rc==-1)
            {
                opvec = new std::vector<struct OPHead*>;
                opvec->push_back(&p_primop->ophead);
                this->p_ops->insert(std::pair<uint64_t,std::vector<struct OPHead*>*>(p_ophead->offset,opvec));
                log->debug_log("pushed back, created vec.");
            }
            else
            {
                std::map<uint64_t, std::vector<struct OPHead*>* >::iterator it = this->p_ops->find(p_ophead->offset);
                it->second->push_back(&p_primop->ophead);
                log->debug_log("pushed back. no lower bound found.");
            }                   
            rc = 0;          
        }        
        pthread_mutex_unlock(&ops_mutex);
        if(bitfield_equal(&p_primop->participants, &p_primop->received_from))
        {
            log->debug_log("Operation is ready.");
            rc = handle_primcoord_phase_1(p_primop);
        }
        else
        {
            log->debug_log("%u:%u",p_primop->participants,p_primop->received_from);
        }
        pthread_mutex_unlock(&p_primop->mutex_private);
        log->debug_log("rc:%u",rc);
        return rc;
    }
        
    int handle_ccc_stripewrite_cancommit(struct dstask_ccc_stripewrite_cancommit_received *p_task)
    {
        int rc = -1;
        bool oplocked=false;
        bool parityvec_found=false;
        struct OPHead *p_ophead = (struct OPHead *) &p_task->dshead.ophead;
        stringstream ss;
        print_operation_head(&p_task->dshead.ophead, ss);
        log->debug_log("%s.\n",ss.str().c_str());        
        log->debug_log("head inum:%llu, csid:%u.seq:%u",p_ophead->inum, p_ophead->cco_id.csid, p_ophead->cco_id.sequencenum);
        struct operation_primcoordinator *p_primop;
        
        filelayout_raid *p_fl = (filelayout_raid *) &p_task->dshead.ophead.filelayout[0];
        StripeId sid = p_raid->getStripeId(p_fl, p_task->dshead.ophead.offset);
        
        uint32_t recv;
        get_received_from_id(&p_task->recv_from, &recv);
        pthread_mutex_lock(&ops_mutex);
        rc = get_op(p_task->dshead.ophead.cco_id, p_task->dshead.ophead.offset,&p_primop);
        if (!rc)
        {
            pthread_mutex_lock(&p_primop->mutex_private);
            oplocked=true;
            log->debug_log("head inum:%llu, csid:%u.seq:%u",p_primop->ophead.inum, p_primop->ophead.cco_id.csid, p_primop->ophead.cco_id.sequencenum);
            participant_settrue(&p_primop->received_from, &p_task->recv_from);
            std::map<StripeId,struct datacollection_primco*>::iterator it = p_primop->datamap_primco->find(sid);
            if (it!=p_primop->datamap_primco->end())
            {
                it->second->versionvec[recv]=p_task->version;
                if (it->second->newobject!=NULL)
                //if (it->second->versionvec[default_groupsize]!=0) 
                {
                    parityvec_found=true;
                    log->debug_log("parityvec found.");
                }                
                for (int i=0; i<=default_groupsize; i++)
                {
                    log->debug_log("version:%u:%u",i,it->second->versionvec[i]);        
                }
            }
            else
            {
                log->debug_log("why is there no map");
            }
            stringstream ss;
            print_struct_participant(&p_primop->received_from, ss);
            log->debug_log("%s.\n",ss.str().c_str());
            log->debug_log("set true");
        }
        else
        {
            p_primop = new struct operation_primcoordinator;
            memcpy(&p_primop->ophead, &p_task->dshead.ophead, sizeof(struct OPHead));
            memset(&p_primop->received_from, 0, sizeof(struct Participants_bf));
            memset(&p_primop->participants, 0, sizeof(struct Participants_bf));
            p_primop->received_from.start = 3;        
            p_primop->mutex_private = PTHREAD_MUTEX_INITIALIZER;
            pthread_mutex_lock(&p_primop->mutex_private);
            oplocked=true;
            fullParticipants(&p_primop->participants, p_fl->raid4.groupsize-1);            
            log->debug_log("Groupsize:%u",p_fl->raid4.groupsize-1);
            
            stringstream ss;
            print_struct_participant(&p_primop->participants, ss);
            log->debug_log("participants:%s.\n",ss.str().c_str());        
            
            participant_settrue(&p_primop->received_from, &p_task->recv_from);
            p_primop->recv_timestamp = time(0);
            p_primop->datalength = p_task->dshead.ophead.length;
            
            p_primop->datamap_primco = new std::map<StripeId,struct datacollection_primco*>;
            struct datacollection_primco *p_dc = new struct datacollection_primco;
            memset(&p_dc->versionvec,0,sizeof(p_dc->versionvec));            
            p_dc->versionvec[recv]=p_task->version;
            log->debug_log("Version of %u is %u",recv,p_task->version);
            parityvec_found=false;
            p_dc->existing=NULL;
            p_dc->fhvalid=false;
            p_dc->finalparity=NULL;
            p_dc->paritymap= new std::map<uint32_t,struct paritydata*>;
            p_dc->newobject=NULL;
            p_primop->datamap_primco->insert(std::pair<StripeId,struct datacollection_primco*>(sid,p_dc));
            log->debug_log("created primco datamap:%p",p_primop->datamap_primco);
            std::vector<struct OPHead*> *opvec;
            if (rc==-1)
            {
                opvec = new std::vector<struct OPHead*>;
                opvec->push_back(&p_primop->ophead);
                this->p_ops->insert(std::pair<uint64_t,std::vector<struct OPHead*>*>(p_ophead->offset,opvec));
                log->debug_log("pushed back, created vec.");
            }
            else
            {
                std::map<uint64_t, std::vector<struct OPHead*>* >::iterator it = this->p_ops->find(p_ophead->offset);
                it->second->push_back(&p_primop->ophead);
                log->debug_log("pushed back. no lower bound found.");
            }                   
            rc = 0;            
        }        
        
        pthread_mutex_unlock(&ops_mutex);
        if(bitfield_equal(&p_primop->participants, &p_primop->received_from) && parityvec_found )
        {
            log->debug_log("Operation is ready.");
            rc = handle_primcoord_stripewrite_phase1(p_primop);
        }
        else
        {
            log->debug_log("%u:%u",p_primop->participants,p_primop->received_from);
        }
        if (oplocked) pthread_mutex_unlock(&p_primop->mutex_private);
        log->debug_log("rc:%u",rc);
        return rc;
    }
        
    /**
     * @Todo    handle stripe information
     * @param p_op
     * @return 
     */
    int handle_ccc_cancommit(struct dstask_cancommit *p_task)
    {
        int rc=-1;
        struct operation_primcoordinator *p_primop;
        rc = get_op(p_task->dshead.ophead.cco_id, p_task->dshead.ophead.offset,&p_primop);
        if (rc)
        {
            log->debug_log("No such operation existing.");
        }
        else
        {
            bool all_stripes_ready = true;
            log->debug_log("created primco datamap:%p",p_primop->datamap_primco);
            log->debug_log("Operation found. stripe id:%u",p_task->stripeid);
            pthread_mutex_lock(&p_primop->mutex_private);
            std::map<StripeId,struct datacollection_primco*>::iterator it = p_primop->datamap_primco->find(p_task->stripeid);
            if (it!=p_primop->datamap_primco->end())
            {
                uint32_t id;
                bool ok = get_received_from_id(&p_task->recv_from,&id);
                if (!ok)
                {
                    log->error_log("invalid received from received...");
                }
                log->debug_log("received from stripeunit:%u, has version:%u.",id, p_task->version);
                struct paritydata *p_pdata = new struct paritydata;
                
                p_pdata->length = p_task->length;
                p_pdata->data = p_task->recv;
                it->second->paritymap->insert(std::pair<uint32_t,struct paritydata*>(id,p_pdata));
                participant_settrue(&p_primop->received_from,&p_task->recv_from);
                it->second->versionvec[id]=p_task->version;
                if (!bitfield_equal(&p_primop->participants, &p_primop->received_from))
                {
                    log->debug_log("phase not ready ready.");
                    all_stripes_ready = false;
                }
                else
                {
                    rc=0;            
                    log->debug_log("Phase ready.");
                }
            }
            else
            {
                log->debug_log("no such stripe found");
            }
            if (all_stripes_ready)
            {
                rc = handle_primcoord_phase_2(p_primop);
            }
            pthread_mutex_unlock(&p_primop->mutex_private);
        }
        log->debug_log("rc:%d",rc);
        return rc;
    }
        
    int handle_ccc_committed(struct dstask_ccc_recv_committed *p_task)
    {
        int rc=-1;
        struct operation_primcoordinator *p_primop;
        rc = get_op(p_task->dshead.ophead.cco_id,p_task->dshead.ophead.offset,&p_primop);
        if (rc)
        {
            log->debug_log("No such operation existing.");
        }
        else
        {
            pthread_mutex_lock(&p_primop->mutex_private);
            log->debug_log("Operation found.");
            uint32_t id;
            get_received_from_id(&p_task->recv_from,&id);
            //p_ex->paritydiffmap->insert(std::pair<uint32_t,void*>(id,p_op->recv_block));
            log->debug_log("received from..stripe unit.%u",id);
            participant_settrue(&p_primop->received_from, id);
            if (bitfield_equal(&p_primop->participants, &p_primop->received_from))
            {
                log->debug_log("phase three ready.");
                rc = handle_primcoord_phase_3(p_primop);
            }
            else
            {
                rc=0;            
                log->debug_log("Phase not ready yet.");
            }
            pthread_mutex_unlock(&p_primop->mutex_private);
        }
        log->debug_log("rc:%d",rc);
        //free(p_task);
        return rc;
    }
    
    int cleanup(struct operation_client_write *op)
    {
        int rc=-1;
        //pthread_mutex_lock(&writeops_mutex);
        log->debug_log("op inum:%llu",op->ophead.inum);
        
        /*std::map<uint32_t,struct operation_group *>::iterator it = op->groups->begin();
        for (it; it!=op->groups->end(); it++)
        {
            std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->sumap.begin();
            for (it2; it2!=it->second->sumap.end(); it2++)
            {
                //free(it2->second->newdata);
                //free(it2->second->paritydata);
                //free(it2->second->olddata);
               // free(it2->second);
            }
            //free(it->second);
        }
        free(op->groups);**/
        log->debug_log("groups deleted.");
        rc=0;
        
        //p_writeops->erase()
        //pthread_mutex_unlock(&writeops_mutex);
        pthread_mutex_lock(&ops_mutex);
        std::map<uint64_t, std::vector<struct OPHead*>* >::iterator it3 = p_ops->find(op->ophead.offset);
        if (it3!=p_ops->end())
        {
            std::vector<struct OPHead*>::iterator it4 = it3->second->begin();
            for (it4; it4!=it3->second->end(); it4++)
            {
                if ((*it4)->cco_id.csid==op->ophead.cco_id.csid && (*it4)->cco_id.sequencenum==op->ophead.cco_id.sequencenum)
                {
                    log->debug_log("found operation to remove");
                    it3->second->erase(it4);
                    //if (it3->second->empty())
                    //{
                    //    log->debug_log("removed vector of offset:%llu",it3->first);
                    //    p_ops->erase(it3);                        
                    //}
                    rc=0;
                    break;
                }
            }
        }        
        pthread_mutex_unlock(&ops_mutex);
        delete op;
        log->debug_log("rc=%u",rc);
        return rc;
    }
    
    int cleanup(struct operation_client_read *op)
    {
        int rc=-1;
        log->debug_log("op inum:%llu",op->ophead.inum);
        
        std::map<uint32_t,struct operation_group *>::iterator it = op->groups->begin();
        for (it; it!=op->groups->end(); it++)
        {
            std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->sumap->begin();
            for (it2; it2!=it->second->sumap->end(); it2++)
            {
                //free(it2->second->newdata);
                //free(it2->second->paritydata);
                //free(it2->second->olddata);
               // free(it2->second);
            }
            //free(it->second);
        }
        delete op->groups;
        log->debug_log("groups deleted.");
        rc=0;
        
        pthread_mutex_lock(&readops_mutex);
        std::map<uint64_t, struct operation_client_read*>::iterator it3 = p_readops->find(ccoid_to_uint64(op->ophead.cco_id));
        if (it3!=p_readops->end())
        {
            p_readops->erase(it3);          
        }        
        pthread_mutex_unlock(&readops_mutex);
        delete op;
        log->debug_log("rc=%u",rc);
        return rc;
    }
    
    int cleanup(struct operation_composite *p_op)
    {
        int rc =0;
        struct OPHead *p_head;        
        pthread_mutex_lock(&writeops_mutex);
        uint64_t ccocomb = ccoid_to_uint64(p_op->ophead.cco_id);
        std::map<uint64_t, struct operation_composite*>::iterator it2 = p_writeops->find(ccocomb);
        p_writeops->erase(it2)      ;
        
        /*std::map<uint32_t,operation*>::iterator it = p_op->ops->begin();
        for (it;it!=p_op->ops->end(); it++)
        {
            p_head = (struct OPHead*) it->second;
            
            switch (p_head->type)
            {
                case operation_client_su_write:
                {
                    log->debug_log("inplement su write cleanup");
                    //cleanup((struct operation_client_write *)p_head);
                    
                    break;
                }
                case operation_client_s_write:
                {
                    log->debug_log("inplement s write cleanup");
                    break;
                }
                case operation_client_read:
                {
                    log->debug_log("inplement read cleanup");
                    break;
                }
            }
        } **/       
        pthread_mutex_unlock(&writeops_mutex);
        return rc;
    }
    
    void cleanup(struct operation_primcoordinator *op)
    {
        pthread_mutex_lock(&ops_mutex);
        struct OPHead *p_existing;
        std::map<uint64_t, std::vector<struct OPHead*>* >::iterator it = this->p_ops->find(op->ophead.offset);
        if (it!=this->p_ops->end())
        {            
            std::vector<struct OPHead*>::iterator it2 = it->second->begin();
            while (it2 != it->second->end())
            {                
                p_existing = *it2;
                if (p_existing->cco_id.csid==op->ophead.cco_id.csid)
                {
                    free_operation_primcoordinator(op);
                    it->second->erase(it2);
                    break;
                }
            }
        }
        pthread_mutex_unlock(&ops_mutex);
    }
    
    void cleanup(struct operation_participant *op)
    {
        pthread_mutex_lock(&partops_mutex);
        std::map<uint64_t, struct operation_participant*>::iterator it = p_partops->find(ccoid_to_uint64(op->ophead.cco_id));
        if (it!= p_partops->end())
        {
            free_operation_participant(op);
            
            p_partops->erase(it);
        }
        pthread_mutex_unlock(&partops_mutex);
    }
    
    int handle_client_read_resp(StripeId sid, StripeUnitId suid, struct data_object *p_do, struct CCO_id ccoid)
    {
        log->debug_log("sid:%u,suid:%u",sid,suid);
        int rc=-1;
        struct operation_client_read *p_op;
        
        rc = get_op(ccoid, &p_op);
        if (!rc)
        {
            log->debug_log("got operation");
            
                stringstream ss;
                print_dataobject_metadata(&p_do->metadata, ss);
                //it2->second->newdata =
                log->debug_log("%s",ss.str().c_str());
            std::map<uint32_t,struct operation_group *>::iterator it = p_op->groups->find(sid);
            if (it!=p_op->groups->end())
            {
                std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->sumap->find(suid);
                if (it2!=it->second->sumap->end())
                {
                    log->debug_log("found stripe unit %u",suid);
                    it2->second->newdata=(char *)p_do->data;
                    it2->second->do_recv = p_do;
                    it2->second->start = p_do->metadata.offset;
                    it2->second->end   = p_do->metadata.offset+p_do->metadata.datalength;
                    rc = 0;
                }
                else
                {
                    log->debug_log("could not find stripe unit %u",suid);
                    rc=-4;
                }
            }
            else
            {
                log->debug_log("no sid:%u found.",sid );
                rc=-3;
            }
        }
        if (rc==0)
        {
            if (check_isready(p_op) )
            {
                if (check_consistency(p_op))
                {
                    log->debug_log("Operation csid:%u, seq:%u is ready",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum);
                    rc = merge_data_objects(p_op);
                    if (rc==0)
                    {
                        log->debug_log("operation complete.");
                        p_op->ophead.status = opstatus_success;
                    }
                    else
                    {
                        p_op->ophead.status = opstatus_failure;
                        log->debug_log("operation failed.");
                    }
                }
                else
                {
                    log->debug_log("inconsistent stripe detected");
                    p_op->ophead.status = opstatus_failure;
                }
            }
            else
            {
                log->debug_log("not ready yet.");
            }
        }
        log->debug_log("rc:%d",rc);
        return rc;
    }

    int insert_paricipant_operation(struct dstask_spn_recv *p_task_sp)
    {
        int rc=-1;
        pthread_mutex_lock(&partops_mutex);
        log->debug_log("inserting:csid:%u,seq:%u",p_task_sp->dshead.ophead.cco_id.csid,p_task_sp->dshead.ophead.cco_id.sequencenum);
        if (p_task_sp->dshead.ophead.subtype==received_spn_write_su || 
            p_task_sp->dshead.ophead.subtype==received_spn_write_s)
        {
            struct operation_participant *p_op=NULL;
            rc = get_op(p_task_sp->dshead.ophead.cco_id, &p_op);
            if (rc)
            {
                log->debug_log("Operation for inum:%llu not existing yet",p_task_sp->dshead.ophead.inum);
                filelayout_raid *p_fl = (filelayout_raid *) &p_task_sp->dshead.ophead.filelayout[0];
                bool iscoord = p_raid->is_coordinator(p_fl, p_task_sp->dshead.ophead.offset, this->id);
                
                //log->debug_log("text:%s",(char*) p_task_sp->data);
                p_op = new struct operation_participant;
                p_op->stripecnt = 0;
                p_op->stripecnt_recv = 0;
                p_op->ophead=p_task_sp->dshead.ophead;
                p_op->ophead.status = opstatus_init;
                p_op->ophead.type = operation_participant_type;
                p_op->ophead.subtype = p_task_sp->dshead.ophead.subtype;
                log->debug_log("subtype:%u.",p_op->ophead.subtype);
                !iscoord ? p_op->iamsecco=false : p_op->iamsecco = true;
                p_op->datamap = new std::map<StripeId,struct dataobject_collection*>;
                
                FileStripeList *p_fsl = p_raid->get_stripes(p_fl,p_task_sp->dshead.ophead.offset,p_task_sp->dshead.ophead.length);
                std::map<StripeId,struct Stripe_layout*>::iterator it = p_fsl->stripemap->begin();
                for (it; it!=p_fsl->stripemap->end(); it++)
                {
                    struct dataobject_collection *p_doc = new struct dataobject_collection;
                    p_doc->fhvalid=false;
                    //if (p_task_sp->dshead.ophead.subtype==received_spn_write_s)
                    //{
                    StripeUnitId suid = p_raid->get_my_stripeunitid(p_fl,p_task_sp->dshead.ophead.offset,this->id);
                    rc = p_docache->get_entry(p_op->ophead.inum, it->first,&p_doc->existing);
                    
                    if (!rc)
                    {
                        p_doc->mycurrentversion = p_doc->existing->metadata.versionvector[suid];
                    }
                    else
                    {
                        p_doc->mycurrentversion = 0;
                        p_doc->existing=NULL;
                    }
                    log->debug_log("found existing _VERSION_:%u",p_doc->mycurrentversion);
                    log->debug_log("reading existing returned:%d",rc);
                    //}
                    //else
                    //{
                     //   p_doc->existing = NULL;
                    //}
                    p_doc->recv_data = NULL;
                    p_doc->parity_data=NULL;
                    p_op->datamap->insert(std::pair<StripeId,struct dataobject_collection*>(it->first,p_doc));
                    p_op->stripecnt++;
                    log->debug_log("inserted stripe id:%u",it->first);
                }
                p_op->participants = p_task_sp->participants;
                p_op->ophead.status = opstatus_init;
                p_op->primcoordinator = get_coordinator(p_fl,p_task_sp->dshead.ophead.offset);   
                rc = insert(p_op);
                log->debug_log("inserted:rc=%d",rc);
                free_FileStripeList(p_fsl);
            }
            
            std::map<StripeId,struct dataobject_collection*>::iterator it = p_op->datamap->find(p_task_sp->stripeid);
            if (it!=p_op->datamap->end())
            {                
                it->second->recv_data = p_task_sp->data;
                it->second->recv_length = p_task_sp->length;
                //log->debug_log("text:%s",(char*) it->second->recv_data);
                it->second->parity_data = NULL;
                p_op->stripecnt_recv++;
                rc=0;                
                log->debug_log("Inserted request data for stripe id :%u",it->first);
            }
            if (check_isready(p_op))
            {
                log->debug_log("check:subtype:%u.",p_op->ophead.subtype);
                rc = participant_handle_ready((struct OPHead *) p_op);
                log->debug_log("ready: rc=%d",rc);
            }
            else
            {
                log->debug_log("not ready yet");
            }   
        }
        else
        {
            log->debug_log("unknown operation. %u",p_task_sp->dshead.ophead.subtype);
        }   
        pthread_mutex_unlock(&partops_mutex);
        log->debug_log("rc:%d",rc);
        return rc;
    }
    
    
    /**
     * @Todo memleak check
     * @param p_task
     * @return 
     */
    int add_spn_datablock(struct dstask_spn_recv *p_task)
    {
        int rc=-1;
        bool locked=true;
        pthread_mutex_lock(&ops_mutex);
        log->debug_log("start");
        if (p_task->dshead.ophead.subtype==received_spn_write_s)
        {            
            filelayout_raid *p_fl = (filelayout_raid *) &p_task->dshead.ophead.filelayout[0];
            struct operation_primcoordinator *p_primop;
            struct data_object *p_do = new struct data_object;
            p_do->data = p_task->data;
            p_do->metadata.ccoid = p_task->dshead.ophead.cco_id;
            p_do->metadata.operationlength  = p_task->dshead.ophead.length;
            memcpy(&p_do->metadata.filelayout[0], &p_task->dshead.ophead.filelayout[0], sizeof(p_do->metadata.filelayout));
            p_do->metadata.offset = p_task->dshead.ophead.offset;
            p_do->metadata.datalength = p_task->length;
            log->debug_log("operations length:%u, datalength:%u",p_do->metadata.operationlength,p_do->metadata.datalength);

            //for (int i=0; i<p_fl->raid4.groupsize-1; i++)
            //{
                //p_docache->get_entry(p_task->dshead.ophead.inum, p_task->stripeid,&p_do);
                //log->debug_log("VERSION:%u",p_do->metadata.versionvector[p_fl->raid4.groupsize-1]);
            //    p_do->metadata.versionvector[i]=1;
            //}
            rc = get_op(p_task->dshead.ophead.cco_id, p_task->dshead.ophead.offset, &p_primop);
            if (rc)
            {
                p_primop = new struct operation_primcoordinator;
                memcpy(&p_primop->ophead, &p_task->dshead.ophead, sizeof(struct OPHead));
                memset(&p_primop->received_from, 0, sizeof(struct Participants_bf));
                memset(&p_primop->participants, 0, sizeof(struct Participants_bf));
                p_primop->mutex_private = PTHREAD_MUTEX_INITIALIZER;
                p_primop->received_from.start = 3;        
                p_primop->ophead.status=opstatus_init;
                log->debug_log("stripe id is:%u",p_task->stripeid);
                memset(&p_do->metadata.versionvector[0],0,sizeof(p_do->metadata.versionvector));
               // p_docache->get_next_version_vector(p_task->dshead.ophead.inum, p_task->stripeid, &p_do->metadata.versionvector[0], p_fl->raid4.groupsize-1);
                
                log->debug_log("STATUS:csid:%u,seq:%u,status:%u",p_primop->ophead.cco_id.csid,p_primop->ophead.cco_id.sequencenum, p_primop->ophead.status);
                fullParticipants(&p_primop->participants, p_fl->raid4.groupsize-1) ;            
                log->debug_log("Groupsize:%u",p_fl->raid4.groupsize-1);

                //StripeId sid = p_raid->getStripeId(p_fl, p_task->dshead.ophead.offset);

                stringstream ss;
                print_struct_participant(&p_primop->participants, ss);
                log->debug_log("participants:%s.\n",ss.str().c_str());        

                p_primop->recv_timestamp = time(0);
                p_primop->datalength = p_task->length;
                p_primop->datamap_primco = new std::map<StripeId,struct datacollection_primco*>;
                struct datacollection_primco *p_dc = new struct datacollection_primco;
                memcpy(&p_dc->versionvec[0],&p_do->metadata.versionvector[0], sizeof(p_dc->versionvec));                
                log->debug_log("VERSION:%u, size:%u",p_do->metadata.versionvector[p_fl->raid4.groupsize-1],sizeof(p_dc->versionvec));
                p_dc->fhvalid=false;
                p_dc->existing=NULL;
                p_dc->finalparity=NULL;
                p_dc->paritymap=NULL;// new std::map<uint32_t,struct paritydata*>;
                p_dc->newobject=p_do;
                p_primop->datamap_primco->insert(std::pair<StripeId,struct datacollection_primco*>(p_task->stripeid,p_dc));
                log->debug_log("created primco datamap:%p",p_primop->datamap_primco);
               // pthread_mutex_lock(&globallock_mutex);
                
                if (rc==-1)
                {
                    std::vector<struct OPHead*> *opvec;
                    std::map<uint64_t,std::vector<struct OPHead*>*>::iterator it_v = this->p_ops->find(p_task->dshead.ophead.offset);
                    if (it_v!=this->p_ops->end())
                    {
                        opvec = it_v->second;
                        opvec->push_back(&p_primop->ophead);
                    }
                    else
                    {
                        opvec = new std::vector<struct OPHead*>;
                        opvec->push_back(&p_primop->ophead);
                        this->p_ops->insert(std::pair<uint64_t,std::vector<struct OPHead*>*>(p_task->dshead.ophead.offset,opvec));                    
                    }
                    log->debug_log("pushed back, created vec offset:%llu",p_task->dshead.ophead.offset);
                }
                else
                {
                    std::map<uint64_t, std::vector<struct OPHead*>* >::iterator it = this->p_ops->find(p_task->dshead.ophead.offset);
                    it->second->push_back(&p_primop->ophead);
                    log->debug_log("pushed back. no lower bound found.");
                }                   
                rc = 0;            
                //pthread_mutex_unlock(&globallock_mutex);
            }
            else
            {
                pthread_mutex_lock(&p_primop->mutex_private);
                pthread_mutex_unlock(&ops_mutex);
                locked=false;
                 
                //p_docache->get_next_version_vector(p_task->dshead.ophead.inum, p_task->stripeid, &p_do->metadata.versionvector[0], p_fl->raid4.groupsize-1);
               
                log->debug_log("unknown operation existing");
                //p_primop->datamap_primco = new std::map<StripeId,struct datacollection_primco*>;
                StripeId sid = p_raid->getStripeId(p_fl, p_task->dshead.ophead.offset);
                std::map<StripeId,struct datacollection_primco*>::iterator it = p_primop->datamap_primco->find(sid);
                if (it!=p_primop->datamap_primco->end())
                {
                    it->second->newobject = p_do;
                }
/*                memcpy(p_dc->versionvec,p_do->metadata.versionvector, sizeof(p_dc->versionvec));                
                p_dc->existing=NULL;
                p_dc->finalparity=NULL;
                p_dc->fhvalid=false;
                p_dc->paritymap=NULL;// new std::map<uint32_t,struct paritydata*>;
                p_dc->newobject=p_do;
                //p_primop->datamap_primco->insert(std::pair<StripeId,struct datacollection_primco*>(p_task->stripeid,p_dc));
 **/
                pthread_mutex_unlock(&p_primop->mutex_private);
                log->debug_log("inserted primco datamap:%p",p_primop->datamap_primco);
            }
                                            
            if(bitfield_equal(&p_primop->participants, &p_primop->received_from) )
            {
                pthread_mutex_lock(&p_primop->mutex_private);
                pthread_mutex_unlock(&ops_mutex);
                locked=false;
                log->debug_log("Operation is ready.");
                rc = handle_primcoord_stripewrite_phase1(p_primop);
                pthread_mutex_unlock(&p_primop->mutex_private);
            }
        }
        else
        {
            log->debug_log("error");
        }
        if (locked) pthread_mutex_unlock(&ops_mutex);
        log->debug_log("rc:%d",rc);
        return rc;
    }
    
    void timeout_check()
    {
        log->debug_log("Timeoutcheck...");
        std::map<uint64_t, std::vector<struct OPHead*>* >::iterator it = this->p_ops->begin();
        std::vector<struct OPHead*>::iterator vecit;
        pthread_mutex_lock(&ops_mutex);
        for (it; it!=this->p_ops->end(); it++)
        {
            vecit = it->second->begin();
            for (vecit; vecit!=it->second->end(); vecit++)
            {
                time_t now = time(0);
                struct operation_primcoordinator *p_op = (struct operation_primcoordinator *)*vecit;
                
                if (OPERATION_TIMEOUT_LEVEL_A <= difftime(now,p_op->recv_timestamp))
                {
                    log->debug_log("TIMEOUT! inum:%llu, csid:%u, seq:%u",p_op->ophead.inum, p_op->ophead.cco_id.csid, p_op->ophead.cco_id.sequencenum);
                }                
                else
                {
                    char *c = ctime(&p_op->recv_timestamp);
                    log->debug_log("operation:%s",c);
                }
            }
        }
        pthread_mutex_unlock(&ops_mutex);
    }
    
    
    int handle_directwrite(struct dstask_spn_recv *p_task)
    {                 
        filelayout_raid *p_fl = (filelayout_raid *) &p_task->dshead.ophead.filelayout[0];
            
        struct operation_participant *p_direct = new struct operation_participant;
        p_direct->ophead = p_task->dshead.ophead;
        p_direct->datamap = new std::map<StripeId,struct dataobject_collection*>();
        StripeId sid = p_raid->getStripeId(p_fl,p_task->dshead.ophead.offset);
        struct dataobject_collection *p_col  = new struct dataobject_collection;
        p_col->mycurrentversion = 1;
        p_col->recv_data = p_task->data;
        p_col->recv_length = p_task->length;
        for (int i=0; i<p_fl->raid4.groupsize; i++)
        {
            p_col->versionvector[i]=2;
        }
        p_direct->datamap->insert(std::pair<StripeId,struct dataobject_collection*>(sid,p_col));
        p_fileio->write_file(p_direct);
        std::map<StripeId,struct dataobject_collection*>::iterator it = p_direct->datamap->find(sid);
        
        struct data_object *p_obj = it->second->newblock;
        p_docache->set_entry(p_task->dshead.ophead.inum, sid, p_obj);
        fsync(p_col->filehandle);
        close(p_col->filehandle);
        //free(p_col->recv_data);
        delete p_col;
        delete p_direct->datamap;
        delete p_direct;
        return 0;
    }


    
private:
    Logger *log;
    Libraid4 *p_raid;
    serverid_t id;
    int (*p_queuePush)(queue_priorities, OPHead*);
    uint32_t (*p_cb_getSeq)();
    std::map<uint64_t, std::vector<struct OPHead*>* > *p_ops;
    std::map<uint64_t, struct operation_client_read*> *p_readops;
    std::map<uint64_t, struct operation_composite*> *p_writeops;
    std::map<uint64_t, struct operation_participant*> *p_partops;
    //std::map<StripeId, >*p_switchmap;
    pthread_mutex_t globallock_mutex;
    pthread_mutex_t ops_mutex;
    pthread_mutex_t partops_mutex;
    pthread_mutex_t writeops_mutex;
    pthread_mutex_t readops_mutex;
    pthread_mutex_t switchmap_mutex;
    doCache *p_docache;
    Filestorage *p_fileio;
    bool operation_active;
        
    
    int participant_handle_ready(struct OPHead *p_op)
    {
        int rc=-1;
        log->debug_log("inum:%llu",p_op->inum);
        if (p_op->subtype==received_spn_write_su)
        {
            rc = handle_ready_phase1(p_op);
        }
        else if (p_op->subtype==received_spn_write_s)
        {
            rc = handle_stripewrite_ready_phase1((struct operation_participant*)p_op);
        }
        log->debug_log("rc:%d",rc);
        return rc;
    }
    
    bool conflict_check(uint64_t existing_start, uint64_t existing_end, struct OPHead *p_new)
    {
        if (p_new->offset > existing_end  || p_new->offset+p_new->length < existing_start)
        {
            return false;
        }
        return true;
    }    
    
    int handle_primcoord_stripewrite_phase1(struct operation_primcoordinator *p_op)
    {
        int rc=-1;
        log->debug_log("Primcoord phase 1 done:offset:%llu,length:%llu",p_op->ophead.offset,p_op->ophead.length);
        p_op->ophead.status = opstatus_docommit;
        log->debug_log("STATUS:csid:%u,seq:%u,status:%u",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum, p_op->ophead.status);
        /*struct dstask_write_fileobject *p_dstask = new struct dstask_write_fileobject;
        memcpy(&p_dstask->dshead.ophead,&p_op->ophead,sizeof(struct OPHead));
        p_dstask->dshead.ophead.type=ds_task_type;
        p_dstask->dshead.ophead.subtype = dstask_write_fo;        
        **/
                       
        std::map<StripeId,struct datacollection_primco*>::iterator it = p_op->datamap_primco->begin();
        filelayout_raid *p_fl = (filelayout_raid*) &p_op->ophead.filelayout[0];       
        for (it; it!=p_op->datamap_primco->end(); it++)
        {
           // p_dstask->data_col = it->second;
            log->debug_log("stripe id is:%u",it->first);
            
            p_docache->get_next_version_vector(p_op->ophead.inum, it->first, &it->second->versionvec[0], p_fl->raid4.groupsize-1);
                //p_docache->get_entry(p_task->dshead.ophead.inum, p_task->stripeid,&p_do);
            for (int i=0; i<=default_groupsize; i++)
            {
                log->debug_log("version:%u:%u",i,it->second->versionvec[i]);        
            }
            log->debug_log("stripe id %u",it->first);
            if (it->second->newobject!=NULL)
            {   
                memcpy(&it->second->newobject->metadata.versionvector[0], &it->second->versionvec[0], sizeof(it->second->newobject->metadata.versionvector));
                log->debug_log("Operation ready.");
                struct Stripe_layout *p_sl = p_raid->get_stripe(p_fl,p_op->ophead.offset);
                std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = p_sl->blockmap->begin();
                for (it2; it2!=p_sl->blockmap->end(); it2++)
                {
                    struct operation_dstask_docommit *p_task = create_dstask_send_docommit((operation*)p_op);
                    p_task->receiver = it2->second->assignedto;
                    p_task->vvmap.stripeid=it->first;

                    memcpy(&p_task->vvmap.versionvector[0],&it->second->versionvec[0], sizeof(p_task->vvmap.versionvector));
                    //p_task->vvmap.versionvector[default_groupsize] = it->second->versionvec[default_groupsize];
                    log->debug_log("task pointer:%p",p_task);
                    rc = p_queuePush(prim_recv, (struct OPHead*)p_task);
                    log->debug_log("pushed. for receiver:%u",p_task->receiver);
                    //rc = p_docache->get_next_version_vector(p_op->ophead.inum,it->first,it_vec->second->versionvec,default_groupsize);
                }
                free_StripeLayout(p_sl);    
                //p_docache->reset_version_vector(p_op->ophead.inum ,it->first, default_groupsize);
            }
            else
            {
                rc=-1;
                break;
            }
        }    
        if(rc==0)
        {
            //memcpy(&p_dstask->data_col->newobject->metadata.versionvector[0],&p_dstask->data_col->versionvec[0], sizeof(p_dstask->data_col->newobject->metadata.versionvector));
            //p_queuePush(realtimetask, (struct OPHead*)p_dstask);        
            //log->debug_log("do commit messages queued.");
            rc = write_datamap_do_disk(p_op);
            memset(&p_op->received_from,0,sizeof(p_op->received_from));
        }
//        else
//        {
            //delete p_dstask;
  //      }
        log->debug_log("rc:%d",rc);
        return rc;
    }
    
    int handle_primcoord_phase_1(struct operation_primcoordinator *p_op)
    {
        int rc=-1;
        p_op->ophead.status = opstatus_prepare;
        log->debug_log("STATUS:csid:%u,seq:%u,status:%u",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum, p_op->ophead.status);
        //stringstream ss;
        //print_op_primcoordinator(p_op, ss);
        //log->debug_log("%s.\n",ss.str().c_str());
        
        filelayout_raid *p_fl = (filelayout_raid*) &p_op->ophead.filelayout[0];
        struct FileStripeList *p_fs = p_raid->get_stripes(p_fl,p_op->ophead.offset, p_op->ophead.length);

        std::map<StripeId,struct Stripe_layout*>::iterator it = p_fs->stripemap->begin();
        for (it; it!=p_fs->stripemap->end();it++)
        {
            //p_docache->;
            log->debug_log("FilestripeList: stripeID:%llu",it->first);
            p_raid->printStripeLayout(it->second);
            std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->blockmap->begin();
            for (it2; it2!=it->second->blockmap->end();it2++)
            {
                struct dstask_ccc_send_prepare *p_task = new struct dstask_ccc_send_prepare;
                memcpy(&p_task->dshead.ophead, &p_op->ophead , sizeof(struct OPHead));
                p_task->dshead.ophead.subtype = send_prepare;
                p_task->dshead.ophead.type = ds_task_type;
                p_task->receiver=it2->second->assignedto;
                rc = p_queuePush(prim_recv, (struct OPHead*)p_task);
                log->debug_log("pushed will be sent to %u. task pointer:%p",p_task->receiver,p_task);
            }
        }
        memset(&p_op->received_from,0,sizeof(p_op->received_from));
        free_FileStripeList(p_fs);
        log->debug_log("rc:%d",rc);
        return rc;
    }
    
    int handle_primcoord_phase_2(struct operation_primcoordinator *p_op)
    {
        int rc=-1;
        //std::vector<struct operation_dstask_docommit*> msgvec
        log->debug_log("start phase 2");
        p_op->ophead.status = opstatus_docommit;
        log->debug_log("STATUS:csid:%u,seq:%u,status:%u",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum, p_op->ophead.status);
        /*
        struct dstask_writetodisk *p_dstask = new struct dstask_writetodisk;
        p_dstask->dshead.ophead=p_op->ophead;
        p_dstask->dshead.ophead.type=ds_task_type;
        p_dstask->dshead.ophead.subtype = prim_docommit;
        p_dstask->p_op = p_op;
        p_queuePush(realtimetask, (struct OPHead*)p_dstask);        */
        memset(&p_op->received_from,0,sizeof(p_op->received_from));

        filelayout_raid *p_fl = (filelayout_raid*) &p_op->ophead.filelayout[0];
        struct FileStripeList *p_fs = p_raid->get_stripes(p_fl,p_op->ophead.offset, p_op->ophead.length);
        std::map<StripeId,struct Stripe_layout*>::iterator it = p_fs->stripemap->begin();
        for (it; it!=p_fs->stripemap->end();it++)
        {
            std::map<StripeId,struct datacollection_primco*>::iterator it_vec = p_op->datamap_primco->find(it->first);
            //if (it_vec->second->existing==NULL)
           // {
           //     log->debug_log("reading block:sid %u",it->first);

            //}
            //rc = p_docache->get_next_version(p_op->ophead.inum,it->first,&version);
            //log->debug_log("get next version returned: %u, rc:%d",version,rc);

            rc = p_docache->get_next_version_vector(p_op->ophead.inum,it->first,it_vec->second->versionvec,default_groupsize);
            log->debug_log("rc:%d. got current version vector",rc);
            //it_vec->second->versionvec[default_groupsize] = version;
            log->debug_log("parity version:%u",it_vec->second->versionvec[default_groupsize]);        
            log->debug_log("FilestripeList: stripeID:%llu",it->first);
            //p_raid->printStripeLayout(it->second);
            std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->blockmap->begin();
            for (it2; it2!=it->second->blockmap->end();it2++)
            {
                struct operation_dstask_docommit *p_task = create_dstask_send_docommit((operation*)p_op);
                p_task->receiver = it2->second->assignedto;
                p_task->vvmap.stripeid=it->first;
                memcpy(&p_task->vvmap.versionvector[0],&it_vec->second->versionvec[0], sizeof(p_task->vvmap.versionvector));
                //p_task->vvmap.versionvector[default_groupsize] = it_vec->second->versionvec[default_groupsize];

                //log->debug_log("v0:%u, v1:%u, v2:%u",p_task->vvmap.versionvector[0],p_task->vvmap.versionvector[1],p_task->vvmap.versionvector[2]);
                //log->debug_log("task pointer:%p",p_task);
                rc = p_queuePush(realtimetask, (struct OPHead*)p_task);
                log->debug_log("pushed. for receiver:%u",p_task->receiver);
            }
        }
        free_FileStripeList(p_fs);
        log->debug_log("do commit messages queued."); 

        //log->debug_log("inum:%llu",p_.ophead.inum);
        rc = calculate_parity_block(p_op);
        if (rc)
        {
            log->debug_log("error occured while creating parity block.");
        }
        else
        {
            rc = write_datamap_do_disk(p_op);
            /*
            std::map<StripeId,struct datacollection_primco*>::iterator its = p_op->datamap_primco->begin();
            for (its; its!=p_op->datamap_primco->end(); its++)
            {
                log->debug_log("Creating new object structure...");
                its->second->newobject = new struct data_object;
                its->second->newobject->data = its->second->finalparity;
                memcpy(&its->second->newobject->metadata.ccoid, &p_op->ophead.cco_id, sizeof(struct CCO_id));
                its->second->newobject->metadata.datalength = p_op->datalength;
                its->second->newobject->metadata.offset  = p_op->ophead.offset;

                memcpy(&its->second->newobject->metadata.filelayout[0], &p_op->ophead.filelayout[0], sizeof(p_op->ophead.filelayout));
                        its->second->newobject->metadata.operationlength = p_op->ophead.length;
                memcpy(&its->second->newobject->metadata.versionvector[0], &(its->second->versionvec[0]), sizeof(its->second->versionvec));

                //filelayout_raid *p_fl = ( filelayout_raid*) &p_op->ophead.filelayout[0];
                //StripeId sid = p_raid->getStripeId(p_fl,p_op->ophead.offset);

                /*rc = p_docache->get_unconfirmed(p_op->ophead.inum,sid,&its->second->existing);
                if (rc)
                {
                    log->debug_log("request for existing  rc:%d",rc);
                    its->second->newobject->metadata.versionvector[default_groupsize]=1;
                }
                else
                {
                    its->second->newobject->metadata.versionvector[default_groupsize]=its->second->existing->metadata.versionvector[default_groupsize]+1;
                }**
                stringstream ss;
                print_dataobject_metadata(&its->second->newobject->metadata,ss);
                log->debug_log("%s",ss.str().c_str());
                log->debug_log("write to disk");
                rc = p_fileio->write_file(&p_op->ophead, its->second); 
                log->debug_log("Object written to disk:rc=%d",rc);
                if (!rc)
                {
                    rc = p_docache->parityUnconfirmed(p_op->ophead.inum, its->first, its->second->newobject);
                    log->debug_log("Cache updated...rc=%d",rc);
                }
                else
                {
                    log->warning_log("Writing failed... rc=%d",rc);
                }
            }**/
        }
        log->debug_log("rc:%d",rc);
        return rc;
    }  
    
    int write_datamap_do_disk(struct operation_primcoordinator *p_op)
    {
        int rc=0;
        std::map<StripeId,struct datacollection_primco*>::iterator its = p_op->datamap_primco->begin();
        for (its; its!=p_op->datamap_primco->end(); its++)
        {
            log->debug_log("Creating new object structure...");
            if (its->second->newobject==NULL)
            {
                its->second->newobject = new struct data_object;
                its->second->newobject->data = its->second->finalparity;
                memcpy(&its->second->newobject->metadata.ccoid, &p_op->ophead.cco_id, sizeof(struct CCO_id));
                its->second->newobject->metadata.datalength = p_op->datalength;
                its->second->newobject->metadata.offset  = p_op->ophead.offset;

                memcpy(&its->second->newobject->metadata.filelayout[0], &p_op->ophead.filelayout[0], sizeof(p_op->ophead.filelayout));
                its->second->newobject->metadata.operationlength = p_op->ophead.length;
                log->debug_log("created newobject.");
             }
             memcpy(&its->second->newobject->metadata.versionvector[0], &its->second->versionvec[0], sizeof(its->second->newobject->metadata.versionvector));
        
            //filelayout_raid *p_fl = ( filelayout_raid*) &p_op->ophead.filelayout[0];
            //StripeId sid = p_raid->getStripeId(p_fl,p_op->ophead.offset);

            /*rc = p_docache->get_unconfirmed(p_op->ophead.inum,sid,&its->second->existing);
            if (rc)
            {
                log->debug_log("request for existing  rc:%d",rc);
                its->second->newobject->metadata.versionvector[default_groupsize]=1;
            }
            else
            {
                its->second->newobject->metadata.versionvector[default_groupsize]=its->second->existing->metadata.versionvector[default_groupsize]+1;
            }**/
            stringstream ss;
            print_dataobject_metadata(&its->second->newobject->metadata,ss);
            log->debug_log("%s",ss.str().c_str());
            log->debug_log("write to disk");
            rc = p_fileio->write_file(&p_op->ophead, its->second); 
            log->debug_log("Object written to disk:rc=%d",rc);
            if (!rc)
            {
                rc = p_docache->parityUnconfirmed(p_op->ophead.inum, its->first, its->second->newobject);
                log->debug_log("Cache updated...rc=%d",rc);
            }
            else
            {
                log->warning_log("Writing failed... rc=%d",rc);
            }
        }
        log->debug_log("rc:%d",rc);
        return rc;
    }
    
    int handle_primcoord_phase_3(struct operation_primcoordinator *p_op)
    {
        int rc=-1;
        log->debug_log("STATUS:csid:%u,seq:%u,status:%u",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum, p_op->ophead.status);
        p_op->ophead.status = opstatus_success;
        
        std::map<StripeId,struct datacollection_primco*>::iterator it2 = p_op->datamap_primco->begin();
        for(it2; it2!= p_op->datamap_primco->end(); it2++)
        {
            while (!it2->second->fhvalid) 
            {
                usleep(100); 
                //busy wait for the file to be written
                // if fvalid is false, processing the phase_3 at the participant side
                // was faster than finishing phase_2 at the coordinator
            }
            try
            {
                if(!p_fileio->no_sync)
                {
                    log->debug_log("flushing file buffer");
                    fsync(it2->second->filehandle);
                    log->debug_log("flushed.");
                }
                log->debug_log("Closing...");
                close(it2->second->filehandle);
                log->debug_log("Closed");                
            }
            catch (...)
            {
                log->error_log("ERROR:flush or close failed...");
            }
        }        
        log->debug_log("STATUS:csid:%u,seq:%u,status:%u",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum, p_op->ophead.status);
        filelayout_raid *p_fl = (filelayout_raid*) &p_op->ophead.filelayout[0];
        struct FileStripeList *p_fs = p_raid->get_stripes(p_fl,p_op->ophead.offset, p_op->ophead.length);
        
        std::map<StripeId,struct Stripe_layout*>::iterator it = p_fs->stripemap->begin();
        for (it; it!=p_fs->stripemap->end();it++)
        {
            log->debug_log("FilestripeList: stripeID:%llu",it->first);
            std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->blockmap->begin();
            for (it2; it2!=it->second->blockmap->end();it2++)
            {
                struct dstask_ccc_send_result *p_task = new struct dstask_ccc_send_result;
                p_task->receiver=it2->second->assignedto;
                memcpy(&p_task->dshead.ophead, &p_op->ophead , sizeof(struct OPHead));
                p_task->dshead.ophead.subtype = send_result;   
                p_task->dshead.ophead.type = ds_task_type;                
                rc = p_queuePush(realtimetask, (struct OPHead*)p_task);
                log->debug_log("pushed for receiver:%u. task pointer:%p",p_task->receiver, p_task);
                //p_task=NULL;
                log->debug_log("task pointer:%p", p_task);
            }
        }
        //clean up...
        it2 = p_op->datamap_primco->begin();
        //for(it2; it2!= p_op->datamap_primco->end(); it2++)
        while (it2!=p_op->datamap_primco->end())
        {
            log->debug_log("parity confirm start sid:%u",it2->first);
            rc = p_docache->parityConfirm(p_op->ophead.inum, it2->first, it2->second->versionvec[default_groupsize] );
            log->debug_log("parity confirm rc:%d, sid:%u",rc,it2->first);
            if (it2->second->paritymap!=NULL)
            {
                log->debug_log("clean up parity map");
                std::map<uint32_t,struct paritydata*>::iterator itpmap = it2->second->paritymap->begin();
                while (itpmap!=it2->second->paritymap->end())
                {
                    log->debug_log("deleting mapentry");
                    if (itpmap->second->data!=NULL)free(itpmap->second->data);
                    it2->second->paritymap->erase(itpmap++);
                }
                log->debug_log("deleting map");
                delete it2->second->paritymap;
            }
            if (it2->second->existing!=NULL)
            {
                log->debug_log("deleting existing block");
                if (it2->second->existing->data!=NULL)free(it2->second->existing->data);
                delete it2->second->existing;
            }
            //if (it2->second->finalparity!=it2->second->newobject->data) free(it2->second->finalparity);
            it2->second->finalparity=NULL;
            it2->second->newobject=NULL;
            log->debug_log("delete datamap primco entry:%u",it2->first);
            p_op->datamap_primco->erase(it2++);
            log->debug_log("done.");            
        }
        log->debug_log("removeing data map");
        delete p_op->datamap_primco;
        log->debug_log("removed data map");
        
        pthread_mutex_lock(&ops_mutex);
        log->debug_log("got ops lock");
        bool found=false;
        std::map<uint64_t, std::vector<struct OPHead*>*>::iterator itops = this->p_ops->find(p_op->ophead.offset);
        if(itops!=this->p_ops->end())
        {
            log->debug_log("found entry");
            std::vector<struct OPHead*>::iterator itvec = itops->second->begin();
            for (itvec; itvec!= itops->second->end(); itvec++)
            {
                log->debug_log("check csid:%u,seq:%u",(*itvec)->cco_id.csid,(*itvec)->cco_id.sequencenum);
                if ((*itvec)->cco_id.csid==p_op->ophead.cco_id.csid && (*itvec)->cco_id.sequencenum==p_op->ophead.cco_id.sequencenum)
                {
                    itops->second->erase(itvec);
                    log->debug_log("removed operation from vector.");
                    found=true;
                    break;
                }
            }
        }
        if (!found)
        {
            log->debug_log("ERROR: entry not found");
        }
        pthread_mutex_unlock(&ops_mutex);
        //memset(&p_op->received_from,0,sizeof(p_op->received_from));        
        free_FileStripeList(p_fs);
        log->debug_log("rc:%d",rc);
        return rc;
    }    
      
    int handle_stripewrite_ready_phase1(struct operation_participant *p_op)
    {
        int rc=-1;
        p_op->ophead.status = opstatus_cancommit;
        log->debug_log("STATUS:csid:%u,seq:%u,status:%u",p_op->ophead.cco_id.csid,p_op->ophead.cco_id.sequencenum, p_op->ophead.status);
        std::map<StripeId,struct dataobject_collection*>::iterator it = p_op->datamap->begin();
        for (it; it!=p_op->datamap->end(); it++)
        {
            filelayout_raid *p_fl = (filelayout_raid *) &p_op->ophead.filelayout[0];            
            StripeUnitId suid = p_raid->get_my_stripeunitid(p_fl,p_op->ophead.offset,this->id);
            rc = p_docache->get_next_version_vector(p_op->ophead.inum, it->first, &it->second->versionvector[0], suid);
            
            struct dstask_ccc_stripewrite_cancommit *p_task = new struct dstask_ccc_stripewrite_cancommit;

            //p_task->version = ++it->second->mycurrentversion;
            p_task->version = it->second->versionvector[suid];
            log->debug_log("Next _VERSION_ will be:%u",p_task->version);
            p_task->receiver = p_op->primcoordinator;

            p_task->dshead.ophead = p_op->ophead;
            p_task->dshead.ophead.subtype = send_ccc_stripewrite_cancommit;
            p_task->dshead.ophead.type = ds_task_type;

            rc = p_queuePush(sec_recv,(struct OPHead*)p_task);
            log->debug_log("pushed op");
        }
        
        log->debug_log("done:%d",rc);
        return rc;
    }
    
    int handle_ready_phase1(OPHead *p_head)
    {
        int rc;
        log->debug_log("start");
        p_head->status = opstatus_init;
        log->debug_log("STATUS:csid:%u,seq:%u,status:%u",p_head->cco_id.csid,p_head->cco_id.sequencenum, p_head->status);
        if (p_head->type == operation_participant_type)
        {
            log->debug_log("participant");
            struct operation_participant *p_p = (struct operation_participant*) p_head;
            struct dstask_ccc_send_received *p_task = new struct dstask_ccc_send_received;
            p_task->dshead.ophead = *p_head;
            p_task->dshead.ophead.subtype = send_received;
            p_task->dshead.ophead.type = ds_task_type;
            p_task->participants = p_p->participants;
            rc = p_queuePush(sec_recv,(struct OPHead*)p_task);
            p_p->stripecnt_recv=0;
            log->debug_log("pushed op");
            
        }            
        else if (p_head->type == operation_client_type || 
                (p_head->type == operation_client_read))
        {
            log->debug_log("client operation type push phase 1");
            rc = p_queuePush(prim_recv, p_head);
        }
        else
        {
            log->debug_log("unknown type detected:%u.",p_head->type);
            rc=-1;
        }
        log->debug_log("rc:%d",rc);
        return rc;
    }
    
    bool check_isready(struct operation_client_read *p_op)
    {
        log->debug_log("check isready for client read inum %u",p_op->ophead.inum);
        std::map<uint32_t,struct operation_group *>::iterator it = p_op->groups->begin();
        for(it; it!=p_op->groups->end(); it++)
        {
            if (!it->second->isready)
            {                
                std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->sumap->begin();
                for (it2; it2!=it->second->sumap->end(); it2++)
                {
                    if (it2->second->newdata == NULL)
                    {
                        log->debug_log("sid:%u, suid:%u not ready yet",it->first, it2->first);
                        return false;
                    }
                }
                it->second->isready=true;
            }
            else
            {
                log->debug_log("sid:%u ready",it->first);
            }
        }
        p_op->isready=true;
        return true;
    }
    
    bool check_isready(struct operation_participant *p_op)
    {
        log->debug_log("check isready for participant inum %u, stripecnt:%u",p_op->ophead.inum,p_op->stripecnt);
        if (p_op->stripecnt!=p_op->stripecnt_recv) return false;
        
        std::map<StripeId,struct dataobject_collection*>::iterator it = p_op->datamap->begin();
        for (it; it!=p_op->datamap->end(); it++)
        {
            if (it->second->recv_data==NULL) return false;
        }        
        //p_op->isready=true;
        return true;
    }
    
    int merge_data_objects(struct operation_client_read *p_op)
    {
        int rc=0;
        log->debug_log("merge data %u",p_op->ophead.inum);
        std::map<uint32_t,struct operation_group *>::iterator it = p_op->groups->begin();
        char *tmp = (char*) *(p_op->resultdata);
        uint32_t tmp1;
        for(it; it!=p_op->groups->end(); it++)
        {
            std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->sumap->begin();
            for (it2; it2!=it->second->sumap->end(); it2++)
            {
                if (it2->second->newdata != NULL)
                {
                    tmp1 = it2->second->end-it2->second->start;
                    log->debug_log("su size:%u tp %p",tmp1,tmp);
                    memcpy(tmp,it2->second->newdata, tmp1);
                    log->debug_log("cp.");
                    tmp+=tmp1;
                }
                else
                {
                    log->debug_log("Error, invalid block");
                    rc=-1;
                    break;
                }
                free(it2->second->newdata);
            }
        }
        log->debug_log("rc=%u",rc);
        return rc;
    }
    
    bool check_consistency(struct operation_client_read *p_op)
    {
        filelayout_raid *p_fl = (filelayout_raid *)&p_op->ophead.filelayout[0];
        std::map<uint32_t,struct operation_group *>::iterator it = p_op->groups->begin();
        for (it;it!=p_op->groups->end(); it++)
        {
            log->debug_log("Check stripe id:%u",it->second->stripe_id);
            std::map<StripeUnitId,struct StripeUnit*>::iterator it2 = it->second->sumap->begin();
            for (it2; it2!=it->second->sumap->end(); it2++)
            {
                stringstream ss;
                print_dataobject_metadata(&it2->second->do_recv->metadata, ss);
                //it2->second->newdata =
                log->debug_log("stripe unit id:%u,\n%s",it2->first,ss.str().c_str());
                log->debug_log("groupsize:%u",p_fl->raid4.groupsize-1);
                int i=0;
                for (i;i<p_fl->raid4.groupsize-1;i++)
                {
                    uint32_t meta_vec = it2->second->do_recv->metadata.versionvector[i];
                    log->debug_log("version of id:%d is %u",i,meta_vec);
                    
                    std::map<StripeUnitId,struct StripeUnit*>::iterator it_tmp = it->second->sumap->find(i);
                    if (it_tmp!=it->second->sumap->end())
                    {
                        if (meta_vec > it_tmp->second->do_recv->metadata.versionvector[i])
                        {
                            log->debug_log("error occured");
                            return false;
                        }
                        if (!check_metadata_consistency(it_tmp->second->do_recv))
                        {
                            log->debug_log("Received invalid data object:sid:%u,suid:%u",it->second->stripe_id,it2->first);
                            return false;
                        }
                    }
                    else
                    {
                        log->debug_log("Block %u not read.",i);
                    }    
                }
                log->debug_log("stripe_unit:%u ok",it2->first);
            }
        }
        return true;
    }
    
    bool check_metadata_consistency(struct data_object *p_do)
    {
        uint32_t checksum;            
        calc_parity(&p_do->metadata, sizeof(p_do->metadata), &checksum);
        if (checksum==p_do->checksum) 
        {
            return true;
        }
        stringstream ss;
        print_dataobject_metadata(&p_do->metadata, ss);
        //it2->second->newdata =
        log->debug_log("%s",ss.str().c_str());
        log->debug_log("received checksum:%u",p_do->checksum);
        log->debug_log("Calculated:%u",checksum);
        return false;
    }
    
    
};


class OpManager {
public:
    OpManager(Logger *p_log, int (*p_cb)(queue_priorities, OPHead*), serverid_t myid,doCache *p_docache,Filestorage *fileio);
    OpManager(const OpManager& orig);
    virtual ~OpManager();
    //uint32_t getSequenceNumber();
    int dataserver_insert(struct dstask_spn_recv *p_task_sp);
    
    int client_insert_read(struct operation_client_read *op, struct EInode *p_einode);
    //int client_insert(struct operation_client_write *op, void *newdata);
    int client_insert(struct operation_composite *op, void *newdata);
    int client_result(InodeNumber inum, struct CCO_id ccoid, opstatus result);
    int client_handle_read_response(struct data_object *p_do, InodeNumber inum, StripeId stripeid, StripeUnitId stripeunitid, struct CCO_id ccoid);
    
    //int dataserver_prim_operation(struct operation_primcoordinator *p_op);
    int get_operation_structure(struct OPHead *p_head, struct OPHead **p_op);
    int get_operation_structure(struct OPHead *p_head, struct operation_participant **p_op);
    
    int handle_received_msg(struct dstask_ccc_received *p_task);
    int handle_prepare_msg(struct dstask_ccc_recv_prepare *p_task, struct operation_participant **part_out);
    int handle_cancommit_msg(struct dstask_cancommit *p_task);
    int handle_committed_msg(struct dstask_ccc_recv_committed *p_task);
    int handle_docommit_msg(struct operation_dstask_docommit *p_task, struct operation_participant **part_out);
    int handle_result_msg(struct dstask_proccess_result *p_task, struct operation_participant **part_out);
    int handle_stripewrite_cancommit(struct dstask_ccc_stripewrite_cancommit_received *p_task);
    int handle_directwrite_msg(struct dstask_spn_recv *p_task);
    
    int cleanup(struct operation_composite *op);
    int cleanup(struct operation_client_write *op);
    int cleanup(struct operation_client_read *op);
    void cleanup(struct operation_participant *op);
    void cleanup(struct operation_primcoordinator *op);
    
private:
    std::map<InodeNumber, StripeManager*> *p_opmap;
    pthread_mutex_t globallock_mutex;
    pthread_mutex_t opmap_getentry_mutex;
    Logger *log;
    serverid_t id;
    doCache *p_docache;
    Libraid4 *p_raid;
    Filestorage *p_fileio;
    int (*p_queuePush)(queue_priorities, OPHead*);
    
    int insert( void *data);
    
    int get_entry(InodeNumber inum, StripeManager **p_sm);
    void opmap_lock();
    void opmap_unlock();
};


#endif	/* OPMANAGER_H */
