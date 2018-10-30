/*
 * FileStorageDevice.h
 *
 *  Created on: 12.04.2011
 *      Author: chrkl
 */

#ifndef FILESTORAGEDEVICE_H_
#define FILESTORAGEDEVICE_H_

#include <stdlib.h>
#include <list>
#include <string>

#include "storage.h"
#include "global_types.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

/**
 * File based storage device (e.g. NFS device or local Disk)
 */
class FileStorageDevice : public AbstractStorageDevice
{
public:
    FileStorageDevice(char *path);
    virtual ~FileStorageDevice() {};
    char *get_identifier();
    void set_identifier(char *identifier);
    void read_object(const char *identifier, off_t offset, size_t length, void *data);
    void write_object(const char *identifier, off_t offset, size_t length, void *data);
    void write_object(const char *identifier, off_t offset, size_t length, void *data, bool no_sync);
    void truncate_object(const char *identifier, off_t length);
    size_t get_object_size(const char *identifier);
    bool has_object(const char *identifier);
    void remove_object(const char *identifier);
    std::list<std::string> *list_objects();
private:
    char *identifier;
    Pc2fsProfiler *profiler;
};

#endif /* FILESTORAGEDEVICE_H_ */
