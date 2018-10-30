
#include "components/DataObjectCache/doCache.h"

static bool DEACTIVATE_BUSY_BIT = true;
static const uint8_t gc_inum_timeout = 60;

static void print_dataobject_metadata(struct dataobject_metadata *p_obj, stringstream& ss)
{
    ss << "struct dataobject_metadata:\n";
    ss << "csid:" << p_obj->ccoid.csid << ", seq:" << p_obj->ccoid.sequencenum << "\n" ;
    ss << "datalength:" << p_obj->datalength << "\noffset:" << p_obj->offset;
    ss << "\noperationlength:" << p_obj->operationlength  << "\n";
    for (int i=0; i<default_groupsize; i++)
    {
       ss << "version" << i << ":" << p_obj->versionvector[i] << "\n";
    }
}

static void print_dataobject(struct data_object *p_obj, stringstream& ss)
{
    ss << "struct data_object:\n";
    print_dataobject_metadata(&p_obj->metadata, ss);
    ss << "data...\nchecksum:" << p_obj->checksum;
}

doCache::doCache(Logger* p_log, Filestorage *p_f ,serverid_t servid)
{
    p_fileio = p_f;
    log = p_log;        
    p_cache = new std::map<InodeNumber,struct stripe_cache_entry*>();
    mutex = PTHREAD_MUTEX_INITIALIZER;
    id = servid;
    p_raid = new Libraid4(log); 
}

doCache::doCache(const doCache& orig)
{    
    
}

doCache::~doCache()
{    
}

/**
 * @param it
 * @return 
 * 
 * Assumption: cache entry lock has been taken by calling instance
 */
uint64_t doCache::initialize_cache_entry(std::map<InodeNumber,struct stripe_cache_entry*>::iterator cacheit, StripeId sid)
{
    //int rc=-1;
    log->debug_log("initialize for inum:%llu,sid:%u",cacheit->first, sid);
    struct cache_entry *entry = new struct cache_entry;
    entry->unconfirmed = new std::map<uint64_t, struct data_object*>();
    entry->confirmed = new std::map<uint64_t, struct data_object*>();
    entry->entry_mutex = PTHREAD_MUTEX_INITIALIZER;
    entry->dirty = false;
    
    if (p_fileio->read_stripe_object(cacheit->first, sid, &entry->current)!=0)
    {
        log->debug_log("No block available, vecsize:%u",sizeof_versionvec);        
        entry->current=NULL;
        //entry->versioncnt = 0; //cacheit->second->versioncounter=0;
        memset(&entry->versionvec[0],0,sizeof_versionvec);
        entry->myvec_entry=NULL;
    }
    else
    {            
        log->debug_log("stripeid entry for %u read and inserted.",sid);
        filelayout_raid *fl = (filelayout_raid *) &entry->current->metadata.filelayout[0];
        StripeUnitId versionindex = p_raid->get_my_stripeunitid(fl,entry->current->metadata.offset,this->id);
        memcpy(&entry->versionvec, &entry->current->metadata.versionvector, sizeof_versionvec);
        memcpy(&entry->versionvec,&entry->current->metadata.versionvector, sizeof_versionvec);
        entry->myvec_entry=&entry->versionvec[versionindex];
        //rc=0;
    }
    cacheit->second->map->insert(std::pair<StripeId,struct cache_entry*>(sid,entry));
    return 0;//++entry->versioncnt;
}

struct stripe_cache_entry* doCache::create_stripe_cache_entry()
{
        struct stripe_cache_entry *p_ent =  new struct stripe_cache_entry;
        p_ent->map = new std::map<StripeId,struct cache_entry*>();
        //p_ent->dirtycnter=0;
        p_ent->dirty=true;
        p_ent->busy=true;
        //p_ent->last_modified = time(0);
        p_ent->smutex = PTHREAD_MUTEX_INITIALIZER;
        return p_ent;
}

int doCache::reset_version_vector(InodeNumber inum ,StripeId sid, uint8_t vindex)
{
    int rc=0;
    log->debug_log("inum:%llu, sid:%u",inum,sid);
    pthread_mutex_lock(&mutex);
    std::map<InodeNumber,struct stripe_cache_entry*>::iterator it = p_cache->find(inum);
    if (it==p_cache->end())
    {
        struct stripe_cache_entry *p_ent = create_stripe_cache_entry();
        it = p_cache->insert(it,std::pair<InodeNumber,struct stripe_cache_entry*>(inum,p_ent));
        log->debug_log("inserted map for inum:%u",inum);
    }
    pthread_mutex_lock(&it->second->smutex);
    pthread_mutex_unlock(&mutex);
    std::map<StripeId,struct cache_entry*>::iterator it2 = it->second->map->find(sid);
    if (it2 == it->second->map->end())
    {
        initialize_cache_entry(it, sid);
        log->debug_log("initalized...");
        it2 = it->second->map->find( sid);
        it2->second->myvec_entry = &it2->second->versionvec[vindex];
    }
    for (int i=0;i<sizeof_versionvec/sizeof(uint32_t); i++)
    {
        it2->second->versionvec[i]=1;
    }
    pthread_mutex_unlock(&it->second->smutex);
    return rc;
}

int doCache::get_next_version_vector(InodeNumber inum, StripeId sid, uint32_t *versionvec, uint8_t vindex)
{
    log->debug_log("request for %llu.%u, vindex=%u",inum,sid,vindex);
    int rc=0;
    pthread_mutex_lock(&mutex);
    std::map<InodeNumber,struct stripe_cache_entry*>::iterator it = p_cache->find(inum);
    if (it==p_cache->end())
    {
        struct stripe_cache_entry *p_ent = create_stripe_cache_entry();
        it = p_cache->insert(it,std::pair<InodeNumber,struct stripe_cache_entry*>(inum,p_ent));
        log->debug_log("inserted map for inum:%u",inum);
    }
    pthread_mutex_lock(&it->second->smutex);
    pthread_mutex_unlock(&mutex);
    log->debug_log("Got stripe mutex");
    it->second->busy=true;
    std::map<StripeId,struct cache_entry*>::iterator it2 = it->second->map->find(sid);
    if (it2 == it->second->map->end())
    {
        initialize_cache_entry(it, sid);
        log->debug_log("initalized...");
        it2 = it->second->map->find( sid);
        it2->second->myvec_entry = &it2->second->versionvec[vindex];
        //*it2->second->myvec_entry = 0;
    }
    log->debug_log("got stripe");
    if (it2->second->myvec_entry==NULL)
    {
        log->debug_log("initialize my entry pointer");
        it2->second->myvec_entry=&it2->second->versionvec[vindex];
    }            
    (*it2->second->myvec_entry)++;
    uint32_t *tmpvec=versionvec;
    log->debug_log("MYNEXT:Version:%u",*it2->second->myvec_entry);
    for (int i=0;i<sizeof_versionvec/sizeof(uint32_t); i++)
    {
        if (*tmpvec!=0 && i!=vindex)
        {
            it2->second->versionvec[i]=*tmpvec;
        }
        else
        {
            *tmpvec=it2->second->versionvec[i];
        }
        log->debug_log("%i:val=%u",i,it2->second->versionvec[i]);
        tmpvec++;
    }
//    memcpy(versionvec,&it2->second->versionvec,sizeof_versionvec);
    log->debug_log("MYNEXT:Version:%u",*it2->second->myvec_entry);
    pthread_mutex_unlock(&it->second->smutex);
    log->debug_log("return:rc=%d",rc);    
    return rc;
}

int doCache::get_unconfirmed(InodeNumber inum, StripeId sid, struct data_object **p_out)
{
    log->debug_log("request for %llu.%u",inum,sid);
    int rc=-1;
    pthread_mutex_lock(&mutex);
    std::map<InodeNumber,struct stripe_cache_entry*>::iterator it = p_cache->find(inum);
    if (it==p_cache->end())
    {
        struct stripe_cache_entry *p_ent =  create_stripe_cache_entry();
        it = p_cache->insert(it,std::pair<InodeNumber,struct stripe_cache_entry*>(inum,p_ent));
        log->debug_log("inserted map for inum:%u",inum);
    }
    pthread_mutex_lock(&it->second->smutex);
    pthread_mutex_unlock(&mutex);
    std::map<StripeId,struct cache_entry*>::iterator it2 = it->second->map->find(sid);
    if (it2 == it->second->map->end())
    {
        initialize_cache_entry(it, sid);
        pthread_mutex_unlock(&it->second->smutex);
    }
    else
    {
        pthread_mutex_lock(&it2->second->entry_mutex);
        pthread_mutex_unlock(&it->second->smutex);
        log->debug_log("Found stripeid:%u",it2->first);
        std::map<uint64_t, struct data_object*>::reverse_iterator itunconf = it2->second->unconfirmed->rbegin();
        if (itunconf!= it2->second->unconfirmed->rend())
        {
           *p_out=itunconf->second; 
        }
        else
        {
            *p_out = it2->second->current;
        }
        pthread_mutex_unlock(&it2->second->entry_mutex);
        log->debug_log("resturn entry with csid:%u",(*p_out)->metadata.ccoid.csid);
        rc=0;
    }
    /*/
    log->debug_log("Block read from disk.");
    stringstream ss;
    print_dataobject(*p_out,ss);
    log->debug_log("object:\n%s",ss.str().c_str());
    */
    return rc;
}
 
int doCache::get_entry(InodeNumber inum, StripeId sid, struct data_object **p_out)
{
    log->debug_log("request for %llu.%u",inum,sid);
    int rc=-1;
    pthread_mutex_lock(&mutex);
    std::map<InodeNumber,struct stripe_cache_entry*>::iterator it = p_cache->find(inum);
    if (it==p_cache->end())
    {
        struct stripe_cache_entry *p_ent = create_stripe_cache_entry();
        it = p_cache->insert(it,std::pair<InodeNumber,struct stripe_cache_entry*>(inum,p_ent));
        log->debug_log("inserted map for inum:%u",inum);
    }
    
    pthread_mutex_lock(&it->second->smutex);
    pthread_mutex_unlock(&mutex);
    
    std::map<StripeId,struct cache_entry*>::iterator it2 = it->second->map->find(sid);
    if (it2 == it->second->map->end())
    {
        initialize_cache_entry(it, sid);    
        it2 = it->second->map->find(sid);
    }
    log->debug_log("Found stripeid:%u",it2->first);
    *p_out = it2->second->current;    
    pthread_mutex_unlock(&it->second->smutex);
    if (*p_out!=NULL)
    {
        rc=0;
    }
    /*/
    log->debug_log("Block read from disk.");
    stringstream ss;
    print_dataobject(*p_out,ss);
    log->debug_log("object:\n%s",ss.str().c_str());
    */
    log->debug_log("rc:%d",rc);
    return rc;
}
 
int doCache::set_entry(InodeNumber inum, StripeId sid, struct data_object *p_in)
{
    int rc=-1;
    pthread_mutex_lock(&mutex);
    log->debug_log("Inum:%llu,sid:%u",inum,sid);
    std::map<InodeNumber,struct stripe_cache_entry*>::iterator it = p_cache->find(inum);
    if (it==p_cache->end())
    {     
        struct stripe_cache_entry *p_ent = create_stripe_cache_entry();
        it = p_cache->insert(it, std::pair<InodeNumber,struct stripe_cache_entry*>(inum,p_ent));
        log->debug_log("created map for inum:%llu",inum);        
    }
    
    pthread_mutex_lock(&it->second->smutex);
    
    log->debug_log("Found cache entry for inum:%llu",inum);
    std::map<StripeId,struct cache_entry*>::iterator it2 = it->second->map->find(sid);
    if (it2 == it->second->map->end())
    {
        struct cache_entry *entry = new struct cache_entry;
        entry->current=p_in;
        entry->unconfirmed=new std::map<uint64_t, struct data_object*>();
        entry->confirmed = new std::map<uint64_t, struct data_object*>();
        //entry->versioncnt=1;
        entry->dirty=true;
        filelayout_raid *fl = (filelayout_raid *) &entry->current->metadata.filelayout[0];
        StripeUnitId versionindex = p_raid->get_my_stripeunitid(fl,entry->current->metadata.offset,this->id);
        entry->myvec_entry = &entry->versionvec[versionindex];
        entry->entry_mutex=PTHREAD_MUTEX_INITIALIZER;
        it->second->map->insert(std::pair<StripeId,struct cache_entry*>(sid,entry));
        it->second->busy=true;
        pthread_mutex_unlock(&it->second->smutex);
        log->debug_log("No stripeid entry for %u. created it.",sid);  
        rc=0;
    }        
    else
    {
        pthread_mutex_lock(&it2->second->entry_mutex);

        log->debug_log("Found stripeid:%u",it2->first);  
        if (it2->second->current!=NULL)
        {
            log->debug_log("deleting csid:%u,seq:%u.",it2->second->current->metadata.ccoid.csid,it2->second->current->metadata.ccoid.sequencenum);
            if (it2->second->current->data!=NULL) free(it2->second->current->data);
            //delete it2->second->current;
        }
        it2->second->current = p_in;
        it->second->busy=true;
        it->second->dirty=true;
        it2->second->dirty=true;
        rc=0;
        log->debug_log("inserted:seq:%u",it2->second->current->metadata.ccoid.sequencenum);
        pthread_mutex_unlock(&it2->second->entry_mutex);
    }
    pthread_mutex_unlock(&it->second->smutex);
    //it->second->last_modified = time(0);
    log->debug_log("rc:%d",rc);
    pthread_mutex_unlock(&mutex);
    return rc;
}

int doCache::parityUnconfirmed(InodeNumber inum, StripeId sid, struct data_object *p_in)
{     
    int rc=-1;
    uint8_t versionindex = default_groupsize;
    log->debug_log("Inum:%llu,sid:%u,version:%u, seq:%u",inum,sid,p_in->metadata.versionvector[versionindex],p_in->metadata.ccoid.sequencenum);
    pthread_mutex_lock(&mutex);
    
    std::map<InodeNumber,struct stripe_cache_entry*>::iterator it = p_cache->find(inum);
    if (it==p_cache->end())
    {     
        struct stripe_cache_entry *p_ent = create_stripe_cache_entry();
        it = p_cache->insert(it, std::pair<InodeNumber,struct stripe_cache_entry*>(inum,p_ent));
        log->debug_log("created map for inum:%llu",inum);        
    }
    log->debug_log("Found cache entry for inum:%llu",inum);
    pthread_mutex_lock(&it->second->smutex);
    std::map<StripeId,struct cache_entry*>::iterator it2 = it->second->map->find(sid);
    if (it2 == it->second->map->end())
    {
        struct cache_entry *entry = new struct cache_entry;
        entry->current=p_in;
        entry->dirty=false;
        entry->unconfirmed=new std::map<uint64_t, struct data_object*>();
        entry->confirmed = new std::map<uint64_t, struct data_object*>();
        entry->entry_mutex=PTHREAD_MUTEX_INITIALIZER;
        filelayout_raid *fl = (filelayout_raid *) &entry->current->metadata.filelayout[0];
        StripeUnitId versionindex = p_raid->get_my_stripeunitid(fl,entry->current->metadata.offset,this->id);
        entry->myvec_entry = &entry->versionvec[versionindex];
        it->second->map->insert(std::pair<StripeId,struct cache_entry*>(sid,entry));
        log->debug_log("No stripeid entry for %u. created it.",sid);  
        rc=0;
        it2 = it->second->map->find(sid);
    }
    //std::map<uint64_t,struct data_object*>::iterator it3 = it2->second->unconfirmed->find(p_in->metadata.versionvector[versionindex]);
    it2->second->unconfirmed->insert(std::pair<uint64_t,struct data_object*>(p_in->metadata.versionvector[versionindex],p_in));
    log->debug_log("stripe id:%u, inserted version:%u",it2->first,p_in->metadata.versionvector[versionindex]);
    rc=0;

    pthread_mutex_unlock(&it->second->smutex);
    pthread_mutex_unlock(&mutex);
    log->debug_log("rc:%d",rc);
    return rc;
}


int doCache::parityConfirm(InodeNumber inum, StripeId sid, uint64_t version)
{
    int rc=-1;
    uint8_t versionindex = default_groupsize;
    log->debug_log("inum:%llu,sid:%u,version:%llu, versionindex:%u",inum,sid,version, versionindex);
    pthread_mutex_lock(&mutex);
    std::map<InodeNumber,struct stripe_cache_entry*>::iterator it = p_cache->find(inum);
    if (it!=p_cache->end())
    {    
        pthread_mutex_lock(&it->second->smutex);
        log->debug_log("find entry sid:%u",sid);
        std::map<StripeId,struct cache_entry*>::iterator it2 = it->second->map->find(sid);
        pthread_mutex_unlock(&mutex);
        if (it2 != it->second->map->end())
        { 
            pthread_mutex_lock(&it2->second->entry_mutex);
            //pthread_mutex_unlock(&it->second->smutex);
            log->debug_log("get unconfirmed version:%llu",version);
            std::map<uint64_t,struct data_object*>::iterator it3 = it2->second->unconfirmed->find(version);
            log->debug_log("acquire mutex...");
            if (it3 != it2->second->unconfirmed->end())
            {
                uint64_t current_version = (it2->second->current!=0) ? it2->second->current->metadata.versionvector[versionindex] : 0;
                log->debug_log("Current version is...%u:",current_version);
                if (current_version+1 == version)
                {   
                    if (it2->second->current!=0)
                    {
                        log->debug_log("deleting csid:%u,seq:%u.",it2->second->current->metadata.ccoid.csid,it2->second->current->metadata.ccoid.sequencenum);
                        free(it2->second->current->data);
                        delete it2->second->current;      
                    }
                    it2->second->current=it3->second;
                    log->debug_log("%p:%p",it2->second->current,it3->second);
                    rc=0;
                    log->debug_log("switched from version %u to %u: current-> reflects:%u",current_version,version,it2->second->current->metadata.versionvector[versionindex]);
                    std::map<uint64_t,struct data_object*>::iterator itconf = it2->second->confirmed->begin();
                    uint64_t current_version2;
                    while (itconf!=it2->second->confirmed->end())
                    {
                        log->debug_log("next it");
                        current_version2 = it2->second->current->metadata.versionvector[versionindex];
                        log->debug_log("current version is:%u",current_version2);
                        if (current_version2+1==itconf->second->metadata.versionvector[versionindex])
                        {
                            free(it2->second->current->data);
                            delete it2->second->current;      
                            it2->second->current=itconf->second;                            
                            it2->second->confirmed->erase(itconf++);
                            log->debug_log("switch again: version %u to +1",current_version2);
                        }
                        else
                        {
                            ++itconf;
                        }
                    }
                    log->debug_log("Version check done");
                    //if (!it2->second->dirty)
                    //{
                        //it->second->dirtycnter++;
                        it->second->dirty=true;
                        it2->second->dirty=true;
                        log->debug_log("DIRTY:inum:%llu,sid:%u",it->first, it2->first);
                    //}
                        log->debug_log("Pointer:%p, current version:%u",it2->second->current,it2->second->current->metadata.versionvector[versionindex]);
                }
                else
                {
                    it2->second->confirmed->insert(std::pair<uint64_t,struct data_object*>(version,it3->second));
                    log->debug_log("version %u added to confirmed map.",version);
                    rc=0;
                }
                it3->second=NULL;
                it2->second->unconfirmed->erase(it3);
                //it->second->last_modified = time(0);
            }
            else
            {
                log->debug_log("no unconfirmed version:%llu",version);
            }
            it->second->busy=true;
            pthread_mutex_unlock(&it2->second->entry_mutex);
        }
        else
        {       
            log->debug_log("no entry for sid:%u",sid);
        }        
        pthread_mutex_unlock(&it->second->smutex);
    }
    else
    {
        pthread_mutex_unlock(&mutex);
        log->debug_log("no inum entry for %llu",inum);
    }
    log->debug_log("rc:%d",rc);
    return rc;
}


int doCache::garbage_collection()
{
    int rc = 0;
    log->debug_log("Start garbage collection");
    filelayout_raid *fl;
    uint64_t currentversion;
    StripeUnitId versionindex;
    std::map<StripeId,struct cache_entry*>::iterator itsid;
    std::map<InodeNumber,struct stripe_cache_entry*>::iterator it = p_cache->begin();
    //bool dodecrement;
    for (it;it!=p_cache->end(); it++)
    {        
        if (!it->second->busy || DEACTIVATE_BUSY_BIT)// && it->second->last_modified + gc_inum_timeout < time(0))
        {    
            if (it->second->dirty)
            {           
                log->debug_log("Inum:%llu dirty",it->first );
                if (it->second->map!=NULL)
                {
                    log->debug_log("get mutex...");
                    if (pthread_mutex_lock(&it->second->smutex)==0)
                    {
                        log->debug_log("got it.");                        
                        if (!it->second->map->empty())
                        {
                            log->debug_log("map not empty");
                            itsid = it->second->map->begin();
                            if (itsid->second->current!=NULL)
                            {
                                log->debug_log("got first item");
                                //log->debug_log("itsid: version count %u, scid:%u",*itsid->second->myvec_entry, itsid->second->current->metadata.ccoid.csid);
                                fl = (filelayout_raid*) &itsid->second->current->metadata.filelayout[0];
                                versionindex = p_raid->get_my_stripeunitid(fl,itsid->second->current->metadata.offset,this->id);           
                                log->debug_log("Versionindex:%u",versionindex);
                                it->second->dirty=false;
                                for (itsid; itsid!=it->second->map->end(); itsid++)
                                {
                                    if (itsid->second->dirty)
                                    {                        
                                        if (pthread_mutex_trylock(&itsid->second->entry_mutex)==0)
                                        {
                                            try
                                            {
                                                currentversion=itsid->second->current->metadata.versionvector[versionindex];
                                                log->debug_log("Pointer:%p,Currentversion of inum:%llu, sid:%u is %u",itsid->second->current,it->first, itsid->first, currentversion);
                                                rc = p_fileio->remove_lower_than(it->first, itsid->first, currentversion);
                                                itsid->second->dirty=false;
                                                //dodecrement=true;
                                                pthread_mutex_unlock(&itsid->second->entry_mutex);
                                            }
                                            catch (...)
                                            {
                                                pthread_mutex_unlock(&itsid->second->entry_mutex);
                                                rc=-1;
                                            }
                                        }
                                        else
                                        {
                                            log->debug_log("mutex not received");
                                            it->second->dirty=true;
                                        }
                                    }
                                    else
                                    {
                                        log->debug_log("inum:%llu stripeid %u not dirty",it->first,itsid->first);
                                    }
                                }
                            }
                            else
                            {
                                log->debug_log("No current entry yet.");
                            }
                            
                        }
                        else
                        {
                            log->debug_log("Map empty");
                        }
                        pthread_mutex_unlock(&it->second->smutex);
                    }
                    else
                    {
                        log->debug_log("mutex busy");
                    }
                }
                else
                {
                    log->debug_log("empty map...");
                }
            } 
//            if (dodecrement) 
  //          {
                //it->second->dirtycnter--;
    //        }
        }
        else
        {
            log->debug_log("file %llu busy",it->first);
        }
        it->second->busy=false;
    }
    return rc;
}
    