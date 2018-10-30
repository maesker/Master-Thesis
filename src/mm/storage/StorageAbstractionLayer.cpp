/*
 * StorageAbstractionLayer.cpp
 *
 *  Created on: 09.04.2011
 *      Author: chrkl
 */

#include <stdlib.h>
#include "global_types.h"
#include "mm/storage/storage.h"
#include "mm/storage/StorageAbstractionLayer.h"

StorageAbstractionLayer::StorageAbstractionLayer(char *path)
{
    this->path = path;
    this->partition_manager = NULL;
    this->storage_type = file_based_storage;
    this->lock_manager = new LockManager();
}

StorageAbstractionLayer::StorageAbstractionLayer(std::list<std::string> *devices, const char *mount_dir, AbstractMountHelper *mh, const char *host_identifier, int host_rank, int total_hosts)
{
    this->partition_manager = new PartitionManager(devices, mount_dir, mh, host_identifier, host_rank, total_hosts);
    this->storage_type = partition_based_storage;
}

StorageAbstractionLayer::~StorageAbstractionLayer()
{
    if(this->partition_manager)
        delete this->partition_manager;

    /* if(this->lock_manager)
    	delete this->lock_manager; */
}

void StorageAbstractionLayer::read_object(InodeNumber subtree_root_inode, const char *identifier, off_t offset, size_t length, void *data)
{
    if(this->storage_type == partition_based_storage)
    {
        Partition *p = this->partition_manager->get_partition(subtree_root_inode);
        p->read_object(identifier, offset, length, data);
    }
    else if(this->storage_type == file_based_storage)
    {
        FileStorageDevice device = FileStorageDevice(this->path);
        device.read_object(identifier, offset, length, data);
    }
}

void StorageAbstractionLayer::write_object(InodeNumber subtree_root_inode, const char *identifier, off_t offset, size_t length, void *data)
{
	this->write_object(subtree_root_inode, identifier, offset, length, data, false);
}

void StorageAbstractionLayer::write_object(InodeNumber subtree_root_inode, const char *identifier, off_t offset, size_t length, void *data, bool no_sync)
{
    if(this->storage_type == partition_based_storage)
    {
        Partition *p = this->partition_manager->get_partition(subtree_root_inode);
        p->write_object(identifier, offset, length, data);
    }
    else if(this->storage_type == file_based_storage)
    {
    	FileStorageDevice device = FileStorageDevice(this->path);
        device.write_object(identifier, offset, length, data, no_sync);
    }
}

void StorageAbstractionLayer::truncate_object(InodeNumber subtree_root_inode, const char *identifier, off_t length)
{
    if(this->storage_type == partition_based_storage)
    {
        Partition *p = this->partition_manager->get_partition(subtree_root_inode);
        p->truncate_object(identifier, length);
    }
    else if(this->storage_type == file_based_storage)
    {
    	FileStorageDevice device = FileStorageDevice(this->path);
        device.truncate_object(identifier, length);
    }
}

size_t StorageAbstractionLayer::get_object_size(InodeNumber subtree_root_inode, const char *identifier)
{
    if(this->storage_type == partition_based_storage)
    {
        Partition *p = this->partition_manager->get_partition(subtree_root_inode);
        return p->get_object_size(identifier);
    }
    else
    {
    	FileStorageDevice device = FileStorageDevice(this->path);
        return device.get_object_size(identifier);
    }
}

void StorageAbstractionLayer::remove_object(InodeNumber subtree_root_inode, const char* identifier)
{
    if(this->storage_type == partition_based_storage)
    {
        Partition *p = this->partition_manager->get_partition(subtree_root_inode);
        p->remove_object(identifier);
    }
    else if(this->storage_type == file_based_storage)
    {
    	FileStorageDevice device = FileStorageDevice(this->path);
        device.remove_object(identifier);
    }
}

void StorageAbstractionLayer::lock_object(InodeNumber subtree_root_inode, const char* identifier)
{
    if(this->storage_type == partition_based_storage)
    {
        Partition *p = this->partition_manager->get_partition(subtree_root_inode);
        p->lock_object(identifier);
    }
    else
    {
    	this->lock_manager->lock_object(identifier);
    }
}

void StorageAbstractionLayer::unlock_object(InodeNumber subtree_root_inode, const char* identifier)
{
    if(this->storage_type == partition_based_storage)
    {
        Partition *p = this->partition_manager->get_partition(subtree_root_inode);
        p->unlock_object(identifier);
    }
    else
    {
    	this->lock_manager->unlock_object(identifier);
    }
}

std::list<std::string> *StorageAbstractionLayer::list_objects(InodeNumber subtree_root_inode)
{
    if(this->storage_type == partition_based_storage)
    {
        Partition *p = this->partition_manager->get_partition(subtree_root_inode);
        return p->list_objects();
    }
    else
    {
    	FileStorageDevice device = FileStorageDevice(this->path);
        return device.list_objects();
    }
}

StorageType StorageAbstractionLayer::get_storage_type()
{
    return this->storage_type;
}

PartitionManager *StorageAbstractionLayer::get_partition_manager()
{
    if(this->storage_type == partition_based_storage)
        return this->partition_manager;
    else
        return NULL;
}
