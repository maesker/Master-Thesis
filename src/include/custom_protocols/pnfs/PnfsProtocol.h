/** 
 * @file   PnfsProtocol.h
 * Author: markus
 *
 * Created on 6. Januar 2012, 11:08
 */

#ifndef PNFSPROTOCOL_H
#define	PNFSPROTOCOL_H

#include "global_types.h"
#include "mm/mds/MetadataServer.h"
#include "custom_protocols/pnfs/Pnfs_data.h"
#include "custom_protocols/global_protocol_data.h"
#include "custom_protocols/IProtocol.h"
#include "mm/mds/MdsSessionManager.h"
#include "logging/Logger.h"



class PnfsProtocol : public IProtocol{
public:
    PnfsProtocol();
    PnfsProtocol(const PnfsProtocol& orig);
    virtual ~PnfsProtocol();
    
    uint32_t    get_max_msg_len();
    
    size_t      handle_request(custom_protocol_reqhead_t *head, void *req, void *resp);
    PET_MINOR   handle_helloworld(struct PNFS_HelloWorld_req_t* , struct PNFS_HelloWorld_resp_t* );
        
private:
        MetadataServer *p_mds;
        Logger *log;
};



#endif	/* PNFSPROTOCOL_H */

