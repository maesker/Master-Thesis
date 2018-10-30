/*
 * StorageAbstractionLayer.h
 *
 *  Created on: 09.04.2011
 *      Author: chrkl
 */

#ifndef STORAGEABSTRACTIONLAYER_H_
#define STORAGEABSTRACTIONLAYER_H_

#include <stdlib.h>
#include <list>
#include <string>
#include "storage.h"

#include "mm/storage/LockManager.h"

class PartitionManager;
class DataDistributionManager;

typedef enum
{
    partition_based_storage,
    file_based_storage
} StorageType;

/**
 * Presents a uniform view to a set of storage devices.
 */
class StorageAbstractionLayer
{
public:
    StorageAbstractionLayer(char *path);
    StorageAbstractionLayer(std::list<std::string> *devices, const char *mount_dir, AbstractMountHelper *mh, const char *host_identifer, int host_rank, int total_hosts);
    virtual ~StorageAbstractionLayer();
    void read_object(InodeNumber subtree_root_inode, const char *identifier, off_t offset, size_t length, void *data);
    void write_object(InodeNumber subtree_root_inode, const char *identifier, off_t offset, size_t length, void *data);
    void write_object(InodeNumber subtree_root_inode, const char *identifier, off_t offset, size_t length, void *data, bool no_sync);
    void truncate_object(InodeNumber subtree_root_inode, const char *identifier, off_t length);
    size_t get_object_size(InodeNumber subtree_root_inode, const char *identifier);
    void remove_object(InodeNumber subtree_root_inode, const char *identifier);
    void lock_object(InodeNumber subtree_root_inode, const char *identifier);
    void unlock_object(InodeNumber subtree_root_inode, const char *identifier);
    std::list<std::string> *list_objects(InodeNumber subtree_root_inode);
    StorageType get_storage_type();
    PartitionManager *get_partition_manager();

private:
    PartitionManager *partition_manager;
    StorageType storage_type;
    LockManager *lock_manager;
    char *path;
};

#endif /* STORAGEABSTRACTIONLAYER_H_ */
