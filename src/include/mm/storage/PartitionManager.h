#ifndef PARTITIONMANAGER_H
#define PARTITIONMANAGER_H

class Partition;
class ChangeOwnershipDAOAdapter;
class PartitionManager;

#include <list>
#include <vector>

#include "Partition.h"
#include "global_types.h"

/**
 * @brief Manages Partition objects for each available storage partition
 */
class PartitionManager
{
public:
    PartitionManager(std::list<std::string> *devices, const char *mount_directory, AbstractMountHelper *mount_helper, const char *host_identifier, int host_rank, int total_hosts);
    virtual ~PartitionManager();
    void set_server_list(std::vector<std::string> *server_list, int rank, int total_hosts);
    Partition *get_partition(InodeNumber root_inode);
    Partition *get_partition_by_identifier(char *identifier);
    Partition *get_free_partition();
    Partition *get_free_owned_partition();
    AbstractMountHelper *get_mount_helper();
    char *get_mount_directory();
    std::list<Partition*> *get_partitions();
    void recalculate_ownerships(int host_rank, int total_hosts);
    char *get_host_identifier();
private:
    Partition *get_free_remote_partition();
    std::list<Partition*> *partitions;
    char own_host_identifier[SERVER_LEN];
    char mount_directory[MAX_PATH_LEN];
    AbstractMountHelper *mount_helper;
    int host_rank;
    int total_hosts;
    std::vector<std::string> *server_list;
};

#endif // PARTITIONMANAGER_H
