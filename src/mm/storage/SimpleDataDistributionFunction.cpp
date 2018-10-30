/*
 * SimpleDataDistributionFunction.cpp
 *
 *  Created on: 24.04.2011
 *      Author: chrkl
 */

#include "mm/storage/storage.h"

SimpleDataDistributionFunction::SimpleDataDistributionFunction()
{
}

AbstractStorageDevice *SimpleDataDistributionFunction::get_data_location(const char *identifier)
{
    return this->device;
}

AbstractStorageDevice *SimpleDataDistributionFunction::get_metadata_location(const char *identifier)
{
    return this->device;
}

void SimpleDataDistributionFunction::change_data_storage_devices(std::list<AbstractStorageDevice*> *oldDevices, std::list<AbstractStorageDevice*> *newDevices)
{
    AbstractStorageDevice *device = newDevices->front();
    if(device)
        this->device = device;
    else
        throw StorageException("No device");
}

void SimpleDataDistributionFunction::change_metadata_storage_devices(std::list<AbstractStorageDevice*> *oldDevices, std::list<AbstractStorageDevice*> *newDevices)
{
    AbstractStorageDevice *device = newDevices->front();
    if(device)
        this->device = device;
    else
        throw StorageException("No device");
}
