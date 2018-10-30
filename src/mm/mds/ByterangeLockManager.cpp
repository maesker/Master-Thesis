/* 
 * File:   ByterangeLockManager.cpp
 * Author: markus
 * 
 * Created on 11. Januar 2012, 09:43
 */

#include "mm/mds/ByterangeLockManager.h"

ByterangeLockManager::ByterangeLockManager(Logger *p_log) {
    this->locks = new struct lock_register;
    this->log = p_log;
    this->locks->globallock_mutex = PTHREAD_MUTEX_INITIALIZER;
}

ByterangeLockManager::ByterangeLockManager(const ByterangeLockManager& orig) {
}

ByterangeLockManager::~ByterangeLockManager() 
{
    pthread_mutex_destroy(&locks->globallock_mutex);
    std::map<InodeNumber,struct file_lock*>::iterator it = locks->flocks.begin();
    for (it;it!=locks->flocks.end();it++)
    {
        std::map<uint64_t,struct byterange_lock*>::iterator it2 = it->second->blocks.begin();
        for (it2;it2!=it->second->blocks.end();it2++)
        {
            delete it2->second;
        }
        delete it->second;
        pthread_mutex_destroy(&it->second->filelock_mutex);
    }    
    delete locks;
}

int ByterangeLockManager::lockObject_init(InodeNumber *inum, ClientSessionId *csid, uint64_t *start, uint64_t *end, time_t *expires)
{
    int ret = -1;
    time_t exp =  time(NULL) + BR_LOCK_EXPIRATION_TIME_IN_SEC;
    ret = lockObject(inum,csid,start,end,&exp);
    *expires = exp;
    return ret;
}


int ByterangeLockManager::lockObject(InodeNumber *inum, ClientSessionId *csid, uint64_t *start, uint64_t *end, time_t *expires)
{
    int ret = -1;
    log->debug_log("start:inum:%llu",*inum);
    pthread_mutex_lock(&(this->locks->globallock_mutex));
    std::map<InodeNumber,struct file_lock*>::iterator it = this->locks->flocks.find(*inum);
    if(it == this->locks->flocks.end())
    {
        struct file_lock *pfl = new struct file_lock;
        this->locks->flocks.insert(std::pair<InodeNumber,struct file_lock*>(*inum,pfl));
        it = this->locks->flocks.find(*inum);
        it->second->filelock_mutex = PTHREAD_MUTEX_INITIALIZER;
    }
    struct file_lock *p_fl = it->second;
    pthread_mutex_unlock(&(this->locks->globallock_mutex));
    int loop=0;
    int clockflag=0;
    int bwretries=1000;
    int bwretries_limit = bwretries*0.7;
    while (loop<bwretries)
    {
        loop++; // do not loop...
        pthread_mutex_lock(&p_fl->filelock_mutex);
        log->debug_log("got mutex");
        if (isLockable(p_fl,*start,*end))
        {
            if (loop>bwretries_limit || clockflag>1)
            {
                log->debug_log("create lock");
                struct byterange_lock *p_block = new struct byterange_lock;
                p_block->end = *end;
                p_block->owner = *csid;
                *expires = time(NULL) + BR_LOCK_EXPIRATION_TIME_IN_SEC;
                p_block->expires = *expires;
                pair<map<uint64_t,struct byterange_lock*>::iterator,bool> re = p_fl->blocks.insert(std::pair<uint64_t,struct byterange_lock*>(*start,p_block));
                if (re.second==false)
                {
                    log->debug_log("failed to lock object:start:%llu",re.first->first);
                }
                else
                {
                    log->debug_log("locked from %llu to %llu",*start,*end);
                    ret = 0;
                }
                loop=bwretries;
                // successfully locked.
            }
            else
            {
                clockflag++;
            }
            pthread_mutex_unlock(&(p_fl->filelock_mutex));        
        }        
        else
        {
            pthread_mutex_unlock(&(p_fl->filelock_mutex));        
            *expires=0;
            log->debug_log("not lockable");
            usleep(50);            
            clockflag=0;
        }
    }
    log->debug_log("done.");
    return ret;
}

int ByterangeLockManager::importLock(struct lock_exchange_struct *p_lock)
{
    return lockObject(&p_lock->inum, &p_lock->owner, &p_lock->start, &p_lock->end, &p_lock->expires);
}

int ByterangeLockManager::releaseLockObject(InodeNumber inum, uint64_t start)
{
    pthread_mutex_lock(&(this->locks->globallock_mutex));
    std::map<InodeNumber,struct file_lock*>::iterator it = this->locks->flocks.find(inum);
    if(it != this->locks->flocks.end())
    {
        it->second->blocks.erase(start);
        log->debug_log("removed lock:inum:%u, start:%llu",inum,start);
    }    
    pthread_mutex_unlock(&(this->locks->globallock_mutex));
}

int ByterangeLockManager::updateLockObject(InodeNumber inum, uint64_t start, uint64_t end)
{
    int ret = -1;
    std::map<InodeNumber,file_lock*>::iterator it;
    
    it = this->locks->flocks.find(inum);
    if(it != this->locks->flocks.end())
    {
        struct file_lock *p_fl = it->second;
        std::map<InodeNumber,byterange_lock*>::iterator itbrl;
        pthread_mutex_lock(&(p_fl->filelock_mutex));
        itbrl = p_fl->blocks.find(start);
        if (itbrl != p_fl->blocks.end())
        {
            if (end=0)
            {   
                // remove entry
                free(itbrl->second);
                p_fl->blocks.erase(itbrl);
                ret = 0;
            }
            else if (isLockable(p_fl,itbrl->second->end,end)) // check if end can be extended
            {
                itbrl->second->end = end;
                itbrl->second->expires = time(NULL) + BR_LOCK_EXPIRATION_TIME_IN_SEC;
                ret = 0;
                log->debug_log("locked:expires at %llu",itbrl->second->expires );
                // successfully locked.
            }
            
        }
        pthread_mutex_unlock(&(p_fl->filelock_mutex));        
    }
    return 0;
}

bool ByterangeLockManager::isLockable(struct file_lock *p_fl, uint64_t start, uint64_t end)
{
    bool ret=false;
    log->debug_log("check: %llu to %llu",start,end);
    std::map<uint64_t,byterange_lock*>::iterator it, itb;
    it = p_fl->blocks.begin();
    if(it == p_fl->blocks.end())
    {
        log->debug_log("no locks existing. return true");
        ret=true;
    }    
    else
    {
        it = p_fl->blocks.begin();
        while (it != p_fl->blocks.end())
        {
            log->debug_log("existing lock found with start:%llu, end:%llu",it->first,it->second->end);
            if (it->first >= start ||  (start <= it->second->end && it->second->end >= start ) )    // range conflict
            {
                if (it->second->expires < time(NULL))
                {
                    // conflict with valid lock
                    log->debug_log("conflict detected.");
                    return false;
                }
            }
            it++;
            if(it->first > end) break;
        }
        ret = true; // no conflict detected
    }
    log->debug_log("ok");
    return ret;
}      


//ByterangeLockManager::clientCacheInsert()