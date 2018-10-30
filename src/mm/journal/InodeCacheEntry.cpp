/**
 * @file InodeCacheEntry.cpp
 * @class InodeCacheEntry
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @date 01.08.2011
 *
 * @brief Represents a cache entry for an einode.
 * An entry contains the einode and a list of all updates, which are not written.
 * The list with the updates holds the information about each update, the type of the update and the location.
 * Further data:
 * parent id: The current parent id of the einode.
 * old parent: The previous parent id of the einode. The parent id can changed after a move operation.
 * created new: Flag that defines whether this entry is a new created one, not on storage or exists on storage.
 * trash: Flag that tags an entry as trash.
 */


#include "mm/journal/InodeCacheEntry.h"
#include "iostream"


/**
 * @brief Constructor of the InodeCacheEntry class.
 * Creates a cache entry for the given einode.
 * @param einode The einode that will be cached.
 */
InodeCacheEntry::InodeCacheEntry(const EInode& einode)
{
	ps_profiler = Pc2fsProfiler::get_instance();

	ps_profiler->function_start();

	this->indode_number = einode.inode.inode_number;
	this->einode = EInode(einode);
	this->parent_id = INVALID_INODE_ID;
	this->created_new = false;
	this->trash = false;
	this->old_parent_id = INVALID_INODE_ID;

	ps_profiler->function_end();
}

/**
 * @brief Constructor of the InodeCacheEntry class.
 * Creates a cache entry with the modifications data of an einode.
 * @param indode_number The number of the inode.
 * @param type The operation type.
 * @param chunk_number The chunk id of where the journaling operations is located.
 */
InodeCacheEntry::InodeCacheEntry(InodeNumber indode_number, OperationType type, int32_t chunk_number)
{
	ps_profiler = Pc2fsProfiler::get_instance();
	ps_profiler->function_start();

	this->indode_number = indode_number;
	ModificationMapping mm;
	mm.chunk_number = chunk_number;
	mm.type = type;
	changes_list.push_back(mm);
	einode.inode.inode_number = INVALID_INODE_ID;
	this->old_parent_id = INVALID_INODE_ID;
	this->trash = false;


	ps_profiler->function_end();
}

/**
 * @brief Get the inode number from the cache entry.
 * @return The number of the inode.
 */
InodeNumber InodeCacheEntry::get_indode_number() const
{
	return indode_number;
}

/**
 * @brief Sets the inode number.
 * @param indode_number The number of the inode.
 */
void InodeCacheEntry::set_indode_number(InodeNumber indode_number)
{
	this->indode_number = indode_number;
}

/**
 * @brief Adds an operation type and the location of the journaling operation to the cache.
 * @param type The operation type.
 * @param chunk_number The location of the journaling operation.
 */
void InodeCacheEntry::add(OperationType type, int32_t chunk_number)
{
	ps_profiler->function_start();

	ModificationMapping mm;
	mm.chunk_number = chunk_number;
	mm.type = type;
	changes_list.push_back(mm);

	ps_profiler->function_end();
}

/**
 * @brief Get the type of the last operation of the inode that was cached.
 * @param[out] type Reference to set the last operation type.
 * @return 0 if the operation was successful, 1 if the list of the entry is empty.
 */
int32_t InodeCacheEntry::get_last_type(OperationType& type) const
{
	ps_profiler->function_start();

	int32_t rtrn = 1;

	if(!changes_list.empty())
	{
		rtrn = 0;
		type = changes_list.back().type;
	}

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Get the chunk number of the last operation of the inode that was cached.
 * @return The last chunk number.
 */
int32_t InodeCacheEntry::get_last_chunk_number() const
{
	return changes_list.back().chunk_number;
}

/**
 * @brief Get the chunk number of the first operation of the inode that was cached.
 * @return The first chunk number.
 */
int32_t InodeCacheEntry::get_first_chunk_number() const
{
	return changes_list.front().chunk_number;
}

/**
 * @brief Get the modification of last operation of the inode that was cached.
 * @return The last modification.
 */
ModificationMapping InodeCacheEntry::get_last_modification() const
{
	return changes_list.back();
}

/**
 * @brief Get the list with all modifications of the inode.
 * @return Reference of the list.
 */
void InodeCacheEntry::get_list(list<ModificationMapping>& list) const
{
	list = changes_list;
}

/**
 * @brief Counts the number of entries inside the cache entry.
 * @return The number of entries.
 */
int32_t InodeCacheEntry::count() const
{
	return changes_list.size();
}

/**
 * @brief Gets the einode.
 * @return The einide.
 */
const EInode& InodeCacheEntry::get_einode() const
{
	return einode;
}

/**
 * @brief Sets the einode.
 * @param einode The einode.
 */
void InodeCacheEntry::set_einode(const EInode& einode)
{
	this->einode = einode;
}

/**
 * @brief Updates the inode cache entry.
 * The einode attributes are updated by using a bitfield.
 * @param einode The einode with the new attribute values.
 * @param bitfield Represents the attributes that are changed.
 * @return 0 if the operation was successful, 1 the cache was not updated because the
 * update only contained atime and mtime update and this values are up-to-date in the cache.
 */
int32_t InodeCacheEntry::update_inode_cache(const EInode& einode, int32_t bitfield)
{
	ps_profiler->function_start();

	/*
	 * check for mtime/atime
	 * used for optimizations, because the nfs client tries to update the
	 * mtime/atime although attributes are up-to-date
	 * The check is done to avoid one unnecessary journaling operation.
	 */
	if(  only_mtime_atime(bitfield) )
	{
		if(this->einode.inode.mtime == einode.inode.mtime && this->einode.inode.atime == einode.inode.atime)
		{
			ps_profiler->function_end();
			return 1;
		}
	}

	// check if this is the first operaiton to this cache entry
	if(this->einode.inode.inode_number == INVALID_INODE_ID)
	{
		this->created_new = true;
		this->einode = einode;

		ps_profiler->function_end();
		return 0;
	}

	// otherwise it is an update operation
	else if(bitfield == 0)
		this->einode = einode;

	if(IS_CTIME_SET(bitfield))
		this->einode.inode.ctime = einode.inode.ctime;
	if (IS_ATIME_SET(bitfield))
		this->einode.inode.atime = einode.inode.atime;

	if (IS_MTIME_SET(bitfield))
		this->einode.inode.mtime = einode.inode.mtime;

	// mode is a special case, it is also a bitfield
	if (IS_MODE_SET(bitfield))
	{
        mode_t new_mode = einode.inode.mode;
        //see 'man 2 chmod' for a description of the macros
        update_mode( new_mode, S_ISUID );
        update_mode( new_mode, S_ISGID );
        update_mode( new_mode, S_ISVTX );
        update_mode( new_mode, S_IRUSR );
        update_mode( new_mode, S_IWUSR );
        update_mode( new_mode, S_IXUSR );
        update_mode( new_mode, S_IRGRP );
        update_mode( new_mode, S_IWGRP );
        update_mode( new_mode, S_IXGRP );
        update_mode( new_mode, S_IROTH );
        update_mode( new_mode, S_IWOTH );
        update_mode( new_mode, S_IXOTH );
	}
	if (IS_SIZE_SET(bitfield))
		this->einode.inode.size = einode.inode.size;

	if (IS_GID_SET(bitfield))
		this->einode.inode.gid = einode.inode.gid;

	if (IS_UID_SET(bitfield))
		this->einode.inode.uid = einode.inode.uid;

	if (IS_NLINK_SET(bitfield))
		this->einode.inode.link_count = einode.inode.link_count;

	if (IS_HAS_ACL_SET(bitfield))
		this->einode.inode.has_acl = einode.inode.has_acl;

	ps_profiler->function_end();

	return 0;
}

/**
 * @brief Gets the set of all chunk id, that containing modification of the inode.
 * @param[out] chunk_list The list where the chunk id would be written.
 */
void InodeCacheEntry::get_chunk_set(set<int32_t>& chunk_set)
{
	ps_profiler->function_start();

	list<ModificationMapping>::iterator it;
	for(it = changes_list.begin(); it != changes_list.end(); it++)
	{
		chunk_set.insert(it->chunk_number);
	}

	ps_profiler->function_end();
}

/**
 * @brief Test whether the inode is a new created one.
 * @return true if the inode is new created, otherwise false.
 */
bool InodeCacheEntry::is_created_new() const
{
	return created_new;
}

/**
 * @brief Sets the created new flag.
 * @param b The boolean flag.
 */
void InodeCacheEntry::set_created_new(bool b)
{
	this->created_new = b;
}

/**
 * @brief Test whether the inode is tagged as trash.
 * @return true if it is tagged as trash, otherwise false.
 */
bool InodeCacheEntry::is_trash() const
{
	return trash;
}

/**
 * @brief Sets the trash flag.
 * @param b The boolean flag.
 */
void InodeCacheEntry::set_trash(bool b)
{
	this->trash = b;
}

/**
 * @brief Sets the parent id of the inode.
 * @param parent_id The parent id.
 */
void InodeCacheEntry::set_parent_id(InodeNumber parent_id)
{
	this->parent_id = parent_id;
}

/**
 * @brief Gets the parent id.
 * The parent id must be only set, if the inode is a new created inode, the created_new must be also set.
 * @return The parent id of the inode.
 */
InodeNumber InodeCacheEntry::get_parent_id() const
{
	return parent_id;
}


/**
 * @brief Clears the the list that holds the modification information.
 */
void InodeCacheEntry::clear()
{
	changes_list.clear();
}

/**
 * @bbrief Updates the mode bitfield.
 * @param new_mode Bitfield with the new mode flags.
 * @parma flag The flag to set.
 */
void InodeCacheEntry::update_mode(int new_mode, int flag)
{
	ps_profiler->function_start();

	//determine if flag should be set
	if(new_mode & flag)
	{
		//set the flag
		einode.inode.mode |= flag;
	} else {
		//make sure that it is not set
		einode.inode.mode &=(~flag);
	}

	ps_profiler->function_end();
}

/**
 * @brief Gets the inode number of the old parent.
 * @return the inode number of the old parent.
 */
InodeNumber InodeCacheEntry::get_old_parent_id() const
{
	return this->old_parent_id;
}

/**
 * @brief Sets the old parent id.
 * @param inode_number The inode number of the old parent id.
 */
void InodeCacheEntry::set_old_parent_id(InodeNumber inode_number)
{
	this->old_parent_id = inode_number;
}

/**
 * Checks whether only the mtime a atime is set.
 * Used for optimizations.
 * @return true if only mtime and atime are set, otherwise false.
 */
bool InodeCacheEntry::only_mtime_atime(int32_t bitfield)
{
	if(IS_ATIME_SET(bitfield) && IS_MTIME_SET(bitfield))
	{
		if( !IS_GID_SET(bitfield) && !IS_HAS_ACL_SET(bitfield) && !IS_MODE_SET(bitfield)
				&& !IS_NLINK_SET(bitfield) && !IS_SIZE_SET(bitfield) && !IS_UID_SET(bitfield) )
		{
			return true;
		}
	}

	return false;
}
