#ifndef ABSTRACTDATADISTRIBUTIONFUNCTION_H_
#define ABSTRACTDATADISTRIBUTIONFUNCTION_H_

#include <list>
#include <stdlib.h>
#include "storage.h"

/**
 * Abstract representation of a data distribution function
 */
class AbstractDataDistributionFunction
{
public:
    AbstractDataDistributionFunction() {};
    ~AbstractDataDistributionFunction() {};
    virtual AbstractStorageDevice *get_data_location(const char *identifier) = 0;
    virtual AbstractStorageDevice *get_metadata_location(const char *identifier) = 0;
    virtual void change_data_storage_devices(std::list<AbstractStorageDevice*> *oldDevices, std::list<AbstractStorageDevice*> *newDevices) = 0;
    virtual void change_metadata_storage_devices(std::list<AbstractStorageDevice*> *oldDevices, std::list<AbstractStorageDevice*> *newDevices) = 0;
};

#endif
