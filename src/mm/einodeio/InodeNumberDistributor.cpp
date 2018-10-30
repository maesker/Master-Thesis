/*
 * InodeNumberDistributor.cpp
 *
 *  Created on: 22.06.2011
 *      Author: sergerit
 */
#include "mm/einodeio/InodeNumberDistributor.h"
#include "mm/storage/PartitionManager.h"
#include "mm/storage/Partition.h"

#include <math.h>

using namespace std;

/** @brief Constructor for InodeNumberDistributor
 *
 *  @param[in] rank Rank of current MDS (e.g. Position in global list of MDSs)
 *  @param[in] sal Pointer to StorageAbstractionLayer instance, this parameter is optional, no persistent storage if NULL
 *  @param[in] partition The subtree partition the inode distribution should be related to
 *
 *  @throws EInodeIOException If rank is larger than total number if inode number partitions
 */
InodeNumberDistributor::InodeNumberDistributor(int rank, StorageAbstractionLayer *sal, InodeNumber partition)
{
    char object_name[MAX_NAME_LEN];
    this->rank = rank;
    this->last_number = 0;
    this->partition = partition;

    pthread_mutex_init(&(this->lock), NULL);
    this->profiler = Pc2fsProfiler::get_instance();

    if(rank > (pow(2, (PARTITION_OFFSET_BYTES * 8))))
        throw EInodeIOException("Invalid rank for InodeNumberDistributor");

    // try to find persistent information about inode number distribution if sal and partition are given
    if(sal && this->partition)
    {
        PartitionManager *pm = sal->get_partition_manager();
        InodeNumberConfig config;
        snprintf(object_name, MAX_NAME_LEN, "%s%d", CONFIG_FILE_PREFIX, this->rank);
        this->storage_abstraction_layer = sal;

        if(sal->get_storage_type() == partition_based_storage)
        {
            // Using partition based storage
            std::list<Partition *> *partitions = pm->get_partitions();

            // try to find larges used inode number on each partition
            for(std::list<Partition*>::iterator it = partitions->begin(); it != partitions->end(); it++)
            {
                if((*it)->get_root_inode() != 0 && (*it)->get_state() != inactive)
                {
                    if(sal->get_object_size((*it)->get_root_inode(), object_name) > 0)
                    {
                        this->storage_abstraction_layer->read_object(this->partition, object_name, 0, sizeof(InodeNumberConfig), &config);
                        if(config.allocated_numbers > this->last_number)
                        {
                            this->last_number = config.allocated_numbers;
                            this->last_written_number = config.allocated_numbers;
                        }
                    }
                }
            }
        }
        else if(sal->get_storage_type() == file_based_storage)
        {
            // Using simple storage
            if(sal->get_object_size(this->partition, object_name) > 0)
            {
                this->storage_abstraction_layer->read_object(this->partition, object_name, 0, sizeof(InodeNumberConfig), &config);
                this->last_number = config.allocated_numbers;
                this->last_written_number = config.allocated_numbers;
            }
        }
    }
    else
    {
        this->storage_abstraction_layer = NULL;
    }

    if(!this->last_number)
    {
        // calculate default inode number rage, if no persistent information found
        this->last_number = rank;
        this->last_number *= pow(2, ((sizeof(InodeNumber) - PARTITION_OFFSET_BYTES) * 8));
        this->last_written_number = last_number;
        this->limit = rank + 1;
        this->limit *= pow(2, ((sizeof(InodeNumber) - PARTITION_OFFSET_BYTES) * 8));
        this->limit--;
    }
}

/** @brief Get next free inode number
 *
 *  @retval Inode number
 *  @throws EInodeIOException if MDS is out of inode numbers
 */
InodeNumber InodeNumberDistributor::get_free_inode_number()
{
    /* We are maintaining two counters for used inode numbers, one is stored in main memory
     * and one is stored on disk. The counter on disk is increased by WRITE_INTERVAL, after
     * that the range between the disk counter and the memory counter can be used. The disk
     * counter is maintained to avoid a disk access on each allocation */
	profiler->function_start();
    profiler->function_sleep();
	pthread_mutex_lock(&(this->lock));
    profiler->function_wakeup();
    InodeNumber result;

    do
    {
		if(this->last_written_number > this->last_number || !this->storage_abstraction_layer)
		{
			// Generate number from memory counter if disk interval is not used completely
			result = ++(this->last_number);
		}
		else if(this->last_written_number <= this->last_number)
		{
			// increase memory interval by increasing disk counter
			if(this->last_written_number + WRITE_INTERVAL < this->limit)
				this->last_written_number += WRITE_INTERVAL;
			else if(this->last_written_number == this->limit)
			{
				pthread_mutex_unlock(&(this->lock));
				profiler->function_end();
				throw EInodeIOException("MDS is out of inode numbers");
			}
			else
				this->last_written_number = this->limit;

			this->write_config();
			result = ++(this->last_number);
		}
    }
    while (result == FS_ROOT_INODE_NUMBER);

    pthread_mutex_unlock(&(this->lock));
	profiler->function_end();
    return result;
}

void InodeNumberDistributor::write_config()
{
    profiler->function_start();
    if(this->storage_abstraction_layer && this->partition)
    {
        InodeNumberConfig config;
        char object_name[MAX_NAME_LEN];

        snprintf(object_name, MAX_NAME_LEN, "%s%d", CONFIG_FILE_PREFIX, this->rank);
        config.rank = this->rank;
        config.allocated_numbers = this->last_written_number;

        try{
        	this->storage_abstraction_layer->write_object(this->partition, object_name, 0, sizeof(InodeNumberConfig), &config);
        }
        catch(StorageException)
        {}
    }
    profiler->function_end();
}
