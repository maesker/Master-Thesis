#ifndef MOUNTHELPER_H
#define MOUNTHELPER_H

#include "mm/storage/AbstractMountHelper.h"

#define FS_TYPE_SIZE 64

/**
 * @brief Helper class for mounting and unmounting partitions
 */
class MountHelper: public AbstractMountHelper
{
public:
    MountHelper();
    MountHelper(char *fs_type, unsigned int mountflags);
    virtual ~MountHelper() {};
    void set_fs_type(char *fs_type);
    void set_mountflags(unsigned int mountflags);
    void mount_partition(char *device, char *mountpoint);
    void mount_partition_ro(char *device, char *mountpoint);
    void unmount_partition(char *mountpoint);
private:
    char fs_type[FS_TYPE_SIZE];
    unsigned int mountflags;
};

#endif // DUMMYMOUNTHELPER_H
