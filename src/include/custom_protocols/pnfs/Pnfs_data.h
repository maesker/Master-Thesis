/* 
 * File:   Pnfs_data.h
 * Author: markus
 *
 * Created on 15. April 2012, 21:31
 */

#ifndef PNFS_DATA_H
#define	PNFS_DATA_H


#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <cstring>
#include <ctype.h>

#include "custom_protocols/global_protocol_data.h"

const uint32_t pnfs_zmq_max_msg_len = 1024;

struct PNFS_HelloWorld_req_t{
    struct custom_protocol_reqhead_t head;
    char data[8];
};

struct PNFS_HelloWorld_resp_t{
    struct custom_protocol_resphead_t head;
    char data[8];
};



struct PNFS_CreateSession_req_t{
    struct custom_protocol_reqhead_t head;
    gid_t gid;
    uid_t uid;
};

struct PNFS_CreateSession_resp_t{
    struct custom_protocol_resphead_t head;
    ClientSessionId csid;
};


struct PNFS_DataserverLayout_req_t{
    struct custom_protocol_reqhead_t head;
};

struct PNFS_DataserverLayout_resp_t{
    struct custom_protocol_resphead_t head;
    uint32_t count;
};

struct PNFS_DataserverAddress_req_t{
    struct custom_protocol_reqhead_t head;
    serverid_t dsid;
};
struct PNFS_DataserverAddress_resp_t{
    struct custom_protocol_resphead_t head;
    ipaddress_t address;
};

struct PNFS_ByterangeLock_req_t{
    struct custom_protocol_reqhead_t head;
    ClientSessionId csid;
    InodeNumber inum;
    uint64_t start;
    uint64_t end;
};

struct PNFS_ByterangeLock_resp_t{
    struct custom_protocol_resphead_t head;
    time_t expires;
};


struct PNFS_ReleaseLock_req_t{
    struct custom_protocol_reqhead_t head;
    ClientSessionId csid;
    InodeNumber inum;
    uint64_t start;
    uint64_t end;
};

struct PNFS_ReleaseLock_resp_t{
    struct custom_protocol_resphead_t head;
};
#endif	/* PNFS_DATA_H */

