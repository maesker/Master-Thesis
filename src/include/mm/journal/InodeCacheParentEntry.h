/**
 * @file InodeCacheParentEntry.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.09.2011
 */

#ifndef INODECACHEPARENTENTRY_H_
#define INODECACHEPARENTENTRY_H_

#include <map>
#include <pthread.h>
#include <sys/time.h>

#include "mm/journal/InodeCacheEntry.h"
#include "mm/journal/Operation.h"
#include "mm/einodeio/EmbeddedInodeLookUp.h"
#include "mm/journal/CommonJournalTypes.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

using namespace std;

class InodeCacheParentEntry {
public:
	InodeCacheParentEntry(InodeNumber inode_number);
	virtual ~InodeCacheParentEntry();

	int32_t add_entry(InodeNumber inode_number, const EInode& einode);
	int32_t remove_entry(InodeNumber inode_number);
	int32_t update_entry(InodeNumber inode_number, int32_t chunk_id, const Operation* operation);

	int32_t rename( InodeNumber inode_number, int32_t chunk_id, FsObjectName* new_name);
	int32_t move_from(InodeNumber inode_number, InodeNumber& old_parent);
	int32_t move_to( InodeNumber inode_number, InodeNumber old_parent_id, int32_t chunk_id, const EInode& einode);

	bool get_entry(InodeNumber inode_number, InodeCacheEntry& entry) const;
	int32_t get_einode(InodeNumber inode_number, EInode& einode) const;

	bool check_trash(InodeNumber inode_number) const;

	int32_t get_last_change(InodeNumber inode_number, OperationType& type) const;

	CacheStatusType lookup_by_object_name(const FsObjectName* name, InodeNumber& inode_number) const;

	void read_dir(ReaddirOffset offset, ReadDirReturn& rdir_result) const;

	void get_to_update_set(set<InodeNumber>& update_set, map<InodeNumber, InodeNumber>& moved_map) const;
	void get_to_delete_set(set<InodeNumber>& delete_set) const;

	int32_t handle_write_back_delete(InodeNumber inode_number, set<int32_t>& chunk_set);
	int32_t handle_write_back_update(InodeNumber inode_number, set<int32_t>& chunk_set, EInode& einode);

	uint32_t size();

	timeval get_time_stamp() const;

	bool is_dirty() const;
	void set_dirty(bool b);

	bool is_full_present() const;
	bool unsafe_is_full_present() const;
	void set_full_present(bool b);

	InodeNumber get_inode_number() const;

	void lock_object();
	void unlock_object();

	void clear_trash();

private:
	int32_t write_back_delete(InodeNumber inode_number, set<int32_t>& chunk_set);

	mutable pthread_mutex_t mutex;

	void erase_from_cache(InodeNumber inode_number);
	void add_to_cache(map<InodeNumber, InodeCacheEntry>::iterator &it);


	vector<map<InodeNumber, InodeCacheEntry>::iterator > random_access_cache; /* Contains iterators that are pointing to the elements of the cache map,
																				 to provide random access to the cache. */

	unordered_map<InodeNumber, int32_t > consistency_map; /* Mapping from map cache to random access cache. */
	map<InodeNumber, InodeCacheEntry> cache_map; /*< Map holds the child entries of the parent entry. */
	map<InodeNumber, InodeCacheEntry> trash_map; /*< Map holds all child entries, which are tagged as deleted, but still on the storage yet */

	unordered_map<string, InodeNumber> name_cache; /*< Cache for object names, to provide fast access for look up by name. */


	InodeNumber inode_number; /*< Identifier of the object */
	bool dirty; /*< Flag identifies whether the state of the cache is modified or it is consistent with the storage. */
	bool full_present; /*< Flag identifies whether the whole directory is presented in cache. */
	mutable timeval time_stamp; /*< Timestamp identifies the last access to the object or one of the children */

	Pc2fsProfiler* ps_profiler;
};

#endif /* INODECACHEPARENTENTRY_H_ */
