#ifndef MDS_JOURNAL_INTERFACE_LIB_H_
#define MDS_JOURNAL_INTERFACE_LIB_H_


/** 
 * @brief Metadata Journal Interface library
 * @file mds_journal_interface_lib.h
 * @author Markus Maesker, maesker@gmx.net
 * @date 15.07.11
 * */


#include "pc2fsProfiler/Pc2fsProfiler.h"
#include "exceptions/MDSException.h"
#include "mm/journal/Journal.h"

#include "global_types.h"
#include "message_types.h"
#include "ReadDirReturn.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <zmq.h>


/** 
 @brief Object type the MetadataServer interacts with. 
 * 
 * In our current 
 * implementation the MetadataServer core performes EInode operations on 
 * the Journal and all other modules are transparent to the MetadataServer 
 * layer.
 */
typedef Journal Object;

/** 
 * @brief Interface to the move einode storage backend functions.
 * @param[in] p_inode_number pointer to the inode number of the
 * corresponding fs object.
 * @param[out] p_inode_number_list pointer to the inode object to write the
 * resulting inode data to.
 * @return integer return code
 * */
typedef int (Object::*MoveEinodeCbIf)(
    InodeNumber* new_parent_id,
    FsObjectName *new_name,
    InodeNumber* old_parent_id,
    FsObjectName* old_name
);

int generic_handle_move_einode(
    struct FsalMoveEinodeRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    MoveEinodeCbIf p_cb_func,
    Object *obj
);


/** 
 * @brief Interface to the parent inode hierarchy storage backend functions.
 * @param[in] p_inode_number pointer to the inode number of the
 * corresponding fs object.
 * @param[out] p_inode_number_list pointer to the inode object to write the
 * resulting inode data to.
 * @return integer return code
 * */
typedef int (Object::*ParentInodeHierarchyCbIf)(
    InodeNumber *p_inode_number,
    inode_number_list_t *p_inode_number_result_list
);

int generic_handle_parent_inode_hierarchy(
    struct FsalInodeNumberHierarchyRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    ParentInodeHierarchyCbIf p_cb_func,
    Object *obj
);

/** @brief Interface to the parent inode numbers lookup storage backend
 * functions.
 * @param[in] p_inode_number pointer to the inode number of the
 * corresponding fs object.
 * @param[out] p_inode_number_list pointer to the inode object to write the
 * resulting inode data to.
 * @return integer return code
 * */
typedef int (Object::*ParentInodeNumbersLookupRequestCbIf)(
    inode_number_list_t *p_inode_number_request_list,
    inode_number_list_t *p_inode_number_result_list
);

int generic_handle_parent_inode_numbers_lookup(
    struct FsalParentInodeNumberLookupRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    ParentInodeNumbersLookupRequestCbIf p_cb_func,
    Object *obj
);


/** @brief Interface to the inode request storage backend functions.
 * @param[in] p_inode_number pointer to the inode number of the
 * corresponding fs object.
 * @param[out] p_inode pointer to the inode object to write the
 * resulting inode data to.
 * @return integer return code
 * */

typedef int (Object::*InodeRequestCbIf)(
    InodeNumber *p_inode_number,
    mds_inode_t *p_inode
);

int generic_handle_inode_request(
    struct FsalEInodeRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    InodeRequestCbIf p_cb_func,
    Object *obj
);


/** @brief Interface to the einode request storage backend functions.
 * @param[in] p_inode_number pointer to the inode number of the
 * corresponding fs object.
 * @param[in] p_einode pointer to the EInode structure to write the
 * data to.
 * @return integer return code
 * */

typedef int (Object::*EInodeRequestCbIf)(
    InodeNumber *p_inode_number,
    struct EInode  *p_einode
);

int generic_handle_einode_request(
    struct FsalEInodeRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    EInodeRequestCbIf p_cb_func,
    Object *obj
);


/** @brief Interface to the create einode storage backend functions.
 * @param[in] p_parent_inode_number pointer to the inode number of the
 * directory the fs object is supposed to be written to.
 * @param[in] p_einode pointer to the einode structure that is supposed
 * to be written to storage.
 * @return integer return code
 * */
typedef int (Object::*CreateFileEInodeRequestCbIf)(
    InodeNumber *p_parent_inode_number,
    struct EInode *p_einode
);

int generic_handle_create_file_einode_request(
    struct FsalCreateEInodeRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    InodeNumber *p_inode_number,
    filelayout *p_fl,
    CreateFileEInodeRequestCbIf p_cb_func,
    Object *obj
);


/** @brief Interface to the update attributes storage backend functions.
 * @param[in] p_inode_number pointer to the inode number of the
 * corresponding fs object.
 * @param[in] p_attrs pointer to the attributes structure that contains
 * the new attribute values.
 * @param[in] attribute_bitfield a bitfield to describe the values of
 * the attrs structure that contain new data and musst be updated.
 * @return integer return code
 * */
typedef int (Object::*UpdateAttributesRequestCbIf)(
    InodeNumber *p_inode_number,
    inode_update_attributes_t *p_attrs,
    int *p_attribute_bitfield
);

int generic_handle_update_attributes_request (
    struct FsalUpdateAttributesRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    UpdateAttributesRequestCbIf p_cb_func,
    Object *obj
);


/** @brief Interface to the delete einode storage backend functions.
 * @param[in] p_inode_number pointer to the inode number of the
 * corresponding fs object.
 * @return integer return code
 * */
typedef int (Object::*DeleteEInodeRequestCbIf)(
    InodeNumber *p_inode_number
);

int generic_handle_delete_einode_request (
    struct FsalDeleteInodeRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    DeleteEInodeRequestCbIf p_cb_func,
    Object *obj
);


/** @brief Interface to the read dir storage backend functions.
 * @param[in] p_inode_number pointer to the inode number of the
 * corresponding fs object.
 * @param[in] p_offset pointer to the ReaddirOffset type to define the
 * offset within the storage that defines the directory entries to return.
 * @param[out] p_rdir pointer to the ReadDirReturn structure that
 * the resulting data are written to.
 * @return integer return code
 * */
typedef int (Object::*ReadDirRequestCbIf)(
    InodeNumber *p_inode_number,
    ReaddirOffset *p_offset,
    struct ReadDirReturn *p_rdir
);

int generic_handle_readdir_request (
    struct FsalReaddirRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    ReadDirRequestCbIf p_cb_func,
    Object *obj
);


/** @brief Interface to the inode lookup storage backend functions.
 * @param[in] p_parent_inode_number pointer to the directories inode
 * number of the corresponding fs object directory.
 * @param[in] p_name pointer to the name of the fs object.
 * @param[out] p_result pointer to the inode number type to write the
 * resulting inode number to
 * @return integer return code
 * */
typedef int (Object::*LookupInodeNumberByNameCbIf)(
    InodeNumber *p_parent_inode_number,
    FsObjectName *p_name,
    InodeNumber *p_result_inode
);

int generic_handle_lookup_inode_number_request(
    struct FsalLookupInodeNumberByNameRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    LookupInodeNumberByNameCbIf p_cb_func,
    Object *obj
);


/** look at the c file for comments
 * */
int zmq_response_to_fsal_structure(void *p_fsaldata, size_t fsalsize,
                                   zmq_msg_t *p_response );

int rebuild_and_fill_zmq_msg(void *p_data ,size_t size,zmq_msg_t *p_msg);

/** @brief Enumerated error codes to work with
 * */
enum FsalErrorCodes
{
    fsal_global_ok,

    zmq_send_error,
    zmg_recv_error,
    zmq_msg_init_error,
    zmq_msg_init_size_error,

    generic_handle_readdir_request_error_readdirreturn_to_large,
    fsal_lookup_inode_requested_name_exceeds_length_limits,
    fsal_parent_inode_number_lookup_request,
    fsal_parent_inode_hierarchy_error,
    fsal_lookup_inode_by_name_callback_error,
    fsal_read_dir_request_error,
    fsal_create_file_einode_error,
    fsal_create_file_einode_attrs_exceed_fsalattrs,
    fsal_delete_einode_error,
    fsal_update_attributes_attrs_exceed_fsalattrs,
    fsal_update_attributes_error,
    fsal_request_einode_size_mismatch,
    fsal_request_einode_error,
    fsal_unknown_request,
    fsal_move_einode_error,
    
    fsal_zmq_c_zmq_send_and_receive_error_,
    zmq_response_to_fsal_structure_error_message_size_mismatch
};

int fsal_error_mapper(int rc, int _error);

#endif
