#ifndef METADATASERVER_H
#define METADATASERVER_H

/**
 * @file MetadataServer.h
 * @author Markus Mäsker, maesker@gmx.net
 * @date 15.7.11
 * */

#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sstream>
#include <string.h>
#include <malloc.h>
#include <zmq.hpp>

#include "global_types.h"
#include "coco/coordination/MltHandler.h"


#include "exceptions/MDSException.h"
#include "exceptions/ConfigurationManagerException.h"

#include "mm/mds/MdsSessionManager.h"
#include "mm/mds/ByterangeLockManager.h"
#include "mm/mds/SubtreeManager.h"
#include "mm/mds/MockupObjects.h"
#include "mm/mds/mds_journal_interface_lib.h"
#include "mm/mds/mds_sys_tools.h"
#include "mm/mds/AbstractMdsComponent.h"
#include "mm/mds/test/dump_data.h"

#include "mm/einodeio/InodeNumberDistributor.h"

#include "mm/journal/JournalManager.h"
#include "mm/journal/Journal.h"

#include "mm/storage/storage.h"

#include "coco/communication/CommonCommunicationTypes.h"
#include "coco/prefix_perm/PrefixPermCommunicationHandler.h"

#include "components/network/ServerManager.h"
#include "custom_protocols/global_protocol_data.h"
#include "custom_protocols/control/CPNetraid_client.h"

#include "components/raidlibs/Libraid4.h"
#include "components/raidlibs/raid_data.h"


#ifdef __cplusplus
extern "C"
{
#endif

#include "ebwriter.h"
#include "mlt/mlt.h"
#include "mlt/mlt_file_provider.h"

#ifdef __cplusplus
}
#endif

#define DEFAULT_GANESHA_CONF DEFAULT_CONF_DIR "/" "default_ganesha_zmq.conf"
#define GANESHA_MAIN_CONF GANESHA_CONF_DIR "/" GANESHA_ZMQ_CONFIG_FILE_NAME

#define MSG_LONG_INT_LENGTH sizeof(long)
//#define MDS_CONF_FILE DEFAULT_CONF_DIR "/" "mds.conf"

#define MEMDIFF(mi_1, mi_2) {if(mi_1.uordblks-mi_2.uordblks){ printf("DIFF:%d\n",mi_1.uordblks-mi_2.uordblks);}}


//static int zmq_fallback_error_message(zmq_msg_t *p_response, Fsal_Msg_Sequence_Number seqnum);


class MetadataServer: public AbstractMdsComponent
{
public:
    static MetadataServer *get_instance();
    static MetadataServer *create_instance(JournalManager *p_jm, MltHandler *p_mlt_handler, std::string basedir);
    ~MetadataServer();
    int32_t run();
    int32_t stop();
    int16_t verify();    
    
    int32_t handle_move_einode_request(zmq_msg_t *p_msg, void *p_fsal_resp_ref);
    int32_t handle_einode_request(zmq_msg_t *p_msg, void *p_fsal_resp);
    int32_t handle_lookup_inode_number_request(zmq_msg_t *p_msg, void *p_fsal_resp);
    int32_t handle_read_dir_request(zmq_msg_t *p_msg, void *p_fsal_resp);
    int32_t handle_update_attributes_request(zmq_msg_t *p_msg, void *p_fsal_resp);
    int32_t handle_delete_inode_request(zmq_msg_t *p_msg, void *p_fsal_resp);
    int32_t handle_create_file_einode_request(zmq_msg_t *p_msg, void *p_fsal_resp);
    int32_t handle_parent_inode_number_lookup_request(zmq_msg_t *p_msg, void *p_fsal_resp);
    int32_t handle_parent_inode_hierarchy_request(zmq_msg_t *p_msg, void *p_fsal_resp_ref);
    int32_t handle_migration_create_partition(InodeNumber *inum);
    int32_t handle_migration_quit_partition(InodeNumber *inum);
    int32_t investigate_subtree_merge_potential();
    int32_t handle_populate_prefix_perm(zmq_msg_t *p_msg);
    int32_t handle_update_prefix_perm(zmq_msg_t *p_msg);
    int32_t handle_ganesha_configuration_update();
    int32_t set_storage_abstraction_layer(StorageAbstractionLayer *p_sal);
    int         set_dataserver_sshpw(std::string pw);
    
    int32_t handle_pnfs_create_session(gid_t g, uid_t, ClientSessionId *scid);
    int32_t handle_pnfs_byterangelock(ClientSessionId csid, InodeNumber inum, uint64_t start, uint64_t end, time_t *expires);
    int32_t handle_pnfs_releaselock(ClientSessionId csid, InodeNumber inum, uint64_t start, uint64_t end);
    int32_t handle_pnfs_dataserver_layout(uint32_t *count);
    int32_t handle_pnfs_dataserver_address(serverid_t *dsids, ipaddress_t *addresses,uint16_t *port);
    
    int32_t get_filelayout(InodeNumber inum, filelayout *fl);
    int32_t get_einode(InodeNumber inum, struct EInode *p_einode);
    
    void trace_incoming_message( zmq_msg_t* request, uint32_t message_id);
    void trace_outgoing_message( zmq_msg_t* request, uint32_t message_id);

    
    
private:
    explicit MetadataServer(JournalManager *p_jm,MltHandler *p_mlt_handler,std::string basedir, std::string conffile);
    static MetadataServer *p_instance;
    short mds_id;
    InodeNumber root_inode_number;
    MltHandler *p_mlt_handler;
    SubtreeManager *p_subtree_manager;
    InodeNumberDistributor *p_inumdist;
    StorageAbstractionLayer *p_sal;
    
    CPNetraid_client *p_cpclient;
    MdsSessionManager *p_sessionman;
    ByterangeLockManager *p_byterangelock;
    ServerManager *p_dataserver;
    Libraid4 *p_raid;
    
    Journal* get_responsible_journal(InodeNumber *p_subtree_root_inode_number);
    uint32_t register_dataserver();
    int32_t generateNewInodeNumber (InodeNumber *p_inum);
    int32_t block_subtree(InodeNumber *p_inum);
    int32_t handle_referral_request(FsalLookupInodeNumberByNameRequest *fsalmsg, InodeNumber *result);
    
    bool fs_root_flag;
    std::string ganesha_process_name;
    std::string path_export_conf;
    uint16_t worker_threads;
    mutable pthread_mutex_t mutex_write_export_block;
    std::string user;
    std::string dataserver_sshpw;

    int32_t write_export_conf(struct mlt *p_mlt, const char *path);
    
    int32_t get_directory_inode_fsal_response(void *p_fsal, InodeNumber inum);
    int32_t send_sighup(); 

    mutable pthread_mutex_t mutex_trace_file;
    ofstream trace_file; 
    void setup_message_tracing();

};

/** 
 * @enum Metadata error codes.
 * */
enum MdsErrorCodes
{
    mds_global_ok,
    mds_write_export_block_config_error,
    mds_send_sighup_error,
    mds_no_matching_joural_found,
    mds_no_inode_number_distributor_registered,
    mds_message_router_startup_error,
    mds_parameter_error_empty_pointer,
    mds_new_journal_creation_error,
    mds_new_mlt_entry_creation_error,
    mds_wont_delete_root_einode_error,
    mds_wont_change_root_einode_attributes_error,
    mds_journal_backend_einode_request_error,
    mds_storage_exception_thrown,
    mds_mji_exception_thrown,
    mds_send_sighub_exception,
    mds_ganesha_conf_dir_error,
};

#endif // METADATASERVER_H
