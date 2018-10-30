#ifndef CLIENT_H
#define	CLIENT_H


/** 
 * @file   Client.h
 * @author Markus Maesker maesker@gmx.net
 *
 * @date 21. Dezember 2011, 01:55
 */

#include <stdio.h>
#include <sys/types.h>
#include "zmq.h"
#include <zmq.hpp>
#include <pthread.h>
#include <string>
#include <vector>
#include <map>

#include "global_types.h"
#include "message_types.h"
#include "tools/sys_tools.h"

/** @todo  repalce this
 * */      
#include "mm/mds/ByterangeLockManager.h"
#include "custom_protocols/pnfs/Pnfsdummy_client.h"
//#include "custom_protocols/pnfs/PnfsProtocol.h"
#include "custom_protocols/global_protocol_data.h"

#include "components/network/AsynClient.h"
#include "custom_protocols/storage/SPdata.h"
#include "custom_protocols/storage/SPNetraid_client.h"
#include "logging/Logger.h"
#include "components/configurationManager/ConfigurationManager.h"



class Client {
    public:
        Client();
        virtual ~Client();
        void* run();
        
        int acquireSID(serverid_t id);
        int acquireByteRangeLock(serverid_t *id, InodeNumber *inum, uint64_t *start, uint64_t *end);
        int releaselock(serverid_t *id, InodeNumber *inum, uint64_t *start, uint64_t *end);
     
        int getDataserverLayouts(serverid_t id);
        int getDataserverAddresses(serverid_t *mds, vector<serverid_t> *dsids);

        int handle_write(       struct EInode *p_einode, size_t offset, size_t length, void *data);
        int handle_write_lock(  struct EInode *p_einode, size_t offset, size_t length, void *data);
        int handle_read(        struct EInode *p_einode, size_t offset, size_t length, void **data);
        int pingpong();
        Pnfsdummy_client *p_pnfs_cl;
        
private:
        SPNetraid_client *p_spn_cl;
        ConfigurationManager *p_cm;
        
        ClientSessionId csid;

        ByterangeLockManager *p_brlman;
        uint32_t ds_count;
        Logger *log;
        std::string mds_address;        
};


#endif	/* CLIENT_H */

