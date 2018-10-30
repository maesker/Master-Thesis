/* 
 * File:   libraid5.cpp
 * Author: markus
 * 
 * Created on 13. Januar 2012, 23:34
 */

#include "components/raidlibs/Libraid5.h"

Libraid5::Libraid5(Logger *p_log) {
    p_sman = NULL;
    log = p_log;
    groupsize = default_groupsize;
    susize = 10;
    servercnt = 8;
}

Libraid5::Libraid5(const Libraid5& orig) {
}

Libraid5::~Libraid5() {
    log->debug_log("done");
}

void Libraid5::setGroupsize(uint8_t size)
{
    groupsize = size;
}

void Libraid5::setStripeUnitSize(size_t size)
{
    susize  = size;
}

void Libraid5::setServercnt(uint16_t cnt)
{
    servercnt = cnt;
}

void Libraid5::register_server_manager(ServerManager *p_servermanager)
{
    p_sman = p_servermanager;
    log->debug_log("done");
}

int Libraid5::create_initial_filelayout(InodeNumber inum, filelayout *p_fl)
{
    log->debug_log("inum:%llu",inum);
    int rc = -1;
    if (p_sman!=NULL)
    {
        uint32_t ds_cnt;
        rc = p_sman->get_server_count(&ds_cnt);
        log->debug_log("dscnt:%u",ds_cnt);
        
        struct filelayout_raid5 *p_fl_r5 = (struct filelayout_raid5 *) p_fl;
        p_fl_r5->type = raid5;
        p_fl_r5->groupsize = this->groupsize;
        p_fl_r5->servercount = this->servercnt;
        p_fl_r5->stripeunitsize = this->susize;

        for (serverid_t i=0; i<this->servercnt; i++)
        {
            p_fl_r5->serverids[i] = i;
        }
        rc=0;
    }
    log->debug_log("rc=%u",rc);
    return rc;
}

uint8_t Libraid5::is_coordinator(struct filelayout_raid5 *fl, size_t offset, serverid_t myid)
{
    log->debug_log("myid:%llu.",myid);
    struct Stripe_layout *slay = get_stripe(fl,offset);
    if (slay->parityserver_id==myid)
    {
        log->debug_log("is parity server.");
        return 2;
    }
    StripeId sid = get_stripe_id(fl,offset);
    StripeUnitId suid = get_stripeunit_id(fl,offset);
    serverid_t servid = get_server_id (fl, sid, suid);
    //printStripeLayout(slay);
    if (servid == myid)
    {
        log->debug_log("is secondary coordinator.");
        return 1;
    }
    log->debug_log("only participator.");
    return 0;
}

serverid_t Libraid5::get_secondary_coord(struct filelayout_raid5 *fl, size_t offset)
{
    return get_server_id(fl,get_stripe_id(fl,offset), get_stripeunit_id(fl,offset));    
}

serverid_t Libraid5::get_primary_coord(struct filelayout_raid5 *fl, size_t offset)
{
    return _get_parity_id(fl,get_stripe_id(fl,offset));
}

serverid_t Libraid5::get_parity_id(filelayout_raid5 *fl, StripeId id)
{
    return _get_parity_id(fl,id);
}

StripeId Libraid5::getStripeId(struct filelayout_raid5 *fl, size_t offset)
{
    return get_stripe_id(fl,offset);
}

StripeUnitId Libraid5::get_my_stripeunitid(struct filelayout_raid5 *fl, size_t offset, serverid_t myid)
{
    StripeUnitId suid=0;
    struct Stripe_layout *p_slay = get_stripe(fl, offset);
    std::map<StripeUnitId,struct StripeUnit*>::iterator it = p_slay->blockmap->begin();
    for (it; it!=p_slay->blockmap->end();it++)
    {
        if (it->second->assignedto==myid)
        {
            suid=it->first;
            log->debug_log("Successful.");
            break;
        }
    }
    log->debug_log("ret:%u",suid);
    return suid; 
}

struct Stripe_layout* Libraid5::get_stripe(filelayout_raid5 *fl, size_t offset)
{
    log->debug_log("offset:%llu",offset);
    struct Stripe_layout *p_res = getNewStripeLayout();
    StripeId sid = get_stripe_id(fl,offset);
    size_t stripe_size = get_stripe_size(fl);
    log->debug_log("StripeId:%u,stripe size:%llu",sid,stripe_size);
    size_t stripestart =get_stripe_start(fl,offset);
    size_t stripeend = stripestart+stripe_size;
    size_t i = stripestart;
    log->debug_log("stripestart:%llu, stripeend:%llu",stripestart,stripeend);
    while (i<stripeend)
    {
        struct StripeUnit *unit = (struct StripeUnit*) malloc(sizeof(struct StripeUnit));
        unit->start=i;
        StripeUnitId bid = get_stripeunit_id(fl,i);
        unit->assignedto = get_server_id(fl,sid,bid);
        if (i+fl->stripeunitsize < stripeend)
        {
            unit->end = i+fl->stripeunitsize ;
        }
        else
        {
            unit->end = stripeend ;
        }
        log->debug_log("stripeunitid:%u, assigned to id:%u, start:%u-%u;",bid,unit->assignedto,unit->start,unit->end);
        i = i+fl->stripeunitsize;
        p_res->blockmap->insert(std::pair<StripeUnitId,struct StripeUnit*>(bid,unit));        
    }
    p_res->parityserver_id = get_parity_id(fl,sid);
    log->debug_log("Parity of stripe id %u is id:%u. end",sid,p_res->parityserver_id);
    return p_res;
}

struct FileStripeList* Libraid5::get_stripes(filelayout_raid5 *fl, size_t offset, size_t length)
{
    log->debug_log("start...");
    struct FileStripeList *p_result = getNewFileStripeList();
    size_t i = offset;
    size_t stripe_size = get_stripe_size(fl);
    log->debug_log("Stripesize=%llu",stripe_size);
    StripeId stripeid = get_stripe_id(fl,offset);
    log->debug_log("Stripeid:%llu",stripeid);
    while (i<length)
    {   
        log->debug_log("i=%llu",i);
        struct Stripe_layout *p_slay = get_stripe(fl,offset,stripe_size);        
        p_result->stripemap->insert(std::pair<StripeId,struct Stripe_layout*>(stripeid,p_slay));
        stripeid++;
        i+=stripe_size;
        p_result->size++;        
    }    
    return p_result;
}

struct Stripe_layout* Libraid5::get_stripe(filelayout_raid5 *fl, size_t offset, size_t length)
{
    log->debug_log("offset:%llu",offset);
    struct Stripe_layout *p_res = getNewStripeLayout();
    StripeId sid = get_stripe_id(fl,offset);
    size_t stripe_size = get_stripe_size(fl);
    log->debug_log("StripeId:%u,stripe size:%llu",sid,stripe_size);
    size_t stripestart =get_stripe_start(fl,offset);
    size_t stripeend = stripestart+stripe_size;
    size_t i = stripestart;
    log->debug_log("stripestart:%llu, stripeend:%llu",stripestart,stripeend);
    while (i<length)
    {
        struct StripeUnit *unit = (struct StripeUnit*) malloc(sizeof(struct StripeUnit));
        unit->start=i;
        StripeUnitId bid = get_stripeunit_id(fl,i);
        unit->assignedto = get_server_id(fl,sid,bid);
        if (i+fl->stripeunitsize < stripeend)
        {
            unit->end = i+fl->stripeunitsize ;
        }
        else
        {
            unit->end = stripeend ;
        }
        log->debug_log("stripeunitid:%u, assigned to id:%u, start:%u-%u;",bid,unit->assignedto,unit->start,unit->end);
        i = i+fl->stripeunitsize;
        p_res->blockmap->insert(std::pair<StripeUnitId,struct StripeUnit*>(bid,unit));        
    }
    p_res->parityserver_id = get_parity_id(fl,sid);
    log->debug_log("Parity of stripe id %u is id:%u. end",sid,p_res->parityserver_id);
    return p_res;
}


void Libraid5::printFL(filelayout_raid5* fl)
{
    //log->debug_log("Filelayout: Startnode:%u, Nodecount:%u, Stripesize:%u, Groupcount:%u.",fl->startnode, fl->nodecount, fl->stripeunitsize, fl->groupcount);
}

void Libraid5::printFileStripeList(struct FileStripeList *p)
{
    log->debug_log("FilestripeList: size:%llu",p->size);
    std::map<StripeId,struct Stripe_layout*>::iterator it = p->stripemap->begin();
    for (it; it!=p->stripemap->end();it++)
    {
        log->debug_log("FilestripeList: stripeID:%llu",it->first);
        printStripeLayout(it->second);
    }    
}

void Libraid5::printStripeLayout(struct Stripe_layout *p)
{
    log->debug_log("Stripelayout: parity server id:%llu",p->parityserver_id);
    std::map<StripeUnitId,struct StripeUnit*>::iterator it = p->blockmap->begin();
    for (it; it!=p->blockmap->end();it++)
    {
        log->debug_log("Stripelayout: StripeUnitID:%llu",it->first);
        log->debug_log("Stripelayout: %llu-%llu assigned to %llu",it->second->start,it->second->end, it->second->assignedto);
    }    
}

serverid_t Libraid5::get_server_id(struct filelayout_raid5 *fl, StripeId sid, StripeUnitId id)
{    
    uint32_t gid = get_groupid(fl,sid);
    uint32_t groupoffset = id%(fl->groupsize-1);
    serverid_t serv = gid*fl->groupsize + groupoffset;
    serverid_t parity = get_parity_id(fl,sid);
    //std::cout << "stripeid:" << sid << "parity get server id" << parity << std::endl ;
    if (serv>=parity)
    {
        serv=serv+1;
        if (serv%fl->groupsize == 0)
        {
            serv-fl->groupsize;
        }
    }
    return serv;
}

struct Stripe_layout* Libraid5::getNewStripeLayout()
{
    struct Stripe_layout *res = (struct Stripe_layout*) malloc(sizeof(struct Stripe_layout));
    res->blockmap = new std::map<StripeUnitId,struct StripeUnit*>;
    return res;
}

struct FileStripeList* Libraid5::getNewFileStripeList()
{
    struct FileStripeList* res = (struct FileStripeList*) malloc(sizeof(struct FileStripeList));
    res->stripemap = new std::map<StripeId,struct Stripe_layout*>;
    res->size = 0;
    return res;
}


static inline uint32_t get_groupid(struct filelayout_raid5 *fl, StripeId sid)
{
   return (sid/fl->groupsize)%(fl->servercount/fl->groupsize);
}

static inline uint32_t get_groupcount(struct filelayout_raid5 *fl)
{
    return fl->servercount/fl->groupsize;
}

inline size_t get_stripe_size(struct filelayout_raid5 *fl)
{
    return (fl->groupsize-1)*fl->stripeunitsize;
}

static inline StripeId get_stripe_id(struct filelayout_raid5 *fl, size_t offset)
{
    return (offset/get_stripe_size(fl));
}

static size_t get_stripe_start(struct filelayout_raid5 *fl, size_t offset)
{
    size_t stripesize  = get_stripe_size(fl);
    return (offset/stripesize)*stripesize;
}

static size_t get_stripe_end(struct filelayout_raid5 *fl, size_t offset)
{
    size_t stripesize  = get_stripe_size(fl);
    return ((offset/stripesize)*stripesize + stripesize);
}
      
//static inline serverid_t get_start_parity_server(filelayout_raid5 *fl)
//{
//    return fl->serverids[fl->groupsize-1];
//}

static inline serverid_t _get_parity_id(filelayout_raid5 *fl, StripeId id)
{
    return fl->serverids[id%fl->servercount];
}

static StripeUnitId get_stripeunit_id(struct filelayout_raid5 *fl, size_t offset)
{
    StripeId stripeid = get_stripe_id(fl,offset);
    StripeUnitId s1 = (offset-stripeid*get_stripe_size(fl))/fl->stripeunitsize;    
    return (stripeid*(fl->groupsize-1))+s1;
}

