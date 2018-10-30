#ifndef ABSTRACTMOUNTHELPER_H
#define ABSTRACTMOUNTHELPER_H

class AbstractMountHelper
{

public:
    AbstractMountHelper() {};
    virtual ~AbstractMountHelper() {};
    virtual void mount_partition(char *device, char *mountpoint) {};
    virtual void mount_partition_ro(char *device, char *mountpoint) {};
    virtual void unmount_partition(char *mountpoint) {};
};

#endif // ABSTRACTMOUNTHELPER_H
