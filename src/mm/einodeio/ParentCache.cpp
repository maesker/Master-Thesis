#include <unordered_map>
#include "mm/einodeio/ParentCache.h"

/**
 * @brief   Constructor of ParentCache class
 *
 * @param[in]   max_size Max number of cached elements
 */
ParentCache::ParentCache(long unsigned int max_size)
{
    this->max_size = max_size;
    this->parents = new std::unordered_map<InodeNumber, ParentCacheEntry>();
    pthread_mutex_init(&(this->parents_lock), NULL);
    this->profiler = Pc2fsProfiler::get_instance();
}

/**
 * @brief   Destructor of ParentCache class
 */
ParentCache::~ParentCache()
{
    delete this->parents;
}

/**
 * @brief   Get parent inode number of $inode
 *
 * @param[in]   inode Inode number
 * @retval  Parent inode number of $inode
 * @throws  ParentCacheException
 */
ParentCacheEntry ParentCache::get_parent(InodeNumber inode)
{
	profiler->function_start();
	profiler->function_sleep();
    pthread_mutex_lock(&(this->parents_lock));
    profiler->function_wakeup();

    ParentCacheEntry result;

    std::unordered_map<InodeNumber, ParentCacheEntry>::const_iterator it;

    if ((it = this->parents->find(inode)) == this->parents->end())
    {
        pthread_mutex_unlock(&(this->parents_lock));
        profiler->function_end();
        throw ParentCacheException("Cannot find parent inode");
    }
    else
    {
        result = it->second;
    }

    pthread_mutex_unlock(&(this->parents_lock));
    profiler->function_end();
    return result;
}

/**
 * @brief   Add parent inode number of $inode to the cache
 *
 * @param[in]   inode Inode number
 * @param[in]   parent Parent inode number of $inode
 */
void ParentCache::set_parent(InodeNumber inode, InodeNumber parent, off_t offset)
{
	profiler->function_start();
	profiler->function_sleep();
    pthread_mutex_lock(&(this->parents_lock));
    profiler->function_wakeup();
    std::unordered_map<InodeNumber, ParentCacheEntry>::const_iterator it;
    ParentCacheEntry entry;

    entry.parent = parent;
    entry.offset = offset;

    if (((it = this->parents->find(inode)) != this->parents->end()))
    {
        // Delete value, if still exists
        this->parents->erase(it);
    }

    this->parents->insert(std::unordered_map<InodeNumber, ParentCacheEntry>::value_type(inode, entry));

    pthread_mutex_unlock(&(this->parents_lock));
    profiler->function_end();
}

/**
 * @brief 	Delete parent inode number from cache
 *
 * @param[in]	inode Inode number
 */
void ParentCache::delete_parent(InodeNumber inode)
{
	profiler->function_start();
	profiler->function_sleep();
    pthread_mutex_lock(&(this->parents_lock));
    profiler->function_wakeup();
    std::unordered_map<InodeNumber, ParentCacheEntry>::const_iterator it;

    if (((it = this->parents->find(inode)) != this->parents->end()))
    {
        // Delete value, if still exists
        this->parents->erase(it);
    }

    pthread_mutex_unlock(&(this->parents_lock));
    profiler->function_end();
}
