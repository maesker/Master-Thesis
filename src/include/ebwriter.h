#ifndef EBWRITER_H_
#define EBWRITER_H_

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mlt/mlt.h"
#include "global_types.h"
#include <assert.h>


/** 
 * @brief Define an exported subtree
 * Entries will be written to config file and read by ganesha
 * 
 * @author  Markus Maesker, maesker@gmx.net
 * @date 26.6.11
 * @version 0.3
 * @file ebwriter.h
 * **/
  
/**
 * @struct export_block
 * @brief Structure to hold the data written to a single export 
 * block. The single export blocks a joined into a linked list.
 * 
 * The detailed comments on the use of the single arguments is taken from the 
 * ganesha projects documentary.
 * */
struct export_block{
	/* Export block definition, taken from nfs-ganesha-adminguide.pdf, 
	 * (check out ganesha-projects webpage)
	 * 
	 * - Examples refer to the real config file syntax. 
	 * 	 Such as Export_Id = 1; 
	 * - The structs identifier names might differ (for example 
	 * 	 lowercase: Export_Id becomes export_id)
	 * 
	 * Convention:
	 * - i am not sure, but i think there musst be a blank before and 
	 * 	 after '='.
	 * - Quotes to encapsulate the values are added automatic
	 * - values must not contain the comment character (atm.)
	 * */ 

    /** 
    * @brief referral string to define remote machine 
    * 
    * The argument Referral is to be explained a little bit. Its content is made of 3 part : (local pseudofs path):(remote path)@(server)
    * the 'local pseudofs path' is the full path to the referral directory in server A pseudofs, you have to consider the value of 'Pseudo' is the first Export block (or do 'mount -t nfs4 A:/ /mnt' do get the path to use)
    * the remote path is the path on B for the referral
    * the last part should contain the server IP address. The server hostname could be used as well, but at the time I am writting this, it seems like the NFSv4 client in the kernel as problems in resolving hostname, so the explicit use of the IP address is preferable. 
    * */
    char *referral;
	
	/**
     * @brief intern list pointer */
	struct export_block *next;
	
	
	/* * * * * * * * * * * * * * * * * * * * * * * * * * * *
	 * * * * * * * mandatory arguments   * * * * * * * * * * 
	 * * * * * * * * * * * * * * * * * * * * * * * * * * * */
	 	
	/** 
     * @var export_id
	 * @brief Identifier of the exported file system
     * 
	 * non zero integer
	 * Example: Export_Id = 1;
	 * */
	int export_id; 
	
	
	/** @var path
     * @brief The path to export via nfs,.
     * 
	 * Example Path = "/nfs/my_dir_to_export" ;
	 * here *path = "/nfs/my_dir_to_export" ;
	 * NOT *path = "\"/nfs/my_dir_to_export\" \; " ;
	 * */
  	char *path;
  
	/** @var pseudo
	 * @brief a NFSv4 speciﬁc key that shows the path, in NFSv4 
	 * pseudo ﬁle system were the ’actual’ mount point resides.
     * 
	 * e.g : Pseudo = ”/nfsv4/pseudofs/nfs mount entry #1” ;
	 * */
	char *pseudo; 

	/* * * * * * * * * * * * * * * * * * * * * * * * * * * *
	 * * * * * * * optional arguments  * * * * * * * * * * *
	 * * * * * * * * * * * * * * * * * * * * * * * * * * * */
	
	/** @var access
	 * @brief The list of clients that can access the export entry.
     * 
	 * e.g: Access = ”Machine3,Machine10*,NetworkB,@netgroupY”;
	 * NB: Using Access = ”*” ; will allow everybody to access the 
	 * mount point.*/
	 char *access;
	 
	 
	 /**  @var root_access
      * @brief The list of clients (see above) that have root access rights.
      * 
	  * (root remains root when crossing the mount point).
	  * e.g: Root Access = ”localhost,12.13.0.0,@nfsclient” ; */
	char *root_access;
	
	
	/** @var access_type
     * @brief Describes the kind of access you can have of the mount point. 
     * 
	 *  Acceptable values are:
	 *  - RO: Read-Only mount point
	 *  – RW: Read-Write mount points
	 *  – MDONLY: The mount point is RW for metadata, but data accesses 
	 * 			 are forbidden (both read and write). This means you 
	 * 			 can do everything on md, but nothing on data.
	 *  - MDONLY RO: Like RO, but reading data is forbidden. You can 
	 * 				read only metadata.
	 *  e.g: Access Type = ”RW”; */
	char *access_type;	
	
	
	/** @var anonymous_root_uid
	 * @brief The uid to be used for root when no root access was speciﬁed for
	 * this clients. 
     * 
     * This is the deﬁnition of the ’nobody’ user. 
	 * ”Traditional” value is -2 
	 * e.g: Anonymous root uid = -2 ;*/
	int anonymous_root_uid;
	
	/** @var nosuid 
	 * @brief a ﬂag (with value TRUE or FALSE) showing if setuid bit is kept.
     * 
     * e.g : NOSUID = TRUE;*/
	short nosuid;
	
	
	/** @var nosgid
     * @brief a ﬂag (with value TRUE or FALSE) showing if setgid bit is kept.
     * 
	 * e.g : NOSGID= TRUE;*/
	short nosgid;
	 
	 
	/** @var nfs_protocols
	  * @brief The version(s) of the NFS protocol you can use for mounting 
	  * this export entry
      * 
	  * e.g: NFS Protocols = ”2,3,4” ;*/
	char *nfs_protocols;
	
	/** @var transport_protocols
     * @brief The transport layer to use to mount this entry.
     * 
     * This should be UDP or TCP or a list.
	 * e.g : Transport Protocols = ”UDP,TCP” ;*/
	 char *transport_protocols;
	 
	/** @var sec_type
	 * @brief List of supported RPC SEC GSS authentication ﬂavors for this
	 * export. 
     * 
     * It can be a coma-separated list of the following values:
	 * sys, krb5i, krb5p.
	 * e.g : SecType = ”sys,krb5”;*/
	char *sec_type;
	
	/** @var maxoffsetread
	 * @brief Maximum offset allowed for a read operation (no limit if not 
	 * speciﬁed). 
     * 
     * This could be useful for prevently from ”evil” use
	 * of NFS read (like access a TB long ﬁle via NFS)
	 * e.g : MaxRead = 409600; */
	int maxoffsetread;
	
	
	/** @var maxoffsetwrite
	 * @brief Like MaxOﬀsetRead, but for Write operation
     * 
	 *  e.g : MaxWrite = 409600;*/
	int maxoffsetwrite;
	
	/** @var maxread
	 * @brief The value to be returned to client when NFS3 FSINFO is called. 
	 * Better solution is not to use this keys and so keep the default
	 * values, optimized for NFSv3.*/
	int maxread;

	/** @var maxwrite 
	 * @brief The value to be returned to client when NFS3 FSINFO is called. 
	 * Better solution is not to use this keys and so keep the default
	 * values, optimized for NFSv3.*/
	int maxwrite;
    
    /** @var prefread 
	 * @brief The value to be returned to client when NFS3 FSINFO is called. 
	 * Better solution is not to use this keys and so keep the default
	 * values, optimized for NFSv3.*/
	int prefread;
    
    /** @var prefwrite
	 * @brief The value to be returned to client when NFS3 FSINFO is called. 
	 * Better solution is not to use this keys and so keep the default
	 * values, optimized for NFSv3.*/
	int prefwrite;
    
    /** @var prefreaddir
	 * @brief The value to be returned to client when NFS3 FSINFO is called. 
	 * Better solution is not to use this keys and so keep the default
	 * values, optimized for NFSv3.*/
	int prefreaddir; 
	
	
	/** @var filesystem_id
	 * @brief The ﬁlesystem id to provide to the client for this export entry.
     * 
	 * NFS Client will use this value to address their internal 
	 * metadata cache. In NFSv4, both major and minor values are used,
	 * in NFSv2 and NFSv3, only the major is used.
	 * e.g. : Filesystem id = 100.1 ;*/
	double filesystem_id;
	
	/** @var privilegedport
	 * @brief A ﬂag (TRUE or FALSE) to specify is a client using an ephemere
	 * port should be rejecting or not.
     * 
	 * e.g. PrivilegedPort = FALSE ;
     * The value should be specified using 0 and 1 for false and true.*/
	short privilegedport;
	
	/** @var cache_data
	 * @brief A ﬂag (TRUE or FALSE) To specify if ﬁles content should be 
	 * cached by the GANESHA server or not
     * 
	 * e.g : Cache Data = TRUE ;
     * The value should be specified using 0 and 1 for false and true.*/
	char *cache_data;
	
	/** @var max_cache_size
     * @brief If export entry is datacached, this value deﬁnes the maximum 
	 * size of the ﬁles stored in this cache. 
     * 
     * @todo this should be evauated to find the best performing value.
     * */
	int max_cache_size;
	
	/** @var fs_specific
	 * @brief A comman separated list of information used by the FSAL to 
	 * perform initialization. 
     * 
     * See FSAL documentation for detail.
     * e.g. (for HPSS/FSAL): FS Speciﬁc = ”cos=1”   */
	char *fs_specific;
	 
	/** @var tag
	 * @brief A way of providing a shorter path for mounting an entry. 
     * 
     * For example, you could mount an entry like this:
     * @verbatim mmount -o vers=3 nfsserver:/nfs/my_exported_directory @endverbatim
	 * But if you speciﬁed ”Tag = ganesha;”, you can simply do
	 * @verbatim mount -o vers=3 nfsserver:ganesha /mnt   @endverbatim
	 **/
	 char *tag; 
} ;

/**
 * @public
 * @brief Creates the Export configuration file.
 * @param[in] p_mlt pointer to the mlt structure that represents the MDS environment
 * @param[in] p_target char pointer representing the path to write the configuration to
 * @param[in] mds pointer to the server structure that represents this mds instance
 * 
 * The function walks through the mlt tree structure and investigates every entry. 
 * On each entry, it reads the necessary information from the mlt entry to fill 
 * a new export block structure and add it to the export_block linked list. 
 * 
 * If the mlt entrys server does not match the provided mds parameter, a referral entry
 * to this server is added to the export block entry.
 * 
 * @return integer representing success or fail.
 **/
int create_export_block_conf(struct mlt *p_mlt, const char *p_target, struct asyn_tcp_server *mds);

/** 
 * @brief Sends a signal to the provided process name.
 * @param[in] processname const char pointer representing the processname to send the signal to
 * @param[in] signal_number short integer representing the desired signal
 * 
 * Example:
 * @verbatim
   processname: zmq.ganesha.nfsd
   signal_number: 1 (this is a SIGHUP)
   @endverbatim
 * */
int send_signal(const char *processname, short signal_number);

pid_t get_pid(char *p_processname);

/** @def EXPORT_DEF_PSEUDO 
 * @brief default export_block.pseudo value  */
#define EXPORT_DEF_PSEUDO "/fs"

/** @def EXPORT_DEF_ACCESS 
@brief default export_block.access value*/
#define EXPORT_DEF_ACCESS "*"

/** @def EXPORT_DEF_ROOT_ACCESS 
@brief default export_block.root_access value*/
#define EXPORT_DEF_ROOT_ACCESS "*"

/** @def EXPORT_DEF_ACCESS_TYPE
 * @brief default export_block.access_type value */
#define EXPORT_DEF_ACCESS_TYPE "RW"

/** @def EXPORT_DEF_ANON_ROOT_UID
 * @brief default export_block.anonymous_root_uid value */
#define EXPORT_DEF_ANON_ROOT_UID -2

/** @def EXPORT_DEF_NOSUID
 * @brief default export_block.nosuid value */
#define EXPORT_DEF_NOSUID 0

/** @def EXPORT_DEF_NOSGID
 * @brief default export_block.nosgid value */
#define EXPORT_DEF_NOSGID 0

/** @def EXPORT_DEF_NFS_PROTOCOLS
 * @brief default export_block.nfs_protocols value */
#define EXPORT_DEF_NFS_PROTOCOLS "4"

/** @def EXPORT_DEF_TRANSPORT_PROTOCOL
 * @brief default export_block.transport_protocols value */
#define EXPORT_DEF_TRANSPORT_PROTOCOL "TCP"

/** @def EXPORT_DEF_SEC_TYPE
 * @brief default export_block.sec_type value */
#define EXPORT_DEF_SEC_TYPE "sys"

/** @def EXPORT_DEF_MAXOFFSETREAD
 * @brief default export_block.maxoffsetread value */
#define EXPORT_DEF_MAXOFFSETREAD 409600

/** @def EXPORT_DEF_MAXOFFSETWRITE
 * @brief default export_block.maxoffsetwrite value */
#define EXPORT_DEF_MAXOFFSETWRITE 409600

/** @def EXPORT_DEF_MAXREAD
 * @brief default export_block.maxread value */
#define EXPORT_DEF_MAXREAD 409600

/** @def EXPORT_DEF_MAXWRITE
 * @brief default export_block.maxwrite value */
#define EXPORT_DEF_MAXWRITE 409600

/** @def EXPORT_DEF_PREFREAD
 * @brief default export_block.prefread value */
#define EXPORT_DEF_PREFREAD 409600

/** @def EXPORT_DEF_PREFWRITE
 * @brief default export_block.prefwrite value */
#define EXPORT_DEF_PREFWRITE 409600

/** @def EXPORT_DEF_PREFREADDIR 
  @brief default export_block.prefreaddir value */
#define EXPORT_DEF_PREFREADDIR 409600

/** @def EXPORT_DEF_FILESYSTEM_ID
 * @brief default export_block.filesystem_id value
 * @todo how to specify this? MDS ID?*/
#define EXPORT_DEF_FILESYSTEM_ID 100 

/** @def EXPORT_DEF_PRIVILEGEDPORT
 * @brief default export_block.privilegedport value*/
#define EXPORT_DEF_PRIVILEGEDPORT 0

/** @def EXPORT_DEF_CACHE_DATA
 * @brief default export_block.cache_data value*/
#define EXPORT_DEF_CACHE_DATA "FALSE"

/** @def EXPORT_DEF_MAX_CACHE_SIZE
 * @brief default export_block.max_cache_size */
#define EXPORT_DEF_MAX_CACHE_SIZE 1024*1024

/** @def EXPORT_DEF_FS_SPECIFIC
 * @brief default export_block.fs_specific value*/
#define EXPORT_DEF_FS_SPECIFIC "1"

/** @def EXPORT_DEF_TAG
 * @brief default export_block.tag value */
#define EXPORT_DEF_TAG = "ganesha"

#endif
