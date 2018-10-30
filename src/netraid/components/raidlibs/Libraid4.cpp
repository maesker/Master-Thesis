/* 
 * File:   libraid4.cpp
 * Author: markus
 * 
 * Created on 13. Januar 2012, 23:34
 */

#include "components/raidlibs/Libraid4.h"

Libraid4::Libraid4(Logger *p_log) {
    p_sman = NULL;
    log = p_log;
    groupsize = default_groupsize+1;
    susize = 65536;
    servercnt = groupsize;
}

Libraid4::Libraid4(const Libraid4& orig) {
}

Libraid4::~Libraid4() {
    log->debug_log("done");
}

void Libraid4::setGroupsize(uint8_t size)
{
    groupsize = size;
}

void Libraid4::setStripeUnitSize(size_t size)
{
    susize  = size;
}

void Libraid4::setServercnt(uint16_t cnt)
{
    servercnt = cnt;
}

void Libraid4::register_server_manager(ServerManager *p_servermanager)
{
    p_sman = p_servermanager;
    log->debug_log("done");
}

int Libraid4::create_initial_filelayout(InodeNumber inum, filelayout *p_fl)
{
    log->debug_log("inum:%llu",inum);
    int rc = -1;
    if (p_sman!=NULL)
    {
        uint32_t ds_cnt;
        rc = p_sman->get_server_count(&ds_cnt);
        log->debug_log("dscnt:%u",ds_cnt);
        
        struct filelayout_raid4 *p_fl_r4 = (struct filelayout_raid4 *) p_fl;
        p_fl_r4->type = raid4;
        p_fl_r4->groupsize = this->groupsize;
        p_fl_r4->servercount = this->servercnt;
        p_fl_r4->stripeunitsize = this->susize;

        for (serverid_t i=0; i<this->servercnt; i++)
        {
            p_fl_r4->serverids[i] = i;
        }
        rc=0;
    }
    log->debug_log("rc=%u",rc);
    return rc;
}


uint8_t Libraid4::is_coordinator(filelayout_raid *fl, size_t offset, serverid_t myid)
{
    log->debug_log("offset:%u,myid:%u.",offset,myid);
    uint8_t res=0;
    serverid_t coord = get_coordinator(fl,offset);
    log->debug_log("coord:%u",coord);
    if (coord==myid)
    {
        log->debug_log("is parity server.");
        res = 2;
    }
    else
    {
        StripeId sid = get_stripe_id(fl,offset);
        StripeUnitId suid = get_stripeunit_id(fl,offset);
        serverid_t servid = get_server_id (fl, sid, suid);
        log->debug_log("sid:%u,suid:%u,secco id:%u",sid,suid,servid);
        if (servid == myid)
        {
            log->debug_log("is secondary coordinator.");
            res = 1;
        }
        else
        {
             log->debug_log("only participator.");   
        }
    }    
    return res;
}

serverid_t Libraid4::get_secondary_coord( filelayout_raid *fl, size_t offset)
{
    return get_server_id(fl,get_stripe_id(fl,offset), get_stripeunit_id(fl,offset));    
}
/*/
serverid_t Libraid4::get_primary_coord( filelayout_raid *fl, size_t offset)
{
    return _get_parity_id(fl,get_stripe_id(fl,offset));
}

serverid_t Libraid4::get_parity_id(filelayout_raid *fl, StripeId id)
{
    return _get_parity_id(fl,id);
}*/

StripeId Libraid4::getStripeId(filelayout_raid *fl, size_t offset)
{
    return get_stripe_id(fl,offset);
}


StripeUnitId Libraid4::get_my_stripeunitid(filelayout_raid *fl, size_t offset, serverid_t myid)
{
    StripeUnitId suid=0;
    log->debug_log("offset:%llu",offset);
    if (is_coordinator(fl,offset,myid)==2) 
    {
        log->debug_log("groupsize:int:%d, uint:%u",fl->raid4.groupsize,fl->raid4.groupsize);
        suid=(fl->raid4.groupsize-1);
    }
    else
    {
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
        free_StripeLayout(p_slay);
    }
    log->debug_log("ret:%d",suid);
    return suid; 
}

struct Stripe_layout* Libraid4::get_stripe(filelayout_raid *fl, size_t offset)
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
        log->debug_log("unit start:%u",i);
        struct StripeUnit *unit = new struct StripeUnit;
        unit->start=i;
        StripeUnitId bid = get_stripeunit_id(fl,i);
        log->debug_log("suid:%u",bid);
        unit->assignedto = get_server_id(fl,sid,bid);
        log->debug_log("unit assigned to:%u",unit->assignedto);
        if (i+fl->raid4.stripeunitsize < stripeend)
        {
            unit->end = i+fl->raid4.stripeunitsize;
        }
        else
        {
            unit->end = stripeend ;
        }
        unit->newdata=NULL;
        unit->olddata=NULL;
        unit->paritydata=NULL;
        unit->do_recv=NULL;
        log->debug_log("stripeunitid:%u, assigned to id:%u, start:%u-%u;",bid,unit->assignedto,unit->start,unit->end);
        i = i+fl->raid4.stripeunitsize;
        p_res->blockmap->insert(std::pair<StripeUnitId,struct StripeUnit*>(bid,unit));        
    }
    p_res->parityserver_id = get_coordinator(fl,offset);
    log->debug_log("Parity of stripe id %u is id:%u. end",sid,p_res->parityserver_id);
    return p_res;
}

struct FileStripeList* Libraid4::get_stripes(filelayout_raid *fl, size_t offset, size_t length)
{
    log->debug_log("start...");
    struct FileStripeList *p_result = getNewFileStripeList();
    size_t stripe_size = get_stripe_size(fl);
    log->debug_log("Stripesize=%llu",stripe_size);
    StripeId stripeid = get_stripe_id(fl,offset);
    log->debug_log("Stripeid:%llu",stripeid);
    size_t operation_end = offset+length;
    size_t stripestart=offset;
    size_t stripeend;
    while (stripestart<operation_end)
    {   
        //stripestart = get_stripe_start(fl,stripestart);
        stripeend = (operation_end<stripestart+stripe_size) ? operation_end : stripestart+stripe_size;
        log->debug_log("start=%llu, end=%llu",stripestart,stripeend);
        struct Stripe_layout *p_slay = get_stripe(fl,stripestart,stripeend);        
        p_result->stripemap->insert(std::pair<StripeId,struct Stripe_layout*>(stripeid,p_slay));
        stripeid++;
        stripestart=stripeend+1;
        p_result->size++;        
    }    
    return p_result;
}

struct Stripe_layout* Libraid4::get_stripe(filelayout_raid *fl, size_t offset, size_t length)
{
    log->debug_log("offset:%llu, length:%llu",offset,length);
    struct Stripe_layout *p_res = getNewStripeLayout();
    StripeId sid = get_stripe_id(fl,offset);
    size_t stripe_size = get_stripe_size(fl);
    log->debug_log("StripeId:%u,stripe size:%llu",sid,stripe_size);
    
    size_t stripestart =get_stripe_start(fl,offset);
    size_t stripeend = stripestart+stripe_size;
    size_t i = offset;
    bool isfull = stripestart==offset ? true: false;
    if (isfull)
    {
         isfull = (offset+length)>=stripeend ? true : false;
    }
    p_res->isfull=isfull;
    log->debug_log("stripestart:%llu, stripeend:%llu, operation covers fullstripe:%u",stripestart,stripeend,(uint8_t)isfull);
    while (i<length)
    {
        log->debug_log("unit start:%u",i);
        struct StripeUnit *unit = new struct StripeUnit;
        unit->newdata=NULL;
        unit->olddata=NULL;
        unit->paritydata=NULL;
        unit->do_recv=NULL;
        unit->start=i;
        StripeUnitId bid = get_stripeunit_id(fl,i);
        log->debug_log("suid:%u",bid);
        unit->assignedto = get_server_id(fl,sid,bid);
        if (i+fl->raid4.stripeunitsize < stripeend)
        {
            unit->end = i+fl->raid4.stripeunitsize ;
        }
        else
        {
            unit->end = stripeend ;
        }
        log->debug_log("stripeunitid:%u, assigned to id:%u, start:%u-%u;",bid,unit->assignedto,unit->start,unit->end);
        i = i+fl->raid4.stripeunitsize;
        p_res->blockmap->insert(std::pair<StripeUnitId,struct StripeUnit*>(bid,unit));        
    }
    log->debug_log("get coordinator");
    p_res->parityserver_id = get_coordinator(fl,offset);
    log->debug_log("Parity of stripe id %u is id:%u. end",sid,p_res->parityserver_id);
    return p_res;
}


void Libraid4::printFL(filelayout_raid *fl)
{
    //log->debug_log("Filelayout: Startnode:%u, Nodecount:%u, Stripesize:%u, Groupcount:%u.",fl->startnode, fl->nodecount, fl->stripeunitsize, fl->groupcount);
}

void Libraid4::printFileStripeList(struct FileStripeList *p)
{
    log->debug_log("FilestripeList: size:%llu",p->size);
    std::map<StripeId,struct Stripe_layout*>::iterator it = p->stripemap->begin();
    for (it; it!=p->stripemap->end();it++)
    {
        log->debug_log("FilestripeList: stripeID:%llu",it->first);
        printStripeLayout(it->second);
    }    
}

void Libraid4::printStripeLayout(struct Stripe_layout *p)
{
    log->debug_log("Stripelayout: parity server id:%llu",p->parityserver_id);
    std::map<StripeUnitId,struct StripeUnit*>::iterator it = p->blockmap->begin();
    for (it; it!=p->blockmap->end();it++)
    {
        log->debug_log("Stripelayout: StripeUnitID:%llu",it->first);
        log->debug_log("Stripelayout: %llu-%llu assigned to %llu",it->second->start,it->second->end, it->second->assignedto);
    }    
}


struct Stripe_layout* Libraid4::getNewStripeLayout()
{
    struct Stripe_layout *res = new struct Stripe_layout;
    res->blockmap = new std::map<StripeUnitId,struct StripeUnit*>();
    return res;
}

struct FileStripeList* Libraid4::getNewFileStripeList()
{
    struct FileStripeList* res = new struct FileStripeList;
    res->stripemap = new std::map<StripeId,struct Stripe_layout*>();
    res->size = 0;
    return res;
}

static size_t get_stripe_start( filelayout_raid *fl, size_t offset)
{
    size_t stripesize  = get_stripe_size(fl);
    return (offset/stripesize)*stripesize;
}

static size_t get_stripe_end( filelayout_raid *fl, size_t offset)
{
    size_t stripesize  = get_stripe_size(fl);
    return ((offset/stripesize)*stripesize + stripesize);
}
      
inline serverid_t _get_parity_id(filelayout_raid *fl, StripeId id)
{
    return fl->raid4.serverids[(get_groupid(fl,id)*fl->raid4.groupsize)%(fl->raid4.servercount/fl->raid4.groupsize)+fl->raid4.groupsize-1];
}

static StripeUnitId get_stripeunit_id( filelayout_raid *fl, size_t offset)
{
    StripeId stripeid = get_stripe_id(fl,offset);
    StripeUnitId s1 = (offset-stripeid*get_stripe_size(fl))/fl->raid4.stripeunitsize;    
    return ((stripeid*(fl->raid4.groupsize-1))+s1)%(fl->raid4.groupsize-1);
}

static inline serverid_t get_server_id_gid( filelayout_raid *fl, uint16_t groupid, StripeUnitId suid)
{
    return fl->raid4.serverids[(groupid*fl->raid4.groupsize)%(fl->raid4.servercount)+suid];
}

inline serverid_t get_server_id( filelayout_raid *fl, StripeId sid, StripeUnitId id)
{    
    return get_server_id_gid(fl,get_groupid(fl,sid),id);
}