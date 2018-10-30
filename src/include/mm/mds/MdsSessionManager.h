/* 
 * File:   MdsSessionManager.h
 * Author: markus
 *
 * Created on 11. Januar 2012, 01:58
 */

#ifndef MDSSESSIONMANAGER_H
#define	MDSSESSIONMANAGER_H

#include <stdlib.h>
#include <map>
#include <time.h>
#include <pthread.h>

#include "global_types.h"
#include "coco/communication/UniqueIdGenerator.h"


struct session_file_locks_t {
    // erst den lockmanager implementieren.
};

struct session_entry {
    gid_t gid;
    uid_t uid;
    time_t last_contact;
    //std::map<InodeNumber,session_file_locks_t*> files;  
};

class MdsSessionManager {
public:
    MdsSessionManager();
    MdsSessionManager(const MdsSessionManager& orig);
    virtual ~MdsSessionManager();
    
    
    ClientSessionId addSession(gid_t g, uid_t u);
    
private:
    std::map<ClientSessionId,struct session_entry*> sessionmap;
    UniqueIdGenerator *idgen;
    ClientSessionId getNewId();
    pthread_mutex_t idgenmutex, smap_mutex ;
};

#endif	/* MDSSESSIONMANAGER_H */

