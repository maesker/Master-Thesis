/**
 * @file PartitionManagerAccess.cpp
 * @class PartitionManagerAccess
 * @date 15 August 2011
 * @author Denis Dridger
 *
 *
 * @brief provides convenient access to PartitionManager
 *
 */

#include <cstddef>
#include <stdlib.h>
#include <stdio.h>
#include <cassert>
#include "coco/loadbalancing/PartitionManagerAccess.h"


PartitionManagerAccess* PartitionManagerAccess::__instance = NULL;

PartitionManagerAccess::PartitionManagerAccess(StorageAbstractionLayer *storage_abstraction_layer)
{
	this->storage_abstraction_layer  = storage_abstraction_layer;
}

PartitionManagerAccess::~PartitionManagerAccess()
{
	
}


void PartitionManagerAccess::create_instance(StorageAbstractionLayer *storage_abstraction_layer)
{
	if ( __instance == NULL )
	{
		__instance = new PartitionManagerAccess(storage_abstraction_layer);
	}
}

PartitionManagerAccess* PartitionManagerAccess::get_instance()
{
		assert(__instance);
		return __instance;
}

PartitionManager *PartitionManagerAccess::get_partition_manager()
{
	return this->storage_abstraction_layer->get_partition_manager();
}


/*
access_counter_return_t *PartitionManagerAccess::determine_subtree_to_move()
{	
}
*/

