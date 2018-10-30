#include <stdlib.h>
#include "mm/storage/storage.h"

DataDistributionManager::DataDistributionManager(AbstractDataDistributionFunction *ddf)
{
    if (ddf == NULL)
        throw StorageException("Not DataDistributionFunction given");
    else
        this->data_distribution_function = ddf;
}

std::list<AbstractStorageDevice*> DataDistributionManager::get_metadata_storage_devices()
{
    return this->metadata_storage_devices;
}

AbstractStorageDevice *DataDistributionManager::get_metadata_location(const char *identifier)
{
    AbstractStorageDevice *device;
    device = this->data_distribution_function->get_metadata_location(identifier);
    return device;
}

void DataDistributionManager::add_metadata_storage_devices(std::list<AbstractStorageDevice*> devices)
{

}

void DataDistributionManager::remove_metadata_storage_devices(std::list<AbstractStorageDevice*> devices)
{

}
