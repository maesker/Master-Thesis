/**
 * @file JournalCache.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 */

#ifndef JOURNALCACHE_H_
#define JOURNALCACHE_H_

#include <map>
#include <pthread.h>

#include "mm/journal/JournalChunk.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

using namespace std;

class JournalCache
{
public:
	JournalCache();
	virtual ~JournalCache();

	void clear_closed(StorageAbstractionLayer *sal);
	int32_t add(JournalChunk* chunk);
	void remove(int32_t chunk_id);
	JournalChunk* get(int32_t chunk_id);
	int32_t size() const;
	int32_t get_operation(Operation& target, uint64_t operation_id, int32_t chunk_id, int32_t operation_number);
	
	int32_t get_operations(int32_t chunk_id, uint64_t operation_id, vector<Operation>& operations);

private:
	/* Defines the journal cache */
	map<int32_t, JournalChunk*> chunk_map;
	map<int32_t, JournalChunk*>::iterator it;

	mutable pthread_mutex_t mutex;

	Pc2fsProfiler* ps_profiler;
};

#endif /* JOURNALCACHE_H_ */
