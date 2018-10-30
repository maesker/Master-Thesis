#include <sys/mount.h>
#include <string.h>
#include <stdio.h>

#include "mm/storage/MountHelper.h"
#include "mm/storage/StorageException.h"
#include "global_types.h"

/**
 * @brief Default constructor of MountHelper
 */
MountHelper::MountHelper()
{
    this->mountflags = 0;
    memset(this->fs_type, 0 , FS_TYPE_SIZE);
}

/**
 * @brief Constructor of MountHelper
 *
 * @param[in] fs_type File system type (e.g. ext3)
 * @param[in] mountflags Mountflags (see mount(2))
 */
MountHelper::MountHelper(char *fs_type, unsigned int mountflags)
{
    strncpy(this->fs_type, fs_type, FS_TYPE_SIZE);
    this->mountflags = mountflags;
}

/**
 * @brief Set fs_type
 * @param[in] fs_type
 */
void MountHelper::set_fs_type(char *fs_type)
{
    strncpy(this->fs_type, fs_type, FS_TYPE_SIZE);
}

/**
 * @brief Set mountflags
 * @param[in] mountflags
 */
void MountHelper::set_mountflags(unsigned int mountflags)
{
    this->mountflags = mountflags;
}

/**
 * @brief Mount partition
 *
 * @param[in] device Device to mount
 * @param[in] mountpoint Mountpoint
 * @throws StorageException
 */
void MountHelper::mount_partition(char* device, char* mountpoint)
{
    char device_path[MAX_PATH_LEN];

    if(!strstr("/dev", device))
        snprintf(device_path, MAX_PATH_LEN, "/dev/%s", device);
    else
        snprintf(device_path, MAX_PATH_LEN, "%s", device);

    mount(device_path, mountpoint, this->fs_type, mountflags, "");
}

/**
 * @brief Mount partition in read only mode
 *
 * @param[in] device Device to mount
 * @param[in] mountpoint Mountpoint
 * @throws StorageException
 */
void MountHelper::mount_partition_ro(char* device, char* mountpoint)
{
    char device_path[MAX_PATH_LEN];

    if(!strstr("/dev", device))
        snprintf(device_path, MAX_PATH_LEN, "/dev/%s", device);
    else
        snprintf(device_path, MAX_PATH_LEN, "%s", device);


    mount(device_path, mountpoint, this->fs_type, mountflags | MS_RDONLY, ""); 
}

/**
 * @brief Unmount partition
 *
 * @param[in] mountpoint Mountpoint
 * @throws StorageException
 */
void MountHelper::unmount_partition(char* mountpoint)
{
    umount(mountpoint);
}
