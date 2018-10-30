/**
 * @file Operation.cpp
 * @class Operation
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @date 01.05.2011
 *
 * @brief This class represents the operation cache.
 *
 * The operation cache holds all not written back operations.
 * All methods provided by this class are thread safe.
 */

#include <iostream>

#include "mm/journal/OperationCache.h"



/**
 * @brief Default constructor of the OperationCache.
 * Initializes the mutex.
 */
OperationCache::OperationCache()
{
	mutex = PTHREAD_MUTEX_INITIALIZER;
	ps_profiler = Pc2fsProfiler::get_instance();
}

/**
 * @brief Destructor of OperationCache.
 * Destroys the mutex.
 */
OperationCache::~OperationCache()
{
	pthread_mutex_destroy(&mutex);
}


/**
 * @brief Adds a entry to the cache.
 * @parma operation_id The id of the operation entry.
 * @param chunk_number The chunk number.
 * @param status The last operation status.
 */
void OperationCache::add(uint64_t operation_id, int32_t chunk_number, OperationStatus status)
{
	ps_profiler->function_start();

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();


	it = cache_map.find(operation_id);

	// check if operation is already in the cache
	if(it == cache_map.end())
	{
		OperationCacheEntry opc(operation_id, chunk_number, status);
		cache_map.insert(pair<uint64_t, OperationCacheEntry>(operation_id, opc));
	}
	else
		(*it).second.add(chunk_number, status);
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}


/**
 * @brief Adds a entry to the cache.
 * @parma operation_id The id of the operation entry.
 * @param operation_number The operation number.
 * @param chunk_number The chunk number.
 * @param status The last operation status.
 */
void OperationCache::add(uint64_t operation_id, int32_t operation_number, int32_t chunk_number, OperationStatus status)
{

	ps_profiler->function_start();

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	it = cache_map.find(operation_id);
	// check if operation is already in the cache
	if(it == cache_map.end())
	{
		OperationCacheEntry opc(operation_id, chunk_number, status);
		cache_map.insert(pair<uint64_t, OperationCacheEntry>(operation_id, opc));
	}
	else
		(*it).second.add(chunk_number, operation_number, status);

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}

/**
 * @brief Gets the last chunk of an operation.
 * @param operation_id The id of the operation.
 * @return The last chunk number of the operation. -1 the cache do not contain the operation with the given id.
 */
int32_t OperationCache::get_last_chunk(uint64_t operation_id) const
{
	ps_profiler->function_start();
	int32_t rtrn = -1;


	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();


	cit = cache_map.find(operation_id);
	if(cit != cache_map.end())
		rtrn = (*cit).second.get_last_chunk();

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Gets all chunks of an operation.
 * @param[in] operation_id The id of the operation.
 * @param[out] chunk_set The set to put all the chunk numbers.
 * @return The number of the chunks, -1 if the cache do not contain any entries with the given id.
 */
int32_t OperationCache::get_all_chunks(uint64_t operation_id, set<int32_t>& chunk_set) const
{
	ps_profiler->function_start();
	int32_t rtrn = -1;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();


	cit = cache_map.find(operation_id);
	if(cit != cache_map.end())
		rtrn = (*cit).second.get_all_chunks(chunk_set);

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();

	return rtrn;
}

/**
 * @brief Removes the entry with the specified operation id from the cache.
 * @param operation_id The id of the operation.
 * return 0 if the entry was removed, -1 if the cache do not contain any entries with the given id.
 */
int32_t OperationCache::remove(uint64_t operation_id)
{
	ps_profiler->function_start();
	int32_t rtrn = -1;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();


	it = cache_map.find(operation_id);
	if(it != cache_map.end())
	{
		cache_map.erase(it);
		rtrn = 0;
	}

	ps_profiler->function_end();
	pthread_mutex_unlock(&mutex);
	return rtrn;
}

/**
 * @brief Counts the operations with the specified id inside the cache.
 * @param operation_id The id of the operation.
 * @return The number of operations.
 */
int32_t OperationCache::count(uint64_t operation_id) const
{
	ps_profiler->function_start();

	int32_t rtrn = 0;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();



	cit = cache_map.find(operation_id);
	if(cit != cache_map.end())
		rtrn = (*cit).second.count();

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Gets the next operation number of the operation with the given id.
 * @param operation_id The id of the operation.
 * @return The next opration number, 1 is returned if the cache do not contain the operation with the given id.
 */
int32_t OperationCache::get_next_number(uint64_t operation_id) const
{
	ps_profiler->function_start();
	int32_t rtrn = 1;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	cit = cache_map.find(operation_id);
	if(cit != cache_map.end())
		rtrn = (*cit).second.get_next_number();

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Gets the last status of the operation.
 * @param operation_id The id of the operation.
 * @return The last operation status.
 * InvalidStatus is returned, if the cache do not contain the operation with the given id.
 */
OperationStatus OperationCache::get_last_status(int64_t operation_id) const
{
	ps_profiler->function_start();

	OperationStatus status = OperationStatus::InvalidStatus;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();


	cit = cache_map.find(operation_id);
	if(cit != cache_map.end())
		status = (*cit).second.get_last_status();

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return status;
}

/**
 * @brief Adds an entry to the cache.
 * @param operation_id The id of the operation.
 * @param entry The entry to add to the cache.
 * @return 0 if the operation was successful, -1 if the entry with the given id is already in the cache.
 */
int32_t OperationCache::add_entry(uint64_t operation_id, const OperationCacheEntry& entry)
{
	ps_profiler->function_start();

	int32_t rtrn = -1;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();


	it = cache_map.find(operation_id);
	if( it != cache_map.end())
	{
		cache_map.insert(pair<uint64_t, OperationCacheEntry>(operation_id, entry));
		rtrn = 0;
	}

	pthread_mutex_unlock(&mutex);
	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Gets a set of committed operations.
 * Puts all committed operations into the set.
 * @param[out] set The set to store all committed operations.
 * @return
 */
int32_t OperationCache::get_operation_set(set<uint64_t>& set) const
{
	ps_profiler->function_start();
	int32_t rtrn = 0;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();


	for(cit = cache_map.begin(); cit != cache_map.end(); cit++)
	{
		if(cit->second.get_last_status() == OperationStatus::Committed)
		{
			set.insert(cit->first);
		}
	}

	pthread_mutex_unlock(&mutex);
	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Gets the list of all chunk id, that containing the operations with the given operation id.
 * @param[in] operation_id The id of the operation.
 * @param[out] chunk_list The list where the chunk id would be written.
 * @return 0 if the operation  with the given id is known and the call was successful,
 * -1 if the cache do not contain an entry with the given id.
 */
int32_t OperationCache::get_chunk_set(uint64_t operation_id, set<int32_t>& chunk_set) const
{
	ps_profiler->function_start();

	int32_t rtrn = -1;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	cit = cache_map.find(operation_id);
	if (cit != cache_map.end())
	{
		cit->second.get_chunk_set(chunk_set);
		rtrn = 0;
	}

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Gets the last information of an operation.
 * @param[in] operation_id The id of the operation.
 * @param[out] om The object that will contain the last information.
 * @return 0 if the operation was successful, -1 of the cache does not provides any entry for the operation with the given id.
 */
int32_t OperationCache::get_last(uint64_t operation_id, OperationMapping& om) const
{
	ps_profiler->function_start();

	int32_t rtrn = -1;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();


	cit = cache_map.find(operation_id);
	if(cit != cache_map.end())
	{
		om = cit->second.get_last();
		rtrn = 0;
	}
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();

	return rtrn;
}

/**
 * @brief Gets all open operations.
 * Writes the ids of all the operations which are not committed and aborted into the set.
 * @param[out] open_operations Set will hold all the open operation ids.
 */
void OperationCache::get_all_open(set<uint64_t> open_operations) const
{
	ps_profiler->function_start();

	OperationStatus status;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();


	for(cit = cache_map.begin(); cit != cache_map.end(); cit++)
	{
		status = cit->second.get_last().status;
		if(status != OperationStatus::Aborted && status != OperationStatus::Committed)
		{
			open_operations.insert(cit->first);
		}
	}

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}
