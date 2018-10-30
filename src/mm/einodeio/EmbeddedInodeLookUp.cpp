/*
 * EmbeddedInodeLookUp.cpp
 *
 *  Created on: 29.04.2011
 *      Author: sergerit
 */
#include <string.h>
#include <stdio.h>
#include <string>

#include "EmbeddedInode.h"
#include "mm/einodeio/EInodeIOException.h"
#include "mm/einodeio/EmbeddedInodeLookUp.h"
#include "mm/einodeio/ParentCache.h"
#include "mm/storage/storage.h"
#include "global_types.h"

using namespace std;


/**
 * @brief Constructor of EmbeddedInodeLookUp
 *
 * @param[in] sal Pointer to StorageAbstractionLayer
 * @param[in] root_inode_number Root inode of handled subtree
 */
EmbeddedInodeLookUp::EmbeddedInodeLookUp(StorageAbstractionLayer *sal, InodeNumber root_inode_number)
{
    this->storage_abstraction_layer = sal;
    this->parent_cache = new ParentCache(PARENT_CACHE_SIZE);
    this->root_inode_number = root_inode_number;
    this->profiler = Pc2fsProfiler::get_instance();
}

/**
 * @brief Destructor of EmbeddedInodeLookUp
 */
EmbeddedInodeLookUp::~EmbeddedInodeLookUp()
{
    delete this->parent_cache;
}

/**
 * @brief Get EInode
 *
 * @param[out] einode Pointer to store result (has to be allocated by caller)
 * @param[in] inode_number Inode number to get
 *
 * @throws ParentCacheException If parent inode number of $inode_number not cached
 * @throws EInodeIOException If Inode not found
 */
void EmbeddedInodeLookUp::get_inode(EInode *einode, InodeNumber inode_number)
{
	profiler->function_start();
    ParentCacheEntry parent;
    char parent_name[MAX_NAME_LEN];
    try{
    	parent = this->parent_cache->get_parent(inode_number);
    }
    catch(ParentCacheException)
    {
    	profiler->function_end();
    	throw ParentCacheException("Parent not found");
    }
    snprintf(parent_name, MAX_NAME_LEN, "%llu", parent.parent);
    int object_len = this->count_inodes(parent.parent);
    int i;

    this->storage_abstraction_layer->lock_object(this->root_inode_number, parent_name);
    try{
    	int object_len = this->count_inodes(parent.parent);

		// Try to find inode at cached offset
		if(parent.offset < object_len)
		{
			EInode result;
			this->storage_abstraction_layer->read_object(this->root_inode_number, parent_name, (off_t) parent.offset * sizeof(EInode), sizeof(EInode), (void *) &result);

			if (result.inode.inode_number == inode_number)
			{
				*einode = result;
				this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
				profiler->function_end();
				return;
			}
		}

		// Cached offset is outdated, iterate over whole object
		for (i = 0; i < object_len; i++)
		{
			EInode result;
			this->storage_abstraction_layer->read_object(this->root_inode_number, parent_name, (off_t) i * sizeof(EInode), sizeof(EInode), (void *) &result);

			if (result.inode.inode_number == inode_number)
			{
				*einode = result;
				this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
				profiler->function_end();
				return;
			}
		}
    }
    catch(StorageException)
    {
    }

    this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
    profiler->function_end();
    throw EInodeIOException("Inode not found");
}

/**
 * @brief Get EInode by filename
 *
 * @param[out] node Pointer to store result (has to be allocated by caller)
 * @param[in] parent_inode_number Inode number of parent directory
 * @param[in] identifier Inode identifier
 *
 * @throws EInodeIOException If inode not found
 */
void EmbeddedInodeLookUp::get_inode(EInode* node, InodeNumber parent_inode_number, char* identifier)
{
	profiler->function_start();
    char parent_name[MAX_NAME_LEN];
    snprintf(parent_name, MAX_NAME_LEN, "%llu", parent_inode_number);

    int i;

    this->storage_abstraction_layer->lock_object(this->root_inode_number, parent_name);
    try{
    	int object_len = this->count_inodes(parent_inode_number);
		for (i = 0; i < object_len; i++)
		{
			EInode result;
			this->storage_abstraction_layer->read_object(this->root_inode_number, parent_name, (off_t) i * sizeof(EInode), sizeof(EInode), &result);

			if (strncmp(result.name, identifier, MAX_NAME_LEN) == 0)
			{
				this->parent_cache->set_parent(result.inode.inode_number, parent_inode_number, i);
				*node = result;
				this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
				profiler->function_end();
				return;
			}
		}
    }
    catch(StorageException)
    {
    }

    this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
    profiler->function_end();
    throw EInodeIOException("Inode not found");
}

/**
 * @brief Get EInode by inode number
 *
 * @param[out] node Pointer to store result (has to be allocated by caller)
 * @param[in] parent_inode_number Inode number of parent directory
 * @param[in] inode Inode number
 *
 * @throws EInodeIOException If inode not found
 */
void EmbeddedInodeLookUp::get_inode(EInode *node, InodeNumber parent_inode_number, InodeNumber inode)
{
    profiler->function_start();
	char parent_name[MAX_NAME_LEN];
    snprintf(parent_name, MAX_NAME_LEN, "%llu", parent_inode_number);
    int i;

    this->storage_abstraction_layer->lock_object(this->root_inode_number, parent_name);
    try{
    	int object_len = this->count_inodes(parent_inode_number);
		for (i = 0; i < object_len; i++)
		{
			EInode result;
			this->storage_abstraction_layer->read_object(this->root_inode_number, parent_name, (off_t) i * sizeof(EInode), sizeof(EInode), &result);

			if (result.inode.inode_number == inode)
			{
				this->parent_cache->set_parent(result.inode.inode_number, parent_inode_number, i);
				*node = result;
				this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
				profiler->function_end();
				return;
			}
		}
    }
    catch(StorageException)
    {
    }

    this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
    profiler->function_end();
    throw EInodeIOException("Inode not found");
}

/**
 * @brief Overwrite or create inode
 *
 * @param[in] node Pointer to EInode
 * @param[in] parent_inode_number Inode number of parent direcotry
 * @param[in] no_sync
 *
 * @throws EInodeIOException If combination of name and inode number does not match to an existing inode
 */
void EmbeddedInodeLookUp::write_inode(EInode *node, InodeNumber parent_inode_number, bool no_sync)
{
    char parent_name[MAX_NAME_LEN];
    int i;
    profiler->function_start();
    snprintf(parent_name, MAX_NAME_LEN, "%llu", parent_inode_number);

    this->storage_abstraction_layer->lock_object(this->root_inode_number, parent_name);
    try{
    	int object_len = this->count_inodes(parent_inode_number);
		// Try to find offset in parent cache
		try{
			ParentCacheEntry parent = this->parent_cache->get_parent(node->inode.inode_number);
			if(parent.offset <= object_len)
			{
				EInode einode;
				this->storage_abstraction_layer->read_object(this->root_inode_number, parent_name, (off_t) parent.offset * sizeof(EInode), sizeof(EInode), (void *) &einode);
				if (strcmp(einode.name, node->name) == 0)
				{
					if (einode.inode.inode_number == node->inode.inode_number)
					{
						this->storage_abstraction_layer->write_object(this->root_inode_number, parent_name, (off_t) parent.offset * sizeof(EInode), sizeof(EInode), (void *) node, no_sync);
						this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
						profiler->function_end();
						return;
					}
				}
			}
		}
		catch(ParentCacheException)
		{}

		// Offset not found in parent cache
		for (i = 0; i < object_len; i++)
		{
			EInode einode;
			this->storage_abstraction_layer->read_object(this->root_inode_number, parent_name, (off_t) i * sizeof(EInode), sizeof(EInode), (void *) &einode);

			if (strcmp(einode.name, node->name) == 0)
			{
				if (einode.inode.inode_number == node->inode.inode_number)
				{
					this->storage_abstraction_layer->write_object(this->root_inode_number, parent_name, (off_t) i * sizeof(EInode), sizeof(EInode), (void *) node, no_sync);
					this->parent_cache->set_parent(node->inode.inode_number, parent_inode_number, i);
					this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
					profiler->function_end();
					return;
				}
				else
				{
					this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
					profiler->function_end();
					throw EInodeIOException("Entry already existing");
				}
			}
		}

		this->storage_abstraction_layer->write_object(this->root_inode_number, parent_name, (off_t) object_len * sizeof(EInode), sizeof(EInode), (void *) node, no_sync);
		this->parent_cache->set_parent(node->inode.inode_number, parent_inode_number, object_len);
    }
    catch(StorageException)
    {}
    this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
    profiler->function_end();
}

/**
 * @brief Overwrite or create inode
 *
 * @param[in] node Pointer to EInode
 * @param[in] parent_inode_number Inode number of parent direcotry
 *
 * @throws EInodeIOException If combination of name and inode number does not match to an existing inode
 */
void EmbeddedInodeLookUp::write_inode(EInode *node, InodeNumber parent_inode_number)
{
	this->write_inode(node, parent_inode_number, false);
}

/**
 * @brief Overwrite existing inode
 *
 * @param[in] node Pointer to inode
 *
 * @throws EInodeIOException If nullpointer given
 * @throws ParentCacheException If $node not known by parent cache
 */
void EmbeddedInodeLookUp::write_inode(EInode* node)
{
    if(node)
    {
        ParentCacheEntry parent = this->parent_cache->get_parent(node->inode.inode_number);
        this->write_inode(node, parent.parent);
    }
    else
    {
        throw EInodeIOException("No EInode given");
    }
}

/**
 * @brief Write set of updates to one object with single sync
 *
 * @param[in] updates Pointer to list of updated inodes
 * @param[in] parent Parent inode number
 *
 * @throws EInodeIOException
 */
void EmbeddedInodeLookUp::write_update_set(std::list<EInode*> *updates, InodeNumber parent)
{
	for(std::list<EInode*>::iterator it = updates->begin(); it != updates->end(); it++)
	{
		if((*it) != updates->back())
		{
			// Write without sync
			this->write_inode((*it), parent, true);
		}
		else
		{
			// Sync after write
			this->write_inode((*it), parent, false);
		}
	}
}

/**
 * @brief Delete inode
 *
 * Either filename or inode number have to be given
 *
 * @param[in] parent_id Inode number of parent directory
 * @param[in] path_to_file Filename to delete
 * @param[in] inode_number Inode number to delete
 *
 * @throws EInodeIOException If both, filename and inode number are not given
 * @throws EInodeIOException If inode not found
 *
 */
void EmbeddedInodeLookUp::delete_inode(InodeNumber parent_id, char *path_to_file, InodeNumber inode_number, bool no_sync)
{
    int i = 0;
    int inode_position = -1;
    char parent_name[MAX_NAME_LEN];

    profiler->function_start();
    snprintf(parent_name, MAX_NAME_LEN, "%llu", parent_id);

    if (!((path_to_file || inode_number) && !(path_to_file  && inode_number)))
    {
    	profiler->function_end();
        throw EInodeIOException("Either name or inode number has to be set");
    }

    this->storage_abstraction_layer->lock_object(this->root_inode_number, parent_name);
    try{
		int object_len = this->count_inodes(parent_id);
		if (object_len == 1)
		{
			// Only one EInode in object => delete whole object
			EInode einode;
			this->storage_abstraction_layer->read_object(this->root_inode_number, parent_name, 0, sizeof(EInode), (void *) &einode);

			if((path_to_file && strncmp(einode.name, path_to_file, MAX_NAME_LEN) == 0) ||
					(einode.inode.inode_number == inode_number))
			{
				this->storage_abstraction_layer->remove_object(this->root_inode_number, parent_name);
				this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
				profiler->function_end();
				return;
			}
		}

		for (i = 0; i < object_len; i++)
		{
			EInode einode;
			this->storage_abstraction_layer->read_object(this->root_inode_number, parent_name, (off_t) i * sizeof(EInode), sizeof(EInode), (void *) &einode);

			if (path_to_file != NULL)
			{
				if (strncmp(einode.name, path_to_file, MAX_NAME_LEN) == 0)
				{
					inode_position = i;
					break;
				}
			}
			else
			{
				if (einode.inode.inode_number == inode_number)
				{
					inode_position = i;
					break;
				}
			}
		}

		if (inode_position == -1)
		{
			profiler->function_end();
			throw EInodeIOException("Inode not found");
		}
		else
		{
			if (inode_position == object_len - 1)
			{
				// Last EInode has to be deleted => just truncating object
				this->storage_abstraction_layer->truncate_object(this->root_inode_number, parent_name, (object_len - 1) * sizeof(EInode));
			}
			else
			{
				// Overwrite EInode to delete with last EInode in object and truncate
				EInode last_einode;
				this->storage_abstraction_layer->read_object(this->root_inode_number, parent_name, (off_t) (object_len - 1) * sizeof(EInode), sizeof(EInode), (void *) &last_einode);
				this->storage_abstraction_layer->write_object(this->root_inode_number, parent_name, (off_t) (inode_position) * sizeof(EInode), sizeof(EInode), (void *) &last_einode, no_sync);
				this->storage_abstraction_layer->truncate_object(this->root_inode_number, parent_name, (object_len - 1) * sizeof(EInode));
			}
		}
    }
    catch(StorageException)
    {}
    this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
    profiler->function_end();
}

/**
 * @brief Delete inode
 *
 * @param[in] inode_number Inode to delete
 *
 * @throws ParentCacheException If $inode_number not found in ParentCache
 */
void EmbeddedInodeLookUp::delete_inode(InodeNumber inode_number)
{
    ParentCacheEntry parent;
    parent = this->parent_cache->get_parent(inode_number);
    this->delete_inode(parent.parent, NULL, inode_number, false);
    this->parent_cache->delete_parent(inode_number);
}

void EmbeddedInodeLookUp::delete_inode_set(InodeNumber parent, std::list<InodeNumber> *inodes)
{
	for(std::list<InodeNumber>::iterator it = inodes->begin(); it != inodes->end(); it++)
	{
		if((*it) != inodes->back())
		{
			this->delete_inode(parent, NULL, *it, true);
		}
		else
		{
			this->delete_inode(parent, NULL, *it, false);
		}
	}
}

/**
 * @brief Read directory entries
 *
 * @param[in] dir_id Inode number of directory to read_dir
 * @param[in] offset Position to start reading
 * @retval Directory entries
 *
 * @throws ParentCacheException If $dir_id not known by ParentCache
 */
ReadDirReturn EmbeddedInodeLookUp::read_dir(InodeNumber dir_id, off_t offset)
{
    ReadDirReturn result;
    memset(&result, 0, sizeof(ReadDirReturn));
    int i;
	char dir_name[MAX_NAME_LEN];

	profiler->function_start();
	snprintf(dir_name, MAX_NAME_LEN, "%llu", dir_id);

    this->storage_abstraction_layer->lock_object(this->root_inode_number, dir_name);
    try{
		int dir_size = this->count_inodes(dir_id);
		result.dir_size = dir_size;

		if(dir_size == 0)
		{
		    this->storage_abstraction_layer->unlock_object(this->root_inode_number, dir_name);
		    profiler->function_end();
			return result;
		}

		int result_size = min((int) (dir_size - offset), FSAL_READDIR_EINODES_PER_MSG);

		result.nodes_len = result_size;
		this->storage_abstraction_layer->read_object(this->root_inode_number, dir_name, offset * sizeof(EInode), result_size * sizeof(EInode), (void *) &(result.nodes));

		for (i = 0; i < result_size; i++)
		{
			this->parent_cache->set_parent(result.nodes[i].inode.inode_number, dir_id, offset + i);
		}
    }
    catch(StorageException)
    {}

    this->storage_abstraction_layer->unlock_object(this->root_inode_number, dir_name);
    profiler->function_end();
    return result;
}

/**
 * @brief Count inodes in directory
 *
 * @param[in] parent_inode_number Inode number of directory
 * @retval Number of directory entries
 *
 * @throws EInodeIOException if directory not existing
 */
int EmbeddedInodeLookUp::count_inodes(InodeNumber parent_inode_number)
{
    char parent_name[MAX_NAME_LEN];
    snprintf(parent_name, MAX_NAME_LEN, "%llu", parent_inode_number);
    profiler->function_start();
    try
    {
    	int result = this->storage_abstraction_layer->get_object_size(this->root_inode_number, parent_name) / sizeof(EInode);
    	profiler->function_end();
        return result;
    }
    catch(StorageException)
    {
    	profiler->function_end();
        throw EInodeIOException("Directory not found");
    }
    return 0; // Never reached, just to avoid compiler warnings
}

/**
 * @brief Get parent inode number from cache
 *
 * @param[in] inode_number Inode number
 * @retval Parent inode number of $inode_number
 *
 * @throws ParentCacheException If no cache entry found
 */
InodeNumber EmbeddedInodeLookUp::get_parent(InodeNumber inode_number)
{
	profiler->function_start();
	ParentCacheEntry parent = this->parent_cache->get_parent(inode_number);
	profiler->function_end();
    return parent.parent;
}


/**
 * @brief Get parent inodes till partition root
 *
 * @param[IN]	inode_number	InodeNumber of the directory from which the parents have to be looked up
 * @param[IN]	parent_list		List of the parent hierarchy to be returned
 * @retval	0 if successfully done
 */
int EmbeddedInodeLookUp::get_parent_hierarchy(InodeNumber inode_number, inode_number_list_t* parent_list)
{
	profiler->function_start();
	if(!parent_list)
	{
		profiler->function_end();
		return -1;
	}

	try
	{
		parent_list->items = 0;
		while(inode_number != this->root_inode_number && parent_list->items < ZMQ_INODE_NUMBER_LISTSIZE)
		{
			inode_number = this->get_parent(inode_number);
			parent_list->inode_number_list[parent_list->items++] = inode_number;
		}
	}
	catch(ParentCacheException)
	{
		parent_list->inode_number_list[parent_list->items++] = INVALID_INODE_ID;
	}
	profiler->function_end();
	return 0;
}

/**
 * @brief Get path of $inode up to the subtree root
 *
 * @param[in] inode Inode number
 * @retval Path of $inode
 *
 * @throws ParentCacheException If path can not be resolved by parent cache
 */
std::string EmbeddedInodeLookUp::get_path(InodeNumber inode)
{
    std::string result = "";
    InodeNumber current = inode;
    profiler->function_start();
    try{
		do
		{
			EInode node;
			this->get_inode(&node, current);

			if(result != "")
				result = "/" + result;

			result = std::string(node.name) + result;
		}
		while((current = this->get_parent(current)) && current != this->root_inode_number);
    }
    catch(ParentCacheException)
    {
    	profiler->function_end();
    	throw ParentCacheException("Path cannot be resolved");
    }
    result = "/" + result;
    profiler->function_end();
    return result;
}

/**
 * @brief Move $inode from $old_parent to $new_parent
 *
 * @param inode
 * @param old_parent
 * @param new_parent
 * @throws EInodeIOException
 */
void EmbeddedInodeLookUp::move_inode(InodeNumber inode, InodeNumber old_parent, InodeNumber new_parent)
{
	EInode einode;
	this->get_inode(&einode, old_parent, inode);
	this->delete_inode(inode);
	this->write_inode(&einode, new_parent);
}

void EmbeddedInodeLookUp::create_inode(InodeNumber parent, EInode *node)
{
	this->create_inode(parent, node, false);
}

/**
 * @brief Append inode to directory $parent
 *
 * No further checks are done
 *
 * @param[in] parent
 * @param[in] node
 * throws EInodeIOException
 */
void EmbeddedInodeLookUp::create_inode(InodeNumber parent, EInode *node, bool no_sync)
{
	profiler->function_start();
	char parent_name[MAX_NAME_LEN];
	long int object_len;

	snprintf(parent_name, MAX_NAME_LEN, "%llu", parent);
	this->storage_abstraction_layer->lock_object(this->root_inode_number, parent_name);
	object_len = this->storage_abstraction_layer->get_object_size(this->root_inode_number, parent_name);
	try
	{
		this->storage_abstraction_layer->write_object(this->root_inode_number, parent_name, object_len, sizeof(EInode), node, no_sync);
		this->parent_cache->set_parent(node->inode.inode_number, parent, object_len);
	}
	catch(StorageException)
	{
		this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
		profiler->function_end();
		throw EInodeIOException("Inode creation failed");
	}
	this->storage_abstraction_layer->unlock_object(this->root_inode_number, parent_name);
	profiler->function_end();
}

void EmbeddedInodeLookUp::create_inode_set(std::list<EInode*> *inodes, InodeNumber parent)
{
	for(std::list<EInode*>::iterator it = inodes->begin(); it != inodes->end(); it++)
	{
		EInode *inode = *it;
		if((*it) != inodes->back())
			this->create_inode(parent, inode, true);
		else
			this->create_inode(parent, inode, false);
	}
}

/**
 * @brief Get inode of complete path
 *
 * @param[out] node Result
 * @param[in] path Path
 * @throws EInodeIOException
 */
void EmbeddedInodeLookUp::resolv_path(EInode *node, std::string path)
{
	InodeNumber parent = this->root_inode_number;
	std::string delimiter = "/";

	string::size_type lastPos = path.find_first_not_of(delimiter, 0);
	string::size_type pos = path.find_first_of(delimiter, lastPos);
	EInode result;

	while (string::npos != pos || string::npos != lastPos)
	{
		this->get_inode(&result, parent, (char *)path.substr(lastPos, pos - lastPos).data());
        lastPos = path.find_first_not_of(delimiter, pos);
        pos = path.find_first_of(delimiter, lastPos);
        parent = result.inode.inode_number;
	}

	*node = result;
}
