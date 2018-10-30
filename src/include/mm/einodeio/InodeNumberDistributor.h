/*
 * InodeNumberDistributor.h
 *
 *  Created on: 21.06.2011
 *      Author: sergerit
 */

#ifndef INODENUMBERDISTRIBUTOR_H_
#define INODENUMBERDISTRIBUTOR_H_

#include "global_types.h"
#include "mm/einodeio/EInodeIOException.h"
#include "mm/storage/StorageAbstractionLayer.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

#include <pthread.h>

using namespace std;

#define PARTITION_OFFSET_BYTES  2
#define WRITE_INTERVAL          1024
#define CONFIG_FILE_PREFIX      "inode_allocation_"

typedef struct
{
    int rank;
    InodeNumber allocated_numbers;
} InodeNumberConfig;

/** @brief Generates free inode numbers
 *  
 *  For generating inode numbers, the first two bytes of the whole space
 *  of inode numbers are used to identify a partition of the inode numbers
 *  space. A MDS which has allocated a partition, may use the other 6 bytes
 *  for creating unique inode numbers.
 */
class InodeNumberDistributor
{
public:
    InodeNumberDistributor (int rank, StorageAbstractionLayer *sal, InodeNumber partition);
    virtual ~InodeNumberDistributor () {};
    InodeNumber get_free_inode_number();

private:
    void write_config();
    int rank;
    InodeNumber last_number;
    InodeNumber last_written_number;
    InodeNumber limit;
    InodeNumber partition;
    StorageAbstractionLayer *storage_abstraction_layer;
    Pc2fsProfiler *profiler;
    pthread_mutex_t lock;
};

#endif /* INODENUMBERDISTRIBUTOR_H_ */
