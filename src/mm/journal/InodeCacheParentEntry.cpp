/**
 * @file InodeCacheParentEntry.cpp
 * @class InodeCacheParentEntry
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @date 01.09.2011
 *
 * @brief Represents an parent entry. (In file system view a directory.)
 * The parent entry can holds all the children entries of it.
 * It has two state flags, dirty and full present.
 * The full present flag identifies that the whole directory is in the cache, only after a read dir.
 * As soon as the full present flag is set (readdir), all further operations will only involve the cache,
 * without involving the storage.
 *
 * The dirty flag identifies whether the directory has updates in the cache,
 * so the cache is inconsistent with the storage.
 * It is used by the write back process, to identify whether this cache must be written back or not.
 *
 * The main cache is represented by a map, where the keys are inode numbers and @see InodeCacheEntry are the values.
 * It provides access by the inode number.
 * An additional hash map is used to provide access by the einode name, here the keys are stings
 * and values are inode number.
 * So to access an einode by name, first the name_cache is accessed to to get the corresponding inode number and
 * afterwards access the main cache to get the desired einode.
 *
 * Tree maps and hash maps provide fast access by identifier, but have poor performance if random access is desired,
 * because to access to an element at a random position we have to iterate to this position.
 * Random access is necessary for the readdir operation, because it is performed only by reading a small parts
 * of a directory, say 10 entries, the cache always just gets the starting position.
 * To overcome this problem an we are using an vector which contains iterator objects which are pointing to the
 * elements in the cache map. There is an additional hash map which uses inode number as identifiers and
 * integer values which identify the elements in the vector. It is used to have access in bidirectional way, from
 * cache map to the vector and back.
 */

#include <algorithm>
#include <cstring>
#include <unordered_map>

#include "mm/journal/InodeCacheParentEntry.h"
#include "mm/journal/InodeCacheException.h"
#include "mm/einodeio/EInodeIOException.h"

/**
 * @brief Constuctor of InodeCacheEntry.
 * Initialize the mutex and sets the flags.
 * @param inode_number The inode number for the object as identifier.
 */
InodeCacheParentEntry::InodeCacheParentEntry(InodeNumber inode_number)
{
	this->inode_number = inode_number;

	dirty = true;
	full_present = false;
	mutex = PTHREAD_MUTEX_INITIALIZER;
	gettimeofday(&time_stamp, 0);
	ps_profiler = Pc2fsProfiler::get_instance();
}

/*
 * @brief Destructor of InodeCacheEntry.
 */
InodeCacheParentEntry::~InodeCacheParentEntry()
{
	// TODO Auto-generated destructor stub
}

/**
 * @brief Adds a new entry into the cache.
 * This method should be used, if an inode is loaded from the storage in to the cache.
 * @param inode_number The inode number of the new entry.
 * @param einode The reference of the einode object.
 * @return 0 if the operation was successful, 1 if an entry with the same id is already present in the cache,
 * -1 if an entry with the same id is in the trash map.
 */
int32_t InodeCacheParentEntry::add_entry(InodeNumber inode_number, const EInode& einode)
{
	ps_profiler->function_start();

	int32_t rtrn = -1;
	map<InodeNumber, InodeCacheEntry>::iterator it;
	pair<map<InodeNumber, InodeCacheEntry>::iterator, bool> p;
	// first check the trash map
	it = trash_map.find(inode_number);

	if(it == trash_map.end())
	{
		it = cache_map.find(inode_number);

		if (it == cache_map.end())
		{
			InodeCacheEntry e(einode);
			e.set_parent_id(this->inode_number);
			e.set_created_new(false);
			p = cache_map.insert( pair <InodeNumber, InodeCacheEntry> ( inode_number, e ));
			name_cache.insert( pair <string, InodeNumber> ( einode.name, inode_number) );

			add_to_cache(p.first);

			rtrn = 0;
			gettimeofday(&time_stamp, 0);
		}
	}

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Removes an entry from the cache.
 * @param inode_number The inode number of the entry.
 * @return 0 if the operation was successful, -1 if no entry with the inode number is present.
 */
int32_t InodeCacheParentEntry::remove_entry(InodeNumber inode_number)
{
	ps_profiler->function_start();

	int32_t rtrn = -1;
	map<InodeNumber, InodeCacheEntry>::iterator it;
	int32_t position = 0;
	InodeNumber moved_element = 0;

	it = cache_map.find(inode_number);

	if(it != cache_map.end())
	{
		name_cache.erase( it->second.get_einode().name );
		cache_map.erase(it);

		erase_from_cache(inode_number);

		rtrn = 0;
	}

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Updates an entry in the cache.
 * This method should be used for updates of an inode or to create a new inode in the cache, that is not on the storage yet.
 * @param inode_number The inode number of the entry.
 * @param chunk_id The id of the chunk, where the updates are present.
 * @param operation Pointer to the operation object, that holds all necessary update information.
 */
int32_t InodeCacheParentEntry::update_entry(InodeNumber inode_number, int32_t chunk_id, const Operation* operation)
{
	ps_profiler->function_start();
	int32_t rtrn = 0;
	map<InodeNumber, InodeCacheEntry>::iterator it;
	it = cache_map.find(inode_number);

	// inode is not in the cache yet, a new inode entry will be created.
	if(it == cache_map.end())
	{
		// create a new entry
		pair<map<InodeNumber, InodeCacheEntry>::iterator, bool> p;
		InodeCacheEntry e( operation->get_einode() );
		e.set_parent_id( this->inode_number);
		e.set_indode_number( inode_number );

		// this inode is not on the storage yet
		e.set_created_new( true );
		e.add( operation->get_type(), chunk_id );

		p = cache_map.insert( pair<InodeNumber, InodeCacheEntry> ( inode_number, e ) );
		name_cache.insert( pair<string, InodeNumber> ( e.get_einode().name, inode_number ));

		add_to_cache(p.first);
	}

	// inode is already in the cache, just make some updates
	else
	{


		// check whether the inode should be deleted
		if(operation->get_type() == OperationType::DeleteINode)
		{
			it->second.add( operation->get_type(), chunk_id );

			name_cache.erase(it->second.get_einode().name);
			// put the entry from the cache in to the trash map
			trash_map.insert(pair<InodeNumber, InodeCacheEntry>(inode_number, it->second));
			cache_map.erase(it);

			erase_from_cache(inode_number);
		}
		else if ( operation->get_type() == OperationType::SetAttribute)
		{
			rtrn = it->second.update_inode_cache( operation->get_einode(), operation->get_bitfield() );

			if(rtrn == 0)
			{
				it->second.add( operation->get_type(), chunk_id );
			}
		}
	}

	gettimeofday(&time_stamp, 0);
	// the state of the entry is now dirty, not consistent to the storage
	dirty = true;

	ps_profiler->function_end();

	return rtrn;
}

/**
 * @brief Renames the inode object.
 * @param inode_number The number of the inode.
 * @param chunk_id The id of the chunk which holds the journal operation.
 * @param new_name Pointer to the name string.
 * @return 0 if the operation was successful, otherwise -1.
 */
int32_t InodeCacheParentEntry::rename( InodeNumber inode_number, int32_t chunk_id, FsObjectName* new_name)
{
	ps_profiler->function_start();

	int32_t rtrn = -1;

	map<InodeNumber, InodeCacheEntry>::iterator it;
	EInode einode;
	it = cache_map.find(inode_number);

	if( it != cache_map.end())
	{
		it->second.add( OperationType::RenameObject, chunk_id );
		einode = it->second.get_einode();

		// update the name cache
		name_cache.erase(einode.name);
		name_cache.insert( pair<string, InodeNumber>(*new_name, inode_number) );

		strcpy(einode.name, (char*) new_name);
		it->second.set_einode(einode);
		rtrn = 0;
	}

	gettimeofday(&time_stamp, 0);
	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Moves inode out from this parent cache entry.
 * @param inode_number The inode number that should me moved.
 * @param new_parent_id The inode number of the entry which should be the new parent.
 * @param operation Pointer to the operation data.
 * @param[out] old_parent The inode number of the old parent,
 * because the old parent identifies the original parent it can be different due to multiple move operations.
 * @return 0 if the operation was successful, -1 if the inode is unknown.
 */
int32_t InodeCacheParentEntry::move_from( InodeNumber inode_number, InodeNumber& old_parent)
{
	ps_profiler->function_start();

	int32_t rtrn = -1;
	map<InodeNumber, InodeCacheEntry>::iterator it;
	InodeNumber parent_id;

	it = cache_map.find(inode_number);

	if( it != cache_map.end())
	{

		parent_id = it->second.get_old_parent_id();
		if( parent_id != INVALID_INODE_ID)
		{
			old_parent = parent_id;
		}

		// a new parent, the inode will be removed from this parent entry
		// put the entry from the cache in to the trash map
		name_cache.erase(it->second.get_einode().name);
		trash_map.insert(pair<InodeNumber, InodeCacheEntry>(inode_number, it->second));
		cache_map.erase(it);
		erase_from_cache(inode_number);
		rtrn = 0;

	}

	gettimeofday(&time_stamp, 0);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Moves an inode into this cache entry.
 * @param inode_number The number of the inode.
 * @param old_parent_id The number of the old entry.
 * @param chunk_id The id of the chunk which holds this journaling operation.
 * @param einode The einode object.
 * @return 0 if the operation was successful, -1 if an entry with the same id is alredy in the cache.
 */
int32_t InodeCacheParentEntry::move_to( InodeNumber inode_number, InodeNumber old_parent_id, int32_t chunk_id, const EInode& einode)
{
	ps_profiler->function_start();

	int32_t rtrn = -1;

	map<InodeNumber, InodeCacheEntry>::iterator it;
	pair<map<InodeNumber, InodeCacheEntry>::iterator, bool> p;

	it = cache_map.find(inode_number);

	if( it == cache_map.end())
	{
		// create a new entry
		InodeCacheEntry e( einode );
		e.set_parent_id( this->inode_number);
		e.set_indode_number( inode_number );
		e.set_old_parent_id(old_parent_id);

		// this inode is not on the storage yet
		e.set_created_new( true );
		e.add( OperationType::MoveInode, chunk_id );

		name_cache.insert( pair<string, InodeNumber>( e.get_einode().name, inode_number ));
		p = cache_map.insert( pair<InodeNumber, InodeCacheEntry> ( inode_number, e ) );
		add_to_cache(p.first);
		rtrn = 0;
	}

	gettimeofday(&time_stamp, 0);

	ps_profiler->function_end();
	return rtrn;
}


/**
 * @brief Gets an entry from the cache.
 * @param[in] inode_number The inode number of the entry.
 * @param[out] entry The wanted entry.
 * @return true if the entry is in the cache, otherwise false.
 */
bool InodeCacheParentEntry::get_entry(InodeNumber inode_number, InodeCacheEntry& entry) const
{
	ps_profiler->function_start();
	bool rtrn = false;
	map<InodeNumber, InodeCacheEntry>::const_iterator cit;
	cit = cache_map.find(inode_number);

	if(cit != cache_map.end())
	{
		entry = cit->second;
		rtrn = true;
	}

	gettimeofday(&time_stamp, 0);
	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Gets an einode from the cache.
 * @param[int] inode_number The number of the inode.
 * @param[out] einode The wanted einode.
 * @return 0 if the einode is in the cache, otherwise -1.
 */
int32_t InodeCacheParentEntry::get_einode(InodeNumber inode_number, EInode& einode) const
{
	ps_profiler->function_start();

	int32_t rtrn = -1;
	map<InodeNumber, InodeCacheEntry>::const_iterator cit;
	cit = cache_map.find(inode_number);

	if(cit != cache_map.end())
	{
		einode = cit->second.get_einode();
		rtrn = 0;
	}

	gettimeofday(&time_stamp, 0);
	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Test wether an inode is tagged as deleted.
 * @param[in] inode_number The inode number of the entry.
 * @return true if the entry is in the cache an tagged as deleted, otherwise false.
 */
bool InodeCacheParentEntry::check_trash(InodeNumber inode_number) const
{
	ps_profiler->function_start();

	bool rtrn = false;
	map<InodeNumber, InodeCacheEntry>::const_iterator cit;
	cit = trash_map.find(inode_number);

	if(cit != cache_map.end())
	{
		rtrn = true;
	}

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Get the last cached type of the inode with the given inode number.
 * @param inode_number The inode number.
 * @param type Reference to set the last operation type.
 * @return 0 if the an entry with the inode number is holding by the cache and the last type was set.
 * 1 if the an entry with the inode number is holding by the cache but without any chages on the inode.
 * -1 the inode is not in the cache.
 */
int32_t InodeCacheParentEntry::get_last_change(InodeNumber inode_number, OperationType& type) const
{
	ps_profiler->function_start();

	int32_t rtrn = -1;
	map<InodeNumber, InodeCacheEntry>::const_iterator cit;
        ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
        ps_profiler->function_wakeup();

	cit = cache_map.find(inode_number);
	if(cit == cache_map.end())
	{
		// check the trash map
		cit = trash_map.find(inode_number);
		if(cit != trash_map.end())
		{
			type = OperationType::DeleteINode;
			rtrn = 0;
		}
	}
	else
	{
		rtrn = cit->second.get_last_type(type);
	}
	gettimeofday(&time_stamp, 0);
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Lookup einode object by name.
 * @param[in] name The name of the einode object.
 * @param[out] The wanted inode number.
 * @return Retruns a value of CacheStatusType.
 * Present if inode is in the cache and is stable.
 * Deleted if inode is in the cache is is tagged as deleted.
 * NotPresent if the inode is not in the cache.
 */
CacheStatusType InodeCacheParentEntry::lookup_by_object_name(const FsObjectName* name, InodeNumber& inode_number) const
{
	ps_profiler->function_start();


	gettimeofday(&time_stamp, 0);
	CacheStatusType type = CacheStatusType::NotPresent;
	unordered_map<string, InodeNumber>::const_iterator it = name_cache.find(*name);

	if(it != name_cache.end())
	{
		inode_number = it->second;
		type = CacheStatusType::Present;
	}

	ps_profiler->function_end();
	return type;
}


/**
 * @brief Reads the cached directory.
 * @param[in] offset The offset.
 * @param[out] rdir_result The result of the readdir operation.
 */
void InodeCacheParentEntry::read_dir(ReaddirOffset offset, ReadDirReturn& rdir_result) const
{
	ps_profiler->function_start();

	gettimeofday(&time_stamp, 0);

	map<InodeNumber, InodeCacheEntry>::const_iterator cit;
	ReaddirOffset i = 0;

	rdir_result.nodes_len = 0;

	// get the dir size
	rdir_result.dir_size = cache_map.size();

	// if the offet is higher than the dir size, return
	if(rdir_result.dir_size < offset || rdir_result.dir_size == 0)
	{
		rdir_result.nodes_len = 0;
		ps_profiler->function_end();
		return;
	}

	cit = random_access_cache[offset];

	do
	{
		rdir_result.nodes[rdir_result.nodes_len] = cit->second.get_einode();
		++rdir_result.nodes_len;
		++cit;
	}while(rdir_result.nodes_len < FSAL_READDIR_EINODES_PER_MSG && cit != cache_map.end());

	ps_profiler->function_end();
}

/**
 * @brief Get the set of inodes which must be written back to storage.
 * @param[out] update_set The set with the inodes.
 * @param[out] moved_map The map contains the moved inodes with old parent id as value.
 */
void InodeCacheParentEntry::get_to_update_set(set<InodeNumber>& update_set, map<InodeNumber, InodeNumber>& moved_map) const
{
	ps_profiler->function_start();

	map<InodeNumber, InodeCacheEntry>::const_iterator cit;
        ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
        ps_profiler->function_wakeup();
	for(cit = cache_map.begin(); cit != cache_map.end(); ++cit)
	{
		if( cit->second.count() > 0 )
		{
			if(cit->second.get_old_parent_id() != INVALID_INODE_ID)
			{
				moved_map.insert(pair<InodeNumber, InodeNumber>(cit->first, cit->second.get_old_parent_id()));
			}
			else
			{
				update_set.insert(cit->first);
			}
		}
	}
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}

/**
 * @brief Get the set of inodes which must be deleted from storage.
 * @param[out] delete_set The set with the inodes.
 */
void InodeCacheParentEntry::get_to_delete_set(set<InodeNumber>& delete_set) const
{
	ps_profiler->function_start();

	map<InodeNumber, InodeCacheEntry>::const_iterator cit;
        ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
        ps_profiler->function_wakeup();
	for(cit = trash_map.begin(); cit != trash_map.end(); ++cit)
	{
		delete_set.insert(cit->first);
	}
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}


/**
 * @brief Writes back an inodes, the inode will be removed from the long term storage.
 * @param[in] inode_number The inode_number of the inode to delete.
 * @param[out] chunk_set The set to write all chunk identifiers, which are holding updates for this inode.
 * @param[in] einode_io The EmbeddedInodeLookUp object.
 * @return 0 if the write back operation was successful and the entry was tagged as trash,
 * ready to remove it from storage, 1 if the write back operation was successful and the entry
 * was removed from cache and storage operation is unnecessary.
 * -1 if the inode is not in the cache.
 */
int32_t InodeCacheParentEntry::handle_write_back_delete(InodeNumber inode_number, set<int32_t>& chunk_set)
{
	ps_profiler->function_start();
	int32_t rtrn = -1;
        ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
        ps_profiler->function_wakeup();

	rtrn =write_back_delete(inode_number, chunk_set);

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Writes back an inodes, a new einode will be created on the storage, or updated if it already exists.
 * @param[in] inode_number The inode_number of the inode to write.
 * @param[out] chunk_set The set to write all chunk identifiers, which are holding updates for this inode.
 * @param[out] einode_io The Einode to write on the storage.
 * @return 0 if the write back operation was successful and ready to update the storage,
 * 1 if it is a new entry and must be created on the storage.
 * 2 if the entry was tagged as deleted before the write back on this entry starts and can be removed from the storage now
 * 3 if the entry was tagged as deleted and was a new created entry so a storage operation in unnecessary.
 * -1 if the inode is not in the cache
 */
int32_t InodeCacheParentEntry::handle_write_back_update(InodeNumber inode_number, set<int32_t>& chunk_set, EInode& einode)
{
	ps_profiler->function_start();

	int32_t rtrn = -1;
	map<InodeNumber, InodeCacheEntry>::iterator it;
        ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
        ps_profiler->function_wakeup();

	it = cache_map.find(inode_number);

	if(it != cache_map.end())
	{
		// get the chunk set with updates
		it->second.get_chunk_set(chunk_set);

		einode = it->second.get_einode();

		// if the inode is a new one, cache only
		if(it->second.is_created_new())
		{
			it->second.set_created_new(false);
			rtrn = 1;
		}
		// the inode is on the storage, update it
		else
		{
			rtrn = 0;
		}
		it->second.clear();
	}
	// inode was tagged as deleted, before the write  operations starts
	else
	{
		rtrn = write_back_delete(inode_number, chunk_set);

		if(rtrn == 0)
		{
			rtrn = 2;
		}
		else if ( rtrn == 1)
		{
			rtrn = 3;
		}
		else
		{
			rtrn = -1;
		}
	}

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}


/*
 * @brief Cache size.
 * @return Number of entries holding by this cache.
 */
uint32_t InodeCacheParentEntry::size()
{
	return cache_map.size();
}

/**
 * @brief Gets time stamp.
 * @return The last access to this cache entry.
 */
timeval InodeCacheParentEntry::get_time_stamp() const
{
	return time_stamp;
}


/**
 * @brief Tests the dirty flag.
 * @return true If the entry contains modification and is not consistent with the storage, otherwise false.
 */
bool InodeCacheParentEntry::is_dirty() const
{
	ps_profiler->function_start();
	bool b;
        ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
        ps_profiler->function_wakeup();
	b = dirty;
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return b;
}

/**
 * @brief Sets the dirty flag.
 * @param b The boolean value.
 */
void InodeCacheParentEntry::set_dirty(bool b)
{
	ps_profiler->function_start();
        ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
        ps_profiler->function_wakeup();
	this->dirty = b;
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}

/**
 * @brief Tests whether the entry holds the whole directory.
 * @return true if the entry holds the whole directory, otherwise false.
 */
bool InodeCacheParentEntry::is_full_present() const
{
	ps_profiler->function_start();

	bool b;
        ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
        ps_profiler->function_wakeup();
	b = full_present;
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return b;
}

/**
 * @brief Tests whether the entry holds the whole directory.
 * This method is not thread safe, the caller must lock the object before.
 * @return true if the entry holds the whole directory, otherwise false.
 */

bool InodeCacheParentEntry::unsafe_is_full_present() const
{
	return full_present;
}

/**
 *  @brief Sets the full present flag.
 *  @param b The boolean value.
 */
void InodeCacheParentEntry::set_full_present(bool b)
{
	ps_profiler->function_start();
        ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
        ps_profiler->function_wakeup();
	this->full_present = b;
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}

/**
 * @brief Gets the inode number of the entry.
 * @return The inode number.
 */
InodeNumber InodeCacheParentEntry::get_inode_number() const
{
	return inode_number;
}

/**
 * @brief Locks the object.
 * @throws InodeCacheException if the locking fails.
 */
void InodeCacheParentEntry::lock_object()
{
	ps_profiler->function_start();
        ps_profiler->function_sleep();
	if(pthread_mutex_lock(&mutex) != 0)
	{
                ps_profiler->function_wakeup();
		ps_profiler->function_end();
		throw InodeCacheException("Locking the mutex failed");
	}
        ps_profiler->function_wakeup();
	ps_profiler->function_end();
}

/**
 * @brief Unlocks the object.
 * @throws InodeCacheException if the unlocking fails.
 */
void InodeCacheParentEntry::unlock_object()
{
	ps_profiler->function_start();
        ps_profiler->function_sleep();
	if(pthread_mutex_unlock(&mutex) != 0)
	{
                ps_profiler->function_wakeup();
		ps_profiler->function_end();
		throw InodeCacheException("Unlocking the mutex failed");
	}
        ps_profiler->function_wakeup();
	ps_profiler->function_end();
}

/**
 * @brief Clears the trash amp.
 */
void InodeCacheParentEntry::clear_trash()
{
	ps_profiler->function_start();
	map<InodeNumber, InodeCacheEntry>::iterator it;
        ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
        ps_profiler->function_wakeup();
	for( it = trash_map.begin(); it != trash_map.end();)
	{
		if(it->second.is_trash())
		{
			trash_map.erase(it++);
		}
		else
		{
			++it;
		}
	}

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}

/**
 * @brief Writes back an inodes, the inode will be removed from the long term storage.
 * @param[in] inode_number The inode_number of the inode to delete.
 * @param[out] chunk_set The set to write all chunk identifiers, which are holding updates for this inode.
 * @param[in] einode_io The EmbeddedInodeLookUp object.
 * @return 0 if the write back operation was successful and the entry was tagged as trash,
 * ready to remove it from storage, 1 if the write back operation was successful and the entry
 * was removed from cache and storage operation is unnecessary.
 *  -1 if the inode is not in the cache.
 */
int32_t InodeCacheParentEntry::write_back_delete(InodeNumber inode_number, set<int32_t>& chunk_set)
{
	ps_profiler->function_start();

	int32_t rtrn = -1;
	map<InodeNumber, InodeCacheEntry>::iterator it;


	it = trash_map.find(inode_number);

	if(it != trash_map.end())
	{
		// get the chunk set with updates
		it->second.get_chunk_set(chunk_set);
		// check whether the inode is on the storage, or cache only
		if(!it->second.is_created_new())
		{

			it->second.set_trash(true);
			rtrn = 0;
		}
		else
		{
			trash_map.erase(it);
			rtrn = 1;
		}

	}

	ps_profiler->function_end();
	return rtrn;
}


/**
 * @brief Adds an entry to the mapping cache.
 * @param it Iterator object which points to an entry inside the cache map.
 */
void InodeCacheParentEntry::add_to_cache(map<InodeNumber, InodeCacheEntry>::iterator &it)
{

	ps_profiler->function_start();
	random_access_cache.push_back(it);
	consistency_map.insert( pair<InodeNumber, int32_t>(it->second.get_einode().inode.inode_number, random_access_cache.size()-1));
	ps_profiler->function_end();
}

/**
 * @brief Removes an entry from the mapping cache.
 * @param inode_number The number of the einode to remove from the cache.
 */
void InodeCacheParentEntry::erase_from_cache(InodeNumber inode_number)
{
	ps_profiler->function_start();
	InodeNumber moved_element = 0;

	int32_t position = consistency_map.find(inode_number)->second;

	if(random_access_cache.size() > 1)
	{
		// save the inode of the last element in the random access cache
		InodeNumber moved_element = random_access_cache[random_access_cache.size() - 1]->second.get_einode().inode.inode_number;

		if(moved_element != inode_number)
		{
			// swap the element with the last element for fast erase
			swap(random_access_cache[position], random_access_cache[random_access_cache.size() - 1 ]);

			// update the position of the moved element
			consistency_map.find(moved_element)->second = position;
		}
	}
	// erase the element
	random_access_cache.pop_back();
	consistency_map.erase(inode_number);

	ps_profiler->function_end();
}
