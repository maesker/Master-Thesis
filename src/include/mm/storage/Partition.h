#ifndef PARTITION_H
#define PARTITION_H

#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <list>
#include <string>

#include "global_types.h"
#include "AbstractMountHelper.h"
#include "LockManager.h"
#include "PartitionManager.h"

#define PARTITION_INFO_OBJECT       "partition_info"
#define RUNNING_OPERATION_OBJECT    "running_operation"
#define SLEEP_TIME                  10000000
#define COPY_BUFFER_SIZE            512
#define OWNED_BY_OTHER_MDS			"other_mds"

typedef enum
{
    active,
    read_only,
    migrating,
    inactive
} PartitionState;

typedef enum
{
    delete_subtree,
    started_migration,
    no_operation
} PartitionOperation;

typedef struct
{
    InodeNumber root_inode;
    char partition_owner[SERVER_LEN];
    PartitionOperation running_operation;
    char migraging_source[DEVICE_NAME_LEN];
} PartitionInfo;

class PartitionManager;

/**
 * @brief Representation of a block device partition
 */
class Partition
{
public:
    Partition(char *identifier, char *host_identifier, int host_rank, int total_hosts, PartitionManager *pm);
    virtual ~Partition();
    void read_object(const char *identifier, off_t offset, size_t length, void *data);
    void write_object(const char *identifier, off_t offset, size_t length, void *data);
    void write_object(const char *identifier, off_t offset, size_t length, void *data, bool no_sync);
    void truncate_object(const char *identifier, off_t length);
    size_t get_object_size(const char *identifier);
    bool has_object(const char *identifier);
    void remove_object(const char *identifier);
    void lock_object(const char *identifier);
    void unlock_object(const char *identifier);
    void mount_rw();
    void mount_ro();
    void reset();
    void start_migration(Partition *source, InodeNumber root_inode);
    void enlarge_subtree(Partition *parent, InodeNumber new_root_inode);
    void truncate_subtree(InodeNumber new_root_inode);
    void remove_subtree(InodeNumber inode_number);
    InodeNumber get_root_inode();
    void set_root_inode(InodeNumber root_inode);
    char *get_identifier();
    char *get_mountpoint();
    PartitionState get_state();
    size_t get_size();
    size_t get_available_space();
    void set_owner(char *owner);
    char *get_owner();
    void list_subtree_objects(InodeNumber root, InodeNumber stop, std::list<std::string> *objects);
    std::list<std::string> *list_objects();
    void recalculate_ownerships(int host_rank, int total_hosts);
private:
    bool in_delete_queue(const char *identifier);
    bool has_update(const char *identifier);
    void write_parition_info();
    bool recover_partition_info();
    void write_objectlist_to_file(std::list<std::string> *objects, const char *filename);
    void read_objectlist_from_file(std::list<std::string> *objects, const char *filename);
    void copy_object(Partition *source, const char *identifier);
    void remove_subtree(InodeNumber inode_number, InodeNumber stop);
    void create_mountpoint();
    void delete_mountpoint();
    bool calculate_ownership(char *identifier);

    InodeNumber root_inode;
    char partition_owner[SERVER_LEN];
    char host_identifier[SERVER_LEN];
    int host_rank;
    int total_hosts;
    char identifier[DEVICE_NAME_LEN];
    char mountpoint[MAX_PATH_LEN];
    PartitionState state;
    PartitionOperation running_operation;
    PartitionManager *partition_manager;
    Partition *migrating_source;
    std::list<std::string> *delete_queue;
    std::list<std::string> *updated_elements;
    AbstractMountHelper *mount_helper;
    LockManager *lock_manager;
};
#endif // PARTITION_H
