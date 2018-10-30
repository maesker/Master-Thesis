/**
 * @file Journal.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.07.2011
 */

#ifndef JOURNAL_H_
#define JOURNAL_H_
#include <list>
#include <stdlib.h>
#include <string>
#include <pthread.h>
#include <map>
#include <string>


#include "logging/Logger.h"
#include "mm/journal/JournalCache.h"
#include "mm/journal/OperationCache.h"
#include "mm/journal/InodeCache.h"
#include "mm/journal/Operation.h"
#include "mm/journal/JournalChunk.h"
#include "mm/einodeio/EmbeddedInodeLookUp.h"
#include "mm/journal/WriteBackProvider.h"
#include "mm/storage/StorageAbstractionLayer.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

using namespace std;

#define LOG_EXTENSION ".log"

class Journal
{
private:
	bool recovery_mode; /* < Flag identifies whether the journal is in recovery mode or not */
	bool writing_back; /* < Identifies whether the write back thread is running or not. */
	pthread_cond_t wbt_cond;
	mutable pthread_mutex_t wbt_mutex;
	pthread_t write_back_thread;

	mutable pthread_mutex_t mutex;

	WriteBackProvider wbc;

	/* Defines  the prefix of the journal. The prefix is always the journal id */
	string prefix;

	/* Pointer to the StorageAbstractionLayer object. */
	StorageAbstractionLayer* sal;

	/* Pointer to the EmbeddedInodeLookUp object. */
	EmbeddedInodeLookUp* inode_io;

	/* Defines the active chunk of the journal. */
	JournalChunk* active_chunk;

	/* The journal cache */
	JournalCache* journal_cache;	
	//vector<JournalChunk*>* journal_cache;

	/* The chunk map holds the same data like the journal cache, it is used for the fast access */
	//map<int32_t, JournalChunk*>* chunk_map;

	/* Cache the inodes of committed operations.*/
	InodeCache* inode_cache;

	/* Caches all distributed operations. */
	OperationCache* operation_cache;
	

	uint64_t journal_id;
	int current_chunk_id;
    Logger *log;
    Pc2fsProfiler* ps_profiler;

private:
	void handle_journal();
	static void* start_handling(void *ptr);

	int journal_operation(Operation* operation);
	int put_into_chunk(Operation* operation);

	bool check_inode(InodeNumber inode_id);

	void init(uint64_t journal_id, int32_t chunk_number);

	static void* start_write_back_thread(void *ptr);
	void handle_full_wbt();
	void handle_smart_wbt();

	void swap_pointers(void * pOne, void * pTwo);
public:

	Journal();
	Journal(uint64_t journal_id);
	Journal(uint64_t journal_id, int32_t chunk_number);
	virtual ~Journal();

	void start();
	void stop();

	int32_t get_journal_size();

	void close_journal();	

	StorageAbstractionLayer& get_sal();

	int32_t add_distributed(uint64_t id, Module module, OperationType type,
			OperationStatus status, char* data, uint32_t data_size);

	int32_t handle_distributed_create_inode(uint64_t operation_id,
			InodeNumber parent_id, const EInode& p_einode);

	int32_t handle_distributed_delete_inode(uint64_t operation_id,
			InodeNumber inode_id);

	int32_t start_migration(InodeNumber inode_id, char* data, int32_t data_size);
	int32_t finish_migration(InodeNumber inode_id);
	int32_t abort_migration(InodeNumber inode_id);

	int32_t get_last_operation(uint64_t operation_id, Operation& operation);
	int32_t get_all_operations(uint64_t operation_id,
			vector<Operation>& operations);
	void get_open_operations(set<uint64_t> open_operations) const;

	bool is_inode(InodeNumber inode_id);

	void set_journal_id(unsigned long inode);
        InodeNumber get_journal_id();

	void set_sal(StorageAbstractionLayer* sal);

	const string& get_prefix() const;

	void set_journal_cache(JournalCache* journal_cache);
	void set_inode_cache(InodeCache* inode_cache);
	void set_operation_cache(OperationCache* operation_cache);
	void set_chunk(int32_t chunk_id);

	void set_inode_io(EmbeddedInodeLookUp* inode_io);

	void run_write_back();

	/** These messages are needed by the MDS */
	int handle_mds_einode_request(InodeNumber *p_inode_number,
			struct EInode *p_einode);
	int handle_mds_create_einode_request(InodeNumber *p_parent_inode_number,
			struct EInode *p_einode);

	int handle_mds_move_einode_request(InodeNumber* new_parent_id, FsObjectName *new_name, InodeNumber* old_parent_id,
		FsObjectName* old_name);

	int handle_mds_update_attributes_request(InodeNumber *p_inode_number,
			inode_update_attributes_t *p_attrs, int *p_attribute_bitfield);
	int handle_mds_delete_einode_request(InodeNumber *p_inode_number);

	int handle_mds_readdir_request(InodeNumber *p_inode_number,
			ReaddirOffset *p_offset, struct ReadDirReturn *p_rdir);
	int handle_mds_lookup_inode_number_by_name(
			InodeNumber *p_parent_inode_number, FsObjectName *p_name,
			InodeNumber *p_result_inode);
	int handle_mds_parent_inode_number_lookup_request(
			inode_number_list_t *p_request_list,
			inode_number_list_t *p_result_list);
	int handle_mds_parent_inode_hierarchy_request(
			InodeNumber *p_inode_number,
			inode_number_list_t *p_result_list);

	int32_t handle_resolve_path(string& path, InodeNumber& inode_number);
	int32_t inode_exists(InodeNumber inode_number);
	int32_t get_path(InodeNumber inode_number, string& path);

	void set_recovery(bool b);
};


void my_einode_to_string(std::string &str, EInode *p_einode);
void print_einode(EInode &p_einode);

#endif /* JOURNAL_H_ */
