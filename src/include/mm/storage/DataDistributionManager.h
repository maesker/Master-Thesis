#ifndef DATADISTRIBUTIONMANAGER_H_
#define DATADISTRIBUTIONMANAGER_H_

#include <stdlib.h>
#include "storage.h"

/**
 * Calculates the location of a given object identifier, using a defined data distribution function
 */
class DataDistributionManager
{
public:
    DataDistributionManager(AbstractDataDistributionFunction *dataDistributionFct);
    virtual ~DataDistributionManager() {};
    std::list<AbstractStorageDevice*> get_metadata_storage_devices();
    AbstractStorageDevice *get_metadata_location(const char *identifier);
    void add_metadata_storage_devices(std::list<AbstractStorageDevice*> devices);
    void remove_metadata_storage_devices(std::list<AbstractStorageDevice*> devices);

private:
    std::list<AbstractStorageDevice*> storage_devices;
    std::list<AbstractStorageDevice*> metadata_storage_devices;
    AbstractDataDistributionFunction *data_distribution_function;
};

#endif
