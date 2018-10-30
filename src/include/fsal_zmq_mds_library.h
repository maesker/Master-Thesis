#ifndef FSAL_ZMQ_MDS_LIBRARY_H_
#define FSAL_ZMQ_MDS_LIBRARY_H_

#include "global_types.h"
#include "message_types.h"
#include "EmbeddedInode.h"
#include "ReadDirReturn.h"
#include <zmq.h>
#include <string.h>
#include <time.h>


/** @brief Interface to the parent inode numbers lookup storage backend 
 * functions.
 * @param[in] p_inode_number pointer to the inode number of the 
 * corresponding fs object.
 * @param[out] p_inode_number_list pointer to the inode object to write the 
 * resulting inode data to.
 * @return integer return code
 * */
typedef int (*ParentInodeNumbersLookupRequestCbIf)(
    inode_number_list_t *p_inode_number_request_list,
    inode_number_list_t *p_inode_number_result_list
);

int generic_handle_parent_inode_numbers_lookup(
  struct FsalParentInodeNumberLookupRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  ParentInodeNumbersLookupRequestCbIf p_cb_func
);


/** @brief Interface to the inode request storage backend functions.
 * @param[in] p_inode_number pointer to the inode number of the 
 * corresponding fs object.
 * @param[out] p_inode pointer to the inode object to write the 
 * resulting inode data to.
 * @return integer return code
 * */

typedef int (*InodeRequestCbIf)(
    InodeNumber *p_inode_number,
    mds_inode_t *p_inode        
);

int generic_handle_inode_request(
  struct FsalEInodeRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  InodeRequestCbIf p_cb_func
);


/** @brief Interface to the einode request storage backend functions.
 * @param[in] p_inode_number pointer to the inode number of the 
 * corresponding fs object.
 * @param[in] p_einode pointer to the EInode structure to write the 
 * data to.
 * @return integer return code
 * */

typedef int (*EInodeRequestCbIf)(
    InodeNumber *p_inode_number,
    struct EInode  *p_einode   
);

int generic_handle_einode_request(
  struct FsalEInodeRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  EInodeRequestCbIf p_cb_func
);


/** @brief Interface to the create einode storage backend functions.
 * @param[in] p_parent_inode_number pointer to the inode number of the 
 * directory the fs object is supposed to be written to.
 * @param[in] p_einode pointer to the einode structure that is supposed
 * to be written to storage.
 * @return integer return code
 * */
typedef int (*CreateFileEInodeRequestCbIf)(
  InodeNumber *p_parent_inode_number,
  struct EInode *p_einode
);

int generic_handle_create_file_einode_request(
  struct FsalCreateEInodeRequest *p_fsalmsg, 
  void *p_fsal_resp_ptr,  
  InodeNumber *p_inode_number,
  CreateFileEInodeRequestCbIf p_cb_func
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
typedef int (*UpdateAttributesRequestCbIf)(
  InodeNumber *p_inode_number,
  inode_update_attributes_t *p_attrs,
  int *p_attribute_bitfield
);

int generic_handle_update_attributes_request (
  struct FsalUpdateAttributesRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  UpdateAttributesRequestCbIf p_cb_func
);


/** @brief Interface to the delete einode storage backend functions.
 * @param[in] p_inode_number pointer to the inode number of the 
 * corresponding fs object.
 * @return integer return code
 * */
typedef int (*DeleteEInodeRequestCbIf)(
  InodeNumber *p_inode_number
);

int generic_handle_delete_einode_request (
  struct FsalDeleteInodeRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  DeleteEInodeRequestCbIf p_cb_func
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
typedef int (*ReadDirRequestCbIf)(
  InodeNumber *p_inode_number,
  ReaddirOffset *p_offset,
  struct ReadDirReturn *p_rdir
);

int generic_handle_readdir_request (
  struct FsalReaddirRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  ReadDirRequestCbIf p_cb_func
);


/** @brief Interface to the inode lookup storage backend functions.
 * @param[in] p_parent_inode_number pointer to the directories inode 
 * number of the corresponding fs object directory.
 * @param[in] p_name pointer to the name of the fs object.
 * @param[out] p_result pointer to the inode number type to write the 
 * resulting inode number to
 * @return integer return code
 * */
typedef int (*LookupInodeNumberByNameCbIf)(
   InodeNumber *p_parent_inode_number,
   FsObjectName *p_name,
   InodeNumber *p_result_inode
);

int generic_handle_lookup_inode_number_request(
  struct FsalLookupInodeNumberByNameRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  LookupInodeNumberByNameCbIf p_cb_func
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
fsal_lookup_inode_by_name_callback_error,
fsal_parent_inode_number_lookup_request,

fsal_create_file_einode_attrs_exceed_fsalattrs,
fsal_update_attributes_attrs_exceed_fsalattrs,
fsal_request_einode_size_mismatch,
fsal_unknown_request,

fsal_zmq_c_zmq_send_and_receive_error_,
zmq_response_to_fsal_structure_error_message_size_mismatch
};

int fsal_error_mapper(int rc, int _error);

#endif
