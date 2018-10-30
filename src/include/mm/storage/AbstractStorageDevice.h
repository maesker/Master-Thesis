#ifndef ABSTRACTSTORAGEDEVICE_H_
#define ABSTRACTSTORAGEDEVICE_H_

#include <stdlib.h>
#include <string>
#include <list>
#include "storage.h"


/**
 * Abstract representation of a storage device
 */
class AbstractStorageDevice
{
public:
    AbstractStorageDevice() {};
    virtual ~AbstractStorageDevice() {};

    virtual char *get_identifier() = 0;
    virtual void set_identifier(char *identifier) {};
    virtual void read_object(const char *identifier, off_t offset, size_t length, void *data) = 0;
    virtual void write_object(const char *identifier, off_t offset, size_t length, void *data) = 0;
    virtual void write_object(const char *identifier, off_t offset, size_t length, void *data, bool no_sync) = 0;
    virtual void truncate_object(const char *identifier, off_t length) = 0;
    virtual size_t get_object_size(const char *identifier) = 0;
    virtual bool has_object(const char *identifier) = 0;
    virtual void remove_object(const char *identifier) = 0;
    virtual std::list<std::string> *list_objects() = 0;
};

#endif
