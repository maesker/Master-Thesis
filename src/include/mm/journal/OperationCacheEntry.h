/**
 * @file OperationCacheEntry.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.07.2011
 */


/*
 * This class represents one entry inside the operation cahce.
 *
 * An operation cache entry holds all not written journal operations, the status and the location,
 * of an operation id.
 *
 * @author Viktor Gottfried
 */

#ifndef OPERATIONCACHEENTRY_H_
#define OPERATIONCACHEENTRY_H_

#include <map>
#include <vector>
#include <set>

#include "CommonJournalTypes.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"


using namespace std;

class OperationCacheEntry
{
public:
	OperationCacheEntry(){};
	OperationCacheEntry( uint64_t id, int32_t chunk_number, OperationStatus status);
	OperationCacheEntry( uint64_t id, int32_t chunk_number, int32_t operation_number, OperationStatus status);
	virtual ~OperationCacheEntry(){};

	uint64_t get_operation_id() const;
	void set_operation_id(uint64_t id);

	void add(int32_t chunk_number, OperationStatus status);
	int32_t add(int32_t chunk_number, int32_t operation_number, OperationStatus status);

	int32_t get_next_number() const;

	int32_t get_last_chunk() const;
	int32_t get_all_chunks(set<int32_t>& chunk_set) const;
	void get_chunk_set(set<int32_t>& chunk_set) const;
	int32_t count() const;

	OperationStatus get_last_status() const;
	OperationMapping get_last() const;
private:
	/* The id of the operation that is holding by the entry. */
	uint64_t operation_id;
	int32_t next_operation_number;
	/* Map operation number to the corresponding OperationMapping object.*/
	map<int32_t, OperationMapping> operation_map;

	Pc2fsProfiler* ps_profiler;
};

#endif /* OPERATIONCACHEENTRY_H_ */
