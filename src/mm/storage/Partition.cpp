#include <string.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>
#include <locale>
#include <errno.h>

#include "mm/storage/Partition.h"
#include "global_types.h"
#include "mm/storage/storage.h"
#include "EmbeddedInode.h"

using namespace std;

/**
 * @brief Constructor of Partition class
 *
 * @param[in] identifier Partition device identifier (e.g. sda1)
 * @param[in] host_identifier Own host identifier (e.g. IP Address)
 * @param[in] host_rank Unique host rank
 * @param[in] total_hosts Total number of hosts
 * @param[in] partition_manager PartitionManager instance
 *
 */
Partition::Partition(char *identifier, char *host_identifier, int host_rank, int total_hosts, PartitionManager *partition_manager)
{
    strncpy(this->identifier, identifier, DEVICE_NAME_LEN);
    this->state = read_only;
    this->migrating_source = NULL;
    this->delete_queue = new std::list<std::string>();
    this->updated_elements = new std::list<std::string>();
    this->lock_manager = new LockManager();
    this->host_rank = host_rank;
    this->total_hosts = total_hosts;
    strncpy(this->host_identifier, host_identifier, SERVER_LEN);
    this->partition_manager = partition_manager;
    this->mount_helper = partition_manager->get_mount_helper();
    snprintf(this->mountpoint, MAX_PATH_LEN, "%s/%s", partition_manager->get_mount_directory(), identifier);

    this->create_mountpoint();


    this->mount_ro();
    if(this->recover_partition_info())
    {
        if(strncmp(this->get_owner(), this->host_identifier, SERVER_LEN) == 0)
        {
            this->mount_rw();
            this->state = active;

            switch(this->running_operation)
            {
            case started_migration:
                this->start_migration(this->migrating_source, 0);
                break;
            case delete_subtree:
                this->remove_subtree(0);
                break;
            case no_operation:
            default:
                break;
            }
        }
    }
    else
    {
        if(this->calculate_ownership(this->identifier))
        {
            this->set_owner(host_identifier);
            this->mount_rw();
        }
        else
        {
        	this->set_owner(OWNED_BY_OTHER_MDS);
            this->state = read_only;
        }
    }
}

/**
 * @brief Destructor of Partition class
 */
Partition::~Partition()
{
    if(this->delete_queue)
        delete this->delete_queue;
    if(this->updated_elements)
    	delete this->updated_elements;
    this->mount_helper->unmount_partition(this->get_mountpoint());
    this->delete_mountpoint();
    delete this->lock_manager;
}

/**
 * @brief   Mount partition in read only mode
 *
 * @throws StorageException
 */
void Partition::mount_ro()
{
    if(this->state == active)
        this->mount_helper->unmount_partition(this->get_mountpoint());

    this->state = read_only;
    this->mount_helper->mount_partition_ro(this->get_identifier(), this->get_mountpoint());
}

/**
 * @brief   Mount partition in write mode
 *
 * @throws StorageException
 */
void Partition::mount_rw()
{
    if(strncmp(this->get_owner(), this->host_identifier, SERVER_LEN) == 0)
    {
        if(this->state == read_only)
            this->mount_helper->unmount_partition(this->get_mountpoint());

        this->state = active;
        this->mount_helper->mount_partition(this->get_identifier(), this->get_mountpoint());
    }
    else
    {
        throw StorageException("Partition owned by other MDS");
    }
}

/**
 * @brief   Unmount partition and delete content
 *
 * @throws StorageException
 */
void Partition::reset()
{
    std::list<std::string> *objects = this->list_objects();
    for(std::list<std::string>::iterator it = objects->begin(); it != objects->end(); it++)
        this->remove_object(it->data());

    this->state = inactive;
    this->mount_helper->unmount_partition(this->get_mountpoint());
    delete objects;
}

/**
 * @brief   Return free space (in bytes) of partition
 *
 * @retval  Available space
 * @throws  StorageException
 */
size_t Partition::get_available_space()
{
    struct statvfs partition_info;

    if((statvfs(this->mountpoint, &partition_info)) < 0 )
    {
        throw StorageException("Cannot get available partition space");
    }
    else
    {
        return partition_info.f_bfree * partition_info.f_bsize;
    }
}

/**
 * @brief   Return total space (in bytes) of partition
 *
 * @retval  Total space
 * @throws  StorageException
 */
size_t Partition::get_size()
{
    struct statvfs partition_info;

    if((statvfs(this->mountpoint,&partition_info)) < 0 )
    {
        throw StorageException("Cannot get partition size");
    }
    else
    {
        return partition_info.f_blocks * partition_info.f_bsize;
    }
}

/**
 * @brief   Get device
 * @retval  Device
 */
char *Partition::get_identifier()
{
    return this->identifier;
}

/**
 * @brief   Get mountpoint
 * @retval  Mountpoint
 */
char *Partition::get_mountpoint()
{
    return this->mountpoint;
}

/**
 * @brief   Get size of an stored object (in bytes)
 *
 * @param[in]   identifier Object name
 * @retval  Object size
 * @throws  StorageException
 */
size_t Partition::get_object_size(const char* identifier)
{
    FileStorageDevice device = FileStorageDevice(this->mountpoint);
    return device.get_object_size(identifier);
}

/**
 * @brief   Get inode number of subtree root
 * @retval  Inode number
 */
InodeNumber Partition::get_root_inode()
{
    return this->root_inode;
}

/**
 * @brief   Get partition state
 * @retval  Partition state
 */
PartitionState Partition::get_state()
{
    return this->state;
}

/**
 * @brief   Read object from partition
 *
 * @param[in]   identifier Object identifier
 * @param[in]   offset Offset in object
 * @param[in]   length Bytes to read
 * @param[out]  data Read data
 * @throws      StorageException
 */
void Partition::read_object(const char* identifier, off_t offset, size_t length, void* data)
{
    FileStorageDevice device = FileStorageDevice(this->mountpoint);
    if(this->state == active || this->state == read_only)
    {
        device.read_object(identifier, offset, length, data);
    }
    else if (this->state == migrating)
    {
        if(this->in_delete_queue(identifier))
        {
            // object deleted while migration
            throw StorageException("Object not existing");
        }
        else if (this->has_object(identifier))
        {
            // object still migrated => read from this partition
            device.read_object(identifier, offset, length, data);
        }
        else
        {
            // object not migrated yet => read from old partition
            this->migrating_source->read_object(identifier, offset, length, data);
        }
    }
    else
    {
        throw StorageException("Object not readable");
    }
}

/**
 * @brief   Delete existing object
 *
 * @param[in]   identifier Object identifier
 * @throws  StorageException
 */
void Partition::remove_object(const char* identifier)
{
    if(this->state == active)
    {
        FileStorageDevice device = FileStorageDevice(this->mountpoint);
        device.remove_object(identifier);
    }
    else if(this->state == migrating)
    {
        this->delete_queue->push_back(identifier);
    }
    else
    {
        throw StorageException("Partition not writeable");
    }
}

/**
 * @brief   Truncate existing object
 *
 * @param[in]   identifier Object identifier
 * @param[in]   length New length
 * @throws  StorageException
 */
void Partition::truncate_object(const char* identifier, off_t length)
{
    FileStorageDevice device = FileStorageDevice(this->mountpoint);
    if(this->state == active)
    {
        device.truncate_object(identifier, length);
    }
    else if(this->state == migrating)
    {
        if(!this->has_object(identifier) && this->migrating_source->has_object(identifier))
        {
            this->copy_object(this->migrating_source, identifier);
        }
        device.truncate_object(identifier, length);
    }
    else
    {
        throw StorageException("Partition not writeable");
    }
}

/**
 * @brief   Write object to partition
 *
 * @param[in]   identifier Object identifier
 * @param[in]   offset Offset in object
 * @param[in]   length Bytes to write
 * @param[out]  data Data to write
 * @throws  StorageException
 */
void Partition::write_object(const char* identifier, off_t offset, size_t length, void* data, bool no_sync)
{
    FileStorageDevice device = FileStorageDevice(this->mountpoint);

    if(this->state == active)
    {
        device.write_object(identifier, offset, length, data, no_sync);
    }
    else if(this->state == migrating)
    {
        if(!this->has_object(identifier) && this->migrating_source->has_object(identifier))
        {
            this->copy_object(this->migrating_source, identifier);
        }
        device.write_object(identifier, offset, length, data, no_sync);
    }
    else
    {
        throw StorageException("Partition not writeable");
    }
}

void Partition::write_object(const char* identifier, off_t offset, size_t length, void* data)
{
	this->write_object(identifier, offset, length, data, false);
}

/**
 * @brief       Check if an object called $identifier exists
 *
 * @param[in]   identifier Object identifier
 * @retval  true, if object exists
 */
bool Partition::has_object(const char* identifier)
{
    if(this->state != inactive)
    {
        FileStorageDevice device = FileStorageDevice(this->mountpoint);
        return device.has_object(identifier);
    }
    else
    {
        throw StorageException("Invalid Partition state");
    }
}

/**
 * @brief   Lock object
 *
 * @param[in]   identifier Object to lock
 */
void Partition::lock_object(const char* identifier)
{
    this->lock_manager->lock_object(identifier);
}

/**
 * @brief   Unlock object
 *
 * @param[in]   identifier Object to unlock
 */
void Partition::unlock_object(const char* identifier)
{
    this->lock_manager->unlock_object(identifier);
}

/**
 * @brief   Set root inode of Partition
 * @param[in]   root_inode Root inode number
 */
void Partition::set_root_inode(InodeNumber root_inode)
{
    this->root_inode = root_inode;
    if(this->state == active)
        this->write_parition_info();
}

/**
 * @brief Migrate subtree with root $root_node to ruccent partition
 *
 * @param[in] source Source Partition
 * @param[in] root_inode Root inode of subtree
 *
 * @throws StorageException
 */
void Partition::start_migration(Partition* source, InodeNumber root_inode)
{
    std::list<std::string> objects;
    this->state = migrating;
    this->migrating_source = source;
    bool recovery = false;

    while(this->running_operation != no_operation && this->running_operation != started_migration)
    {
        // wait until other operations are finished
        usleep(SLEEP_TIME);
    }

    if(this->running_operation == no_operation)
    {
        source->list_subtree_objects(root_inode, 0, &objects);
        this->write_objectlist_to_file(&objects, RUNNING_OPERATION_OBJECT);
        this->running_operation = started_migration;
        this->write_parition_info();
    }
    else
    {
        recovery = true;
        this->read_objectlist_from_file(&objects, RUNNING_OPERATION_OBJECT);
    }

    for(std::list<std::string>::iterator it = objects.begin(); it != objects.end(); it++)
    {
        if(!this->has_object(it->data()) && !this->in_delete_queue(it->data()) && !this->has_update(it->data()))
        {
            this->copy_object(source, it->data());
        }
        else
        {
            if(recovery && this->get_object_size(it->data()) != source->get_object_size(it->data()) &&
                    !this->in_delete_queue(it->data()) && !this->has_update(it->data()))
            {
                // copying was probably failed
                this->remove_object(it->data());
                this->copy_object(source, it->data());
            }
        }
    }

    this->delete_queue->clear();
    source->remove_subtree(root_inode);
    this->running_operation = no_operation;
    this->state = active;
    this->migrating_source = NULL;
    this->write_parition_info();
    this->remove_object(RUNNING_OPERATION_OBJECT);
}

/**
 * @brief Enlarge subtree stored in current partition
 *
 * @param[in] parent Parent subtree partition in file-system hierarchy
 * @param[in] new_root_inode New subtree root inode number (has to be in $parent)
 *
 * @throws StorageException
 */
void Partition::enlarge_subtree (Partition* parent, InodeNumber new_root_inode)
{
    this->set_root_inode(new_root_inode);
    this->start_migration(parent, new_root_inode);
}

/**
 * @brief Truncate subtree stored in current partition
 *
 * @param[in] new_root_inode New subtree root inode number
 *
 * @throws StorageException
 */
void Partition::truncate_subtree (InodeNumber new_root_inode)
{
    InodeNumber old_root_inode = this->get_root_inode();
    this->set_root_inode(new_root_inode);
    this->remove_subtree(old_root_inode, new_root_inode);
}

/**
 * @brief Remove subtree
 *
 * Removes any metadata object stored in the current partition, beginning
 * at the given $inode_number
 *
 * @param[in] inode_number Inode number
 *
 * @throws StorageException
 */
void Partition::remove_subtree (InodeNumber inode_number)
{
    this->remove_subtree(inode_number, 0);
}

/**
 * @brief Remove subtree
 *
 * Removes any metadata object stored in the current partition, beginning
 * at the given $inode_number. The method stops at the given $stop inode
 * number
 *
 * @param[in] inode_number Inode number
 * @param[in] stop Inode number to stop removing at
 *
 * @throws StorageException
 */
void Partition::remove_subtree (InodeNumber inode_number, InodeNumber stop)
{
    std::list<std::string> *subtree_objects = new std::list<std::string>();

    while(this->running_operation != no_operation && this->running_operation != delete_subtree)
    {
        // wait until other operations are finished
        usleep(SLEEP_TIME);
    }

    if(this->running_operation == no_operation)
    {
        this->list_subtree_objects(inode_number, stop, subtree_objects);
        this->running_operation = delete_subtree;
        this->write_objectlist_to_file(subtree_objects, RUNNING_OPERATION_OBJECT);
        this->write_parition_info();
    }
    else
    {
        this->read_objectlist_from_file(subtree_objects, RUNNING_OPERATION_OBJECT);
    }

    for(std::list<std::string>::iterator it = subtree_objects->begin(); it != subtree_objects->end(); it++)
    {
        if(this->has_object(it->data()))
        {
            this->remove_object(it->data());
        }
    }

    this->running_operation = no_operation;
    this->write_parition_info();
    this->remove_object(RUNNING_OPERATION_OBJECT);
    delete subtree_objects;
}

/**
 * @brief Get Partition owner
 *
 * @retval Partition owner
 */
char* Partition::get_owner()
{
    return this->partition_owner;
}

/**
 * @brief Set Parition owner
 *
 * @param[in] owner New Partition owner
 *
 * @throws StorageException
 */
void Partition::set_owner(char* owner)
{
    strncpy(this->partition_owner, owner, SERVER_LEN);
    if(this->state == active)
    {
        if(strncmp(this->get_owner(), this->host_identifier, SERVER_LEN) == 0)
        {
        	this->mount_rw();
            this->write_parition_info();
        }
        else
        {
            this->mount_ro();
        }
    }
}

/**
 * @brief Check if metadata object was removed while current migration
 *
 * @param[in] identifier Object to check
 */
bool Partition::in_delete_queue(const char* identifier)
{
    for (std::list<std::string>::iterator it = this->delete_queue->begin(); it != this->delete_queue->end(); it++)
    {
        if(strcmp(identifier, it->data()) == 0)
            return true;
    }

    return false;
}

/**
 * @brief Check if metadata object was updated while current migration
 *
 * @param[in] identifier Object to check
 */
bool Partition::has_update(const char* identifier)
{
    for (std::list<std::string>::iterator it = this->updated_elements->begin(); it != this->updated_elements->end(); it++)
    {
        if(strcmp(identifier, it->data()) == 0)
            return true;
    }

    return false;
}

/**
 * @brief Write Partition info to disk
 */
void Partition::write_parition_info()
{
    PartitionInfo partition_info;
    strncpy(partition_info.partition_owner, this->get_owner(), SERVER_LEN);
    partition_info.root_inode = this->get_root_inode();
    partition_info.running_operation = this->running_operation;
    strncpy(partition_info.migraging_source, this->migrating_source?this->migrating_source->get_identifier():"", DEVICE_NAME_LEN);

    this->write_object(PARTITION_INFO_OBJECT, 0, sizeof(PartitionInfo), &partition_info);
}

/**
 * @brief Read parition info from disk and set corresponding attributes
 */
bool Partition::recover_partition_info()
{
    if(this->state == active || this->state == read_only)
    {
        if(this->has_object(PARTITION_INFO_OBJECT))
        {
            PartitionInfo partition_info;
            this->read_object(PARTITION_INFO_OBJECT, 0, sizeof(PartitionInfo), &partition_info);

            this->set_owner(partition_info.partition_owner);
            this->set_root_inode(partition_info.root_inode);
            this->running_operation = partition_info.running_operation;

            if(this->running_operation != no_operation)
            {
                this->migrating_source = this->partition_manager->get_partition_by_identifier(partition_info.migraging_source);
            }

            return true;
        }
    }
    this->set_root_inode(0);
    this->running_operation = no_operation;

    return false;
}

/**
 * @brief Creates list of all metadata objects in the file-system hierarchy, starting at $root
 *
 * @param[in] root Inode number to start at
 * @param[in] stop Inode number to stop at
 * @param[out] objects List of subtree objects (has to bee freed by caller)
 *
 * @throws StorageException
 */
void Partition::list_subtree_objects(InodeNumber root, InodeNumber stop, std::list<std::string> *objects)
{
    if(objects == NULL)
        objects = new std::list<std::string>();

    char objStr[MAX_NAME_LEN];
    snprintf(objStr, MAX_NAME_LEN, "%ld", root);
    if(this->has_object(objStr))
        objects->push_back(objStr);
    else
        return;

    if(this->has_object(objStr))
    {
        int i;
        unsigned long object_len = this->get_object_size(objStr) / sizeof(EInode);
        for (i = 0; i < object_len; i++)
        {
            EInode einode;
            this->read_object(objStr, (off_t) i * sizeof(EInode), sizeof(EInode), (void *) &einode);
            if(einode.inode.inode_number != stop)
            {
                this->list_subtree_objects(einode.inode.inode_number, stop, objects);
            }
        }
    }
}

/**
 * @brief Create list of all objects stored in current Partition
 *
 * @retval List of objects
 *
 * @throws StorageException
 */
std::list<std::string> *Partition::list_objects()
{
    FileStorageDevice device = FileStorageDevice(this->mountpoint);
    return device.list_objects();
}

/**
 * @brief Write list of strings to object on partition
 *
 * @param[in] objects List of strings
 * @param[in] filename Object to write at
 *
 * @throws StorageException
 */
void Partition::write_objectlist_to_file(std::list<std::string> *objects, const char *filename)
{
    off_t offset = 0;

    for(std::list<std::string>::iterator it = objects->begin(); it != objects->end(); it++)
    {
        char data[MAX_NAME_LEN];
        strncpy(data, it->data(), MAX_NAME_LEN);
        this->write_object(filename, offset, MAX_NAME_LEN, data);
        offset += MAX_NAME_LEN;
    }
}

/**
 * @brief Read list of strings from object on partition
 *
 * @param[out] objects List of strings at $filename
 * @param[in] filename Object to read from
 *
 * @throws StorageException
 */
void Partition::read_objectlist_from_file(std::list<std::string> *objects, const char *filename)
{
    size_t object_size = this->get_object_size(filename);
    off_t offset;

    for(offset = 0; offset < object_size; offset += MAX_NAME_LEN)
    {
        char data[MAX_NAME_LEN];
        this->read_object(filename, offset, MAX_NAME_LEN, data);
        objects->push_back(data);
    }
}

/**
 * @brief Copy object from $source to current Partition
 *
 * @param[in] source Partition to read from
 * @param[in] identifier Object identifier
 *
 * @throws StorageException
 */
void Partition::copy_object(Partition* source, const char* identifier)
{
    size_t len;
    off_t offset = 0;
    char buffer[COPY_BUFFER_SIZE];

    len = this->migrating_source->get_object_size(identifier);

    while((off_t) len - offset > 0)
    {
        FileStorageDevice device = FileStorageDevice(this->mountpoint);
        source->read_object(identifier, offset, std::min((long unsigned int) COPY_BUFFER_SIZE, len - offset), buffer);
        device.write_object(identifier, offset, std::min((long unsigned int) COPY_BUFFER_SIZE, len - offset), buffer);
        offset += COPY_BUFFER_SIZE;
    }
}

/**
 * @brief Create mount directory for current partition, if not existing
 *
 * @throws StorageException
 */
void Partition::create_mountpoint()
{
    int status;
    status = mkdir(this->get_mountpoint(), S_IRWXU);

    //if(status != 0 || (status == -1 && errno != EEXIST))
    //    throw StorageException("Creating mountpoint failed");
}

/**
 * @brief Delete mountpoint
 */
void Partition::delete_mountpoint()
{
    rmdir(this->get_mountpoint());
}

/**
 * @brief Calculate ownership of partition
 *
 * @param[in] identifier Partition identifier
 * @retval true iff current host is owner of $identifier
 */
bool Partition::calculate_ownership(char *identifier)
{
	locale loc;
	const collate<char>& coll = use_facet<collate<char> >(loc);
	long hash = coll.hash(identifier, identifier + strlen(identifier));

    return (hash % this->total_hosts == this->host_rank);
}

/**
 * @brief Reset $host_rank and $total_hosts and recalculate ownership
 *
 * @param[in] host_rank New unique host rank
 * @param[in] total_hosts Total number of hosts
 */
void Partition::recalculate_ownerships(int host_rank, int total_hosts)
{
    this->host_rank = host_rank;
    this->total_hosts = total_hosts;

    if(this->get_root_inode() == 0 && this->calculate_ownership(this->get_identifier()))
    {
        this->set_owner(this->host_identifier);
    }
}
