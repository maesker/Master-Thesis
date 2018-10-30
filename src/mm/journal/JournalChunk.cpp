/**
 * @file JournalChunk.cpp
 * @class JournalChunk
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.06.2011
 *
 * @brief A JournalChunk is a set of sequential journal operations.
 * A chunk holds a number of operations, the maximum number per chunk is defined by CHUNK_SIZE.
 */

#include <sstream>
#include <iomanip>

#include "mm/journal/FailureCodes.h"
#include "mm/journal/JournalChunk.h"
#include "mm/storage/storage.h"


/**
 * @brief Constuctor of JournalChunk.
 * @param chunk_id The id of the chunk.
 * @param prefix The chunk prefix.
 * @param journal_id The id of the journal which holds teh chunk.
 */
JournalChunk::JournalChunk(int32_t chunk_id, const string& prefix, InodeNumber journal_id)
{

	this->chunk_id = chunk_id;
	this->identifier = prefix;
	identifier.append("_");

	// construct the identifier of the chunk
	ostringstream ost;
	ost << std::setfill('0') << setw(20) << chunk_id;

	identifier.append(ost.str());
	identifier.append(CHUNK_EXTENSION);
	modified = false;
	closed = false;
	this->journal_id = journal_id;

	mutex = PTHREAD_MUTEX_INITIALIZER;

	ps_profiler = Pc2fsProfiler::get_instance();
}

/*
 * @brief Destructor of JournalChunk.
 */
JournalChunk::~JournalChunk()
{
	vector<Operation*>::iterator vit;

	for (vit = operations.begin(); vit != operations.end(); ++vit)
	{
		Operation* op = (*vit);
		delete op;
	}

	pthread_mutex_destroy(&mutex);
}

/**
 * @brief Add a new operation to the chunk.
 * @param operation Pointer to the operation.
 */
int32_t JournalChunk::add_operation(Operation* operation)
{
	ps_profiler->function_start();
	operations.push_back(operation);
	if (operation->get_operation_id() != 0)
		operation_map.insert(pair<uint64_t, Operation*> (operation->get_operation_id(), operation));
	if (operation->get_inode_id() != INVALID_INODE_ID)
		inode_map.insert(pair<InodeNumber, Operation*> (operation->get_inode_id(), operation));


	ps_profiler->function_end();
	return 0;
}


/**
 * @brief Add a new operation to the chunk and writes in to the storage.
 * @param operation Pointer to the operation.
 */
int32_t JournalChunk::add_write_operation(Operation* operation, StorageAbstractionLayer& sal)
{
	pthread_mutex_lock(&mutex);

	try
	{
		sal.write_object(journal_id, identifier.c_str(), operations.size() * sizeof(OperationData),
				sizeof(OperationData), (void*) &operation->get_operation_data());
	} catch (StorageException e)
	{
		pthread_mutex_unlock(&mutex);
		return StorageAccessError;
	}

	add_operation(operation);

	pthread_mutex_unlock(&mutex);

	return 0;
}

/**
 * @brief Get all operation with the given id from the chunk.
 * All operations with the same id are written into the vector.
 * @param operation_id The specified id.
 * @param operation Reference to a vector, where the operations should be stored.
 * @return True if the chunk contains operations with the specified id; otherwise false.
 */
bool JournalChunk::get_operations(uint64_t operation_id, vector<Operation>& operations)
{
	ps_profiler->function_start();

	bool rtrn = false;
	multimap<uint64_t, Operation*>::iterator it;
	pthread_mutex_lock(&mutex);

	if (operation_map.find(operation_id) != operation_map.end())
	{

		Operation* op;
		pair<multimap<uint64_t, Operation*>::iterator, multimap<uint64_t, Operation*>::iterator> p;

		p = operation_map.equal_range(operation_id);
		for (multimap<uint64_t, Operation*>::iterator it = p.first; it != p.second; ++it)
		{
			op = (*it).second;
			operations.push_back(*op);
		}
		rtrn = true;
	}

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();

	return rtrn;
}

/**
 * @brief Get the operation with the specified operation id and number.
 * @param[in] operation_id The specified id.
 * @param[in] operation_number The specified operation number.
 * @param[out] operation Reference to the wanted operation.
 * @return True if the chunk contains operations with the specified id and number; otherwise false.
 */
bool JournalChunk::get_operation(uint64_t operation_id, int operation_number, Operation& operation)
{
	ps_profiler->function_start();

	bool value;
	vector<Operation> vec;

	value = get_operations(operation_id, vec);

	if (value == true)
	{
		vector<Operation>::iterator vit;
		for (vit = vec.begin(); vit != vec.end(); ++vit)
		{
			if (vit->get_operation_number() == operation_number)
			{
				operation = *vit;

				ps_profiler->function_end();
				return true;
			}
		}
	}

	ps_profiler->function_end();
	return false;
}

/**
 * @brief Removes the inode operation from the chunk
 * @param inode_id The id of the inode.
 */
void JournalChunk::remove_inode(InodeNumber inode_id)
{
	ps_profiler->function_start();

	Operation* o = NULL;
	vector<Operation*>::iterator vic;
	pthread_mutex_lock(&mutex);

	inode_map.erase(inode_id);

	for (vic = operations.begin(); vic != operations.end();)
	{
		if ((*vic)->get_inode_id() == inode_id)
		{
			o = *vic;
			vic = operations.erase(vic);
			delete o;
		}
		else
			vic++;
	}

	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}

/**
 * @brief Removes the operation from the chunk.
 * @param operation_id The id of the operation.
 */
void JournalChunk::remove_operation(uint64_t operation_id)
{
	ps_profiler->function_start();

	Operation* o = NULL;
	vector<Operation*>::iterator vic;
	pthread_mutex_lock(&mutex);
	operation_map.erase(operation_id);

	for (vic = operations.begin(); vic != operations.end();)
	{
		if ((*vic)->get_operation_id() == operation_id)
		{
			o = *vic;
			vic = operations.erase(vic);
			delete o;
		}
		else
			vic++;
	}
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}

/**
 * @brief Deletes the chunk from the long term storage.
 * @param sal Reference of the @StorageAbstractionLayer object.
 * @return 0 if the delete operation was successfull, otherwise an error code is returned.
 * StorageAccessError - An error occurred during deleting the chunk from the long term storage.
 */
int32_t JournalChunk::delete_chunk(StorageAbstractionLayer& sal)
{
	ps_profiler->function_start();
	// try to remove the chunk
	try
	{
		sal.remove_object(journal_id, identifier.c_str());
	} catch (StorageException e)
	{
		cout << e.get_message() << endl;
		ps_profiler->function_end();
		return StorageAccessError;
	}

	ps_profiler->function_end();
	return 0;
}

/**
 * @brief Writes the chunk to the file storage.
 * @return 0 if writing was successful, otherwise error code:
 * JournalEmpty - Nothing to write.
 * ChunkToLarge - The chunk size exceeds the defined maximum size.
 * StorageAccessError - Chunk was not written to the storage,
 * because of an unexpected storage access error.
 */
int JournalChunk::write_chunk(StorageAbstractionLayer& sal)
{
	ps_profiler->function_start();

	int rtrn = 0;
	// Check whether journal chunk is empty
	pthread_mutex_lock(&mutex);
	if (operations.empty())
	{
		rtrn = JournalEmpty;
		pthread_mutex_unlock(&mutex);
	}
	else if (CHUNK_SIZE < operations.size())
	{
		rtrn = ChunkToLarge;
		pthread_mutex_unlock(&mutex);
	}
	else
	{

		Operation* op;
		int i = 0;
		//write all the operations from the map list into a file
		for (vector<Operation*>::iterator it = operations.begin(); it != operations.end(); ++it)
		{
			op = (*it);
			try
			{
				sal.write_object(journal_id, identifier.c_str(), i * sizeof(OperationData), sizeof(OperationData),
						(void*) &op->get_operation_data());
			} catch (StorageException e)
			{
				cout << e.get_message() << endl;
				pthread_mutex_unlock(&mutex);
				ps_profiler->function_end();
				return StorageAccessError;
			}
			i++;
		}
	}

	pthread_mutex_unlock(&mutex);
	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Read a chunk from the file storage.
 * @return 0 if reading was successful, otherwise error code:
 * StorageAccessError - Chunk was not read from the storage,
 * because of an unexpected storage access error.
 */
int JournalChunk::read_chunk(StorageAbstractionLayer& sal)
{
	ps_profiler->function_start();

	int chunk_size = 0;
	int n = 0;

	try
	{
		chunk_size = sal.get_object_size(journal_id, identifier.c_str());
	} catch (StorageException e)
	{
		return StorageAccessError;
	}

	if (chunk_size == 0)
	{
		return StorageAccessError;
	}

	n = chunk_size / sizeof(OperationData);

	for (int i = 0; i < n; i++)
	{
		OperationData operation_data;
		try
		{
			sal.read_object(journal_id, identifier.c_str(), i * sizeof(OperationData), sizeof(OperationData),
					(void*) &operation_data);
		} catch (StorageException e)
		{
			return StorageAccessError;
		}
		Operation* operation = new Operation();
		operation->set_operation_data(operation_data);
		add_operation(operation);
	}

	ps_profiler->function_end();
	return 0;
}

/**
 * @brief Get the chunk identifier.
 * @return The identifier.
 */
string& JournalChunk::get_identifier()
{
	return identifier;
}

/**
 * @brief Set the chunk identifier.
 * @param Reference to an identifier string.
 */
void JournalChunk::set_identifier(const string& identifier)
{
	this->identifier = identifier;
}

/**
 * @brief Get the id of the chunk.
 * @return The chunk id.
 */
int JournalChunk::get_chunk_id()
{
	return chunk_id;
}

/**
 * @brief Counts the number of inodes in the chunk with the given inode id.
 * @param inode_id The id of the inodes.
 * @return The number of the inodes.
 */
int32_t JournalChunk::count_inodes(InodeNumber inode_id)
{
	ps_profiler->function_start();

	int32_t rtrn = 0;
	pthread_mutex_lock(&mutex);
	rtrn = inode_map.count(inode_id);
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();

	return rtrn;
}

/**
 * @brief Gets the size of the chunk.
 * @return The number of operations inside the chunks.
 */
int32_t JournalChunk::size()
{
	ps_profiler->function_start();

	int32_t rtrn = 0;
	pthread_mutex_lock(&mutex);
	rtrn = operations.size();
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Tests whether the modified flag is set.
 * @return True if the modified flag is set, otherwise false.
 */
bool JournalChunk::is_modified()
{
	bool rtrn;
	pthread_mutex_lock(&mutex);
	rtrn = modified;
	pthread_mutex_unlock(&mutex);
	return rtrn;
}

/**
 * @brief Sets the modified flag.
 * @param b The new modified flag value.
 */
void JournalChunk::set_modified(bool b)
{
	pthread_mutex_lock(&mutex);
	modified = b;
	pthread_mutex_unlock(&mutex);
}

/**
 * @brief Tests whether the closed flag is set.
 * @return True if the closed flag is set, otherwise false.
 */
bool JournalChunk::is_closed()
{
	ps_profiler->function_start();

	bool rtrn;
	pthread_mutex_lock(&mutex);
	rtrn = closed;
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return rtrn;
}

/**
 * @brief Sets the modified flag.
 * @param b The new modified flag value.
 */
void JournalChunk::set_closed(bool b)
{
	ps_profiler->function_start();

	pthread_mutex_lock(&mutex);
	closed = b;
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
}

/**
 * @brief Get all inode operations of the given inode id.
 * @parma[in] inode_id The inode id.
 * @param[out] inode_list The inode list, where all operations of the inode will be stored.
 * @return The number of the operations with the given inode id.
 */
int32_t JournalChunk::get_inode_operations(InodeNumber inode_id, vector<Operation*>& inode_list)
{
	ps_profiler->function_start();

	int32_t count = 0;
	pthread_mutex_lock(&mutex);
	count = inode_map.count(inode_id);
	if (count > 0)
	{

		multimap<InodeNumber, Operation*>::iterator it;
		pair<multimap<InodeNumber, Operation*>::iterator, multimap<InodeNumber, Operation*>::iterator> p;

		p = inode_map.equal_range(inode_id);
		for (it = p.first; it != p.second; ++it)
		{
			inode_list.push_back((*it).second);
		}
	}
	pthread_mutex_unlock(&mutex);

	ps_profiler->function_end();
	return count;
}

/**
 * @brief Gets all operations of the chunk.
 * Only for recovery!
 * @return The reference to the vector of operations.
 */
vector<Operation*>& JournalChunk::get_operations()
{
	return operations;
}
