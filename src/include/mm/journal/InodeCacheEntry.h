/**
 * @file InodeCacheEntry.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 */

#ifndef INODECACHEENTRY_H_
#define INODECACHEENTRY_H_

#include <list>
#include <set>

#include "global_types.h"
#include "CommonJournalTypes.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"


using namespace std;

class InodeCacheEntry
{
public:
	InodeCacheEntry(){};
	InodeCacheEntry(const EInode& einode);
	InodeCacheEntry( InodeNumber indode_number, OperationType type, int32_t chunk_number);

	virtual ~InodeCacheEntry(){};

    InodeNumber get_indode_number() const;
    void set_indode_number(InodeNumber indode_number);

    void add(OperationType type, int32_t chunk_number);

    int32_t get_last_type(OperationType& type) const;
    int32_t get_last_chunk_number() const;
    int32_t get_first_chunk_number() const;

    int32_t count() const;

    ModificationMapping get_last_modification() const;

    void get_list(list<ModificationMapping>& list) const;
    void get_chunk_set(set<int32_t>& chunk_set);

    const EInode& get_einode() const;
    void set_einode(const EInode& einode);

    int32_t update_inode_cache(const EInode& einode, int32_t bitfield);

    void clear();

    bool is_created_new() const;
    void set_created_new(bool b);

    bool is_trash() const;
    void set_trash(bool b);

    void set_parent_id(InodeNumber parent_id);
    InodeNumber get_parent_id() const;

    InodeNumber get_old_parent_id() const;
    void set_old_parent_id(InodeNumber inode_number);


private:

    bool only_mtime_atime(int32_t bitfield);

    InodeNumber old_parent_id; /* < Identifies from where the file was moved. */
	InodeNumber parent_id;
	InodeNumber indode_number; /* < The inode number of the cache entry. */
	list<ModificationMapping> changes_list; /* < List to cache all operation changes */
	EInode einode;
	bool created_new;
	bool trash;

	Pc2fsProfiler* ps_profiler;

	void update_mode(int new_mode, int flag);
};

#endif /* INODECACHEENTRY_H_ */
