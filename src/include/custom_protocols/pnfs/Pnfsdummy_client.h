/* 
 * File:   Pnfsdummy_client.h
 * Author: markus
 *
 * Created on 6. Januar 2012, 20:32
 */

#ifndef PNFSDUMMY_CLIENT_H
#define	PNFSDUMMY_CLIENT_H

#include <zmq.h>
#include <zmq.hpp>
#include <string>
#include <vector>

#include "global_types.h"

#include "custom_protocols/pnfs/Pnfs_data.h"
#include "custom_protocols/global_protocol_data.h"
#include "custom_protocols/SocketManager/SocketManager.h"
#include "logging/Logger.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "fsal_zmq_client_library.h"
    
#ifdef __cplusplus
}
#endif


struct mds_socket {
    void*       zmq_socket;
    void*       zmq_context;
    void*       zmq_memspace;
    uint32_t    sequencenumber;
    
    pthread_mutex_t mutex;
    //PnfsProtocol   *protocol;
};


class Pnfsdummy_client {
public:
    Pnfsdummy_client(Logger *p_log);
    Pnfsdummy_client(const Pnfsdummy_client& orig);
    virtual ~Pnfsdummy_client();
    
    int addMDS(serverid_t id, std::string ip, uint16_t port);
    
    int handle_pnfs_send_helloworld(serverid_t id);
    int handle_pnfs_send_createsession(serverid_t id, ClientSessionId *csid);
    int handle_pnfs_send_byterangelock(serverid_t *id, ClientSessionId *csid, InodeNumber *inum, uint64_t *start, uint64_t *end, time_t *expires);
    int handle_pnfs_send_releaselock(serverid_t *id, ClientSessionId *csid, InodeNumber *inum, uint64_t *start, uint64_t *end);
    int handle_pnfs_send_dataserverlayout(serverid_t id, uint32_t *dscount);
    int handle_pnfs_send_dataserver_address(serverid_t *mds, serverid_t *dsid, ipaddress_t *address);
    
    int meta_create_file(    InodeNumber *parent, FsObjectName *name, InodeNumber *p_res);
    int meta_create_file(    InodeNumber *partition_root, InodeNumber *parent, FsObjectName *name, InodeNumber *p_res);
    int meta_get_file_inode( InodeNumber *partition_root, InodeNumber *p_inum, struct EInode *p_einode);
    
private:
    SocketManager<struct mds_socket>  *sm;
    Logger *log;
    uid_t uid;
    gid_t gid;
    InodeNumber root;    
    
    
    int send_and_recv(    void *p_data,
                          size_t data_size,
                          zmq_msg_t *p_response,
                          struct mds_socket *socket);
    
    int connectMDS(serverid_t id);    
    inode_create_attributes_t* gen_create_inode_attributes(FsObjectName *name);
        
};

#endif	/* PNFSDUMMY_CLIENT_H */

