/**
 * @file JournalChunk.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.06.2011
 */
#ifndef JOURNALCHUNK_H_
#define JOURNALCHUNK_H_

#include <map>
#include <vector>
#include <pthread.h>

#include "mm/storage/StorageAbstractionLayer.h"
#include "mm/journal/Operation.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

#define CHUNK_EXTENSION  ".chunk"

using namespace std;

class JournalChunk
{
public:
	JournalChunk(){};
	JournalChunk(int32_t chunk_id, const string& prefix, InodeNumber journal_id);
	virtual ~JournalChunk();

	int32_t add_operation(Operation* operation);
	int32_t add_write_operation(Operation* operation, StorageAbstractionLayer& sal);

	int32_t get_chunk_id();

	bool get_operations(uint64_t operation_id, vector<Operation>& operations);
	bool get_operation(uint64_t operation_id, int operation_number, Operation& operation);

	int write_chunk(StorageAbstractionLayer& sal);
	int read_chunk(StorageAbstractionLayer& sal);

	int32_t delete_chunk(StorageAbstractionLayer& sal);

	string& get_identifier();
	void set_identifier(const string& identifier);

	bool is_modified();
	void set_modified(bool b);

	bool is_closed();
	void set_closed(bool b);

	int32_t count_inodes(InodeNumber inode_id);

	int32_t get_inode_operations(InodeNumber inode_id, vector<Operation*>& inode_list);

	int32_t size();

	multimap<InodeNumber, Operation*>& get_inode_map();
	vector<Operation*>& get_operations();

	void remove_inode(InodeNumber inode_id);
	void remove_operation(uint64_t operation_id);


private:
	bool modified;
	int chunk_id;
	InodeNumber journal_id;
	string identifier;

	bool closed; /* Identicates whether the chunk is closed or not. Only a closed chunk can be removed by the write back process. */
	mutable pthread_mutex_t mutex;
	vector<Operation*> operations;
	multimap<uint64_t, Operation*> operation_map;
	multimap<InodeNumber, Operation*> inode_map;

    Pc2fsProfiler* ps_profiler;
};

#endif /* JOURNALCHUNK_H_ */
