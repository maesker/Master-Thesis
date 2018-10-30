/*
 * SimpleDataDistributionFunction.h
 *
 *  Created on: 24.04.2011
 *      Author: chrkl
 */

#ifndef SIMPLEDATADISTRIBUTIONFUNCTION_H_
#define SIMPLEDATADISTRIBUTIONFUNCTION_H_

#include "storage.h"

/**
 * Implementation of the AbstractDataDistributionFunction, which deploys all data to a singe device
 */
class SimpleDataDistributionFunction : public AbstractDataDistributionFunction
{
public:
    SimpleDataDistributionFunction();
    ~SimpleDataDistributionFunction() {};
    AbstractStorageDevice *get_data_location(const char *identifier);
    AbstractStorageDevice *get_metadata_location(const char *identifier);
    void change_data_storage_devices(std::list<AbstractStorageDevice*> *oldDevices, std::list<AbstractStorageDevice*> *newDevices);
    void change_metadata_storage_devices(std::list<AbstractStorageDevice*> *oldDevices, std::list<AbstractStorageDevice*> *newDevices);
private:
    AbstractStorageDevice *device;
};

#endif
