/** 
 * @file   global_protocol_data.h
 * @author Markus Maesker <maesker@gmx.net>
 *
 * @date Created on 6. Januar 2012, 11:14
 */

#ifndef GLOBAL_PROTOCOL_DATA_H
#define	GLOBAL_PROTOCOL_DATA_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <time.h>

#include "global_types.h"
#include <string>


typedef uint8_t PET_MAJOR;
typedef uint8_t PET_MINOR;

typedef uint32_t socketid_t;

#define CCC_ASIO_BASEPORT               20000
#define SPN_ASIO_BASEPORT               30000
#define SPN_ASIO_CLIENT_BACKCHANNEL     40000
#define DEF_MDS_PORT                    5552
//#define CCC_PORT_FRONTEND 12000

//#define DEF_DS_PORT 11000

//#define MR_STORAGE_PREFIX_BACKEND "ipc:///tmp/storage_"
//#define MR_STORAGE_PREFIX_FRONTEND "tcp://*:"
//#define MR_STORAGE_PORT_FRONTEND 21000


#define MR_CONTROL_PREFIX_BACKEND "ipc:///tmp/"
#define MR_CONTROL_PREFIX_FRONTEND "tcp://*:"
#define MR_CONTROL_PORT_FRONTEND 10000

const uint32_t cpn_max_zmq_msg_len = 1024;
//const uint32_t spn_max_zmq_msg_len = 32*1024;

const time_t MSG_TIMEOUT_GENERAL_SEC = 5;
const time_t MSG_TIMEOUT_PNFS_SEC = 5;
const time_t  MSG_TIMEOUT_STORAGE_SEC = 5;
const time_t  MSG_TIMEOUT_CONTROL_SEC = 5;

enum custom_protocol_ids{
    spn_id,
    spnbc_id,
    cpn_id,
    pnfs_id,
    dsc_id,
    task_id,
    ccc_id,
};

enum major_errors{
    MAJOR_TIMEDOUT,
    MAJOR_UNKNOWN_MSG
};

enum queue_priorities {
    realtimetask,
    sec_recv,
    part_recv,
    prim_recv,
    client_prim,
    maintenance,
};

struct protocol_error_t {
    PET_MAJOR major_id;
    PET_MINOR minor_id;
};

struct custom_protocol_reqhead_t {
    custom_protocol_ids protocol_id;
    uint8_t msg_type;
    uint32_t sequence_number;
    time_t creation_time;
    size_t datalength;
    void *datablock;
    char *ip;
};

struct custom_protocol_resphead_t {
    enum custom_protocol_ids protocol_id;
    uint8_t msg_type;
    uint32_t sequence_number;
    time_t creation_time;
    struct protocol_error_t error;
};


struct async_message_head{
    uint32_t msgsize;
    //uint16_t opnum;
    //uint16_t msgtype;
};

struct async_msg_data {
    uint32_t datasize;
    uint16_t opnum;
    uint16_t msgtype;
    void *p_data;
};
        

//const uint8_t PID_pnfsprotocol = 200;
//const uint8_t PID_spnetraid = 201;
//const uint8_t PID_cpnetraid = 202;

const uint8_t MT_helloworld_req = 1;
const uint8_t MT_helloworld_resp = 2;
const uint8_t MT_unknownmsg_resp = 3;

const uint8_t PNFS_CreateSession_req    = 10;

const uint8_t PNFS_CreateSession_resp   = 11;
const uint8_t PNFS_ByterangeLock_req    = 12;
const uint8_t PNFS_ByterangeLock_resp   = 13;
const uint8_t PNFS_DataserverLayout_req = 14;
const uint8_t PNFS_DataserverLayout_resp= 15;
const uint8_t PNFS_DataserverAddress_req = 16;
const uint8_t PNFS_DataserverAddress_resp= 17;
const uint8_t PNFS_ReleaseLock_req      = 18;
const uint8_t PNFS_ReleaseLock_resp     = 19;

const uint8_t SP_DeviceLayout_req       = 120;
const uint8_t SP_DeviceLayout_resp      = 121;
const uint8_t SP_Write_SU_req           = 122;
const uint8_t SP_Write_S_req            = 123;
const uint8_t SP_DIRECT_Write_SU_req    = 126;
const uint8_t SP_DIRECT_Write_S_req     = 125;
const uint8_t SP_Read_req               = 124;
const uint8_t SP_Pingpong_req           = 125;

const uint8_t CP_DistricuteLock_req     = 180;
const uint8_t CP_DistricuteLock_resp    = 181;

const uint8_t MT_CCC_cl_smallwrite      = 254;
const uint8_t MT_CCC_prepare            = 253;
const uint8_t MT_CCC_cancommit          = 252;
const uint8_t MT_CCC_docommit           = 251;
const uint8_t MT_CCC_committed          = 250;
const uint8_t MT_CCC_result             = 249;
const uint8_t MT_CCC_stripewrite_cancommit      = 248;
const uint8_t MT_CCC_pingpong           = 247;
//const uint8_t MT_CCC_stripeunitwrite   = 247;
//const uint8_t ipaddress_array_size = 32; 


void free_custom_protocol_reqhead_t(custom_protocol_reqhead_t *p);
void print_custom_protocol_reqhead(struct custom_protocol_reqhead_t *p, std::stringstream& ss);
void print_custom_protocol_resphead(struct custom_protocol_resphead_t *p);

#endif	/* GLOBAL_PROTOCOL_DATA_H */

