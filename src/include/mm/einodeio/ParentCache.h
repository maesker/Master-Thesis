#ifndef PARENTCACHE_H
#define PARENTCACHE_H

#include <unordered_map>
#include <pthread.h>
#include "global_types.h"
#include "ParentCacheException.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

typedef struct {
	InodeNumber parent;
	off_t offset;
} ParentCacheEntry;

class ParentCache
{
public:
    ParentCache(unsigned long max_size);
    ~ParentCache();
    ParentCacheEntry get_parent(InodeNumber inode);
    void set_parent(InodeNumber inode, InodeNumber parent, off_t offset);
    void delete_parent(InodeNumber inode);

private:
    std::unordered_map<InodeNumber, ParentCacheEntry> *parents;
    unsigned long max_size;
    pthread_mutex_t parents_lock;
    Pc2fsProfiler *profiler;
};

#endif // PARENTCACHE_H
