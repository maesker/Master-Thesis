#ifndef GLOBAL_TYPES_H_
#define GLOBAL_TYPES_H_

/** 
 * @brief Global system wide types
 * @file global_types.h
 * */

#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <malloc.h>
#include <stdio.h>  /* defines FILENAME_MAX */

#define THREADING_TIMEOUT_USLEEP   50
#define THREADING_TIMEOUT_BACKUOFF 1.0
#define THREADING_TIMEOUT_MAXITER  200
#define BASE_THREADNUMBER 4
#define MDS_CONFIG_FILE "/etc/r2d2/mds.conf"
#define DS_CONFIG_FILE "/etc/r2d2/ds.conf"


#ifdef WINDOWS
    #include <direct.h>
    #define GetCurrentDir _getcwd
#else
    #include <unistd.h>
    /** 
     * @brief  Returns the current working directory 
     * @def GetCurrentDir 
     * */
    #define GetCurrentDir getcwd
 #endif

/**
 * @brief The maximal length of a file systems path. 
 * @def MAX_PATH_LEN
 * 
 *   This refers to the size of the string respresentation and therefore
 *   includes delimiters.
 * 
 *  It does NOT define the maximal depth of the file system tree 
 *  structure.*/
#define MAX_PATH_LEN      1024


/** 
 * @brief The maximal length of a file name
 * @def MAX_NAME_LEN 
 * */
#define MAX_NAME_LEN        255


/**
 * @brief The fixed size name of an file system object 
 * @typedef char FsObjectName[MAX_NAME_LEN]
 * */
typedef char FsObjectName[MAX_NAME_LEN];


/**
 * @brief The size of the layout? 
 * @def LAYOUT_INFO_LEN
 * */
#define LAYOUT_INFO_LEN     256

typedef char filelayout[LAYOUT_INFO_LEN];

/** 
 * @brief Maximum size of the ZMQ FSAL Message 
 * @def FSAL_MSG_LEN
 * @todo some performance evaluation should be considered here. 
 * */
#define FSAL_MSG_LEN          608

/** 
 * @brief The number of parent inode numbers to be cached 
 * @def PARENT_CACHE_SIZE
 * */
#define PARENT_CACHE_SIZE		4096

/**
 * @brief  The readdir offset specifier.
 * @typedef uint64_t ReaddirOffset
 * 
 *   The offset referes to the number of einodes inside a directory
 *   
 * This parameter is used during the readdir procedure. Lets say the
 * first request(_some_inodenumber_=123, offset=0) returned the first 
 * ten (0..9) einodes (due to the limited message size). 
 * The second request would then request(123, 10) and receive the 
 * einodes 10..19.*/
typedef uint64_t ReaddirOffset;

/** 
 * @brief Alias for unsigned char
 * @typedef unsigned char uchar
 * */
typedef unsigned char uchar;

/**
 * @brief The number of einodes returned by a single readdir request.
 * @def FSAL_READDIR_EINODES_PER_MSG
 * 
 * Be sure not to break this rule:
 * FSAL_MSG_LEN >= FSAL_READDIR_EINODES_PER_MSG * sizeof(EInode) 
 * */
#define FSAL_READDIR_EINODES_PER_MSG 1


/**
 * @brief Simple typedef to define the Inode number format.
 * @def uint64_t InodeNumber
 * */
typedef uint64_t InodeNumber;

/**
 * @brief Delimiter char for parsing paths 
 * @def DELIM 
 * */
#define DELIM "/"

/**
 * @brief The default location of the mlt.
 * @def MLT_PATH
 * */
#define MLT_PATH "/etc/mlt"

/** 
 * @brief Ganeshas main configuration directory
 * @def GANESHA_CONF_DIR
 * */
#define  GANESHA_CONF_DIR "/etc/nfsganesha"

/** 
 * @brief Export config file name
 * @def GANESHA_EXPORT_CONFIG_FILE_NAME 
 * */
#define GANESHA_EXPORT_CONFIG_FILE_NAME  "export.conf"


/** 
 * @brief Export config file name
 * @def GANESHA_EXPORT_CONFIG_FILE_NAME 
 * */
#define GANESHA_ZMQ_CONFIG_FILE_NAME  "zmq.conf"


/**
 * @brief The inode number of '/'
 * @def FS_ROOT_INODE_NUMBER 
 * 
 * This is a fixed value attribute and must not be changed .
 * */
#define FS_ROOT_INODE_NUMBER 1

/** 
 * @brief ExportId type definition
 * @def uint32_t ExportId_t
 * */
typedef uint32_t ExportId_t;


/** 
 * @brief unnown Attributes 
 * @def ZMQ_INODE_NUMBER_LISTSIZE
 * @todo Is this needed anymore?
 * */
#define ZMQ_INODE_NUMBER_LISTSIZE 32


/** 
 * @brief Inodenumber list retured by parent lookup
 * @typedef inode_number_list_t
 * @todo Is this type used anymore?
 * */
typedef struct 
{
  InodeNumber inode_number_list[ZMQ_INODE_NUMBER_LISTSIZE];
  uint16_t items;
} inode_number_list_t;

/** 
 * @brief Attributes used in update request
 * @typedef inode_update_attributes_t
 * */
typedef struct
{
  time_t atime;
  time_t mtime;
  mode_t mode;
  size_t size;
  gid_t gid;
  uid_t uid;
  nlink_t   st_nlink;
  int has_acl  ;
}inode_update_attributes_t;

/** 
 * @typedef inode_create_attributes_t
 * @brief Attributes used in create request
 * */
typedef struct
{
  mode_t mode;
  size_t size;
  gid_t gid;
  uid_t uid;
  int has_acl ;
  FsObjectName name;
  //size_t susize;
}inode_create_attributes_t;


/** 
 * @brief determines the type of the access_counter return value
 * @typedef ac_balancing_type_t
 * */
typedef enum ac_balancing_type__
{
	AC_SPLIT,
	AC_MOVE,
	AC_DISABLED
} ac_balancing_type_t;

/** 
 * @brief struct containing the inode number of the most accessed subtree as 
 * well as a flag that determines whether the affected partition shall be moved
 * or split
 * 
 * @typedef access_counter_return_t
 * */
typedef struct access_counter_return__
{
  InodeNumber inode_number;
  InodeNumber root_inode;
  ac_balancing_type_t flag;
} access_counter_return_t;


#define PP_POPULATE_MAX_SIZE	18

typedef struct pp_perm_attrib__
{
	mode_t mode;
	uid_t owner;
	gid_t group;
} pp_perm_attrib_t;

typedef struct pp_perm_cache_entry__
{
	InodeNumber inode;
	pp_perm_attrib_t attr;
} pp_perm_cache_entry_t;


/**
 * @brief struct containing the prefix permissions for the partition root created.
 *
 * @typedef prefix_perm_populate_t
 */
typedef struct prefix_perm_populate__
{
	InodeNumber root_inode;
	pp_perm_cache_entry_t entry[PP_POPULATE_MAX_SIZE];
	uchar num_entries;
} prefix_perm_populate_t;


/**
 * @brief struct containing the updated permissions
 *
 * @typedef prefix_perm_update_t
 */
typedef struct prefix_perm_update__
{
	InodeNumber     inode;
	InodeNumber		root_inode;
	pp_perm_attrib_t attr;
} prefix_perm_update_t;


/** 
 * @brief messages sent from loadbalancing module to Ganesha
 * @enum GanLBTypes
 * */

typedef enum ganesha_request_types__
{
	REQ_ACCESS_COUNTER,
	REQ_SWAPS,
	POPULATE_PREFIX_PERM,
	UPDATE_PREFIX_PERM
} ganesha_request_types_t;

/**
 * @brief  The inode number which gets used to mark an invalid inode
 * @def INVALID_INODE_ID
 *  */
#define INVALID_INODE_ID 0

/**
 * @brief  Mode bit 
 * @def MODE_SET
 *  */
#define MODE_SET 0x001

/**
 * @brief Set modes bit
 * @def SET_MODE
 *  */
#define SET_MODE(x)    (x |= MODE_SET)

/** 
 * @brief check if mode bit is set
 * @def IS_MODE_SET
 * */
#define IS_MODE_SET(x)      (x & MODE_SET)

/** 
 * @brief ctime bit
 * @def CTIME_SET
 * */
#define CTIME_SET 0x002

/** @brief Set ctime bit
 * @def SET_CTIME
 * */
#define SET_CTIME(x)    (x |= CTIME_SET)

/** 
 * @brief Check if ctime bit is set
 * @def IS_CTIME_SET
 * */
#define IS_CTIME_SET(x)      (x & CTIME_SET)

/** 
 * @brief mtime bit
 * @def MTIME_SET
 * */
#define MTIME_SET 0x004

/** 
 * @brief set mtime bit
 * @def SET_MTIME
 * */
#define SET_MTIME(x)    (x |= MTIME_SET)

/** 
 * @brief check if mtime bit is set
 * @def IS_MTIME_SET
 * */
#define IS_MTIME_SET(x)      (x & MTIME_SET)

/**
 * @brief size bit
 * @def  SIZE_SET
 * */
#define SIZE_SET 0x008

/**
 * @brief Set size bit
 * @def SET_SIZE
 * */
#define SET_SIZE(x)    (x |= SIZE_SET)

/**
 * @brief check if site bit is set
 * @def IS_SIZE_SET
 * */
#define IS_SIZE_SET(x)      (x & SIZE_SET)

/**
 * @brief atime bit
 * @def ATIME_SET
 * */
#define ATIME_SET 0x010

/**
 * @brief set atime bit
 * @def SET_ATIME
 * */
#define SET_ATIME(x)    (x |= ATIME_SET)

/** 
 * @brief check if atime is set
 * @def IS_ATIME_SET
 * */
#define IS_ATIME_SET(x)      (x & ATIME_SET)

/**
 * @brief nlink bit
 * @def NLINK_SET
 * */
#define NLINK_SET 0x020

/** 
 * @brief set nlink bit
 * @def SET_NLINK
 * */
#define SET_NLINK(x)    (x |= NLINK_SET)

/** 
 * @brief check if nlink is set
 * @def IS_NLINK_SET
 * */
#define IS_NLINK_SET(x)      (x & NLINK_SET)

/**
 * @brief has_acl bit
 * @def HAS_ACL_SET
 * */
#define HAS_ACL_SET 0x040

/** 
 * @brief set has_acl bit
 * @def SET_HAS_ACL
 * */
#define SET_HAS_ACL(x)    (x |= HAS_ACL_SET)

/** 
 * @brief check if has_acl bit is set
 * @def IS_HAS_ACL_SET
 * */
#define IS_HAS_ACL_SET(x)      (x & HAS_ACL_SET)

/**
 * @brief uid set
 * @def UID_SET
 * */
#define UID_SET 0x080

/** 
 * @brief set uid bit
 * @def SET_UID
 * */
#define SET_UID(x)    (x |= UID_SET)

/**
 * @brief check if uid is set
 * @def IS_UID_SET
 * */
#define IS_UID_SET(x)      (x & UID_SET)

/**
 * @brief gid set
 * @def GID_SET
 * */
#define GID_SET 0x100

/** 
 * @brief set gid bit
 * @def SET_GID
 * */
#define SET_GID(x)    (x |= GID_SET)

/**
 * @brief check if uid is set
 * @def IS_GID_SET
 * */
#define IS_GID_SET(x)      (x & GID_SET)


/** 
 * @brief Length of a device name
 * @def DEVICE_NAME_LEN
 * */
#define DEVICE_NAME_LEN     64

/** 
 * @brief 
 * @todo what is this
 * @def SERVER_LEN
 * */
#define SERVER_LEN      16

/**
 * @brief default directory for config files
 * @def DEFAULT_CONF_DIR
 * */
#define DEFAULT_CONF_DIR "conf"

/**
 * @brief default directory to log to
 * @def DEFAULT_LOG_DIR
 * */
#define DEFAULT_LOG_DIR "/tmp"

typedef uint32_t serverid_t;
typedef serverid_t ClientSessionId;
typedef char ipaddress_t[16];
#endif
