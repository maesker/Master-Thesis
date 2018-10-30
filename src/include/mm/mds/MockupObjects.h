#ifndef MOCKUPOBJECTS_H_
#define MOCKUPOBJECTS_H_
/**
 * @author  Markus Maesker, maesker@gmx.net
 * @date 01.06.11
 * @file MockupObjects.h
 *
 * @brief Create and return Mock objects
*/

#include <time.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#include "mlt/mlt.h"

}
#endif

#include "message_types.h"
#include "EmbeddedInode.h"
#include "ReadDirReturn.h"
#include "global_types.h"



FsalDeleteInodeRequest* get_mock_FsalDeleteInodeRequestStructure(
    InodeNumber partition_root,
    InodeNumber inode_num);

FsalEInodeRequest* get_mock_EInodeRequestStructure(
    InodeNumber partition_root,
    InodeNumber inode_num);
    
FsalEInodeRequest* get_mock_FsalEInodeRequestStructure(
    InodeNumber partition_root,
    InodeNumber inode_num);
    
FsalLookupInodeNumberByNameRequest* get_mock_FsalLookupInodeNumberByNameRequestStructure(
    InodeNumber partition_root,
    InodeNumber parent_inode_num,
    FsObjectName *p_name);

FsalReaddirRequest* get_mock_FsalReaddirRequestStructure(
    InodeNumber partition_root,
    InodeNumber inode_num,
    ReaddirOffset offset );

FsalUpdateAttributesRequest* get_mock_FSALUpdateAttrRequest(
    InodeNumber partition_root_inode_number,
    InodeNumber inode_number,
    inode_update_attributes_t attrs,
    int attribute_bitfield);

FsalCreateEInodeRequest* get_mock_FsalCreateFileInodeRequest();

FsalCreateEInodeRequest* get_mock_DIR_FsalCreateFileInodeRequest();

FsalInodeNumberHierarchyRequest* get_mock_FsalParentHierarchyRequest();


char generate_random_fsname_char();
int generate_fsobjectname(FsObjectName *ptr, int len);
InodeNumber generate_random_inodenumber();
time_t generate_time();
int generate_access_rights();
size_t generate_size_tx();
mode_t generate_mode_t();

int generate_gid();
int generate_uid();

int get_mock_inode(
    InodeNumber *inumber,
    mds_inode_t *node
);


int get_mock_readdirreturn(
    InodeNumber *inumber,
    ReaddirOffset *off,
    ReadDirReturn *rdir
);

int get_mock_einode(
    InodeNumber *parent_inode_number,
    struct EInode *einode
);

int get_mock_inode_number_lookup(
    InodeNumber *parent_inode_number,
    FsObjectName *name,
    InodeNumber *inumber
);

int blank_create_file_einode_handler(
    InodeNumber *parent_inode_number,
    struct EInode *einode
);

int blank_delete_einode_handler(
    InodeNumber *inode_number
);

int blank_update_attributes_handler(
    InodeNumber *inode_number,
    inode_update_attributes_t *attrs,
    int *attribute_bitfield
);

struct mlt * create_dummy_mlt();

void generate_export_block(struct export_block *p);
#endif
