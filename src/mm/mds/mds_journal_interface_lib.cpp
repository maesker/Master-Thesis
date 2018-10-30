
/** 
 * @file mds_journal_interface_lib.cpp
 * @brief Metadata Journal Interface library implementation
 * @author Markus Maesker, maesker@gmx.net
 * @date 25.07.11
 * 
 * These functions are used to perform the requested operation on the 
 * EInode layers (Journal, Cache, Storage, EInodeLookup...). 
 * 
 * The MDS handle_* function collected all necessary data and then calls the 
 * associated generic_handle_* -function. This function uses the request 
 * message, the data provided by the MDS and calls the appropriate callback
 * function of the Journal instance. 
 * 
 * The result of the journal operation is used to write the response message
 * to the provided msg pointer. 
 * */

#include "mm/mds/mds_journal_interface_lib.h"
#include <stdio.h>


/** 
 * @brief Calls the parent inode hierarchy backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing
 * the incomming request.
 * @param[out] p_fsal_resp_ref FSAL structure pointer that the result
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 *
 * @details The backend pointed to by p_cb_func musst provide the
 * "ParentInodeHierarchyCbIf" as definied in
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_parent_inode_hierarchy(
    struct FsalInodeNumberHierarchyRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    ParentInodeHierarchyCbIf p_cb_func,
    Object *obj)
{
    int rc = fsal_parent_inode_hierarchy_error;
    try
    {
        Pc2fsProfiler::get_instance()->function_start();
        // cast pointer to response structure and initialize type
        struct FsalInodeNumberHierarchyResponse *p_fsal_resp =
            (struct FsalInodeNumberHierarchyResponse *)p_fsal_resp_ref;
        p_fsal_resp->error = 1;
        p_fsal_resp->type = parent_inode_hierarchy_response;
        //inode_number_list_t inode_number_result_list;
        // callback function processes the input fsalmsg and write the
        // result to inode_number_result_list. rc=0 means ok
        rc = (obj->*p_cb_func)( &p_fsalmsg->inode_number,
                                &p_fsal_resp->inode_number_list);
                                //&inode_number_result_list);
        if (!rc)
        {
            // operation successful. Write result to fsal_response structure.
            //memcpy( &p_fsal_resp->inode_number_list, &inode_number_result_list,
            //        sizeof(inode_number_list_t));
            p_fsal_resp->error = 0;
        }
        else
        {
            // operation not successful. Write error msg to fsal structure
            rc = fsal_parent_inode_hierarchy_error;
            p_fsal_resp->error = fsal_parent_inode_hierarchy_error;
        }
    }
    catch (StorageException e)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw e;
    }
    catch (...)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw MJIException("Handle parent inode hierarchy exception raised.");
    }
    Pc2fsProfiler::get_instance()->function_end();
    return rc;
}


/** @brief Calls the parent inode number lookup storage backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing
 * the incomming request.
 * @param[out] p_fsal_resp_ref FSAL structure pointer that the result
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 *
 * @details The backend pointed to by p_cb_func musst provide the
 * "ParentInodeNumbersLookupRequestCbIf" as definied in
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_parent_inode_numbers_lookup(
    struct FsalParentInodeNumberLookupRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    ParentInodeNumbersLookupRequestCbIf p_cb_func,
    Object *obj)
{
    int rc = 0;
    try
    {
        
        Pc2fsProfiler::get_instance()->function_start();
        // cast pointer to response structure and initialize type
        struct FsalParentInodeNumberLookupResponse *p_fsal_resp =
            (struct FsalParentInodeNumberLookupResponse *)p_fsal_resp_ref;
        p_fsal_resp->type = parent_inode_number_lookup_response;
        //inode_number_list_t inode_number_result_list;
        // callback function processes the input fsalmsg and write the
        // result to inode_number_result_list. rc=0 means ok
        rc = (obj->*p_cb_func)( &p_fsalmsg->inode_number_list,
                                &p_fsal_resp->inode_number_list);
          //                     &inode_number_result_list);
        if (!rc)
        {
            // operation successful. Write result to fsal_response structure.
        //    memcpy( &p_fsal_resp->inode_number_list, &inode_number_result_list,
          //          sizeof(inode_number_list_t));
            p_fsal_resp->error = 0;
        }
        else
        {
            // operation not successful. Write error msg to fsal structure
            rc = fsal_parent_inode_number_lookup_request;
            p_fsal_resp->error = fsal_parent_inode_number_lookup_request;
        }
    }
    catch (StorageException e)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw e;
    }
    catch (...)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw MJIException("Handle parent inode number exception raised.");
    }
    Pc2fsProfiler::get_instance()->function_end();
    return rc;
}


/** @brief Calls the inode number lookup mechanism of the provided
 * backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing
 * the incomming request.
 * @param[out] p_fsal_resp_ref FSAL structure pointer that the result
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 *
 * @details The backend pointed to by p_cb_func musst provide the
 * "LookupInodeNumberByNameCbIf" as definied in
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_lookup_inode_number_request(
    struct FsalLookupInodeNumberByNameRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    LookupInodeNumberByNameCbIf p_cb_func,
    Object *obj  )
{
    int rc = fsal_global_ok;
    try
    {
        Pc2fsProfiler::get_instance()->function_start();
        // cast pointer to response structure and initialize type
        struct FsalLookupInodeNumberByNameResponse *p_fsal_resp =
            (struct FsalLookupInodeNumberByNameResponse *)p_fsal_resp_ref;
        p_fsal_resp->type = lookup_inode_number_response;

        InodeNumber inum;
        // call callback function. Result is written to inum
        rc = (obj->*p_cb_func)( &p_fsalmsg->parent_inode_number ,
                                &p_fsalmsg->name,
                                &inum);
        if (!rc)
        {
            // operation successful, write data to fsal response
            p_fsal_resp->inode_number = inum;
            p_fsal_resp->error = 0;
        }
        else
        {
            // operation not successful. Write errorcode to fsal response
            rc = fsal_lookup_inode_by_name_callback_error;
            p_fsal_resp->inode_number = INVALID_INODE_ID;
            p_fsal_resp->error = fsal_lookup_inode_by_name_callback_error;
        }
    }
    catch (StorageException e)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw e;
    }
    catch (...)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw MJIException("Handle lookup inode number exception raised.");
    }
    Pc2fsProfiler::get_instance()->function_end();
    return rc;
}

/** @brief Calls the einode request mechanism of the provided backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing
 * the incomming request.
 * @param[out] p_fsal_resp_ref FSAL structure pointer that the result
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 *
 * @details The backend pointed to by p_cb_func musst provide the
 * "EInodeRequestCbIf" as definied in
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */

int generic_handle_einode_request(
    struct FsalEInodeRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    EInodeRequestCbIf p_cb_func,
    Object *obj)
{
    Pc2fsProfiler::get_instance()->function_start();
    int rc=fsal_global_ok;
    try
    {
        // cast pointer to response structure and initialize type
        //struct EInode einode;
        struct FsalFileEInodeResponse *p_fsal_resp =
            (struct FsalFileEInodeResponse *) p_fsal_resp_ref;
        p_fsal_resp->type = file_einode_response;
        // callback function writes result to einode. On success rc=0
        rc = (obj->*p_cb_func)( &p_fsalmsg->inode_number,
                                &p_fsal_resp->einode);
        if (!rc)
        {
            // operation successful. Write data to fsal structure.
            //if (sizeof(p_fsal_resp->einode) >= sizeof(einode))
            //{
              //  memcpy(&p_fsal_resp->einode,&einode,sizeof(einode));
                p_fsal_resp->error = 0;
            //}
            //else
            //{
                // this section should not be reached. Could only be caused by
                // different includes
              //  p_fsal_resp->error = fsal_request_einode_size_mismatch;
                //rc = fsal_request_einode_size_mismatch;
            //}
        }
        else
        {
            // error occured. Write error code to fsal structure
            p_fsal_resp->error = fsal_request_einode_error;
            rc = fsal_request_einode_error;
        }
    }
    catch (StorageException e)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw e;
    }
    catch (...)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw MJIException("Handle einode request exception raised.");
    }
    Pc2fsProfiler::get_instance()->function_end();
    return rc;
}

/** @brief Calls the create einode mechanism of the provided backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing
 * the incomming request.
 * @param[out] p_fsal_resp_ref FSAL structure pointer that the result
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 *
 * @details The backend pointed to by p_cb_func musst provide the
 * "CreateFileEInodeRequestCbIf" as definied in
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_create_file_einode_request(
    struct FsalCreateEInodeRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    InodeNumber *p_inode_number,
    filelayout *p_fl,
    CreateFileEInodeRequestCbIf p_cb_func,
    Object *obj)
{
    Pc2fsProfiler::get_instance()->function_start();
    int rc = fsal_global_ok;
    try
    {
        // cast fsal response pointer and initialize type
        struct FsalCreateInodeResponse *p_fsal_resp =
            (struct FsalCreateInodeResponse *) p_fsal_resp_ref;
        p_fsal_resp->type = create_inode_response;
        // create new einode
        struct EInode einode;
        einode.inode.ctime = time(NULL);;
        einode.inode.atime = einode.inode.ctime;
        einode.inode.mtime = einode.inode.ctime;
        einode.inode.size = p_fsalmsg->attrs.size;
        einode.inode.mode = p_fsalmsg->attrs.mode;
        einode.inode.gid = p_fsalmsg->attrs.gid;
        einode.inode.uid = p_fsalmsg->attrs.uid;
        einode.inode.inode_number = *p_inode_number;
        einode.inode.file_type = file;
        einode.inode.link_count = 1;
        memcpy(&einode.name, &p_fsalmsg->attrs.name, MAX_NAME_LEN);
        memcpy(&einode.inode.layout_info[0],p_fl,sizeof(einode.inode.layout_info));
        // call callback to process new einode. rc = 0 means success
        rc = (obj->*p_cb_func)( &p_fsalmsg->parent_inode_number ,
                                &einode);
        if (!rc)
        {
            // new einode successfuly written
            p_fsal_resp->inode_number = *p_inode_number;
            p_fsal_resp->error = 0;
        }
        else
        {
            p_fsal_resp->error = fsal_create_file_einode_error;
        }
    }
    catch (StorageException e)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw e;
    }
    catch (...)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw MJIException("Handle create file einode exception raised.");
    }
    Pc2fsProfiler::get_instance()->function_end();
    return rc;
}


/** @brief Calls the delete einode mechanism of the provided backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing
 * the incomming request.
 * @param[out] p_fsal_resp_ref FSAL structure pointer that the result
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 *
 * @details The backend pointed to by p_cb_func musst provide the
 * "DeleteEInodeRequestCbIf" as definied in
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_delete_einode_request(
    struct FsalDeleteInodeRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    DeleteEInodeRequestCbIf p_cb_func,
    Object *obj)
{
    Pc2fsProfiler::get_instance()->function_start();
    int rc = fsal_global_ok;
    try
    {
        // cast fsal response structure pointer and initialize type
        struct FsalDeleteInodeResponse *p_fsal_resp =
            (struct FsalDeleteInodeResponse *) p_fsal_resp_ref;
        p_fsal_resp->type = delete_inode_response;
        // call callback and get result
        rc = (obj->*p_cb_func)( &p_fsalmsg->inode_number);
        if(!rc)
        {
            // operation successful.
            p_fsal_resp->inode_number = p_fsalmsg->inode_number;
            p_fsal_resp->error = 0;
        }
        else
        {
            // an error occured.
            p_fsal_resp->error = fsal_delete_einode_error;
            rc = fsal_delete_einode_error;
        }
    }
    catch (StorageException e)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw e;
    }
    catch (...)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw MJIException("Handle delete einode exception raised.");
    }
    Pc2fsProfiler::get_instance()->function_end();
    return rc;
}

/** @brief Calls the read dir mechanism of the provided backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing
 * the incomming request.
 * @param[out] p_fsal_resp_ref FSAL structure pointer that the result
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 *
 * @details The backend pointed to by p_cb_func musst provide the
 * "ReadDirRequestCbIf" as definied in
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_readdir_request(
    struct FsalReaddirRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    ReadDirRequestCbIf p_cb_func,
    Object *obj)
{
    Pc2fsProfiler::get_instance()->function_start();
    int rc = fsal_global_ok;
    try
    {
        // cast and initialize fsal response strucre pointer
        struct FsalReaddirResponse *p_fsal_resp =
            (struct FsalReaddirResponse *) p_fsal_resp_ref;
        p_fsal_resp->type = read_dir_response;

        // structure to get result
        //struct ReadDirReturn rdir;
        //callback writes result to rdir.
        rc = (obj->*p_cb_func) (  &p_fsalmsg->inode_number,
                                  &p_fsalmsg->offset,
                                  &p_fsal_resp->directory_content);
        if (!rc)
        {
            //if (sizeof(struct FsalReaddirResponse) >= sizeof(rdir))
            //{
                // operation successful
           //     memcpy(&p_fsal_resp->directory_content, &rdir, sizeof(rdir));
                p_fsal_resp->error = 0;
           // }
           // else
            //{
                // some type mismatch
           //     rc=generic_handle_readdir_request_error_readdirreturn_to_large;
            //    p_fsal_resp->error = generic_handle_readdir_request_error_readdirreturn_to_large;
            //}
        }
        else
        {
            // operation not successful, write error code to fsal response
            p_fsal_resp->error = fsal_read_dir_request_error;
            rc = fsal_read_dir_request_error;
        }
    }
    catch (StorageException e)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw e;
    }
    catch (...)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw MJIException("Handle readdir request exception raised.");
    }
    Pc2fsProfiler::get_instance()->function_end();
    return rc;
}


/** @brief Calls the update attributes mechanism of the provided backend.
 * @param[in] p_fsalmsg pointer to the fsal msg structure representing
 * the incomming request.
 * @param[out] p_fsal_resp_ref FSAL structure pointer that the result
 * of the request is written to.
 * @param[in] p_cb_func function pointer to the storage backend handler.
 *
 * @details The backend pointed to by p_cb_func musst provide the
 * "UpdateAttributesRequestCbIf" as definied in
 * "fsal_zmq_mds_library.h" and return 0 on success.
 * */
int generic_handle_update_attributes_request (
    struct FsalUpdateAttributesRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    UpdateAttributesRequestCbIf p_cb_func,
    Object *obj)
{
    Pc2fsProfiler::get_instance()->function_start();
    int rc=0;
    try
    {
        // cast and initialize fsal response structure pointer
        struct FsalUpdateAttributesResponse *p_fsal_resp =
            (struct FsalUpdateAttributesResponse *) p_fsal_resp_ref;
        p_fsal_resp->type = update_attributes_response;

        // callback processes request and returns status, rc = 0 ok.
        rc = (obj->*p_cb_func)( &p_fsalmsg->inode_number ,
                                &p_fsalmsg->attrs,
                                &p_fsalmsg->attribute_bitfield);
        if (!rc)
        {
            // operation successful
            p_fsal_resp->inode_number = p_fsalmsg->inode_number;
            p_fsal_resp->error = 0;
        }
        else
        {
            // error occured
            p_fsal_resp->inode_number = 0;
            p_fsal_resp->error = fsal_update_attributes_error;
        }
    }
    catch (StorageException e)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw e;
    }
    catch (...)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw MJIException("Handle update attributes exception raised.");
    }
    Pc2fsProfiler::get_instance()->function_end();
    return rc;
}

/** 
 * @todo add docs
 * */
int generic_handle_move_einode(
    struct FsalMoveEinodeRequest *p_fsalmsg,
    void *p_fsal_resp_ref,
    MoveEinodeCbIf p_cb_func,
    Object *obj)
{
    Pc2fsProfiler::get_instance()->function_start();
    int rc=0;
    try
    {
        // cast and initialize fsal response structure pointer
        struct FsalMoveEinodeResponse *p_fsal_resp =
            (struct FsalMoveEinodeResponse *) p_fsal_resp_ref;
        p_fsal_resp->type = move_einode_response;

        // callback processes request and returns status, rc = 0 ok.
        rc = (obj->*p_cb_func)( &p_fsalmsg->new_parent_inode_number ,
                                &p_fsalmsg->new_name,
                                &p_fsalmsg->old_parent_inode_number,
                                &p_fsalmsg->old_name);
        if (!rc)
        {
            // operation successful
            p_fsal_resp->error = 0;
        }
        else
        {
            // error occured
            p_fsal_resp->error = fsal_move_einode_error;
        }
    }
    catch (StorageException e)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw e;
    }
    catch (...)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw MJIException("Handle move einode exception raised.");
    }
    Pc2fsProfiler::get_instance()->function_end();
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
    Pc2fsProfiler::get_instance()->function_start();
    int rc = 0;
    try
    {
        // reinitialize zmq message
        rc = zmq_msg_init_size(p_msg, size);
        if (!rc)
        {
            // write data to zmq message
            memcpy(zmq_msg_data(p_msg),p_data,size);
            //zmq_msg_data(p_msg) = p_data;
        }
        else
        {
            // could not initialize zmq message
            rc = zmq_msg_init_size_error;
        }
    }
    catch (StorageException e)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw e;
    }
    catch (...)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw MJIException("FSAL msg to ZMQ message build error.");
    }
    Pc2fsProfiler::get_instance()->function_end();
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
    Pc2fsProfiler::get_instance()->function_start();
    int rc=0;
    try
    {
        // test whether the fsal structure size fits the zmq message size
        if (fsalsize >= zmq_msg_size(p_response))
        {
            // write zmq data to fsal structure
            memcpy(p_fsaldata, zmq_msg_data(p_response),zmq_msg_size(p_response));
        }
        else
        {
            // zmq message to large for the fsal structure
            rc=zmq_response_to_fsal_structure_error_message_size_mismatch;
        }
    }
    catch (StorageException e)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw e;
    }
    catch (...)
    {
        Pc2fsProfiler::get_instance()->function_end();
        throw MJIException("ZMQ to FSAL message error.");
    }
    Pc2fsProfiler::get_instance()->function_end();
    return rc;
}

