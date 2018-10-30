/**
 * @file OperationCache.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.07.2011
 */

#ifndef OPERATIONCACHE_H_
#define OPERATIONCACHE_H_


#include <map>
#include <vector>
#include <set>
#include <pthread.h>

#include "OperationCacheEntry.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

using namespace std;

class OperationCache
{
public:
	OperationCache();
	virtual ~OperationCache();

	void add(uint64_t operation_id, int32_t chunk_number, OperationStatus status);
	void add(uint64_t operation_id, int32_t operation_number, int32_t chunk_number, OperationStatus status);

	int32_t get_last_chunk(uint64_t operation_id) const;
	int32_t get_all_chunks(uint64_t operation_id, set<int32_t>& chunk_set) const;
	int32_t get_chunk_set(uint64_t operation_id, set<int32_t>& chunk_set) const;
	int32_t get_operation_set(set<uint64_t>& set) const;
	int32_t remove(uint64_t operation_id);

	int32_t count(uint64_t operation_id) const;

	int32_t get_next_number(uint64_t operation_id) const;

	OperationStatus get_last_status(int64_t operation_id) const;

	int32_t add_entry(uint64_t operation_id, const OperationCacheEntry& entry);

	int32_t get_last(uint64_t operation_id, OperationMapping& om) const;

	void get_all_open(set<uint64_t> open_operations) const;
private:
	map<uint64_t, OperationCacheEntry> cache_map;
	mutable map<uint64_t, OperationCacheEntry>::const_iterator cit;
	map<uint64_t, OperationCacheEntry>::iterator it;
	mutable pthread_mutex_t mutex;
	Pc2fsProfiler* ps_profiler;
};

#endif /* OPERATIONCACHE_H_ */
