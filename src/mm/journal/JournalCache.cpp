/**
 * @file JournalCache.cpp
 * @class JournalCache
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 *
 * @brief The JournalCache represents the cache of all journaled operations.
 * The JournalCache holds all JournalChunk, the chunk objects are containing the journal operations.
 * The Cache provides fast access to the operations for the write back process and other modules.
 */

#include "mm/journal/JournalCache.h"
#include "mm/journal/FailureCodes.h"

/**
 * @brief Default constructor of the JournalCache.
 * Initializes the mutex.
 */
JournalCache::JournalCache()
{
	mutex = PTHREAD_MUTEX_INITIALIZER;
	ps_profiler = Pc2fsProfiler::get_instance();
}

/**
* @brief Destructor of JournalCache.
* Removes all chunks and destroys the mutex.
*/
JournalCache::~JournalCache()
{
	for (it = chunk_map.begin(); it != chunk_map.end(); it++)
	{
		delete it->second;
	}
	pthread_mutex_destroy(&mutex);
}

/**
 * @brief Adds the journal chunk to the cache.
 * @param chunk Pointer to the chunk object.
 * @return 0 if the operation was successful, -1 if a chunk with the same id is already in the cache.
 */
int32_t JournalCache::add(JournalChunk* chunk)
{
	int32_t rtrn = 0;
	pthread_mutex_lock(&mutex);

	ps_profiler->function_start();
	it = chunk_map.find(chunk->get_chunk_id());
	if (it != chunk_map.end())
		rtrn = -1;
	else
		chunk_map.insert(pair<int32_t, JournalChunk*> (chunk->get_chunk_id(), chunk));
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();

	return rtrn;
}

/**
 * @brief Removes a chunk from the cache.
 * @param chunk_id The id of the chunk to remove.
 */
void JournalCache::remove(int32_t chunk_id)
{
	pthread_mutex_lock(&mutex);

	ps_profiler->function_start();

	it = chunk_map.find(chunk_id);

	if (it != chunk_map.end())
	{
		JournalChunk* jc = it->second;
		chunk_map.erase(it);

		delete jc;
	}

	ps_profiler->function_end();

	pthread_mutex_unlock(&mutex);
}

/**
 * @brief Gets the chunk with the given id.
 * @param chunk_id The id of the chunk.
 * @return Pointer to the chunk, if a chunk with the given id is holding inside the cache, otherwise NULL.
 */
JournalChunk* JournalCache::get(int32_t chunk_id)
{
	JournalChunk* chunk = NULL;
	pthread_mutex_lock(&mutex);

	ps_profiler->function_start();

	it = chunk_map.find(chunk_id);
	if (it != chunk_map.end())
		chunk = it->second;
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();

	return chunk;
}

/**
 * @brief Gets the the of the journal cache.
 * @return The number of chunks holding by the cache.
 */
int32_t JournalCache::size() const
{
	int32_t size;
	pthread_mutex_lock(&mutex);
	size = chunk_map.size();
	pthread_mutex_unlock(&mutex);
	return size;
}

/**
 * @brief Removes all consecutive chunks which are empty AND closed.
 * @param sal Pointer to the @see StorageAbstractionLayer object.
 */
void JournalCache::clear_closed(StorageAbstractionLayer *sal)
{
	pthread_mutex_lock(&mutex);

	ps_profiler->function_start();

	for (it = chunk_map.begin(); it != chunk_map.end();)
	{
		if (it->second->is_closed() && it->second->size() == 0)
		{
			it->second->delete_chunk(*sal);
			delete it->second;
			chunk_map.erase(it++);
		}
		else
			break;
	}

	ps_profiler->function_end();

	pthread_mutex_unlock(&mutex);
}

/**
 * @brief Gets the last operation with the given id from the cache.
 * @param[out] target The operation which will contain a copy of the desired operation.
 * @param[int] operation_id The id of the operation.
 * @param[in] chunk_id The location of the operation.
 * @param[in] operatin_number The numebr of the operation.
 * @return 0 if the operation was successful, otherwise an error code is returned.
 * NotInCache The cache do not contain any chunk with the given chunk id.
 * InvalidOperation The cache do not contain the operation with the given id, or the operation number is invalid.
 */
int32_t JournalCache::get_operation(Operation& target, uint64_t operation_id, int32_t chunk_id, int32_t operation_number)
{
	int32_t rtrn = 0;
	pthread_mutex_lock(&mutex);

	ps_profiler->function_start();

	if ((it = chunk_map.find(chunk_id)) == chunk_map.end())
	{
		rtrn = JournalFailureCodes::NotInCache;
	}
	else
	{
		if( !it->second->get_operation(operation_id, operation_number, target))
		{
			rtrn = JournalFailureCodes::InvalidOperation;
		}
	}

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Gets all operation with the given id from the chunk with the specified chunk id.
 * @param[in] chunk_id The location of the operation.
 * @param[in] operation_id The id of the operation.
 * @param[out] operations The vector in which all the operations will be written.
 * @return 0 if the operation was successful, otherwise an error code is returned.
 * NotInCache The cache do not contain any chunk with the given chunk id.
 */
int32_t JournalCache::get_operations(int32_t chunk_id, uint64_t operation_id, vector<Operation>& operations)
{
	int32_t rtrn = 0;
	pthread_mutex_lock(&mutex);

	ps_profiler->function_start();

	if ((it = chunk_map.find(chunk_id)) == chunk_map.end())
	{
		rtrn = JournalFailureCodes::NotInCache;
	}
	else
	{
		if( it->second->get_operations(operation_id, operations))
			rtrn = JournalFailureCodes::InvalidOperation;
	}
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}
