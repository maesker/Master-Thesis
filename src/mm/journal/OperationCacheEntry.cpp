/**
 * @file Operation.cpp
 * @class Operation
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @date 01.07.2011
 *
 * @brief This class represents one entry inside the operation cache.
 *
 * An operation cache entry holds all not written journal operations, the status and the location,
 * of an operation id.
 */

#include "mm/journal/OperationCacheEntry.h"

/**
 * @brief Constructor of OperationCacheEntry.
 * @param id The operation id.
 * @param chunk_number The chunk number, identifies the location of this operation in the journal.
 * @param status The status of the operation.
 */
OperationCacheEntry::OperationCacheEntry(uint64_t id, int32_t chunk_number, OperationStatus status)
{
	ps_profiler = Pc2fsProfiler::get_instance();


	ps_profiler->function_start();
	operation_id = id;
	next_operation_number = 1;
	add( chunk_number, status );
	ps_profiler->function_end();
}

/**
 * @brief Constructor of OperationCacheEntry.
 * @param id The operation id.
 * @param chunk_number The chunk number, identifies the location of this operation in the journal.
 * @param operation_number The number of the operation.
 * @param status The status of the operation.
 */
OperationCacheEntry::OperationCacheEntry(uint64_t id, int32_t chunk_number, int32_t operation_number,
		OperationStatus status)
{
	ps_profiler = Pc2fsProfiler::get_instance();

	ps_profiler->function_start();
	operation_id = id;
	next_operation_number = operation_number;
	add( chunk_number, operation_number, status );
	ps_profiler->function_end();
}

/**
 * @brief Get the operation id of the cache entry.
 * @return The operation id of the cache entry.
 */
uint64_t OperationCacheEntry::get_operation_id() const
{
	return operation_id;
}

/**
 * qbrief Set the operation id of the cache entry.
 * @param id The operation id of the cache entry.
 */
void OperationCacheEntry::set_operation_id(uint64_t id)
{
	this->operation_id = id;
}

/**
 * @brief Add a new information about the operation to cache entry.
 * The operation number will be automatically calculated.
 * @param chunk_number The chunk number.
 */
void OperationCacheEntry::add(int32_t chunk_number, OperationStatus status)
{
	ps_profiler->function_start();

	OperationMapping om;
	om.chunk_number = chunk_number;
	om.operation_number = next_operation_number;
	om.status = status;
	operation_map.insert( pair<int32_t, OperationMapping>( next_operation_number, om ) );
	next_operation_number++;

	ps_profiler->function_end();
}

/**
 * @brief Add a new information about the operation to cache entry.
 * @param chunk_number The chunk number.
 * @param operation_number The number of the operation.
 * @return 0 if addition was successful, 1 if an entry with the operation number already exists.
 */
int32_t OperationCacheEntry::add(int32_t chunk_number, int32_t operation_number, OperationStatus status)
{
	ps_profiler->function_start();
	if ( operation_map.count( operation_number ) != 0 )
	{
		ps_profiler->function_end();
		return 1;
	}
	OperationMapping om;
	om.chunk_number = chunk_number;
	om.operation_number = operation_number;
	om.status = status;

	if ( operation_number >= next_operation_number )
		next_operation_number = operation_number + 1;

	operation_map.insert( pair<int32_t, OperationMapping>( operation_number, om ) );

	ps_profiler->function_end();
	return 0;
}

/**
 * @brief Gets the last chunk number of the operation.
 * @return The last chunk number. 0 if the cache is empty.
 */
int32_t OperationCacheEntry::get_last_chunk() const
{
	ps_profiler->function_start();

	int32_t rtrn = 0;

	if ( operation_map.empty() )
	{
		ps_profiler->function_end();
		return 0;
	}
	// access the last element in the map and return the chunk number

	rtrn = (*(--operation_map.end())).second.chunk_number;

	ps_profiler->function_end();
	return rtrn;
}

/*
 * @brief Get all chunk if the operation.
 * @param[out] The list that will contain the chunks.
 * @return The number of chunks, that were put into the list.
 */
int32_t OperationCacheEntry::get_all_chunks(set<int32_t>& chunk_set) const
{
	ps_profiler->function_start();

	int32_t chunks = operation_map.size();
	map<int32_t, OperationMapping>::const_iterator cit;
	for ( cit = operation_map.begin(); cit != operation_map.end(); ++cit )
	{
		chunk_set.insert( (*cit).second.chunk_number );
	}

	ps_profiler->function_end();
	return chunks;
}

/**
 * @brief Gets the next operation number.
 * @return The next operation number.
 */
int32_t OperationCacheEntry::get_next_number() const
{
	return next_operation_number;
}

/**
 * @brief Counts the number of operations inside the cache entry.
 * @return The number of operations.
 */
int32_t OperationCacheEntry::count() const
{
	return operation_map.size();
}

/**
 * qbrief Gets the last status of the operation.
 * @return The last operation status.
 */
OperationStatus OperationCacheEntry::get_last_status() const
{
	return operation_map.rbegin().base()->second.status;
}

/**
 * @brief Gets the set of all chunk id, that containing information about this operation.
 * @param[out] chunk_list The list where the chunk id would be written.
 */
void OperationCacheEntry::get_chunk_set(set<int32_t>& chunk_set) const
{
	ps_profiler->function_start();

	map<int32_t, OperationMapping>::const_iterator cit;
	for ( cit = operation_map.begin(); cit != operation_map.end(); cit++ )
	{
		chunk_set.insert( cit->second.chunk_number );
	}
	ps_profiler->function_end();
}

/**
 * @brief Gets the last information of the operation.
 * @return The object that will contain the last added information.
 */
OperationMapping OperationCacheEntry::get_last() const
{
	return operation_map.rbegin().base()->second;
}
