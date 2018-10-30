/**
 * @file ReadDirReturn.h
 *
 *  Created on: 1.06.2011
 * @author sergerit
 * @brief Return value structure for readdir operations
 */

#ifndef READDIRRETURN_H_
#define READDIRRETURN_H_

#include "global_types.h"
#include "EmbeddedInode.h"

/** 
 @brief Structure defining the returned data structure of an readdir request
 * to the FSAL layer.
 * 
 * A full readdir is a collection of many partial readdir requests. Each request 
 * contains a offset to start from and a number of objects to read. In our 
 * system, due to the ZMQ library we got best performance for a total number 
 * of einode per message FSAL_READDIR_EINODES_PER_MSG = 1.  * 
 * 
 * @param nodes collection of EInodes, in our current implementation this 
 * structure only contains 1 EInode. Can be configured over the 
 * FSAL_READDIR_EINODES_PER_MSG parameter
 * 
 * @param dir_size total size of the directory
 * @param nodes_len number of returned einodes
 * 
 * @todo further investigation of the readdir workflow and the effect of the ZMQ
 * library could improve this operation.
 */
struct ReadDirReturn
{
    // insert the number into global config file
    struct EInode nodes[FSAL_READDIR_EINODES_PER_MSG];
    uint64_t dir_size;
    uint64_t nodes_len;
};

#endif /* READDIRRETURN_H_ */
