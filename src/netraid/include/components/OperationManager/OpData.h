/* 
 * File:   OpData.h
 * Author: markus
 *
 * Created on 9. Mai 2012, 19:28
 */

#ifndef OPDATA_H
#define	OPDATA_H
#include <stdlib.h>
#include <sstream>
#include "pthread.h"
#include <map>
#include "components/raidlibs/raid_data.h"

#include "global_types.h"
#include "EmbeddedInode.h"
#include "custom_protocols/global_protocol_data.h"

#define OPERATION_TIMEOUT_LEVEL_A  2

union operation;

/**
 * @brief coordinated cluster operation id
 * @param csid  client session id
 * @param sequencenum sequence number for the operation
 * 
 * The csid is unique throughout the whole system and the sequence number is 
 * unique for each client. Therefore CCO_id defines a systemwide unique 
 * operation identifier.
 */
struct CCO_id {
    ClientSessionId csid;
    uint32_t sequencenum;
};


struct vvecmap {
    StripeId    stripeid;
    uint32_t    versionvector[default_groupsize+1];
};

struct dataobject_metadata 
{
        struct CCO_id       ccoid;
        size_t              offset;
        size_t              operationlength;
        char                filelayout[256];
        uint32_t            versionvector[default_groupsize+1];
        size_t              datalength;
};
    
struct data_object 
{
    struct dataobject_metadata metadata;
    void                *data;
    uint32_t            checksum;
};

struct dataobject_collection {
    struct data_object  *existing;
    struct data_object  *newblock;    
    void                *recv_data;
    void                *parity_data;
    size_t              recv_length;
    uint32_t            versionvector[default_groupsize+1];
    uint32_t            mycurrentversion;
    int            filehandle;
    bool                fhvalid;
};

struct paritydata {
    size_t length;
    void *data;
};


struct datacollection_primco {
    std::map<uint32_t,struct paritydata*>       *paritymap;
    void                                        *finalparity;    
    struct data_object                           *existing;
    struct data_object                           *newobject;
    uint32_t                                    versionvec[default_groupsize+1];
    int                                    filehandle;
    bool                                        fhvalid;
};

enum optype {
    operation_participant_type,
    operation_primcoord_type,
    operation_client_composite_write_type,
    operation_client_composite_directwrite_type,
    operation_client_su_write,
    operation_client_s_write,
    operation_client_su_write_direct,
    operation_client_s_write_direct,
    operation_client_type,
    operation_client_write,
    operation_client_read,
    ds_task_type,
    cl_task_type,
};

enum tasktypes {
    send_received,
    send_prepare,
    send_docommit,
    send_result,
    send_spn_read,
    send_spn_write,
    received_spn_write_s,
    received_spn_write_su,
    received_spn_directwrite_su,
    received_spn_read,
    received_prepare,
    received_cancommit,
    received_docommit,
    received_received,
    received_committed,
    received_result,
    send_ccc_stripewrite_cancommit,
    received_ccc_stripewrite_cancommit,
    prim_docommit,
    dstask_write_fo,
    regendpoint,
    maintenance_garbagecollection,
    dspingpong,
};

enum opstatus {
    opstatus_init,
    opstatus_prepare,
    opstatus_cancommit,
    opstatus_docommit,
    opstatus_committed,
    opstatus_success,
    opstatus_failure,
    clop_init,
    clop_sent,
    clop_failure,
    clop_success,
    clop_intermediate_alpha,
};

struct Participants_bf {
   unsigned start:2; // start tag, always 3 (11)
   unsigned su0 : 1;
   unsigned su1 : 1;  
   unsigned su2 : 1;
   unsigned su3 : 1;  
   unsigned su4 : 1;
   unsigned su5 : 1;  
   unsigned su6 : 1;
   unsigned su7 : 1;  
   unsigned su8 : 1;
   unsigned su9 : 1;  
   unsigned su10: 1;
   unsigned su11: 1;
   unsigned end : 2; // end tag. always 0 (00)
};

 struct OPHead {
    struct CCO_id       cco_id;
    InodeNumber         inum;
    uint64_t            offset;
    uint64_t            length;
    optype              type;
    uint8_t             subtype;
    uint32_t            stripecnt;
    opstatus            status;
    char                filelayout[256];
};

struct dstask_head {
    struct OPHead               ophead;
};

struct dstask_ccc_smallwrite {
    struct dstask_head          dshead;
    serverid_t                  receiver;
    struct Participants_bf      participants;
};


struct dstask_spn_send_read {
    struct dstask_head          dshead;
    serverid_t                  receiver;
    StripeId                    stripeid;
    StripeUnitId                stripeunitid;
    size_t                      offset;
    size_t                      end;
};

struct dstask_ccc_send_received {
    struct dstask_head          dshead;
    serverid_t                  receiver;
    struct Participants_bf      participants;
};

struct dstask_ccc_send_prepare{
    struct dstask_head          dshead;
    serverid_t                  receiver;
};
struct dstask_ccc_recv_prepare{
    struct dstask_head          dshead;
    serverid_t                  sender;
};

struct dstask_spn_recv {
    struct dstask_head          dshead;
    struct Participants_bf      participants;    
    StripeId                    stripeid;
    size_t                      length;
    void                        *data;
};

struct dstask_endpointreg       {
    struct dstask_head          dshead;
    char                        *ip;
};

struct dstask_writetodisk {
    struct dstask_head          dshead;
    struct operation_primcoordinator *p_op;
};

struct dstask_write_fileobject {
    struct dstask_head          dshead;
    struct datacollection_primco *data_col;
};

struct dstask_proccess_result {
    struct dstask_head          dshead;
};

struct dstask_cancommit {
    struct dstask_head          dshead;
    void                        *recv;
    size_t                      length;
    time_t                      recv_ts;
    struct Participants_bf      recv_from;
    StripeId                    stripeid;
    uint32_t                    version;
};

struct dstask_ccc_recv_committed{
    struct dstask_head          dshead;
    serverid_t                  sender;
    struct Participants_bf      recv_from;
    time_t                      recv_ts;
};

struct dstask_ccc_send_result{
    struct dstask_head          dshead;
    serverid_t                  receiver;
};

struct dstask_ccc_received {
    struct dstask_head      dshead;
    //serverid_t              sender;
    struct Participants_bf      recv_from;
    struct Participants_bf  participants;
    StripeId                stripeid;
};
        

struct dstask_ccc_stripewrite_cancommit {
    struct dstask_head          dshead;
    serverid_t                  receiver;
    uint32_t                    version;
};

struct dstask_ccc_stripewrite_cancommit_received {
    struct dstask_head          dshead;
    serverid_t                  receiver;
    uint32_t                    version;
    struct Participants_bf      recv_from;
};


struct operation_dstask_docommit {
    struct dstask_head          dshead;
    serverid_t                  receiver;
    struct Participants_bf      participants;
    struct vvecmap              vvmap;
};

struct dstask_pingpong {
    struct dstask_head          dshead;
    serverid_t                  sender;
    uint32_t                    counter;
};

struct dstask_maintenance_gc {
    struct dstask_head          dshead;    
//    time_t                      next_due;
};

struct operation_participant {
    struct OPHead                                       ophead;
    std::map<StripeId,struct dataobject_collection*>    *datamap;
    time_t                                              recv_timestamp;
    struct Participants_bf                              participants;
    serverid_t                                          primcoordinator;
    bool                                                iamsecco;
    bool                                                isready;
    uint16_t                                            stripecnt;
    uint16_t                                            stripecnt_recv;    
};

struct operation_primcoordinator {
    struct OPHead                                       ophead;
    time_t                                              recv_timestamp;
    size_t                                              datalength;
    struct Participants_bf                              participants;
    struct Participants_bf                              received_from;
    pthread_mutex_t                                     mutex_private;
    std::map<StripeId,struct datacollection_primco*>    *datamap_primco;
};


struct operation_client {
    struct OPHead                               ophead;
    time_t                                      recv_timestamp;
    struct Participants_bf                      participants;
    std::map<uint32_t,struct operation_group *> *groups;         
};

struct operation_composite {
    struct OPHead                             ophead;
    std::map<uint32_t,operation*>             *ops;
};

struct operation_client_write {
    struct OPHead                               ophead;
    time_t                                      recv_timestamp;
    struct Participants_bf                      participants;
    struct operation_group                      *group;    
};

struct operation_client_read {
    struct OPHead                               ophead;
    time_t                                      recv_timestamp;
    std::map<uint32_t,struct operation_group *> *groups;         
    void                                        **resultdata;
    bool                                        isready;
};

struct operation_group {
    StripeId                                    stripe_id;
    struct Participants_bf                      participants;
    std::map<StripeUnitId,struct StripeUnit*>   *sumap;
    bool                                        isready;
};



union operation {
    struct operation_participant        participant;
    struct operation_primcoordinator    primcoord;
    struct operation_client             client;
    struct dstask_ccc_smallwrite             dstask;
    struct operation_client_write       client_write_op;
    struct operation_client_read        client_read_op;
};


#endif	/* OPDATA_H */

