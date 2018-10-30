/** 
 * @file   ByterangeLockManager.h
 * @author Markus Mäsker <maesker@gmx.net>
 *
 * @date Created on 11. Januar 2012, 09:43
 * 
 * @todo implement client cache, to quickly remove locks of disconnected clients
 */

#ifndef BYTERANGELOCKMANAGER_H
#define	BYTERANGELOCKMANAGER_H

#include <map>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

#include "global_types.h"
#include "logging/Logger.h"

#define BR_LOCK_EXPIRATION_TIME_IN_SEC 600

struct lock_exchange_struct {
    ClientSessionId owner;
    InodeNumber inum;
    uint64_t start;
    uint64_t end;
    time_t expires;
};

struct lock_register {
    std::map<InodeNumber,struct file_lock*> flocks;
    pthread_mutex_t globallock_mutex;
};

struct file_lock {
    std::map<uint64_t,struct byterange_lock*> blocks;
    pthread_mutex_t filelock_mutex;
};

struct byterange_lock {
    ClientSessionId owner;
    uint64_t end;
    time_t expires;
    
};

struct client_lock_cache_entry {
    std::map<InodeNumber,uint64_t> cl_locks;
};

struct client_file_cache_entry {
    std::map<uint64_t,uint64_t> cl_locks;
};

class ByterangeLockManager {
public:
    ByterangeLockManager(Logger *p_log);
    ByterangeLockManager(const ByterangeLockManager& orig);
    virtual ~ByterangeLockManager();
    int lockObject(InodeNumber *inum, ClientSessionId *csid, uint64_t *start, uint64_t *end, time_t *expires);
    int lockObject_init(InodeNumber *inum, ClientSessionId *csid, uint64_t *start, uint64_t *end, time_t *expires);
    int importLock(struct lock_exchange_struct *p_lock);
    
    int updateLockObject(InodeNumber inum, uint64_t start, uint64_t end);
    int releaseLockObject(InodeNumber inum, uint64_t start);
    
private:
    struct lock_register *locks;
    Logger *log;
    
    bool isLockable(struct file_lock *p_fl, uint64_t start, uint64_t end);
    //std::map<ClientSessionId,struct client_lock_cache_entry*> client_lockcache;
    
};

#endif	/* BYTERANGELOCKMANAGER_H */

