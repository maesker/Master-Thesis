/**
 * @file InodeCache.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 */

#ifndef INODECACHE_H_
#define INODECACHE_H_


#include <map>
#include <set>
#include <pthread.h>

#include "InodeCacheParentEntry.h"
#include "Operation.h"
#include "mm/einodeio/EmbeddedInodeLookUp.h"
#include "logging/Logger.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

using namespace std;

class InodeCache
{
public:
	InodeCache();
	virtual ~InodeCache();


	int32_t add_to_cache(InodeNumber inode_number, InodeNumber parent_id, const EInode& einode);

	int32_t get_last_change(InodeNumber inode_number, OperationType& type) const;

	int32_t remove_entry(InodeNumber inode_id);

	void clear();
	
	int32_t move_inode(InodeNumber new_parent_id, FsObjectName *new_name, InodeNumber old_parent_id,
			FsObjectName* old_name, EmbeddedInodeLookUp* einode_io, int32_t chunk_id);

	int32_t rename(InodeNumber parent_id, FsObjectName *new_name, FsObjectName *old_name, EmbeddedInodeLookUp* einode_io, int32_t chunk_id);

	int32_t update_inode_cache(const Operation* operation, int32_t chunk_id);

	const CacheStatusType get_einode(InodeNumber inode_id, EInode& einode, EmbeddedInodeLookUp* einode_io);

	void get_dirty_map(map<InodeNumber, InodeCacheParentEntry*> &dirty_map) const;

	CacheStatusType lookup_by_object_name(FsObjectName* name, InodeNumber parent_id,
			InodeNumber& inode_number, EmbeddedInodeLookUp* einode_io);

	int32_t cache_dir(InodeNumber parent_id, EmbeddedInodeLookUp* einode_io);

	int32_t read_dir(InodeNumber parent_id, ReaddirOffset offset, ReadDirReturn& rdir_result) const;

	void about_cache(vector<AccessData>& info) const;

	InodeCacheParentEntry* get_cache_parent_entry(InodeNumber inode_number);
	InodeNumber get_parent(InodeNumber inode_number) const;

	void set_einode(EmbeddedInodeLookUp* einode_io);

private:
	InodeNumber determine_parent(InodeNumber inode_number) const;
	int32_t fetch_from_storage(EInode& einode, InodeNumber inode_number, EmbeddedInodeLookUp* einode_io);
	int32_t fetch_from_storage(EInode& einode, InodeNumber parent_id, FsObjectName* name, EmbeddedInodeLookUp* einode_io);
	int32_t fetch_from_storage(EInode& einode, InodeNumber parent_id, InodeNumber inode_number, EmbeddedInodeLookUp* einode_io);

	int32_t cache_dir(InodeCacheParentEntry* pe, map<InodeNumber, InodeNumber>& temp_parent_map, EmbeddedInodeLookUp* einode_io);

	void add_to_parent_map(map<InodeNumber, InodeNumber>& m);
	void remove_fron_parent_map(set<InodeNumber>& s);


	map<InodeNumber, InodeCacheParentEntry*> cache_map;
	map<InodeNumber, InodeNumber> parent_cache_map;

	mutable pthread_mutex_t mutex;
	EmbeddedInodeLookUp* einode_io;
	Logger* log;
	Pc2fsProfiler* ps_profiler;
};

#endif /* INODECACHE_H_ */
