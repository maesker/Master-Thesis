#ifndef MESSAGE_TYPES_H_
#define MESSAGE_TYPES_H_

/**
 * @brief Messages for the Ganesha-FSAL <-> MDS communication
 * @file message_types.h
 * @author Markus Mäsker, maesker@gmx.net
 * @version 0.4
 * */

#include <sys/types.h>
#include "EmbeddedInode.h"
#include "ReadDirReturn.h"

/** 
 @brief sequence number to identify ghost messages.
 */
typedef uint32_t Fsal_Msg_Sequence_Number;
/** 
 * @brief 0 means OK, 1..255: can be used to specify error messages
 * @typedef ErrorFlag
 * */
typedef unsigned char ErrorFlag;

/**
 * @brief Dummy type the detailed data of the mlt message need to be defined
 * @todo is this attribute used anywhere?
 * @typedef MltContainer
 * */
typedef char MltContainer;

/** 
 * @brief FSAL - MDS Message types
 * @enum MsgType
 * */
enum MsgType
{
    create_file_einode_request,
    create_inode_response,

    update_attributes_request,
    update_attributes_response,

    delete_inode_request,
    delete_inode_response,

    move_einode_request,
    move_einode_response,

    lookup_inode_number_request,
    lookup_inode_number_response,
    parent_inode_number_lookup_request,
    parent_inode_number_lookup_response,

    parent_inode_hierarchy_request,
    parent_inode_hierarchy_response,

    einode_request,
    file_einode_response,

    read_dir_request,
    read_dir_response,

    fallback_error_msg,
    unknown_request_response,

    populate_prefix_permission,
	update_prefix_permission,

    populate_prefix_permission_rsp,
	update_prefix_permission_rsp
};


/**
 * @param type unknown_request_response
 * @param error ErrorFlag 
 * @brief MDS->Ganesha: mds was not able to identify the request message
 * */
struct FsalUnknownRequestResponse
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    ErrorFlag error;
};

/** 
 * @brief Ganesha->MDS: Request the whole EInode of the FS object represented by inode number.
 * @param type einode_request
 * @param partition_root_inode_number: root inode number of the root object of the subtree the requested inode is in.
 * @param inode_number number of the requested fs object.
 * 
 * Response message is FsalFileEInodeResponse
 * */
struct FsalEInodeRequest
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber partition_root_inode_number;
    InodeNumber inode_number;
};

/** 
 * @brief MDS->Ganesha: Returns EInode structure 
 * @param type file_einode_response
 * @param einode requested fs object as EInode structure.
 * @param error error code. 0 means ok.
 * 
 * Response to FsalEInodeRequest
 */
struct FsalFileEInodeResponse
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    struct EInode einode;
    ErrorFlag error;
};

/** 
 * @brief Ganesha->MDS: Create a new file system object
 * @param type create_file_einode_request
 * @param partition_root_inode_number root inode number of the root object of the subtree the requested inode is in.
 * @param parent_inode_number number of the directory the requested fs object shall be written to.
 * @param attrs inode_create_attributes_t of the created fs object
 * 
 * Response is FsalCreateInodeResponse 
 * */
struct FsalCreateEInodeRequest
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber partition_root_inode_number;
    InodeNumber parent_inode_number;
    inode_create_attributes_t attrs;
};

/** 
 * @brief MDS->Ganesha: returns new inode number of the created object
 * @param type create_inode_response
 * @param inode_number number of the created fs object.
 * @param error error code. 0 means ok.
 * 
 * Response to FsalCreateEInodeRequest
 */
struct FsalCreateInodeResponse
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber inode_number;
    ErrorFlag error;
};

/** 
 * @brief Ganesha->MDS: Request to delete an file system object
 * @param type delete_inode_request
 * @param partition_root_inode_number root inode number of the root object of the subtree the requested inode is in.
 * @param inode_number number of the fs object that gets removed.
 * 
 * Response is FsalDeleteInodeResponse 
 * */
struct FsalDeleteInodeRequest
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber partition_root_inode_number;
    InodeNumber inode_number;
};

/**
 * @brief MDS->Ganesha: File system object has been deleted.
 * @param type delete_inode_response
 * @param inode_number number of the requested fs object.
 * @param error error code. 0 means ok.
 * 
 * Response to FsalDeleteInodeRequest
 * */ 
struct FsalDeleteInodeResponse
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber inode_number;
    ErrorFlag error;
};

/** 
 * @brief Ganesha->MDS: Lookup inode number of a named file system object
 * @param type lookup_inode_number_request
 * @param partition_root_inode_number root inode number of the root object of the subtree the requested inode is in.
 * @param parent_inode_number inode number of the directory.
 * @param name name of the fs object to look up.
 * 
 * Response is FsalLookupInodeNumberByNameResponse 
 * */
struct FsalLookupInodeNumberByNameRequest
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber partition_root_inode_number;
    InodeNumber parent_inode_number;
    FsObjectName name;
};

/** 
 * @brief MDS->Ganesha: Return the requested inode number
 * @param type lookup_inode_number_response
 * @param inode_number number of the requested fs object.
 * @param error error code. 0 means ok.
 * 
 * Response to FsalLookupInodeNumberByNameRequest
 * */
struct FsalLookupInodeNumberByNameResponse
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber inode_number;
    ErrorFlag error;
};

/** 
 * @brief Ganesha->MDS: Read the directory content
 * @param type read_dir_request
 * @param partition_root_inode_number: root inode number of the root object of the subtree the requested inode is in.
 * @param inode_number number of the requested fs object.
 * @param offset Number of files to skip until start reading einodes
 * 
 * Response is FsalReaddirResponse 
 * */
struct FsalReaddirRequest
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber partition_root_inode_number;
    InodeNumber inode_number;
    ReaddirOffset offset;
};

/**
 * @brief MDS->Ganesha: Returns the requested directory content
 * @param type read_dir_response
 * @param directory_content Structure representing a list of einodes.
 * @param error error code. 0 means ok.
 * 
 * Response to FsalReaddirRequest
 * */
struct FsalReaddirResponse
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    struct ReadDirReturn directory_content;
    ErrorFlag error;
};

/** 
 * @brief Ganesha->MDS: Update the specified attributes
 * @param type update_attributes_request
 * @param partition_root_inode_number root inode number of the root object of the subtree the requested inode is in.
 * @param inode_number number of the requested fs object.
 * @param attrs attributes to update
 * @param attribute_bitfield bitfield representing the attributes that get updated.
 * 
 * Response is FsalUpdateAttributeResponse 
 * */
struct FsalUpdateAttributesRequest
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber partition_root_inode_number;
    InodeNumber inode_number;
    inode_update_attributes_t attrs;
    int attribute_bitfield;
};

/** 
 * @brief MDS->Ganesha: response to the update request
 * @param type update_attributes_response
 * @param inode_number number of the requested fs object.
 * @param error error code. 0 means ok.
 * 
 * Response to FsalUpdateAttributesRequest
 * */
struct FsalUpdateAttributesResponse
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber inode_number;
    ErrorFlag error;
};

/** 
 * @brief MDS->Ganesha: Fallback error message
 * @param type fallback_error_msg
 **/
struct FsalFallbackErrorMessage
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    ErrorFlag error;
};


/** 
 * @brief Ganesha->MDS: request all parent inode numbers 
 * @param type parent_inode_hierarchy_request
 * @param partition_root_inode_number partition root identifier
 * @param inode_number inode number of the object whos parents to look up
 * */
struct FsalInodeNumberHierarchyRequest{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber partition_root_inode_number;
    InodeNumber inode_number;
};

/** 
 * @brief MDS->Ganesha response to the FsalInodeNumberHierarchyRequest
 * @param type parent_inode_hierarchy_response
 * @param inode_number_list list of inode numbers
 * @param error error number
 * */
struct FsalInodeNumberHierarchyResponse
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    inode_number_list_t inode_number_list;
    ErrorFlag error;
};


struct FsalParentInodeNumberLookupRequest
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber partition_root_inode_number;
    inode_number_list_t inode_number_list;
    //InodeNumber inode_number;
};

struct FsalParentInodeNumberLookupResponse
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    inode_number_list_t inode_number_list;
    ErrorFlag error;
};

/**
 *
 */
struct AccessCounterRequest
{
	ganesha_request_types_t type;
};

/**
 * For Ganesha->Ganesha Comm
 */
struct FsalPopulatePrefixPermission
{
	enum MsgType type;
	prefix_perm_populate_t pperm_data;
	char server_address[16];
};

/**
 * For Ganesha->Ganesha Comm
 */
struct FsalUpdatePrefixPermission
{
	enum MsgType type;
	prefix_perm_update_t uperm_data;
	char server_address[16];
};


struct FsalPrefixPermPopulateRespone
{
	enum MsgType type;
	ErrorFlag error;
};

struct FsalPrefixPermUpdateRespone
{
	enum MsgType type;
	ErrorFlag error;
};
/**
 * For Ganesha->Ganesha Comm
 */
struct MDSPopulatePrefixPermission
{
	ganesha_request_types_t type;
	prefix_perm_populate_t pperm_data;
};

/**
 * For Ganesha->Ganesha Comm
 */
struct MDSUpdatePrefixPermission
{
	ganesha_request_types_t type;
	prefix_perm_update_t uperm_data;
};


/** 
 @brief Message to move an file system object
 * @param type is move_einode_request
 * @param seqnum sequence number of the message
 * @param partition_root_inode_number inode number of the root inode in the 
 * subtree
 * @param old_name name of the source object
 * @param new_name name of the target object
 * @param old_parent_inode_number inode number of the directory the object is 
 * located
 * @param new_parent_inode_number inode number of the diretory to move the 
 * object to
 */
struct FsalMoveEinodeRequest
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    InodeNumber partition_root_inode_number;
    FsObjectName old_name;
    FsObjectName new_name;
    InodeNumber old_parent_inode_number;
    InodeNumber new_parent_inode_number;
};


/** 
 * @brief response to the move request
 @param type  is move_einode_response
 * @param seqnum sequence number 
 * @param error error message, 0 means success, fail otherwise
 */
struct FsalMoveEinodeResponse
{
    enum MsgType type;
    Fsal_Msg_Sequence_Number seqnum;
    ErrorFlag error;
};

#endif
