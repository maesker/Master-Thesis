#ifndef DATASERVER_H
#define DATASERVER_H

/**
 * @file DataServer.h
 * @author Markus Maesker, maesker@gmx.net
 * @date 20.12.11
 * @brief Main data server instance     
 * */

#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string.h>
#include <cstring>
#include <malloc.h>
#include <zmq.hpp>
#include <vector>

#include "global_types.h"

#include "tools/sys_tools.h"
#include "tools/parity.h"
#include "coco/communication/ConcurrentQueue.h"
#include "abstracts/AbstractComponent.h"

#include "custom_protocols/global_protocol_data.h"
#include "custom_protocols/storage/SPdata.h"
#include "custom_protocols/storage/SPNBC_client.h"
#include "custom_protocols/cluster/CCCdata.h"
#include "custom_protocols/cluster/CCCNetraid_client.h"
#include "custom_protocols/pnfs/Pnfsdummy_client.h"

#include "customExceptions/ComponentException.h"
#include "components/raidlibs/raid_data.h"
#include "components/raidlibs/Libraid4.h"
#include "components/diskio/Filestorage.h"
#include "components/network/ServerManager.h"
#include "components/OperationManager/OpManager.h"
#include "components/DataObjectCache/doCache.h"

#include "mm/mds/ByterangeLockManager.h"
#include "exceptions/MDSException.h"


#define DS_PINGPONG_INTERVAL 998        // + 2 msg over the storage protocol

class DataServer: public AbstractComponent
{
public:
    static DataServer *get_instance();
    static DataServer *create_instance(std::string logdir,std::string basedir, serverid_t id);
    ~DataServer();    
    
    int32_t run();
    int32_t stop();
    int16_t verify();
    serverid_t getId();
    
    static void push_spn_in(void *op);
    static void push_ccc_in(void *op);
    
    static int pushOperation(queue_priorities priority, OPHead *op);    
    void* popOperation();
    
    PET_MINOR handle_DeviceLayout(serverid_t *id);
    int32_t register_lock_object(struct lock_exchange_struct *p_lock);
    
    int  handle_ds_task(struct dstask_head *p_taskhead);
    int  handle_primcoordinator_task(struct OPHead *p_head);
    int  handle_participator_task(struct OPHead *p_head);
    int  handle_ping_msg(struct dstask_pingpong *p_task);

    int  perform_CCC_cl_smallwrite(struct dstask_ccc_send_received *p_task);
    int  perform_CCC_send_prepare(struct dstask_ccc_send_prepare *p_task);
    int  perform_CCC_send_docommit(struct operation_dstask_docommit *p_task);
    int  perform_CCC_send_result(struct dstask_ccc_send_result *p_task);
    int  perform_object_write(struct operation_participant *p_part);
    int  perform_CCC_stripewrite_cancommit(struct dstask_ccc_stripewrite_cancommit *p_task);
    
private:
    explicit DataServer(std::string basedir, std::string conffile,std::string logdir, serverid_t id);
    static DataServer *p_instance;
    
    ServerManager *p_sm;
    ServerManager *p_client_sm;
    ByterangeLockManager *p_brlman;
    OpManager *p_opman;
    Libraid4 *p_raid;
    CCCNetraid_client *p_ccc;
    
    SPNBC_client *p_spnbc;
    Pnfsdummy_client *p_pnfs_cl;
    Filestorage *p_fileio;
    doCache *p_docache;
    serverid_t id;
    serverid_t mdsid;
    int gcinterval;
    int worker_threads_storage;
    
    
    int handle_CCC_prepare(struct dstask_ccc_recv_prepare *p_head);
    int handle_CCC_docommit(struct operation_dstask_docommit *p_task);
    int handle_CCC_result(struct dstask_proccess_result *p_head);
    
    int handle_failure(struct operation_participant *p_part);
    int handle_success(struct operation_participant *p_part);
    
    int getDataserverAddress(serverid_t mds,serverid_t dsid);
    //int calculate_parity_block(struct operation_participant *p_op);
    //int calculate_parity_block(struct operation_primcoordinator *p_op);
    int get_primcoordinator(struct OPHead *p_head, serverid_t *server);
    //int perform_primcoord_docommit(struct dstask_writetodisk *p_task);
    
    int handle_read_req(struct dstask_spn_recv *p_task);
    
    int maintenance_garbage_collection(struct dstask_maintenance_gc *p_task);
};

#endif // DATASERVER_H
