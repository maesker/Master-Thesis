/**
 * @file InodeCache.cpp
 * @class InodeCache
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @date 01.08.2011
 *
 * @brief Represents the inode cache and implements all necessary methods for inode handling,
 * including creation and update of an inode, inode access and the readdir operation.
 *
 *
 * The inode cache is build up in the following fashion:
 * A directory is represented by the @see InodeCacheParentEntry.
 * An entry inside a directory is represented by the @see InodeCacheEntry,
 * which can be a file or also a directory, but here there is no distinguish is made,
 * whether by the journal nor by the cache.
 *
 * The InodeCacheParentEntries entries are holding by a map,
 * referencing by the inode number as the key.
 * The cache provides an interface to access an einode by name or by inode number.
 * The cache is responsible to fetch the data from storage, if its not in the cache.
 */

#include "mm/journal/InodeCache.h"
#include "mm/einodeio/EInodeIOException.h"
#include "mm/journal/InodeCacheException.h"

/**
 * @brief Default constructor of the InodeCache.
 * Initializes the mutex.
 */
InodeCache::InodeCache()
{
	mutex = PTHREAD_MUTEX_INITIALIZER;
	log = new Logger();
	log->set_log_level(1);
	log->set_console_output((bool) 0);


	log->set_log_location(std::string(DEFAULT_LOG_DIR) + "/inodeCache" + ".log");
	log->debug_log( "Initiate inode cache." );
	ps_profiler = Pc2fsProfiler::get_instance();
}

/**
 * @brief Destructor of InodeCache.
 * Destroys the mutex.
 */
InodeCache::~InodeCache()
{
	pthread_mutex_destroy(&mutex);
}

/**
 * @param Adds an einode to the cache.
 * @param inode_number The inode number.
 * @param parent_id The parent id of the inode.
 * @parma eindoe The einode.
 * @return 0 if the operation was successful, -1 if the inode is already in the cache.
 */
int32_t InodeCache::add_to_cache(InodeNumber inode_number, InodeNumber parent_id, const EInode& einode)
{
	ps_profiler->function_start();

	int32_t rtrn = -1;
	map<InodeNumber, InodeCacheParentEntry*>::iterator it;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	parent_cache_map.insert(pair<InodeNumber, InodeNumber>(inode_number, parent_id));

	it = cache_map.find( parent_id );

	if ( it != cache_map.end() )
	{
		// lock the wanted object
		it->second->lock_object();

		// unlock the global mutex
		pthread_mutex_unlock( &mutex );

		rtrn = it->second->add_entry(inode_number, einode);
		it->second->unlock_object();
	}

	// parent entry is not in the cache
	else
	{
		InodeCacheParentEntry *pe = new InodeCacheParentEntry(parent_id);
		cache_map.insert(pair<InodeNumber, InodeCacheParentEntry*>(parent_id, pe));

		map<InodeNumber, InodeNumber> temp_parent_map;
		cache_dir(pe, temp_parent_map, einode_io);
		pe->set_full_present(true);
		rtrn = pe->add_entry(inode_number, einode);


		pthread_mutex_unlock( &mutex );
	}

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Gets the pointer to a parent cache entry.
 * @param inode_numher The inode number of the wanted entry.
 * @return Pointer to the cache entry, NULL if entry does not exists.
 */
InodeCacheParentEntry* InodeCache::get_cache_parent_entry(InodeNumber inode_number)
{
	ps_profiler->function_start();

	InodeCacheParentEntry* pe = NULL;
	map<InodeNumber, InodeCacheParentEntry*>::iterator it;
	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	it = cache_map.find(inode_number);

	if(it != cache_map.end())
	{
		pe = it->second;
	}

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();

	return pe;
}

/**
 * @brief Get the last cached type of the inode with the given inode number.
 * @param inode_number The inode number.
 * @param type Reference to set the last operation type.
 * @return 0 if the an entry with the inode number is holding by the cache and the last type was set.
 * 1 if the an entry with the inode number is holding by the cache but without any chages on the inode.
 * -1 the inode is not in the cache.
 */
int32_t InodeCache::get_last_change(InodeNumber inode_number, OperationType& type) const
{
	ps_profiler->function_start();
	int32_t rtrn = -1;
	InodeNumber parent_id = INVALID_INODE_ID;
	InodeCacheEntry e;
	map<InodeNumber, InodeCacheParentEntry*>::const_iterator cit;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	// get parent id
	parent_id = determine_parent(inode_number);

	if(parent_id == INVALID_INODE_ID)
	{
		pthread_mutex_unlock( &mutex );
		ps_profiler->function_end();
		return rtrn;
	}

	cit = cache_map.find( parent_id );


	if ( cit != cache_map.end() )
	{
		rtrn = cit->second->get_last_change(inode_number, type);
	}

	pthread_mutex_unlock( &mutex );

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @param Removes an entry from the cache.
 * @param inode_id The id of the cache entry.
 * @return 0 if the operation was successful, -1 if the inode_id is unknown.
 */
int32_t InodeCache::remove_entry(InodeNumber inode_id)
{
        ps_profiler->function_start();
	int32_t rtrn = -1;

	map<InodeNumber, InodeCacheParentEntry*>::iterator it;
	map<InodeNumber, InodeNumber>::iterator p_it;
	InodeCacheParentEntry *pe = NULL;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	it = cache_map.find(inode_id);

	if( it != cache_map.end())
	{
		pe = it->second;

		// check that nobody is working on this object
		pe->lock_object();
		pe->unlock_object();
		cache_map.erase(it);
		delete pe;

		// clean parent cache
		for(p_it = parent_cache_map.begin(); p_it != parent_cache_map.end();)
		{
			if(p_it->second == inode_id)
			{
				parent_cache_map.erase(p_it++);
			}
			else
			{
				p_it++;
			}
		}
		rtrn = 0;
	}
	pthread_mutex_unlock( &mutex );
        ps_profiler->function_end();
	return rtrn;
}

/**
 * @param Clears the cache.
 */
void InodeCache::clear()
{
    ps_profiler->function_start();
    ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
    ps_profiler->function_wakeup();
	cache_map.clear();
	pthread_mutex_unlock( &mutex );
    ps_profiler->function_start();
}

/**
 * @param Moves an inode from one old position to a new target position.
 * @param new_parent_id The id of the new parent, the target position.
 * @param new_name The new name string of the inode.
 * @param old_parent_id The id of the old parent, the source position.
 * @param old_name The old name string of the inode.
 * @param einode_io Pointer to the EmbeddedInodeLookUp object.
 * @param chunk_id The id of the chunk which holds this journaling operation.
 * @return 0 if the operation was successful, -1 if the inode is unknown, 1 if the new position
 * has already an entry with the same name.
 */
int32_t InodeCache::move_inode(InodeNumber new_parent_id, FsObjectName *new_name, InodeNumber old_parent_id,
		FsObjectName* old_name, EmbeddedInodeLookUp* einode_io, int32_t chunk_id)
{

	log->debug_log( "Moving %s from %llu to %llu, new name: %s.", (char*)old_name, old_parent_id,
			new_parent_id, (char*) new_name  );

	ps_profiler->function_start();
	int32_t rtrn = -1;
	map<InodeNumber, InodeCacheParentEntry*>::iterator pe1_it;
	map<InodeNumber, InodeCacheParentEntry*>::iterator pe2_it;
	InodeNumber inode_number = INVALID_INODE_ID;
	CacheStatusType type = CacheStatusType::NotPresent;
	EInode einode;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	pe1_it = cache_map.find(old_parent_id);

	if( pe1_it == cache_map.end() )
	{
		pthread_mutex_unlock(&mutex);
		log->debug_log( "Old parent unknown, move failed!" );
		ps_profiler->function_end();
		return rtrn;
	}


	pe2_it = cache_map.find(new_parent_id);

	if( pe2_it == cache_map.end() )
	{
		log->debug_log( "New parent unknown, creating it in the cache." );
		InodeCacheParentEntry* pe = new InodeCacheParentEntry(new_parent_id);
		pair <map<InodeNumber, InodeCacheParentEntry*>::iterator,bool> p;
		p = cache_map.insert(pair<InodeNumber, InodeCacheParentEntry*>(new_parent_id, pe));

		pe2_it = p.first;
	}

	pe1_it->second->lock_object();
	pe2_it->second->lock_object();
	pthread_mutex_unlock(&mutex);

	// first lookup the inode to move
	type = pe1_it->second->lookup_by_object_name(old_name, inode_number);
	if(type == CacheStatusType::Present)
	{
		pe1_it->second->get_einode(inode_number, einode);
		log->debug_log( "Einode is in the cache." );
	}
	if(type == CacheStatusType::Deleted)
	{
		pe1_it->second->unlock_object();
		pe2_it->second->unlock_object();
		ps_profiler->function_end();
		log->debug_log( "Einode was in the cache, but is tagges as deleted, move failed!" );
		return rtrn;
	}
	// not in the cache, get it from storage
	else if (type == CacheStatusType::NotPresent)
	{
		log->debug_log( "Einode is not in the cache, lookup from storage." );
		if( fetch_from_storage(einode, old_parent_id, old_name, einode_io)  == 0)
		{
			inode_number = einode.inode.inode_number;
			// add it to the cache
			pe1_it->second->add_entry(inode_number, einode);
			log->debug_log( "Lookup from storage successful." );
		}
		// inode is not on the storage
		else
		{
			pe1_it->second->unlock_object();
			pe2_it->second->unlock_object();
			ps_profiler->function_end();
			log->debug_log( "Einode is not on the storage, move failed." );
			return rtrn;
		}
	}

	log->debug_log( "Check the target location." );
	// now lookup whether an einode with the same name already exists at the target position
	type = pe2_it->second->lookup_by_object_name(new_name, inode_number);

	// target postion has already an inode with the same name
	if(type == CacheStatusType::Present)
	{
		pe1_it->second->unlock_object();
		pe2_it->second->unlock_object();
		rtrn = 1;
		ps_profiler->function_end();
		log->debug_log( "An einode with the same name is already present at the target location (cache), move failed." );
		return rtrn;
	}
	// lookup the storage // TODO check if it is full present, so storage access is unnecessary
	else if( type == CacheStatusType::NotPresent)
	{
		if( fetch_from_storage(einode, new_parent_id, new_name, einode_io) == 0)
		{
			pe1_it->second->unlock_object();
			pe2_it->second->unlock_object();
			rtrn = 1;
			ps_profiler->function_end();
			log->debug_log( "An einode with the same name is already present at the target location (storage), move failed." );
			return rtrn;
		}
	}

	// all check step complete, next step is the move operation
	int32_t failure_1 = pe1_it->second->move_from(inode_number, old_parent_id);
	log->debug_log( "Moving %s with einode number %llu to %llu. (Number inside einode: %llu).",
			(char*)old_name, inode_number, new_parent_id, einode.inode.inode_number);
	int32_t failure_2 = pe2_it->second->move_to(inode_number, old_parent_id, chunk_id, einode);

	if( failure_1 != 0 || failure_2 != 0 )
	{
		pe1_it->second->unlock_object();
		pe2_it->second->unlock_object();
		ps_profiler->function_end();
		log->error_log( "Unexpected error occurred during the movement operation." );
		throw InodeCacheException("Unexpected error case occurred.");
	}



	rtrn = 0;

	pe1_it->second->unlock_object();
	pe2_it->second->unlock_object();



	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	// set the new parent
	parent_cache_map.erase(inode_number);
	parent_cache_map.insert(pair<InodeNumber, InodeNumber>(inode_number, new_parent_id));
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();

	log->error_log( "Move operation complete!" );
	return rtrn;
}

/**
 * @param Renames and einode.
 * @param parent_id The parent id of the inode.
 * @param new_name Pointer to the new name string.
 * @param old_name Pointer to the old name string.
 * @param einode_io Pointer to the EmbeddedInodeLookUp object.
 * @param chunk_id The id of the chunk which holds this journaling operation.
 * @return 0 if the operation was successful, -1 if the inode is uknown.
 */
int32_t InodeCache::rename(InodeNumber parent_id, FsObjectName *new_name, FsObjectName *old_name,
		EmbeddedInodeLookUp* einode_io, int32_t chunk_id)
{
	ps_profiler->function_start();

	int32_t rtrn = -1;
	map<InodeNumber, InodeCacheParentEntry*>::iterator it;

	InodeNumber inode_number = INVALID_INODE_ID;
	CacheStatusType type = CacheStatusType::NotPresent;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	it = cache_map.find(parent_id);

	if( it != cache_map.end())
	{
		it->second->lock_object();
		pthread_mutex_unlock(&mutex);

		// first lookup the inode number
		type = it->second->lookup_by_object_name(old_name, inode_number);

		// the inode was deleted
		if(type == CacheStatusType::Deleted)
		{
			it->second->unlock_object();
			ps_profiler->function_end();
			return rtrn;
		}
		// the inode is not in the cache, ask the storage
		else if(type == CacheStatusType::NotPresent)
		{
			EInode einode;
			if( fetch_from_storage(einode, parent_id, old_name, einode_io)  == 0)
			{
				inode_number = einode.inode.inode_number;
				// add it to the cache
				it->second->add_entry(inode_number, einode);
			}
			else
			{
				it->second->unlock_object();
				ps_profiler->function_end();
				return rtrn;
			}
		}

		it->second->rename(inode_number, chunk_id, new_name);
		it->second->unlock_object();
		rtrn = 0;
	}
	else
	{
		pthread_mutex_unlock(&mutex);
	}

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Updates the inode cache.
 * @param operation Pointer to the operation object that holds the einode.
 * @param chunk_id The chunk number that holding the operation.
 * @throws InodeCacheException if the parent id is unknown.
 */
int32_t InodeCache::update_inode_cache(const Operation* operation, int32_t chunk_id)
{
	ps_profiler->function_start();
	int32_t rtrn = 0;

	InodeNumber parent_id = operation->get_parent_id();
	map<InodeNumber, InodeCacheParentEntry*>::iterator it;

	log->debug_log( "Trying to acquire mutex..." );
	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();
	log->debug_log( "Acquiring successful!" );

	// check whether the parent id is known
	if(parent_id == INVALID_INODE_ID)
	{
		// parent id is unknown, check the parent cache
		parent_id = determine_parent(operation->get_inode_id());

		// normally this point should be unreachable
		// the parent id of the inode is unknown and not in the parent cache, something went wrong
		if(parent_id == INVALID_INODE_ID)
		{
			pthread_mutex_unlock( &mutex );
			ps_profiler->function_end();
			throw InodeCacheException("Unexpected failure during inode update operation, parent id is unkown!");
		}
	}
	else
	{
		parent_cache_map.insert(pair<InodeNumber, InodeNumber>(operation->get_inode_id(), parent_id));
	}

	it = cache_map.find(parent_id);

	// check if the parent entry is already in the cache
	if ( it == cache_map.end() )
	{
		// new cache entry
		InodeCacheParentEntry *pe = new InodeCacheParentEntry(parent_id);
		cache_map.insert( pair<InodeNumber, InodeCacheParentEntry*> ( parent_id, pe ) );

		map<InodeNumber, InodeNumber> temp_parent_map;
		cache_dir(pe, temp_parent_map, einode_io);
		pe->set_full_present(true);

		pe->update_entry(operation->get_inode_id(), chunk_id, operation);

		pthread_mutex_unlock( &mutex );
	}

	// entry already in the cache
	else
	{
		it->second->lock_object();
		pthread_mutex_unlock( &mutex );

		rtrn = it->second->update_entry(operation->get_inode_id(), chunk_id, operation);
		it->second->unlock_object();
	}
	ps_profiler->function_end();

	return rtrn;
}



/**
 * @brief Gets the einode with the given inode id.
 * @param[in] The inode id.
 * @param[out] The wanted einode.
 * @return Retruns a value of CacheStatusType.
 * Present if einode is in the cache and is stable.
 * Deleted if einode is in the cache is is tagged as deleted.
 * NotPresent if the einode is not in the cache.
 * */
const CacheStatusType InodeCache::get_einode(InodeNumber inode_id, EInode& einode, EmbeddedInodeLookUp* einode_io)
{

	log->debug_log( "Inode Number: %llu", inode_id );
	ps_profiler->function_start();

	CacheStatusType type = CacheStatusType::NotPresent;
	InodeNumber parent_id = INVALID_INODE_ID;
	InodeCacheEntry e;
	map<InodeNumber, InodeCacheParentEntry*>::iterator it;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	// get the parent id
	parent_id = determine_parent(inode_id);

	// if parent id unkown, leave
	if(parent_id == INVALID_INODE_ID)
	{

		pthread_mutex_unlock( &mutex );
		log->debug_log( "Unable to determine the parent id!" );
		ps_profiler->function_end();
		return type;
	}

	it = cache_map.find( parent_id );

	// get parent entry
	if ( it != cache_map.end() )
	{
		it->second->lock_object();
		pthread_mutex_unlock( &mutex );

		// get the einode
		if(it->second->get_einode(inode_id, einode) == 0)
		{
			type = CacheStatusType::Present;
			log->debug_log( "Indeo is in the cache!" );
		}
		// check whether it is tagged as deleted
		else if( it->second->check_trash(inode_id))
		{
			type = CacheStatusType::Deleted;
			log->debug_log( "Inode is tagged as deleted!" );
		}
		// if it is not in the cache, check the storage if the cache holds not the complete directory
		else if( !it->second->unsafe_is_full_present() )
		{
			log->debug_log( "Inode is not in the cache and the directory is not full present, get it from storage." );
			int32_t result = fetch_from_storage(einode, parent_id, inode_id, einode_io);
			if(result == 0)
			{
				it->second->add_entry(inode_id, einode);
				type = CacheStatusType::Present;
				log->debug_log( "Inode is on the storage." );
			}
		}
		it->second->unlock_object();
	}
	else
	{
		pthread_mutex_unlock( &mutex );
	}

	ps_profiler->function_end();
	return type;
}

/**
 * @brief Get set of all modified parent entries.
 * @param dirty_set Set to write the inodes identifiers of the entries.
 */
void InodeCache::get_dirty_map(map<InodeNumber, InodeCacheParentEntry*> &dirty_map) const
{
	ps_profiler->function_start();

	map<InodeNumber, InodeCacheParentEntry*>::const_iterator cit;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	for( cit = cache_map.begin(); cit != cache_map.end(); ++cit)
	{
		if( cit->second->is_dirty() )
		{
			dirty_map.insert(pair<InodeNumber, InodeCacheParentEntry*>( cit->first, cit->second));
		}
	}
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}

/**
 * @brief Lookup einode object by name.
 * @param name The name of the einode object.
 * @param parent_id The parent id of the object.
 * @param inode_number The wanted inode number.
 * @return Retruns a value of CacheStatusType.
 * Present if inode is in the cache and is stable.
 * Deleted if inode is in the cache is is tagged as deleted.
 * NotPresent if the inode is not in the cache.
 */
CacheStatusType InodeCache::lookup_by_object_name(FsObjectName* name, InodeNumber parent_id,
		InodeNumber& inode_number, EmbeddedInodeLookUp* einode_io)
{
	ps_profiler->function_start();
	log->debug_log("Lookup for %s in %llu", (char*) name, parent_id);


	CacheStatusType type = CacheStatusType::NotPresent;
	map<InodeNumber, InodeCacheParentEntry*>::const_iterator cit;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	cit = cache_map.find(parent_id);

	if(cit == cache_map.end())
	{
		log->debug_log("Parent %llu is not in the cache!", parent_id);
		pthread_mutex_unlock(&mutex);
	}
	else
	{
		cit->second->lock_object();
		pthread_mutex_unlock(&mutex);

		type = cit->second->lookup_by_object_name(name, inode_number);

		// if it is not in the cache and cache is not full present, check the storage
		if(type == CacheStatusType::NotPresent && !cit->second->unsafe_is_full_present() )
		{
			EInode einode;

			int32_t result = fetch_from_storage(einode, parent_id, name, einode_io);
			if(result == 0)
			{
				if( cit->second->add_entry(einode.inode.inode_number, einode) == 0)
				{
					type = CacheStatusType::Present;
					cit->second->unlock_object();

					ps_profiler->function_sleep();
					pthread_mutex_lock(&mutex);
					ps_profiler->function_wakeup();

					parent_cache_map.insert(pair<InodeNumber, InodeNumber>(einode.inode.inode_number, parent_id));
					pthread_mutex_unlock(&mutex);
				}
				else
				{
					cit->second->unlock_object();
				}

				ps_profiler->function_end();
				return type;
			}
		}

		cit->second->unlock_object();
		log->debug_log("Look up result: %d ", type);
	}

	ps_profiler->function_end();
	return type;
}

/**
 * @param Loads a directory into the cache.
 * @param parent_id The inode id of the directory.
 * @param Pointer to the EmbeddedInodeLookUp object.
 */
int32_t InodeCache::cache_dir(InodeNumber parent_id, EmbeddedInodeLookUp* einode_io)
{
	ps_profiler->function_start();

	int32_t rtrn = 0;
	map<InodeNumber, InodeCacheParentEntry*>::iterator it;
	InodeCacheParentEntry* pe = NULL;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	it = cache_map.find(parent_id);



	// check wether the entry is already in the cache
	if( it == cache_map.end() )
	{
		// create new entry
		pe = new InodeCacheParentEntry(parent_id);
		cache_map.insert(pair<InodeNumber, InodeCacheParentEntry*>(parent_id, pe));
	}
	else
	{
		pe = it->second;

		// if the directoy is already in the cache, we are finished here
		if(pe->is_full_present())
		{
			pthread_mutex_unlock(&mutex);
			ps_profiler->function_end();
			return rtrn;
		}
	}

	pe->set_full_present(true);
	pe->lock_object();
	pthread_mutex_unlock(&mutex);

	map<InodeNumber, InodeNumber> temp_parent_map;

	rtrn = cache_dir(pe, temp_parent_map, einode_io);

	pe->unlock_object();

	add_to_parent_map(temp_parent_map);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Loads a directory into the cache.
 * @param pe Pointer to the @see InodeCacheParentEntry object.
 * @param temp_parent_map Reference to the temporally parent map.
 * @param Pointer to the EmbeddedInodeLookUp object.
 */
int32_t InodeCache::cache_dir(InodeCacheParentEntry* pe, map<InodeNumber, InodeNumber>& temp_parent_map, EmbeddedInodeLookUp* einode_io)
{
	ps_profiler->function_start();

	int32_t rtrn = 0;

	ReadDirReturn rdir_return;
	rdir_return.nodes_len = 0;
	rdir_return.dir_size = 0;

	ReaddirOffset offset = 0;

	do
	{
		try {
			rdir_return = einode_io->read_dir(pe->get_inode_number(), offset);
		} catch (ParentCacheException& e) {
			pe->unlock_object();
			ps_profiler->function_end();
			return rtrn;
		}

		for(unsigned int i = 0; i < rdir_return.nodes_len; i++)
		{
			pe->add_entry(rdir_return.nodes[i].inode.inode_number, rdir_return.nodes[i]);
			temp_parent_map.insert(pair<InodeNumber, InodeNumber>(rdir_return.nodes[i].inode.inode_number, pe->get_inode_number()));
			offset++;
		}

	}while(offset < rdir_return.dir_size);


	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Reads a cached directory.
 * @param[in] parent_id The inode number of the directory.
 * @param[in] offset The offset.
 * @param[out] rdir_result The result of the readdir operation.
 * @return 0 if the operation was successful, -1 if the directory is not cached.
 */
int32_t InodeCache::read_dir(InodeNumber parent_id, ReaddirOffset offset, ReadDirReturn& rdir_result) const
{
	ps_profiler->function_start();

	int32_t rtrn = -1;
	map<InodeNumber, InodeCacheParentEntry*>::const_iterator cit;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	cit = cache_map.find(parent_id);

	if(cit != cache_map.end())
	{
		rtrn = 0;

		cit->second->lock_object();
		pthread_mutex_unlock(&mutex);

		cit->second->read_dir(offset, rdir_result);
		cit->second->unlock_object();
	}
	else
	{
		pthread_mutex_unlock(&mutex);
		rtrn = -1;
	}

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Gets information about the cached object.
 * @param[out] info The vector that will get all data about the cache.
 */
void InodeCache::about_cache(vector<AccessData>& access_data) const
{
	ps_profiler->function_start();

	map<InodeNumber, InodeCacheParentEntry*>::const_iterator cit;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	for(cit = cache_map.begin(); cit != cache_map.end(); cit++)
	{
		AccessData a_data;
		a_data.inode_numer = cit->first;
		a_data.dirty = cit->second->is_dirty();
		a_data.cache_size = cit->second->size();
		a_data.last_access = cit->second->get_time_stamp();
		access_data.push_back(a_data);
	}


	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}

/**
 * @brief Get the parent id of an einode.
 * @param inode_number The inode number.
 * @return The parent id if the inode number is known by the cache, otherwise INVALID_INODE_ID is returned.
 */
InodeNumber InodeCache::get_parent(InodeNumber inode_number) const
{
	ps_profiler->function_start();

	InodeNumber parent = INVALID_INODE_ID;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	parent = determine_parent(inode_number);


	pthread_mutex_unlock(&mutex);
	ps_profiler->function_end();

	return parent;
}

/**
 * @brief Determines the parent id of an inode.
 * @parent inode_number The inode number.
 * @return The parent id of the inode, INVALID_INODE_ID if the parent id is unkown by the cache.
 */
InodeNumber InodeCache::determine_parent(InodeNumber inode_number) const
{
	ps_profiler->function_start();

	InodeNumber parent_id = INVALID_INODE_ID;
	map<InodeNumber, InodeNumber>::const_iterator cit;
	cit = parent_cache_map.find(inode_number);

	// the parent id is in the parent cache map
	if(cit != parent_cache_map.end())
	{
		parent_id = cit->second;
	}

	ps_profiler->function_end();
	return parent_id;
}


/**
 * @brief Reads an einode from the storage, lookup by inode number.
 * @param einode The reference to the einode, which will be read.
 * @param inode_number The inode number of the wanted einode.
 * @param einode_io Pointer to the EmbeddedInodeLookUp object.
 * @return 0 if the operation was successful, -1 otherwise.
 */
int32_t InodeCache::fetch_from_storage(EInode& einode, InodeNumber inode_number, EmbeddedInodeLookUp* einode_io)
{
	ps_profiler->function_start();
	int32_t rtrn = -1;

	try
	{
		einode_io->get_inode(&einode, inode_number);
		rtrn = 0;
	} catch (ParentCacheException& e)
	{
		// nothing here
	} catch (EInodeIOException& e)
	{
		// nothing here
	}
	ps_profiler->function_end();

	return rtrn;
}

/**
 * Reads an einode from the storage, lookup by name.
 * @param einode The reference to the einode, which will be read.
 * @param parent_id The parent id of the einode.
 * @param name Pointer to the name string of the einode.
 * @param einode_io Pointer to the EmbeddedInodeLookUp object.
 * @return 0 if the operation was successful, -1 otherwise.
 */
int32_t InodeCache::fetch_from_storage(EInode& einode, InodeNumber parent_id, FsObjectName* name, EmbeddedInodeLookUp* einode_io)
{
	ps_profiler->function_start();
	int32_t rtrn = -1;

	try
	{
		einode_io->get_inode(&einode, parent_id, (char*) name);
		rtrn = 0;
	} catch (ParentCacheException& e)
	{
		// nothing here
	} catch (EInodeIOException& e)
	{
		// nothing here
	}
	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Reads an einode from the storage.
 * @param einode The reference to the einode, which will be read.
 * @param parent_id The parent id of the einode.
 * @param inode_number The inode number of the wanted einode.
 * @param einode_io Pointer to the EmbeddedInodeLookUp object.
 * @return 0 if the operation was successful, -1 otherwise.
 */
int32_t InodeCache::fetch_from_storage(EInode& einode, InodeNumber parent_id, InodeNumber inode_number, EmbeddedInodeLookUp* einode_io)
{
	ps_profiler->function_start();
	int32_t rtrn = -1;

	try
	{
		einode_io->get_inode(&einode, parent_id, inode_number);
		rtrn = 0;
	} catch (ParentCacheException& e)
	{
		// nothing here
	} catch (EInodeIOException& e)
	{
		// nothing here
	}
	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Add the elements to the parent cache map.
 * @param m Map with elements to add to the parent cache map.
 */
void InodeCache::add_to_parent_map(map<InodeNumber, InodeNumber>& m)
{
	ps_profiler->function_start();

	map<InodeNumber, InodeNumber>::iterator it;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	for(it = m.begin(); it != m.end(); ++it)
	{
		parent_cache_map.insert(pair<InodeNumber, InodeNumber>(it->first, it->second));
	}
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}

/**
 * @brief Removes the elements from the parent cache map.
 * @param s Set with the elements.
 */
void InodeCache::remove_fron_parent_map(set<InodeNumber>& s)
{
	ps_profiler->function_start();

	set<InodeNumber>::iterator it;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	for(it = s.begin(); it != s.end(); ++it)
	{
		parent_cache_map.erase(*it);
	}
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}


/**
 * @brief Sets the @see EmbeddedInodeLookUp object.
 * @param einode_io Pointer to the EmbeddedInodeLookUp object.
 */
void InodeCache::set_einode(EmbeddedInodeLookUp* einode_io)
{
	this->einode_io = einode_io;
}
