/**
 * @file Journal.cpp
 * @class Journal
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @date 01.06.2011
 *
 * @brief The Journal class represents the journal and is the main interface between the
 * metadata-server and the storage.
 *
 * A journal is named using a 64 bit positive integer value, the InodeNumber.
 * The Journal maintains the journaling functionality and acts an interface to the metadata storage.
 * It has direct access to the @see StorageAbstractionLayer the @see EmbeddedInodeLookUp and is responsible
 * to maintain the metadata consistency.
 *
 * The journal follows a strict consistency policy.
 * Each operation which updates the metadata will only return as soon as the data is
 * written inside a journal entry to the permanent storage.
 * The journal is divided into small chunks @see JournalChunk.
 * Each chunk can contain a fixed number of entries.
 * As soon as an operation arrives at the journal, it is put into a chunk and written to the storage.
 * A chunk exists always simultaneously on the storage and and in the journal cache.
 * A write back thread is responsible to write the data from the journal to the storage.
 * Of course the journal data is on the permanent storage at any time.
 * With writing back data we mean write back the einode data as native einodes
 * to the storage and clean up the journal.
 *
 * After the journal creation, it is necessary to start the write back tread.
 * @see start() Starts the write back thread.
 * @see stop() Stops the write back thread and make an last clean up run.
 */



#include <iostream>
#include <string>
#include <sstream>
#include <string.h>

#include "mm/journal/StringHelper.h"
#include "mm/journal/Journal.h"
#include "mm/journal/JournalException.h"
#include "mm/einodeio/EInodeIOException.h"
#include "mm/einodeio/ParentCacheException.h"
#include "mm/einodeio/EmbeddedInodeLookUp.h"
#include "mm/journal/FailureCodes.h"
#include "mm/journal/CommonJournalTypes.h"
#include "mm/journal/CommonJournalFunctions.h"

#include "global_types.h"

using namespace std;

/**
 * @brief Default constructor of the journal.
 * Initilaize the mutex and condition variable.
 */
Journal::Journal()
{
	mutex = PTHREAD_MUTEX_INITIALIZER;
	wbt_mutex = PTHREAD_MUTEX_INITIALIZER;
	wbt_cond = PTHREAD_COND_INITIALIZER;
};

/**
 * @brief Constructor of the journal.
 * Creates a new journal with the given journal id.
 * @param journal_id The id of the journal.
 */
Journal::Journal(uint64_t journal_id)
{
	init(journal_id, 1);
}

/**
 * @brief Constructor of the journal.
 * Creates a journal with the given journal id and set the chunk number.
 * @param journal_id The id of the journal.
 * @param chunk_number The chunk number, that will be used for the next chunk in the journal.
 */
Journal::Journal(uint64_t journal_id, int32_t chunk_number)
{
	init(journal_id, chunk_number);
}

/**
 * @brief Initialize all journal parameters.
 * Initializes all necessary journal paramaters.
 * @param journal_id The id of the journal.
 * @param chunk_number The chunk number, that will be used for the next chunk in the journal.
 */
void Journal::init(uint64_t journal_id, int32_t chunk_number)
{
	set_journal_id(journal_id);
	log = new Logger();
	log->set_log_level(1);
	log->set_console_output((bool) 0);

	char* filename = (char *) malloc(8);
	snprintf(filename, 8, "%llu", journal_id);
	log->set_log_location(std::string(DEFAULT_LOG_DIR) + "/journal" + filename + ".log");
	log->debug_log( "starting journal." );
	free(filename);

	current_chunk_id = chunk_number;
	journal_cache = new JournalCache();
	active_chunk = new JournalChunk(current_chunk_id, prefix, journal_id);

	journal_cache->add(active_chunk);
	operation_cache = new OperationCache();
	inode_cache = new InodeCache();

	wbc.set_inode_cache(inode_cache);
	wbc.set_journal_cache(journal_cache);
	wbc.set_operation_cache(operation_cache);

	mutex = PTHREAD_MUTEX_INITIALIZER;
	wbt_mutex = PTHREAD_MUTEX_INITIALIZER;
	wbt_cond = PTHREAD_COND_INITIALIZER;

	recovery_mode = false;
	ps_profiler = Pc2fsProfiler::get_instance();
}

/**
* Destructor of the Journal.
*/
Journal::~Journal()
{
	delete journal_cache;
	delete operation_cache;
	delete inode_cache;
	delete log;
	pthread_mutex_destroy(&mutex);
	pthread_mutex_destroy(&wbt_mutex);
}

/**
 * @brief Starts the journal wirte back thread.
 * @throws JournalException If the creation of a thread fails.
 */
void Journal::start()
{
	log->debug_log("Starting journal %ul write back thread!", journal_id);
	writing_back = true;

	if (pthread_create(&write_back_thread, NULL, &Journal::start_write_back_thread, this) != 0)
	{
		// creating a thread failed
		throw JournalException("An error occurred while creating the write back thread!");
	}
}

/**
 * @brief Stops the write back thread.
 * This function blocks until the write back thread finished the process.
 * @throws JournalException If the join of the thread fails.
 */
void Journal::stop()
{
	close_journal();

	pthread_mutex_lock(&wbt_mutex);
	writing_back = false;

	// write back thread may be still in sleep mode, wake it up
	pthread_cond_signal(&wbt_cond);

	pthread_mutex_unlock(&wbt_mutex);
	if (pthread_join(write_back_thread, NULL) != 0)
	{
		// joining a thread failed
		throw JournalException("An error occurred during joining a thread.");
	}

	// runs the write back process the last time to clean up
	run_write_back();
}

/**
 * @brief Helper function to start the write back thread.
 * @param ptr Pointer to the journal manager instance.
 */
void* Journal::start_write_back_thread(void *ptr)
{
	Journal* journal = static_cast<Journal*> (ptr);
	if(SMART_WRITE_BACK)
	{
		journal->handle_smart_wbt();
	}
	else
	{
		journal->handle_full_wbt();
	}
	return NULL;
}

/**
 * @brief This method is used by the write back thread.
 * The thread is executed in a pre defined interval and writes all upates to the storage.
 */
void Journal::handle_full_wbt()
{
	timespec absolute_time;

	while (writing_back)
	{
		pthread_mutex_lock(&wbt_mutex);

		absolute_time.tv_sec = time(NULL) + WRITE_BACK_SLEEP;
		absolute_time.tv_nsec = 0;

		pthread_cond_timedwait(&wbt_cond, &wbt_mutex, &absolute_time);

		run_write_back();

		// check for interruption
		if (writing_back)
			pthread_mutex_unlock(&wbt_mutex);
		else
		{
			pthread_mutex_unlock(&wbt_mutex);
			return;
		}
	}
}

/**
 * @brief This method is used for the write back thread.
 */
void Journal::handle_smart_wbt()
{
	timeval current_time;
	timespec absolute_time;

	vector<AccessData> a_data;
	vector<AccessData>::iterator it;
	uint64_t cache_size = 0;

	log->debug_log("Smart write back started");

	while (writing_back)
	{
		pthread_mutex_lock(&wbt_mutex);


		absolute_time.tv_sec = time(NULL) + WRITE_BACK_SLEEP;
		absolute_time.tv_nsec = 0;

		log->debug_log("Go into sleep mode for %d seconds.", WRITE_BACK_SLEEP);
		pthread_cond_timedwait(&wbt_cond, &wbt_mutex, &absolute_time);
		log->debug_log("Waking up.");

		// get cache access information
		inode_cache->about_cache(a_data);

		// sort the vector in ascending order by access time
		sort(a_data.begin(), a_data.end(), compareAccessTime());
		gettimeofday(&current_time, 0);
		it = a_data.begin();
		// get the first dirty entry
		while( it != a_data.end() && it->last_access.tv_sec < current_time.tv_sec - WRITE_BACK_INTERVALL)
		{
			log->debug_log("Checking access data of: %llu", it->inode_numer);
			cache_size += it->cache_size;
			if(it->dirty)
			{
				log->debug_log("Starting to write directory: %llu", it->inode_numer);
				bool r = wbc.write_back_directory(it->inode_numer);

				// set current time
				gettimeofday(&current_time, 0);
				// if the write back was successful without interrupt get the current cache information again
				if(r)
				{
					log->debug_log("Writing of %llu was successful.", it->inode_numer);
					a_data.clear();
					inode_cache->about_cache(a_data);
					sort(a_data.begin(), a_data.end(), compareAccessTime());
					it = a_data.begin();
				}
				else
				{
					log->debug_log("Writing of %llu was interrupted.", it->inode_numer);
					it++;
				}
			}
			else if(it->last_access.tv_sec < current_time.tv_sec - CACHE_LIFETIME)
			{
				log->debug_log("Cache is out of lifetime: %llu", it->inode_numer);
				inode_cache->remove_entry(it->inode_numer);
				cache_size = cache_size - it->cache_size;
				it++;
			}
			else
			{
				it++;
			}
		}

		cache_size = cache_size + journal_cache->size() * CHUNK_SIZE;

		// check the cache size
		if(cache_size >= MAX_CACHE_SIZE)
		{
			// TODO handle emergency write back
		}

		cache_size = 0;
		a_data.clear();

		// check for interruption
		if (writing_back)
			pthread_mutex_unlock(&wbt_mutex);
		else
		{
			log->debug_log("Leaving write back");
			pthread_mutex_unlock(&wbt_mutex);
			return;
		}
	}
}

/**
 * @brief Sets the current chunk id and creates a new chunk with this id.
 * @param chunk_id The id of the chunk.
 */
void Journal::set_chunk(int32_t chunk_id)
{
	current_chunk_id = chunk_id;
	active_chunk = new JournalChunk(current_chunk_id, prefix, journal_id);
	journal_cache->add(active_chunk);
}


/**
 * @brief Journals an operation.
 * Updates the cache and writes the operation to the long term storage.
 * @parma operation Pointer to the operation.
 * @return 0 if the operation was successful.
 */
int Journal::journal_operation(Operation* operation)
{


	ps_profiler->function_start();

	int rtrn = 0;
	int32_t cache_result = 0;

	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	// if operation is distributed, cache the information of it in the operation cache
	if (operation->get_mode() == OperationMode::Disributed)
	{
		log->debug_log( "distributed, operation id: %lu", operation->get_operation_id() );

		operation_cache->add(operation->get_operation_id(), current_chunk_id, operation->get_status());

		// if the operation includes create/delete or set attribute on a inode, put it to the inode cache
		if (operation->get_type() == OperationType::CreateINode || operation->get_type() == OperationType::DeleteINode
				|| operation->get_type() == OperationType::SetAttribute)
		{
			log->debug_log( "operates on an inode, inode id: %lu", operation->get_inode_id() );
			inode_cache->update_inode_cache(operation, current_chunk_id);
		}
	}

	// the operation is an atomic operation on an inode, cache it
	else if (operation->get_mode() == OperationMode::Atomic)
	{
		log->debug_log( "atomic operation, inode id: %lu", operation->get_inode_id() );
		log->debug_log( "update type: %i", operation->get_type());
		cache_result = inode_cache->update_inode_cache(operation, current_chunk_id);
		log->debug_log( "Inode is now in the cache..." );
	}
	if( cache_result == 0)
	{
		rtrn = put_into_chunk(operation);
	}


	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Puts the operation into the chunk.
 * If the chunk is full, a new chunk is created.
 * Every time this function is called, the active chunk is written to the long term storage.
 * @parma operation Pointer to the operation.
 * @return 0 if the operation was successful, otherwise error code is returned.
 * JournalEmpty - Nothing to write.
 * ChunkToLarge - The chunk size exceeds the defined maximum size.
 * StorageAccessError - Chunk was not written to the storage,
 * because of an unexpected storage access error.
 */
int Journal::put_into_chunk(Operation* operation)
{
	ps_profiler->function_start();
	int rtrn = 0;

	if (!recovery_mode)
	{
		rtrn = active_chunk->add_write_operation(operation, *sal);
		log->debug_log( "Operation written to chunk... " );

		// check whether the journaling of the operation was successful
		if (rtrn != 0)
		{
			ps_profiler->function_end();
			log->error_log( "Error during journalig an operation." );
			return rtrn;
		}

		// if the chunk is full
		if (active_chunk->size() == CHUNK_SIZE)
		{
			log->debug_log( "Chunk is full, create a new one." );

			active_chunk->set_closed(true);

			current_chunk_id++;
			active_chunk = new JournalChunk(current_chunk_id, prefix, journal_id);

			// put the chunk into the journal cache
			journal_cache->add(active_chunk);
		}
	}

	ps_profiler->function_end();
	return 0;
}

/**
 * @brief Checks if the inode already exists in the cache or in the long term storage.
 * @param inode_id The id of the inode.
 * @return true if the inode exists in the cache or on the long term storage, otherwise false.
 */
bool Journal::check_inode(InodeNumber inode_id)
{
	ps_profiler->function_start();

	// check the operation, inode already exists?
	bool rtrn = false;
	EInode einode;
	CacheStatusType cache_type = CacheStatusType::NotPresent;
	cache_type = inode_cache->get_einode(inode_id, einode, inode_io);

	log->debug_log( "Check result for EInode %llu: %d.", inode_id, cache_type );
	switch (cache_type) {

		case CacheStatusType::Present:
			rtrn = true;
			break;
		case CacheStatusType::Deleted:
			rtrn = false;
			break;
		case CacheStatusType::NotPresent:
			rtrn = false;
			break;
		default:
			log->debug_log( "Unexpected type: %d", cache_type );
			rtrn = false;
			break;
	}
	ps_profiler->function_end();

	return rtrn;
}

/**
 * @brief Creates an distributed operation for deleting an existing inode.
 * @param operation_id The id of the distributed operation.
 * @param inode_id The id of the inode.
 * @return 0 if the operation was successful, otherwise an error code is returned.
 * InodeNotExists - Was not able to delete the inode, because it does not exist.
 */
int32_t Journal::handle_distributed_delete_inode(uint64_t operation_id, InodeNumber inode_id)
{
	ps_profiler->function_start();

	int rtrn = 0;

	log->debug_log( "operation_id = %lu, inode_id = %lu", operation_id, inode_id );
	if (!check_inode(inode_id))
	{
		ps_profiler->function_end();
		return JournalFailureCodes::InodeNotExists;
	}
	Operation *o = new Operation(operation_id);
	o->set_operation_number(operation_cache->get_next_number(operation_id));
	o->set_type(OperationType::DeleteINode);
	o->set_status(OperationStatus::Committed);
	o->set_mode(OperationMode::Disributed);
	o->set_inode_id(inode_id);
	o->set_parent_id(INVALID_INODE_ID);
	o->set_module(Module::MDS);

	rtrn = journal_operation(o);

	ps_profiler->function_end();

	return rtrn;
}

/**
 * @brief Creates an distributed operation for creating a new inode.
 * @param inode_id The inode id of the new inode.
 * @param inode_attributes Attributes of the new inode.
 * @param parent_id The inode parent id.
 * @param parant_id The parent id of the inode.
 * @return 0 if the operation was successful, otherwise an error code is returned.
 * InodeExists - Was not able to create an inode, because an indode with the same id already exists.
 */
int32_t Journal::handle_distributed_create_inode(uint64_t operation_id, InodeNumber parent_id, const EInode& p_einode)
{
	log->debug_log( "operation_id = %lu, inode_id = %lu", operation_id, p_einode.inode.inode_number );
	// check if the inode already exists
	if (check_inode(p_einode.inode.inode_number))
		return JournalFailureCodes::InodeExists;

	Operation *o = new Operation(operation_id);
	o->set_operation_number(operation_cache->get_next_number(operation_id));
	o->set_type(OperationType::CreateINode);
	o->set_status(OperationStatus::Committed);
	o->set_mode(OperationMode::Disributed);
	o->set_einode(p_einode);
	o->set_parent_id(parent_id);
	o->set_module(Module::MDS);
	o->set_bitfield(0);

	return journal_operation(o);
}

/**
 * @brief Adds a distributed operation to the journal.
 * @param id The id of the operations.
 * @param module The module, that creates the opeation.
 * @param type The type of the operation.
 * @param status The status of the operation.
 * @param data The data, that includes further information about the operation.
 * @param data_size The size of the data. The data size must be smaller or equal to OPERATION_SIZE.
 * @return 0 if the addition was successfull, otherwise an postive error code is returned.
 */
int32_t Journal::add_distributed(uint64_t id, Module module, OperationType type, OperationStatus status, char* data,
		uint32_t data_size)
{
	log->debug_log( "operation_id = %lu", id );

	Operation *o = new Operation(id);
	o->set_mode(OperationMode::Disributed);
	o->set_operation_number(operation_cache->get_next_number(id));
	o->set_type(type);
	o->set_status(status);
	o->set_module(module);
	o->set_parent_id(INVALID_INODE_ID);
	o->set_inode_id(INVALID_INODE_ID);

	int rt;
	if ((rt = o->set_data(data, data_size)) != 0)
	{
		return rt;
	}

	return journal_operation(o);
}

/**
 * @brief Set the inode id.
 * @param inode_id The inoed id.
 */
void Journal::set_journal_id(unsigned long journal_id)
{
	this->journal_id = journal_id;
	// construct the prefix
	ostringstream ost;
	ost << journal_id;

	prefix = ost.str();
}

/**
 * @brief Gets the inode id.
 * @param inode_id The inoed id.
 */
InodeNumber Journal::get_journal_id()
{
	return this->journal_id;
}

/**
 * @brief Sets the storage abstraction layer.
 * @param sal Reference to the torage abstraction layer.
 */
void Journal::set_sal(StorageAbstractionLayer* sal)
{
	this->sal = sal;
	wbc.set_sal(this->sal);
}

/*
 * @brief Gets the sal object.
 * @return Reference to the sal.
 */
StorageAbstractionLayer& Journal::get_sal()
{
	return *sal;
}

/**
 * @brief Gets the prefix of the journal.
 * @return The prefix.
 */
const string& Journal::get_prefix() const
{
	return prefix;
}

/**
 * @brief Get the last created operation with the given operation id.
 * @param[in] operation_id The id of the wanted operation.
 * @param[out] operation Specifies the last operation.
 * @return 0 if the operation was successful, otherwise an error code is returned.
 * NotInCache The desired operation is not in the cache.
 */
int32_t Journal::get_last_operation(uint64_t operation_id, Operation& operation)
{
	OperationMapping om;
	if (operation_cache->get_last(operation_id, om) != 0)
		return JournalFailureCodes::NotInCache;

	if (journal_cache->get_operation(operation, operation_id, om.chunk_number, om.operation_number) != 0)
		return JournalFailureCodes::NotInCache;

	return 0;
}

/**
 * @brief Get all operation with the given operation id.
 * @param[in] operation_id The id of the wanted operation.
 * @param[out] operations Vector to store the wanted operations.
 * @return The number of the operations with the given id,
 * 0 if the journal does not contain an operation with the given id.
 */
int32_t Journal::get_all_operations(uint64_t operation_id, vector<Operation>& operations)
{
	int32_t count = 0;
	set<int32_t> chunks;

	count = operation_cache->get_all_chunks(operation_id, chunks);

	if (count > 0)
	{
		set<int32_t>::iterator vit;

		// first get all operations and put them into the temporary vector
		for (vit = chunks.begin(); vit != chunks.end(); ++vit)
		{
			journal_cache->get(*vit)->get_operations(operation_id, operations);
		}

		// sort the vector, because returned order is not determined
		sort(operations.begin(), operations.end(), Operation::compareOperationNumberFunctor());
	}
	return count;
}

/**
 * @brief Gets all open operations.
 * Writes the ids of all the operations which are not committed and aborted into the set.
 * @param[out] open_operations Set will hold all the open operation ids.
 */
void Journal::get_open_operations(set<uint64_t> open_operations) const
{
	operation_cache->get_all_open(open_operations);
}

/**
 * @brief Sets the journal cache.
 * @param journal_cache Pointer to the journal cache object.
 */
void Journal::set_journal_cache(JournalCache* journal_cache)
{
	this->journal_cache = journal_cache;
	wbc.set_journal_cache(journal_cache);
}

/**
 * @brief Sets the inode cache.
 * @param inode_cache Pointer to the inode cache object.
 */
void Journal::set_inode_cache(InodeCache* inode_cache)
{
	this->inode_cache = inode_cache;
	wbc.set_inode_cache(inode_cache);
}

/**
 * @brief Sets the operation cache.
 * @param operation_cache Pointer to the operation cache object.
 */
void Journal::set_operation_cache(OperationCache* operation_cache)
{
	this->operation_cache = operation_cache;
	wbc.set_operation_cache(operation_cache);
}

/**
 * @brief Adds an journal entry about the starting of the migration of a subtree.
 */
int32_t Journal::start_migration(InodeNumber inode_id, char* data, int32_t data_size)
{
	// TODO implementation
	return 0;
}

/**
 * Adds an journal entry that the migration was successful finished.
 */
int32_t Journal::finish_migration(InodeNumber inode_id)
{
	// TODO implementation
	return 0;
}

int32_t Journal::abort_migration(InodeNumber inode_id)
{
	// TODO implementation
	return 0;
}

/**
 * @brief Sets the EmbeddedInodeLookUp object.
 * @param inode_io The EmbeddedInodeLookUp object.
 */
void Journal::set_inode_io(EmbeddedInodeLookUp* inode_io)
{
	this->inode_io = inode_io;
	wbc.set_einode_io(this->inode_io);
	inode_cache->set_einode(this->inode_io);
}

/**
 * @brief Runs the write back process.
 */
void Journal::run_write_back()
{
	ps_profiler->function_start();
	wbc.start_full_write_back();
	ps_profiler->function_end();
}

/** 
 * @brief Writes the complete einode data to the provided einode struct pointer
 * @param[in] p_inode_number pointer to the inode number of the corresponding fs object.
 * @param[out] p_einode pointer to the EInode structure to write the data to.
 * @return 0 if the request was successful. -1 if the einode with the given inode id is unknown.
 * */
int Journal::handle_mds_einode_request(InodeNumber *p_inode_number, struct EInode *p_einode)
{
	ps_profiler->function_start();

	log->debug_log( "Request for inode %u received.", *p_inode_number );

	int rtrn = -1;
	CacheStatusType type = inode_cache->get_einode(*p_inode_number, *p_einode, inode_io);


	if( type == CacheStatusType::Present )
	{
		log->debug_log( "Inode is in the cache, returning einode with name:%s.", p_einode->name );
		rtrn = 0;
	}
	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Journaling a create inode operation.
 * @param[in] p_parent_inode_number pointer to the inode number of the 
 * directory the fs object is supposed to be written to.
 * @param[in] p_einode pointer to the einode structure that is supposed
 * to be written to storage.
 * @return 0 on success, otherwise an error code is returned.
 * InodeExists: Creation of an inode failes if the an inode with the same id already exists.
 */
int Journal::handle_mds_create_einode_request(InodeNumber *p_parent_inode_number, struct EInode *p_einode)
{
	log->debug_log( "Create inode %llu in parent inum %llu", p_einode->inode.inode_number, *p_parent_inode_number );
	log->debug_log( "name:%s.", p_einode->name );


	ps_profiler->function_start();

	int rtrn = 0;

	if (check_inode(p_einode->inode.inode_number))
	{
		ps_profiler->function_end();
		return JournalFailureCodes::InodeExists;
	}
	Operation* o = new Operation(0);
	o->set_operation_number(0);
	o->set_type(OperationType::CreateINode);
	o->set_status(OperationStatus::Committed);
	o->set_mode(OperationMode::Atomic);
	o->set_einode(*p_einode);
	o->set_parent_id(*p_parent_inode_number);
	o->set_bitfield(0);

	o->set_module(Module::MDS);

	rtrn = journal_operation(o);

	ps_profiler->function_end();

	return rtrn;
}

/** @brief Check the bitfield and update the flaged attributes of the
 * provided inode number.
 * @param[in] p_inode_number pointer to the inode number of the 
 * corresponding fs object.
 * @param[in] p_attrs pointer to the attributes structure that contains
 * the new attribute values.
 * @param[in] attribute_bitfield a bitfield to describe the values of 
 * the attrs structure that contain new data and musst be updated.
 * @return integer return code
 * */
int Journal::handle_mds_update_attributes_request(InodeNumber *p_inode_number, inode_update_attributes_t *p_attrs,
		int *p_attribute_bitfield)
{
	log->debug_log( "Updating %u.", *p_inode_number );
	log->debug_log( "Updateing mode=%u.", p_attrs->mode );

	ps_profiler->function_start();

	int rtrn = 0;

	if (!check_inode(*p_inode_number))
	{
		ps_profiler->function_end();
		return JournalFailureCodes::InodeNotExists;
	}
	EInode einode;
	einode.inode.atime = p_attrs->atime;
	einode.inode.gid = p_attrs->gid;
	einode.inode.has_acl = p_attrs->has_acl;
	einode.inode.mode = p_attrs->mode;
	einode.inode.mtime = p_attrs->mtime;
	einode.inode.inode_number = *p_inode_number;
	einode.inode.link_count = p_attrs->st_nlink;
	einode.inode.uid = p_attrs->uid;
	einode.inode.size = p_attrs->size;

	Operation *o = new Operation(0);
	o->set_operation_number(0);
	o->set_type(OperationType::SetAttribute);
	o->set_status(OperationStatus::Committed);
	o->set_mode(OperationMode::Atomic);
	o->set_einode(einode);
	o->set_bitfield(*p_attribute_bitfield);
	o->set_parent_id(INVALID_INODE_ID);
	o->set_module(Module::MDS);

	rtrn = journal_operation(o);

	ps_profiler->function_end();

	return rtrn;
}


/**
 * @brief Changes einode name and location.
 * @param new_parent_id The inode number of the new location.
 * @param new_name Pointer to the string of the new name.
 * @param old_parent_id The inode number of the old location.
 * @param old_name Pointer to the string of the old name.
 * @return 0 if the operation was successful, -1 if the inode is unknown,
 * 1 if the the location has already an inode with the same name.
 */
int Journal::handle_mds_move_einode_request(InodeNumber* new_parent_id, FsObjectName *new_name, InodeNumber* old_parent_id,
	FsObjectName* old_name)
{
	log->debug_log( "Old name: %s, from %llu to %llu, new name: %s", (char*) old_name,
			*old_parent_id, *new_parent_id, (char*) new_name );

	ps_profiler->function_start();

	int rtrn = 0;
	OperationType type = OperationType::UnknownType;
	ps_profiler->function_sleep();
	pthread_mutex_lock(&mutex);
	ps_profiler->function_wakeup();

	// check if it is just a remame
	if( *new_parent_id == *old_parent_id)
	{
		type = OperationType::RenameObject;
		rtrn = inode_cache->rename(*old_parent_id, new_name, old_name, inode_io, current_chunk_id);
	}
	// move the einode
	else
	{
		type = OperationType::MoveInode;
		rtrn = inode_cache->move_inode(*new_parent_id, new_name, *old_parent_id, old_name, inode_io, current_chunk_id);
	}

	if(rtrn == 0)
	{
		Operation *operation = new Operation(0);
		operation->set_einode_name(new_name);
		operation->set_operation_number(0);
		operation->set_type(type);
		operation->set_status(OperationStatus::Committed);
		operation->set_mode(OperationMode::Atomic);
		operation->set_parent_id(*new_parent_id);
		operation->set_module(Module::MDS);

		MoveData data;
		data.old_parent = *old_parent_id;
		strcpy((char*) &(data.old_name), (char*) old_name);
		operation->set_data((char*) &data, sizeof(data));

		put_into_chunk(operation);
	}



	pthread_mutex_unlock(&mutex);
	ps_profiler->function_end();

	return rtrn;
}

/**
 * @brief Delete the einode represented by the inode number.
 * @param[in] p_inode_number pointer to the inode number of the 
 * corresponding fs object.
 * @return integer return code
 * */
int Journal::handle_mds_delete_einode_request(InodeNumber *p_inode_number)
{
	log->debug_log( "Deleting %u.", *p_inode_number );

	ps_profiler->function_start();

	int rtrn = 0;

	if (!check_inode(*p_inode_number))
	{
		ps_profiler->function_end();
		return JournalFailureCodes::InodeNotExists;
	}
	Operation *o = new Operation(0);
	o->set_operation_number(0);
	o->set_type(OperationType::DeleteINode);
	o->set_status(OperationStatus::Committed);
	o->set_mode(OperationMode::Atomic);
	o->set_inode_id(*p_inode_number);
	o->set_parent_id(INVALID_INODE_ID);
	o->set_module(Module::MDS);


	rtrn = journal_operation(o);

	ps_profiler->function_end();

	log->debug_log( "Deleted %llu.", *p_inode_number );
	return rtrn;
}

/**
 * @brief Read 'FSAL_READDIR_EINODES_PER_MSG' einode objects starting
 * with an 'p_offset' in directory represented by the inode number.
 * @param[in] p_inode_number pointer to the inode number of the 
 * corresponding fs object.
 * @param[in] p_offset pointer to the ReaddirOffset type to define the
 * offset within the storage that defines the directory entries to return.
 * @param[out] p_rdir pointer to the ReadDirReturn structure that 
 * the resulting data are written to.
 * @return integer return code
 * */
int Journal::handle_mds_readdir_request(InodeNumber *p_inode_number, ReaddirOffset *p_offset,
		struct ReadDirReturn *p_rdir)
{
	log->debug_log( "Readdir request %llu with offet: %llu.", *p_inode_number, *p_offset );

	ps_profiler->function_start();

	int rv = 0;

	// load the directory into the cache
	inode_cache->cache_dir(*p_inode_number, inode_io);

	rv = inode_cache->read_dir(*p_inode_number, *p_offset, *p_rdir);


	log->debug_log("Readdir result size: %llu, nodes_len: %llu", p_rdir->dir_size, p_rdir->nodes_len);

	ps_profiler->function_end();
	return rv;
}

/**
 * @brief Return the inode number of the object represented by p_name.
 * @param[in] p_parent_inode_number pointer to the directories inode 
 * number of the corresponding fs object directory.
 * @param[in] p_name pointer to the name of the fs object.
 * @param[out] p_result pointer to the inode number type to write the 
 * resulting inode number to
 * @return integer return code
 * */
int Journal::handle_mds_lookup_inode_number_by_name(InodeNumber *p_parent_inode_number, FsObjectName *p_name,
		InodeNumber *p_result_inode)
{
	log->debug_log( "Lookup %s in %u.", *p_name, *p_parent_inode_number );

	ps_profiler->function_start();

	int ret = -1;
	CacheStatusType type;
	// first lookup the cache
	type = inode_cache->lookup_by_object_name(p_name, *p_parent_inode_number, *p_result_inode, inode_io);

	if(type == CacheStatusType::Present)
	{
		log->debug_log( "%s Object is in the cache, inode number is: %llu", (char*) *p_name, *p_result_inode);
		ret = 0;
	}
	else
	{
		log->debug_log( "%s Object not the cache.", (char*) *p_name);
	}

	ps_profiler->function_end();
	return ret;
}

/**
 * @brief Return a list of parents inode numbers of the provided
 * inode numbers
 * @param[in] p_request_list pointer to the inode number request list
 * @param[in] p_result_list pointer to the inode number results list
 * @return 0 if ok. non zero in an error.
 * 
 * ATM the inode_number_list_t contains 64 entries. If we keep this 
 * size, we could use a long int to set 'valid bits' for each entry...
 * Another way could be to set the parent inode number to 0.
 * */
int Journal::handle_mds_parent_inode_number_lookup_request(inode_number_list_t *p_request_list,
		inode_number_list_t *p_result_list)
{
	ps_profiler->function_start();

	p_request_list->items = p_request_list->items;
	for (int i = 0; i < p_request_list->items; i++)
	{
		try
		{
			p_result_list->inode_number_list[i] = inode_io->get_parent(p_request_list->inode_number_list[i]);
		} catch (ParentCacheException e)
		{
			p_result_list->inode_number_list[i] = INVALID_INODE_ID;
		}
	}
	ps_profiler->function_end();
	return 0;
}


/** 
 * @brief Return the parent hierarchy of the provided inode.
 * @param[in] p_inode_number pointer to inode number object representing the file systems object.
 * @param[out] p_result_list pointer to a inode_number_list_t structure. Return the parents inode numbers hierarchically.
 * @return 0 if the request was successful, otherwise -1.
 * */
int Journal::handle_mds_parent_inode_hierarchy_request(InodeNumber *p_inode_number, inode_number_list_t *p_result_list)
{
	return inode_io->get_parent_hierarchy(*p_inode_number, p_result_list);
}

/**
 * @brief Resolves inode number from the path.
 * @param[in] path The path string.
 * @param[out] inode_number The resolved inode_number.
 * @return 0 if the operation was successful, -1 if the path cannot be resolved.
 */
int32_t Journal::handle_resolve_path(string& path, InodeNumber& inode_number)
{
	EInode einode;
	try
	{
		inode_io->resolv_path(&einode, path);
		inode_number = einode.inode.inode_number;
	}
	catch(EInodeIOException& e)
	{
		return -1;
	}
	return 0;
}

/**
 * @brief Checks whether the inode exists on the partition.
 * @param inode_number The inode number to check.
 * @return 0 if the einode is present, -1 if not. -2 the inode number was present, but is tagged as deleted now.
 */
int32_t Journal::inode_exists(InodeNumber inode_number)
{
	int32_t rtrn = -1;
	EInode einode;
	CacheStatusType type = inode_cache->get_einode(inode_number, einode, inode_io);

	if(type == CacheStatusType::Present)
	{
		rtrn = 0;
	}
	else if( type == CacheStatusType::NotPresent)
	{
		rtrn = -1;
	}
	else if( type == CacheStatusType::Deleted)
	{
		rtrn = -2;
	}

	return rtrn;
}

/**
 * @brief Gets the relative path fron the current root node to the einode.
 * @param[in] inode_number The inode number.
 * @param[out] path The string to the relative path.
 * @return 0 if the operation was successful, otherwise -1.
 */
int32_t Journal::get_path(InodeNumber inode_number, string& path)
{
	CacheStatusType type = CacheStatusType::NotPresent;
	EInode einode;
	string temp_string;
	int32_t rtrn = -1;

	InodeNumber parent_id = INVALID_INODE_ID;

	while( ( parent_id = inode_cache->get_parent(inode_number) ) != INVALID_INODE_ID)
	{
		type = inode_cache->get_einode(inode_number, einode, inode_io);
		if(type == CacheStatusType::Present)
		{
			if(temp_string.length() == 0)
			{
				temp_string.append(einode.name);
			}
			else
			{
				temp_string.insert(0, "/");
				temp_string.insert(0, einode.name);
			}
		}
		inode_number = parent_id;
	}

	if(temp_string.length() > 0)
	{
		path = temp_string;
		rtrn = 0;
	}

	return rtrn;
}

/**
 * @brief Gets the size of the journal
 * @return The journal size.
 */
int32_t Journal::get_journal_size()
{
	int32_t size;
	size = journal_cache->size();
	return size;
}

/**
 * Sets the recovery flag.
 * @param b A boolean flag.
 */
void Journal::set_recovery(bool b)
{
	this->recovery_mode = b;
}

/**
 * @brief Close the journal, last step before the last write back and clean up.
 */
void Journal::close_journal()
{
	pthread_mutex_lock(&mutex);
	active_chunk->set_closed(true);
	current_chunk_id++;
	active_chunk = new JournalChunk(current_chunk_id, prefix, journal_id);
	journal_cache->add(active_chunk);
	pthread_mutex_unlock(&mutex);
}
