/* 
 * File:   CPNetraid.h
 * Author: markus
 *
 * Created on 10. Januar 2012, 18:51
 */

#ifndef CPNETRAID_H
#define	CPNETRAID_H

#include "custom_protocols/IProtocol.h"
#include "custom_protocols/global_protocol_data.h"
#include "mm/mds/ByterangeLockManager.h"
#include "custom_protocols/control/CPdata.h"

#include "server/DataServer/DataServer.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <cstring>


class CPNetraid : public IProtocol{
public:
    CPNetraid();
    CPNetraid(const CPNetraid& orig);
    virtual ~CPNetraid();
    
    uint32_t    get_max_msg_len();
     
    size_t      handle_request(custom_protocol_reqhead_t *head, void *req, void *resp);
    PET_MINOR   handle_helloworld(struct CPN_HelloWorld_req_t *p_req, struct CPN_HelloWorld_resp_t *p_resp );
    void        register_dataserver(DataServer *p_ds);
    
private:
    DataServer *p_ds; 
};


#endif	/* CPNETRAID_H */

