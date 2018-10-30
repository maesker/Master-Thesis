/**
 * @file WriteBackProvider.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 */

#ifndef WRITEBACKPROVIDER_H_
#define WRITEBACKPROVIDER_H_

#include "mm/journal/JournalCache.h"
#include "mm/journal/JournalChunk.h"
#include "mm/journal/InodeCache.h"
#include "mm/journal/OperationCache.h"
#include "mm/storage/StorageAbstractionLayer.h"
#include "mm/einodeio/EmbeddedInodeLookUp.h"
#include "mm/einodeio/EInodeIOException.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

#include <set>
#include <map>

using namespace std;

class WriteBackProvider
{
public:
	WriteBackProvider();
	virtual ~WriteBackProvider();

	void start_write_back();
	void start_full_write_back();
	bool write_back_directory(InodeNumber parent_number);

	void set_journal_cache(JournalCache* journal_cache);
	void set_chunk_map(map<int32_t, JournalChunk*>* chunk_map);
	void set_operation_cache(OperationCache* operation_cache);
	void set_inode_cache(InodeCache* inode_cache);

	void set_sal(StorageAbstractionLayer* sal);
	void set_einode_io(EmbeddedInodeLookUp* einode_io);
private:

	int32_t handle_smart_delete(InodeCacheParentEntry* pe);
	int32_t handle_smart_write(InodeCacheParentEntry* pe);
	bool access_check(InodeCacheParentEntry* pe);
	void remove_from_journal(InodeNumber inode_number, const set<int32_t> &chunk_set);
	void clean_up();

	bool check_written(Operation* operation);

	set<int32_t> marked_chunks; /*< Indentifies all modified chunks */
	set<InodeNumber> inodes_to_write;
	set<uint64_t> operations_to_write;
	/*
	 * ####################################################
	 * Member variables set by the journal.
	 * ####################################################
	 */
	JournalCache* journal_cache;
	OperationCache* operation_cache; /*< Open operations of the journal. */
	InodeCache* inode_cache;       /*< Inode cache of the journal. */

	string journal_prefix;	/*< The prefix of the journal. */
	StorageAbstractionLayer* sal; /*< Pointer to the Storage Abstraction Layer object. */
	EmbeddedInodeLookUp* einode_io; /*< Pointer to the einode io object. */
	/* ##################################################### */

	Pc2fsProfiler* ps_profiler;
};

#endif /* WRITEBACKPROVIDER_H_ */
