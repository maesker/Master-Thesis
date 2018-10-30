/* 
 * File:   SPdata.h
 * Author: markus
 *
 * Created on 17. Januar 2012, 22:24
 */

#ifndef SPDATA_H
#define	SPDATA_H

#include <string.h>

#include "custom_protocols/global_protocol_data.h"
#include "mm/mds/ByterangeLockManager.h"
#include "components/OperationManager/OpManager.h"

static const uint8_t protocolid = spn_id;

enum spnbc_type
{
    result_msg,
    read_response,
    spn_pingpong,
};

struct SPNBC_head{
    struct custom_protocol_reqhead_t customhead;
    CCO_id              ccoid;
    InodeNumber         inum;
    StripeId            stripeid;
    StripeUnitId        stripeunitid;
};



extern struct ds_socket_t
{
	//pthread_t thread_id;
        uint32_t sequencenumber;
	void* zmq_socket;
	void* zmq_context;
        void* zmq_request_mem;
        pthread_mutex_t mutex;
        //SPNetraid *protocol;        
} ds_socket;

struct SPN_DeviceLayout_req_t{
    struct custom_protocol_reqhead_t head;    
};

struct SPN_DeviceLayout_resp_t{
    struct custom_protocol_resphead_t head;
    serverid_t id;
    uint8_t process_number;
};

struct SPHead {
    struct custom_protocol_reqhead_t customhead;
    struct OPHead       ophead;      
    struct Participants_bf      participants;
};

struct SPN_DirectWrite_req {
    struct SPHead       head;
    StripeId            stripeid;
    //StripeUnitId        stripeunitid;
};

struct SPN_Write_req {
    struct SPHead       head;
    StripeId            stripeid;
    //StripeUnitId        stripeunitid;
};

struct SPN_Read_req {
    struct SPHead       head;
    StripeId            stripeid;
    StripeUnitId        stripeunitid;
    size_t              start;
    size_t              end;
};

struct SPN_Pingpong {
    struct SPHead       head;
    serverid_t          a;
    serverid_t          b;
};

struct SPN_Register_client {
    struct SPHead       head;
    char                *ip;
};

union SPN_message {
    struct SPN_Write_req        write_request;
    struct SPN_Read_req         read_request;
    struct SPN_Register_client  register_client;
};


struct spn_task {
    union SPN_message   *p_msg;
    serverid_t          receiver;
};

void print_SPHead(struct SPHead *p_head, stringstream& ss);
void print_SPN_Message(SPN_message *p_msg, stringstream& ss);
void* spn_ioservicerun(void *ios);
#endif	/* SPDATA_H */

