#include "fsal_zmq_mds_library.h"  
#include "GaneshaProfiler.h"
#include <stdio.h>


/** @brief Calls the parent inode number lookup storage backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing 
 * the incomming request.
 * @param[out] p_fsal_resp_ptr FSAL structure pointer that the result 
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 * 
 * @details The backend pointed to by p_cb_func musst provide the 
 * "ParentInodeNumbersLookupRequestCbIf" as definied in 
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_parent_inode_numbers_lookup(
  struct FsalParentInodeNumberLookupRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  ParentInodeNumbersLookupRequestCbIf p_cb_func)
{
  int rc=0;
  struct FsalParentInodeNumberLookupResponse *fsal_resp = 
         (struct FsalParentInodeNumberLookupResponse *)p_fsal_resp_ptr;
  fsal_resp->type=parent_inode_number_lookup_response;
  inode_number_list_t inode_number_result_list;
  rc = (*p_cb_func)(&p_fsalmsg->inode_number_list,
                    &inode_number_result_list);
  if (!rc)
  {
    memcpy( &fsal_resp->inode_number_list, &inode_number_result_list, 
            sizeof(inode_number_list_t));
    fsal_resp->error = 0;  
  }
  else
  {
    rc=fsal_parent_inode_number_lookup_request;
    fsal_resp->error = 1;  
  }
  return rc;
}


/** @brief Calls the inode number lookup mechanism of the provided 
 * backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing 
 * the incomming request.
 * @param[out] p_fsal_resp_ptr FSAL structure pointer that the result 
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 * 
 * @details The backend pointed to by p_cb_func musst provide the 
 * "LookupInodeNumberByNameCbIf" as definied in 
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_lookup_inode_number_request(
  struct FsalLookupInodeNumberByNameRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  LookupInodeNumberByNameCbIf p_cb_func  )
{ 
  int rc= fsal_global_ok;
  InodeNumber inum;
  rc = (*p_cb_func)(&p_fsalmsg->parent_inode_number ,
                    &p_fsalmsg->name,
                    &inum);
  if (!rc)
  {
    struct FsalLookupInodeNumberByNameResponse *fsal_resp = 
        (struct FsalLookupInodeNumberByNameResponse *)p_fsal_resp_ptr;
    fsal_resp->type=lookup_inode_number_response;
    fsal_resp->inode_number = inum;
    fsal_resp->error = 0;  
  }
  else
  {
    rc=fsal_lookup_inode_by_name_callback_error;
  }
  return rc;
}

/** @brief Calls the einode request mechanism of the provided backend. 
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing 
 * the incomming request.
 * @param[out] p_fsal_resp_ptr FSAL structure pointer that the result 
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 * 
 * @details The backend pointed to by p_cb_func musst provide the 
 * "EInodeRequestCbIf" as definied in 
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */

int generic_handle_einode_request(
  struct FsalEInodeRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  EInodeRequestCbIf p_cb_func)
{ 
  int rc=fsal_global_ok;
  struct EInode einode;
  struct FsalFileEInodeResponse *p_fsal_resp = 
         (struct FsalFileEInodeResponse *) p_fsal_resp_ptr;
  rc = (*p_cb_func)(&p_fsalmsg->inode_number ,
                    &einode);
  if (!rc)
  {    
    p_fsal_resp->type=file_einode_response;
    if (sizeof(p_fsal_resp->einode)>=sizeof(einode))
    {
      memcpy(&p_fsal_resp->einode,&einode,sizeof(einode));
      p_fsal_resp->error = 0;  
    }
    else
    {
      p_fsal_resp->error = fsal_request_einode_size_mismatch;  
      rc=fsal_request_einode_size_mismatch;
    }
  }
  return rc;
}

/** @brief Calls the create einode mechanism of the provided backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing 
 * the incomming request.
 * @param[out] p_fsal_resp_ptr FSAL structure pointer that the result 
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 * 
 * @details The backend pointed to by p_cb_func musst provide the 
 * "CreateFileEInodeRequestCbIf" as definied in 
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_create_file_einode_request(
  struct FsalCreateEInodeRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  InodeNumber *p_inode_number,
  CreateFileEInodeRequestCbIf p_cb_func  )
{ 
  int rc=fsal_global_ok;
  struct EInode einode;
  einode.inode.ctime = time(NULL);  
  einode.inode.atime = time(NULL);
  einode.inode.mtime = time(NULL);
  einode.inode.size = p_fsalmsg->attrs.size;
  einode.inode.mode = p_fsalmsg->attrs.mode;
  einode.inode.gid = p_fsalmsg->attrs.gid;
  einode.inode.uid = p_fsalmsg->attrs.uid;
  einode.inode.inode_number = *p_inode_number;
  //einode.inode.file_type = file;
  einode.inode.link_count = 1;
  memcpy(&einode.name, &p_fsalmsg->attrs.name, MAX_NAME_LEN);
  rc = (*p_cb_func)(  &p_fsalmsg->parent_inode_number ,
                      &einode);
  if (!rc)
  {
    struct FsalCreateInodeResponse *fsal_resp = 
        (struct FsalCreateInodeResponse *) p_fsal_resp_ptr;
    fsal_resp->type=create_inode_response;
    fsal_resp->inode_number = *p_inode_number;
    fsal_resp->error = 0;  
  }
  return rc;
}


/** @brief Calls the delete einode mechanism of the provided backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing 
 * the incomming request.
 * @param[out] p_fsal_resp_ptr FSAL structure pointer that the result 
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 * 
 * @details The backend pointed to by p_cb_func musst provide the 
 * "DeleteEInodeRequestCbIf" as definied in 
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_delete_einode_request(
  struct FsalDeleteInodeRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  DeleteEInodeRequestCbIf p_cb_func)
{   
  int rc=fsal_global_ok;
  rc = (*p_cb_func)( &p_fsalmsg->inode_number);
  if(!rc)
  {
    struct FsalDeleteInodeResponse *fsal_resp = 
        (struct FsalDeleteInodeResponse *) p_fsal_resp_ptr;
    fsal_resp->type=delete_inode_response;
    fsal_resp->inode_number = p_fsalmsg->inode_number;
    fsal_resp->error = 0; 
  }
  return rc;
}

/** @brief Calls the read dir mechanism of the provided backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing 
 * the incomming request.
 * @param[out] p_fsal_resp_ptr FSAL structure pointer that the result 
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 * 
 * @details The backend pointed to by p_cb_func musst provide the 
 * "ReadDirRequestCbIf" as definied in 
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_readdir_request(
      struct FsalReaddirRequest *p_fsalmsg,
      void *p_fsal_resp_ptr, 
      ReadDirRequestCbIf p_cb_func)
{
  int rc=fsal_global_ok;
  struct FsalReaddirResponse *fsal_resp = 
        (struct FsalReaddirResponse *) p_fsal_resp_ptr;
  fsal_resp->type=read_dir_response;   
  struct ReadDirReturn rdir;
  rc = (*p_cb_func) ( &p_fsalmsg->inode_number,
                      &p_fsalmsg->offset,
                      &rdir);
  if (!rc)
  {
    if (sizeof(struct FsalReaddirResponse)>=sizeof(rdir))
    {
      memcpy(&fsal_resp->directory_content, &rdir, sizeof(rdir));
    }
    else
    {
      rc=generic_handle_readdir_request_error_readdirreturn_to_large;
    }
  }
  fsal_resp->error = rc;  
  return rc;
}


/** @brief Calls the update attributes mechanism of the provided backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing 
 * the incomming request.
 * @param[out] p_fsal_resp_ptr FSAL structure pointer that the result 
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 * 
 * @details The backend pointed to by p_cb_func musst provide the 
 * "UpdateAttributesRequestCbIf" as definied in 
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_update_attributes_request (
  struct FsalUpdateAttributesRequest *p_fsalmsg,
  void *p_fsal_resp_ptr,  
  UpdateAttributesRequestCbIf p_cb_func)
{ 
  int rc;
  struct FsalUpdateAttributesResponse *p_fsal_resp = 
      (struct FsalUpdateAttributesResponse *) p_fsal_resp_ptr;
  p_fsal_resp->type=update_attributes_response;
  if(!rc)
  { 
    rc = (*p_cb_func)(&p_fsalmsg->inode_number ,
                      &p_fsalmsg->attrs,
                      &p_fsalmsg->attribute_bitfield);
    if (!rc)
    {
      p_fsal_resp->inode_number = p_fsalmsg->inode_number;
    }
  }
  else
  {
    p_fsal_resp->inode_number = 0;
  }
  p_fsal_resp->error = rc;  
  return rc;
}

/** @brief Initializes the zmq message with the correct size and copys
 * the data.
 * @param[in] p_data Pointer the the address of the data to be copied
 *  into the provided zmq message.
 * @param[in] size size of the p_data space. The amount of data that 
 * will be copied into the zmq message.
 * @param[out] p_msg Pointer to the zmq message that is initialized and
 * filled with data.
 * @return 0 on success. zmq_msg_init_size_error if zmg initialization 
 * fails.
 * */
int rebuild_and_fill_zmq_msg(void *p_data ,size_t size,zmq_msg_t *p_msg)  
{
  int rc = zmq_msg_init_size(p_msg, size);
  if (!rc)
  {
    memcpy(zmq_msg_data(p_msg),p_data,size);
  }
  else
  {
    rc = zmq_msg_init_size_error;
  }
  return rc;
}

/** @brief Writes the content of the zmq message into the provided FSAL 
 * structure.
 * @param[out] p_fsaldata Pointer to the fsal structure to copy the 
 * data to.
 * @param[in] fsalsize Size of the fsal structure.
 * @param[in] p_response ZMQ message pointer that contains the FSAL
 * structure.
 * @return 0 on success, 
 * zmq_response_to_fsal_structure_error_message_size_mismatch if zmq 
 * message arger than fsal structure.
 * */
int zmq_response_to_fsal_structure(
  void *p_fsaldata, size_t fsalsize, zmq_msg_t *p_response)  
{
  int rc=0;
  if (fsalsize >= zmq_msg_size(p_response))
  {
    memcpy(p_fsaldata, zmq_msg_data(p_response),zmq_msg_size(p_response));
  }
  else
  {
    rc=zmq_response_to_fsal_structure_error_message_size_mismatch;
  }     
  return rc;
}

/** @brief Maps error to rc if rc equals 0. 
 *  @param[in] rc return code. If rc equals 0 return rc else set error
 * to rc.
 * @param[in] error Error code to set when rc!=0
 * @return returncode. Either 0 or error.
 * */
int fsal_error_mapper(int rc, int error)
{
  if (rc!=0)
  {
    rc = error;
  }
  return rc;
}
