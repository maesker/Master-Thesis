/* 
 * File:   CCCdata.h
 * Author: markus
 *
 * Created on 17. Januar 2012, 22:24
 */

#ifndef CCCDATA_H
#define	CCCDATA_H

#include <sstream>
#include <string.h>
#include <stdlib.h>
#include "custom_protocols/global_protocol_data.h"
#include "components/OperationManager/OpData.h"

struct CCCHead {
    struct custom_protocol_reqhead_t    customhead;
    serverid_t                          receiver;
    struct Participants_bf              sender_bf;
    //struct OPHead       ophead;      
};


struct CCC_cl_pingpong {
        struct CCCHead          head;
        struct OPHead           ophead;
        serverid_t              sender;
        uint32_t                counter;        
};


struct CCC_cl_stripewrite_cancommit {
        struct CCCHead          head;
        struct OPHead           ophead;
        serverid_t              sender;
        StripeId                stripeid;
        uint32_t                version;
};


struct CCC_cl_smallwrite {
        struct CCCHead          head;
        struct OPHead           ophead;
        serverid_t              sender;
        struct Participants_bf  participants;
        StripeId                stripeid;
};

struct CCC_prepare {
        struct CCCHead          head;
        struct OPHead           ophead;
};

struct CCC_cancommit {
        struct CCCHead          head;
        struct OPHead           ophead;
        uint32_t                nextversion;
        StripeId                stripeip;
        
};
struct CCC_result{
        struct CCCHead          head;
        struct OPHead           ophead;
};

struct CCC_docommit {
        struct CCCHead          head;
        struct OPHead           ophead;
};

struct CCC_committed {
        struct CCCHead          head;
        struct OPHead           ophead;
        //StripeId                stripeid;        
        struct operation_participant       *partop;
};


union CCC_message {
    struct CCC_cl_smallwrite    cl_smallwrite;
    struct CCC_prepare          send_prepare;
    struct CCC_docommit         send_docommit;
};

void print_CCCHead(struct CCCHead *p_head, std::stringstream& ss);
void print_CCC_Message(CCC_message *p_msg, std::stringstream& ss);
struct CCC_cl_smallwrite* create_CCC_cl_smallwrite(struct dstask_ccc_smallwrite *p_task);
#endif	/* CCCDATA_H */

