/*
 * @file EmbeddedInode.h
 *
 *  Created on: 29.04.2011
 *      Author: sergerit
 */

#ifndef EMBEDDEDINODE_H_
#define EMBEDDEDINODE_H_

#include <stdlib.h>
#include "global_types.h"

enum FileType
{
    file,
    directory,
    hardlink
};

/**
 @brief  inode structure holding the metadata of an file system object
 * @param ctime creation time
 * @param atime access time
 * @param mtime last modified time
 * @param uid owners uid
 * @param gid objects group id
 * @param mode objects mode
 * @param has_acl object has an access control list
 * @param inode_number inode number of the file system object
 * @param size size of the file system object
 * @param file_type type of the object (dir, file, link)
 * 
 * @todo currently we only support files and directories. These are handled 
 * within the mode attribute. So file_type is obsulete atm.
 * 
 * @param layout_info representation of the files data layout. 
 * @param link_count link counter. 
 */
typedef struct
{
    time_t ctime;
    time_t atime;
    time_t mtime;

    int uid;
    int gid;
    mode_t mode;

    int has_acl;

    InodeNumber inode_number;
    size_t size;
    enum FileType file_type;
    char layout_info[LAYOUT_INFO_LEN];
    int link_count;
} mds_inode_t;


/** 
 @brief EInode structure definition
 * @param name name of the file, currently limited by MAX_NAME_LEN
 * \param inode inode structure object
 */
struct EInode
{
    char name[MAX_NAME_LEN];
    mds_inode_t inode;
};

#endif /* EMBEDDEDINODE_H_ */
