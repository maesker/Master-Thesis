/* 
 * File:   doCache.h
 * Author: markus
 *
 * Created on 28. April 2012, 10:08
 */

#ifndef DOCACHE_H
#define	DOCACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <vector>
#include <iostream>
#include "pthread.h"

#include "global_types.h"
#include "logging/Logger.h"
#include "components/diskio/Filestorage.h"
#include "components/OperationManager/OpData.h"
#include "components/raidlibs/Libraid4.h"
#include "components/raidlibs/raid_data.h"
#include "tools/parity.h"


static uint32_t sizeof_versionvec = sizeof(uint32_t)*(default_groupsize+1);

struct stripe_cache_entry
{
    std::map<StripeId,struct cache_entry*>      *map;
    //uint64_t            versioncounter;
    pthread_mutex_t     smutex;
    //time_t              last_modified;
    //uint32_t           dirtycnter;
    bool                dirty;
    bool                busy;
};

struct cache_entry
{
    struct data_object *current;
    std::map<uint64_t, struct data_object*> *unconfirmed;
    std::map<uint64_t, struct data_object*> *confirmed;  
    //uint64_t versioncnt;
    uint32_t versionvec[default_groupsize+1];
    uint32_t *myvec_entry;
    pthread_mutex_t entry_mutex;
    bool dirty;
};

class doCache {
public:
    doCache(Logger *p_log, Filestorage *p_fileio, serverid_t servid);    
    doCache(const doCache& orig);
    virtual ~doCache();
    int garbage_collection();
    
    int get_next_version_vector(InodeNumber inum, StripeId sid, uint32_t *versionvec, uint8_t vindex);
    int reset_version_vector(InodeNumber inum , StripeId sid, uint8_t vindex);
    int get_entry(InodeNumber inum, StripeId sid, struct data_object **p_out);
    int get_unconfirmed(InodeNumber inum, StripeId sid, struct data_object **p_out);
    int set_entry(InodeNumber inum, StripeId sid, struct data_object *p_in);
    int parityUnconfirmed(InodeNumber inum, StripeId sid, struct data_object *p_in);
    int parityConfirm(InodeNumber inum, StripeId sid, uint64_t version);
    
    
private:
    Logger *log;
    Filestorage *p_fileio;
    pthread_mutex_t mutex;
    
    std::map<InodeNumber,struct stripe_cache_entry*> *p_cache;
    serverid_t id;
    Libraid4 *p_raid;
    uint64_t initialize_cache_entry(std::map<InodeNumber,struct stripe_cache_entry*>::iterator cacheit, StripeId sid);
    struct stripe_cache_entry* create_stripe_cache_entry();
};

#endif	/* DOCACHE_H */

