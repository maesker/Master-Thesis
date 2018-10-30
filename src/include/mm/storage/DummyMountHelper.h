#ifndef DUMMYMOUNTHELPER_H
#define DUMMYMOUNTHELPER_H

#include "mm/storage/AbstractMountHelper.h"

class DummyMountHelper: public AbstractMountHelper
{
public:
    DummyMountHelper();
    virtual ~DummyMountHelper();
    void mount_partition(char *device, char *mountpoint);
    void mount_partition_ro(char *device, char *mountpoint);
    void unmount_partition(char *mountpoint);
};

#endif // DUMMYMOUNTHELPER_H
