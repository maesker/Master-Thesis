/**
 * @file WriteBackProvider.cpp
 * @class  WriteBackProvider
 * @author Viktor Gottfried
 * @data 01.07.2011
 *
 * @brief Implements the write back functionality.
 */

#include <string.h>
#include <sys/time.h>
#include <iomanip>

#include "mm/journal/WriteBackProvider.h"

/**
 * @brief The Constructor of WriteBackProvider.
 */
WriteBackProvider::WriteBackProvider()
{
	ps_profiler = Pc2fsProfiler::get_instance();
}

/**
 * @brief The Destructor of WriteBackProvider.
 */
WriteBackProvider::~WriteBackProvider()
{
}

/**
 * @brief Starts the write back process, to write all changes from cache to the storage.
 */
void WriteBackProvider::start_full_write_back()
{
	ps_profiler->function_start();
	int32_t result = 0;
	list<InodeNumber> trash_list;
	list<EInode*> update_list;
	list<EInode*> new_list;

	list<EInode*>::iterator list_it;

	EInode *einode = NULL;

	map<InodeNumber, InodeCacheParentEntry*> dirty_parent_map;
	map<InodeNumber, InodeNumber> dirty_moved_map;
	set<InodeNumber> dirty_inodes_set;

	map<InodeNumber, InodeCacheParentEntry*>::iterator dpm_it;
	map<InodeNumber, InodeCacheParentEntry*>::iterator temp_dpm_it;
	map<InodeNumber, InodeNumber>::iterator dmm_it;
	set<InodeNumber>::iterator dis_it;

	set<int32_t> chunk_set;


	// get all dirty parent entries
	inode_cache->get_dirty_map(dirty_parent_map);

	for(dpm_it = dirty_parent_map.begin(); dpm_it != dirty_parent_map.end(); ++dpm_it)
	{
		InodeCacheParentEntry *pe = dpm_it->second;

		// First delete all inodes which are tagged as deleted.
		// get the deleted inodes
		pe->get_to_delete_set(dirty_inodes_set);
		// now remove all as deleted tagged inodes from the storage
		for(dis_it = dirty_inodes_set.begin(); dis_it != dirty_inodes_set.end(); ++dis_it)
		{
			result = pe->handle_write_back_delete(*dis_it, chunk_set);

			if( result == 0)
			{
				trash_list.push_back(*dis_it);
			}
			else if( result == -1)
			{
				cout << "deleting inode " << *dis_it << " failed" << endl;
			}
			remove_from_journal(*dis_it, chunk_set);
			chunk_set.clear();
		}
		dirty_inodes_set.clear();

		// get all inodes with updates
		pe->get_to_update_set(dirty_inodes_set, dirty_moved_map);

		// handle all moved inodes first
		for(dmm_it = dirty_moved_map.begin(); dmm_it != dirty_moved_map.end(); ++dmm_it)
		{
			temp_dpm_it = dirty_parent_map.find(dmm_it->second);

			/*
			 * First remove the einode from the old location.
			 * If this parent is not in the map, it was already written back by a previous run.
			 */
			if(temp_dpm_it != dirty_parent_map.end())
			{
				result = temp_dpm_it->second->handle_write_back_delete(dmm_it->first, chunk_set);

				if( result == 0 )
				{
					list<InodeNumber> temp_trash_list;
					temp_trash_list.push_back(dmm_it->first);
					einode_io->delete_inode_set(temp_dpm_it->second->get_inode_number(), &temp_trash_list);
				}
			}

			// write the moved einode to the new location
			einode = new EInode();
			result = pe->handle_write_back_update(dmm_it->first, chunk_set, *einode);

			// must be new einode
			if( result == 1)
			{
				new_list.push_back(einode);
			}
			// or must be new and tagged as deleted
			else if( result == 3)
			{
				delete einode;
			}
			// should not happen!
			else
			{
				cout << "Something wrong on move operation: " << dmm_it->first << endl;
				delete einode;
			}

			remove_from_journal(dmm_it->first, chunk_set);
			chunk_set.clear();
		}
		dirty_moved_map.clear();

		// now wirte all inodes with updates back to the storage
		for(dis_it = dirty_inodes_set.begin(); dis_it != dirty_inodes_set.end(); ++dis_it)
		{
			einode = new EInode();
			result = pe->handle_write_back_update(*dis_it, chunk_set, *einode);

			// update the einode
			if( result == 0)
			{
				update_list.push_back(einode);
			}
			// a new einode
			else if( result == 1)
			{
				new_list.push_back(einode);
			}
			// einode is tagged as deleted
			else if( result == 2)
			{
				trash_list.push_back(*dis_it);
				delete einode;
			}
			// einode is tagged as deleted, not on storage yet
			else if( result == 3)
			{
				delete einode;
			}
			else
			{
				cout << "Writing inode " << *dis_it << " failed" << endl;
				delete einode;
			}
			remove_from_journal(*dis_it, chunk_set);
			chunk_set.clear();
		}

		try
		{
			einode_io->delete_inode_set(dpm_it->first, &trash_list);
			einode_io->write_update_set(&update_list, dpm_it->first);
			einode_io->create_inode_set(&new_list, dpm_it->first);
		}
		catch (EInodeIOException& e) {
			cout << e.get_message() << endl;
		}

		// TODO write new einodes


		// delete from cache
		pe->clear_trash();
		trash_list.clear();

		for(list_it = update_list.begin(); list_it != update_list.end(); ++list_it)
		{
			delete *list_it;
		}
		update_list.clear();

		for(list_it = new_list.begin(); list_it != new_list.end(); ++list_it)
		{
			delete *list_it;
		}
		new_list.clear();


		dirty_inodes_set.clear();
	}

	// TODO write back the distributed operation here!

	clean_up();
	ps_profiler->function_end();
}

/**
 * @brief Runs the write back process for a single directory.
 * @param parent_number The inode number of the directory.
 * @return true if the operation was successful, false if it was iterrupted.
 */
bool WriteBackProvider::write_back_directory(InodeNumber parent_number)
{

	ps_profiler->function_start();
	InodeCacheParentEntry *pe = inode_cache->get_cache_parent_entry(parent_number);

	set<InodeNumber> dirty_inodes_set;
	set<InodeNumber>::iterator dis_it;

	set<int32_t> chunk_set;

	bool rtrn = true;

	pe->set_dirty(false);
	if( pe != NULL)
	{
		if(handle_smart_delete(pe) != 0)
		{
			pe->set_dirty(true);
			rtrn = false;
		}
		if(rtrn && handle_smart_write(pe) != 0)
		{
			pe->set_dirty(true);
			rtrn = false;
		}
	}

	clean_up();
	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Cleans the journal.
 */
void WriteBackProvider::clean_up()
{
	ps_profiler->function_start();
	set<int32_t>::iterator mc_it;
	for(mc_it = marked_chunks.begin(); mc_it != marked_chunks.end(); ++mc_it)
	{
		JournalChunk* jc = journal_cache->get(*mc_it);
		if(jc != NULL)
		{
			if(jc->size() == 0 && jc->is_closed())
			{
				jc->delete_chunk(*sal);
				journal_cache->remove(*mc_it);
		}
			else
				jc->write_chunk(*sal);
		}
	}
	marked_chunks.clear();

	ps_profiler->function_end();
}

/**
 * @brief Sets the journal cache.
 * @param jounal_cache Pointer to the journal cache.
 */
void WriteBackProvider::set_journal_cache(JournalCache* journal_cache)
{
	this->journal_cache = journal_cache;
}

/**
 * @brief Sets the operation cache.
 * @param operation_cache Pointer to the operation cache.
 */
void WriteBackProvider::set_operation_cache(OperationCache* operation_cache)
{
	this->operation_cache = operation_cache;
}

/**
 *	@brief Sets the inode cache.
 *	@param inode_cache Pointer to the inode cache.
 */
void WriteBackProvider::set_inode_cache(InodeCache* inode_cache)
{
	this->inode_cache = inode_cache;
}

/**
 * @brief Sets the @see StorageAbstractionLayer object.
 * @param sal Pointer to the @see StorageAbstractionLayer object.
 */
void WriteBackProvider::set_sal(StorageAbstractionLayer* sal)
{
	this->sal = sal;
}

/**
 * @brief Sets the EmbeddedInodeLookUp object.
 * @param einode_io Pointer to the @see EmbeddedInodeLookUp object.
 */
void WriteBackProvider::set_einode_io(EmbeddedInodeLookUp* einode_io)
{
	this->einode_io = einode_io;
}


/**
 * @brief Tries to execute the write back delete process on the cache entry.
 * After each delete it checks the last access to the cache and stops the
 * process if an access during the operation occurs.
 * @param pe Pointer to the cached object.
 * @return 0 if the process was successful, -1 if the process was interrupted.
 */
int32_t WriteBackProvider::handle_smart_delete(InodeCacheParentEntry* pe)
{
	ps_profiler->function_start();

	int32_t result = 0;
	list<InodeNumber> trash_list;
	set<InodeNumber> dirty_inodes_set;
	set<InodeNumber>::iterator dis_it;

	unsigned int counter = 0;
	set<int32_t> chunk_set;
	int32_t rtrn = 0;


	if( pe != NULL)
	{
		pe->get_to_delete_set(dirty_inodes_set);
		for(dis_it = dirty_inodes_set.begin(); dis_it != dirty_inodes_set.end() && rtrn == 0; ++dis_it)
		{
			result = pe->handle_write_back_delete(*dis_it, chunk_set);

			if(result == 0)
			{
				trash_list.push_back(*dis_it);
			}

			remove_from_journal(*dis_it, chunk_set);
			chunk_set.clear();
			counter++;

			if(access_check(pe) && counter < dirty_inodes_set.size())
			{
				rtrn = -1;
			}
		}

		try {
			einode_io->delete_inode_set(pe->get_inode_number(), &trash_list);
		} catch (EInodeIOException& e) {
			cout << e.get_message() << endl;
		}

		pe->clear_trash();
	}

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Tries to execute the write back process on the cachy entry.
 * After each write operation, it checks the last access to the cache and stops the
 * process if an access during the operation occurs.
 * @param pe Pointer to the cached object.
 * @return 0 if the process was successful, -1 if the process was interrupted.
 */
int32_t WriteBackProvider::handle_smart_write(InodeCacheParentEntry* pe)
{
	ps_profiler->function_start();
	int32_t result = 0;
	list<EInode*> update_list;
	list<EInode*> new_list;
	list<EInode*>::iterator list_it;
	list<InodeNumber> trash_list;
	EInode *einode;

	map<InodeNumber, InodeNumber> dirty_moved_map;
	set<InodeNumber> dirty_inodes_set;

	map<InodeNumber, InodeNumber>::iterator dmm_it;
	set<InodeNumber>::iterator dis_it;

	set<int32_t> chunk_set;
	unsigned int counter = 0;
	int32_t rtrn = 0;


	pe->get_to_update_set(dirty_inodes_set, dirty_moved_map);

	// handle all moved inodes first
	for(dmm_it = dirty_moved_map.begin(); dmm_it != dirty_moved_map.end()
		&& rtrn == 0; ++dmm_it)
	{
		InodeCacheParentEntry* old_parent;
		old_parent = inode_cache->get_cache_parent_entry(dmm_it->second);
		/*
		 * First remove the einode from the old location.
		 * If this parent is not in the map, it was already written back by a previous run.
		 */
		if(old_parent != NULL)
		{

			result = old_parent->handle_write_back_delete(dmm_it->first, chunk_set);

			if( result == 0 )
			{
				trash_list.push_back(dmm_it->first);
				einode_io->delete_inode_set(old_parent->get_inode_number(), &trash_list);
				trash_list.clear();
			}
			else if( result = -1)
			{
				cout << "deleting inode " << dmm_it->first << " failed" << endl;
			}
		}

		// write the moved einode to the new location
		einode = new EInode();
		result = pe->handle_write_back_update(dmm_it->first, chunk_set, *einode);

		// must be new einode
		if( result == 1)
		{
			new_list.push_back(einode);
		}
		// or must be new and tagged as deleted
		else if( result == 3)
		{
			delete einode;
		}
		// should not happen!
		else
		{
			cout << "Something wrong on move operation: " << dmm_it->first << endl;
			delete einode;
		}

		remove_from_journal(dmm_it->first, chunk_set);
		chunk_set.clear();

		counter++;

		if(access_check(pe) && counter < dirty_moved_map.size())
		{
			 rtrn = -1;
		}

	}
	counter = 0;

	// now wirte all inodes with updates back to the storage
	for(dis_it = dirty_inodes_set.begin(); dis_it != dirty_inodes_set.end() && rtrn == 0; ++dis_it)
	{
		einode = new EInode();
		result = pe->handle_write_back_update(*dis_it, chunk_set, *einode);


		// update the einode
		if( result == 0)
		{
			update_list.push_back(einode);
		}
		// a new einode
		else if( result == 1)
		{
			new_list.push_back(einode);
		}
		// einode is tagged as deleted
		else if( result == 2)
		{
			trash_list.push_back(*dis_it);
			delete einode;
		}
		// einode is tagged as deleted, not on storage yet
		else if( result == 3)
		{
			delete einode;
		}
		else
		{
			cout << "Writing inode " << *dis_it << " failed" << endl;
			delete einode;
		}

		remove_from_journal(*dis_it, chunk_set);
		chunk_set.clear();

		counter++;

		if(access_check(pe) && counter < dirty_moved_map.size())
		{
			rtrn = -1;
		}
	}

	try
	{
		einode_io->delete_inode_set(pe->get_inode_number(), &trash_list);
		einode_io->write_update_set(&update_list, pe->get_inode_number());
		einode_io->create_inode_set(&new_list, pe->get_inode_number());
	}
	catch (EInodeIOException& e) {
		cout << e.get_message() << endl;
	}

	// TODO write new einodes


	// delete from cache
	pe->clear_trash();
	trash_list.clear();

	for(list_it = update_list.begin(); list_it != update_list.end(); ++list_it)
	{
		delete *list_it;
	}
	update_list.clear();

	for(list_it = new_list.begin(); list_it != new_list.end(); ++list_it)
	{
		delete *list_it;
	}
	new_list.clear();


	dirty_inodes_set.clear();

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Checks the last access to an cached object.
 * @param pe Pointer to the cached object.
 * @return true if it was accessed recently (WRITE_BACK_INTERVALL), otherwise false.
 */
bool WriteBackProvider::access_check(InodeCacheParentEntry* pe)
{
	ps_profiler->function_start();

	timeval access_time;
	timeval current_time;
	uint64_t seconds;
	bool rtrn = false;

	access_time = pe->get_time_stamp();
	gettimeofday(&current_time, 0);
	seconds = current_time.tv_sec - access_time.tv_sec;
	if (seconds < WRITE_BACK_INTERVALL)
	{
		rtrn = true;
	}

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Removes an inode from the journal cache.
 * @param inode_number The number of the inode.
 * @param chunk_set Set of chunks that are holding the updates of the inode.
 */
void WriteBackProvider::remove_from_journal(InodeNumber inode_number, const set<int32_t> &chunk_set)
{
	ps_profiler->function_start();
	set<int32_t>::iterator it;

	for(it = chunk_set.begin(); it != chunk_set.end(); ++it)
	{
		JournalChunk* jc = journal_cache->get(*it);
		if( jc != NULL )
		{
			jc->remove_inode(inode_number);
		}
		marked_chunks.insert(*it);
	}
	ps_profiler->function_end();
}
