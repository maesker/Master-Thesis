#include "mm/storage/PartitionManager.h"
#include "mm/storage/storage.h"
#include <string.h>

PartitionManager::PartitionManager(std::list<std::string> *devices, const char *mount_directory, AbstractMountHelper *mount_helper, const char *host_identifier, int host_rank, int total_hosts)
{
    this->partitions = new std::list<Partition*>();
    strncpy(this->mount_directory, mount_directory, MAX_PATH_LEN);
    strncpy(this->own_host_identifier, host_identifier, SERVER_LEN);
    this->mount_helper = mount_helper;
    this->host_rank = host_rank;
    this->total_hosts = total_hosts;
    this->server_list = NULL;

    for(std::list<std::string>::iterator it = devices->begin(); it != devices->end(); it++)
    {
        Partition *p = new Partition((char *)(*it).data(), this->own_host_identifier, host_rank, total_hosts, this);
        this->partitions->push_back(p);
    }
}

PartitionManager::~PartitionManager()
{
    while(!this->partitions->empty())
    {
        Partition *p = this->partitions->front();
        delete p;
        this->partitions->pop_front();
    }
    delete this->partitions;
}

void PartitionManager::set_server_list(std::vector<std::string> *server_list, int rank, int total_hosts)
{
	this->server_list = server_list;
	this->host_rank = rank;
	this->total_hosts = total_hosts;
	this->recalculate_ownerships(rank, total_hosts);
}

/**
 * @brief   Get free Partition
 *
 * @retval  Pointer to Partition object
 */
Partition* PartitionManager::get_free_partition()
{
	Partition *result = NULL;

	try
	{
		result = this->get_free_owned_partition();
	}
	catch(StorageException)
	{
		try
		{
			result = this->get_free_remote_partition();
		}
		catch(...)
		{}
	}

	if(result)
		return result;
	else
		throw StorageException("No free partitions");
}

Partition* PartitionManager::get_free_remote_partition()
{
	return NULL;
}

Partition* PartitionManager::get_free_owned_partition()
{
  Partition *result = NULL;

  for (std::list<Partition*>::iterator it = this->partitions->begin(); it != this->partitions->end(); it++)
  {
      if(strcmp((*it)->get_owner(), this->own_host_identifier) == 0 && (*it)->get_root_inode() == 0)
      {
          return *it;
      }
  }

  throw StorageException("No free partitions");
}

/**
 * @brief   Get partition by given root inode
 *
 * @param[in]   root_inode Root inode number
 * @retval  Pointer to Partition object
 * @throws  StorageException
 */
Partition* PartitionManager::get_partition(InodeNumber root_inode)
{
    Partition *result = NULL;

    for (std::list<Partition*>::iterator it = this->partitions->begin(); it != this->partitions->end(); it++)
    {
        if((*it)->get_root_inode() == root_inode)
            return *it;
    }

    return NULL;
}

/**
 * @brief   Get partition by given identifier
 *
 * @param[in]   identifier
 * @retval  Pointer to Partition object
 * @throws  StorageException
 */
Partition* PartitionManager::get_partition_by_identifier(char* identifier)
{
    Partition *result = NULL;

    for (std::list<Partition*>::iterator it = this->partitions->begin(); it != this->partitions->end(); it++)
    {
        if(strcmp((*it)->get_identifier(), identifier) == 0)
            return *it;
    }

    throw StorageException("Partition not found");
}

/**
 * @brief Get MountHelper object
 *
 * @retval MountHelper
 */
AbstractMountHelper* PartitionManager::get_mount_helper()
{
    return this->mount_helper;
}

/**
 * @brief Get directory where partitions are mounted at
 *
 * @retval Mount directory
 */
char* PartitionManager::get_mount_directory()
{
    return this->mount_directory;
}

/**
 * @brief Get list of all partitions
 *
 * @retval List of partitions
 */
std::list<Partition*> *PartitionManager::get_partitions()
{
    return this->partitions;
}

void PartitionManager::recalculate_ownerships(int host_rank, int total_hosts)
{
    for (std::list<Partition*>::iterator it = this->partitions->begin(); it != this->partitions->end(); it++)
    {
        (*it)->recalculate_ownerships(host_rank, total_hosts);
    }
}

char *PartitionManager::get_host_identifier()
{
	return this->own_host_identifier;
}
