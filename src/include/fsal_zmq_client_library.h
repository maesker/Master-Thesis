#ifndef FSAL_ZMQ_CLIENT_LIBRARY_H
#define FSAL_ZMQ_CLIENT_LIBRARY_H

/** 
 @file fsal_zmq_client_library.h
 
 * 
 */


#include <stdio.h>
#include <sys/types.h>
#include "zmq.h"
#include <pthread.h>
#include "message_types.h"

/*/
const size_t size_fsobject_name;
const size_t size_fsal_inode_number_hierarchy_request;
const size_t size_fsal_parent_inode_number_lookup_request; 
const size_t size_fsal_lookup_inode_number_by_name_request; 
const size_t size_fsal_einode_request;
const size_t size_fsal_readdir_request; 
const size_t size_fsal_create_einode_request; 
const size_t size_fsal_delete_inode_request; 
const size_t size_fsal_update_attributes_request;
const size_t size_fsal_move_einode_request; 
const size_t size_fsal_populate_prefix_permission; 
const size_t size_fsal_update_prefix_permission;
const size_t size_mds_update_prefix_permission; 
const size_t size_mds_populate_prefix_permission; 
**/


#ifdef _WITH_PC2MDS_TEST_ENVIRONMENT
#define USE_TEST_MD_LAYER 1
#else
#define USE_TEST_MD_LAYER 0
#endif


void registermds(char *address);
void setup_socket_array();
void teardown_socket();
void setup_socket();
void* get_socket();

int fsal_lookup_inode(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_parent_inode_num,
    FsObjectName *p_name,
    InodeNumber *p_inode_num);

int fsal_read_einode(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_inode_num,
    struct EInode *p_einode );

int fsal_move_einode(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_new_parent_inode_number,
    InodeNumber *p_old_parent_inode_number,
    FsObjectName *p_old_name,
    FsObjectName *p_new_name);

int fsal_read_dir(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_inode_num,
    ReaddirOffset *p_offset,
    struct ReadDirReturn *p_rdir);

int fsal_create_file_einode(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_parent_inode_number,
    inode_create_attributes_t *p_attrs,
    InodeNumber *p_return_inum);


int fsal_delete_einode(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_inode_number);


int fsal_update_attributes(
    InodeNumber *p_partition_root_inode_number,
    InodeNumber *p_inode_num,
    inode_update_attributes_t *p_attrs,
    int *p_attribute_bitfield);

#endif

