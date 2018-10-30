/* 
 * File:   IProtocol.h
 * Author: markus
 *
 * Created on 6. Januar 2012, 11:35
 */

#ifndef IPROTOCOL_H
#define	IPROTOCOL_H


#include "stdint.h"
#include <cstring>

#include "custom_protocols/global_protocol_data.h"


class IProtocol{
public:
    virtual ~IProtocol(){};
    virtual uint32_t    get_max_msg_len() = 0;
    custom_protocol_ids get_protocol_id() {return protocol_id;}
    
//    virtual size_t      handle_request(custom_protocol_reqhead_t *head, void *req, void *resp) = 0;    
    
protected:
    uint32_t maximal_message_length;
    custom_protocol_ids protocol_id;
};
#endif	/* IPROTOCOL_H */

