/**
 * @file PartitionManagerAccess.h
 *
 * @date 15 August 2011
 * @author Denis Dridger
 *
 *
 * @brief provides convenient access to PartitionManager
 *
 */


#ifndef PARTITIONMANAGERACCESS_H_
#define PARTITIONMANAGERACCESS_H_

#include "mm/storage/StorageAbstractionLayer.h"



class PartitionManagerAccess
{
public:
	PartitionManagerAccess(StorageAbstractionLayer *storage_abstraction_layer);
	virtual ~PartitionManagerAccess();

	static void create_instance(StorageAbstractionLayer *storage_abstraction_layer);

	static PartitionManagerAccess* get_instance();

	PartitionManager *get_partition_manager();

private:
	static PartitionManagerAccess* __instance;
	StorageAbstractionLayer *storage_abstraction_layer;
	
};

#endif 

