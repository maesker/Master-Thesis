/* 
 * File:   MdsSessionManager.cpp
 * Author: markus
 * 
 * Created on 11. Januar 2012, 01:58
 */

#include <map>

#include "mm/mds/MdsSessionManager.h"

MdsSessionManager::MdsSessionManager() {
    idgen = new UniqueIdGenerator();
    idgenmutex = PTHREAD_MUTEX_INITIALIZER;
    smap_mutex = PTHREAD_MUTEX_INITIALIZER;
    
}

MdsSessionManager::MdsSessionManager(const MdsSessionManager& orig) {
}

MdsSessionManager::~MdsSessionManager() {
}

/** 
 @todo check for conflict if sessionid
 */
ClientSessionId MdsSessionManager::addSession(gid_t g, uid_t u)
{
    ClientSessionId scid = getNewId();
    //printf("Sesman:%llu.\n",scid);
    struct session_entry *p_se = (struct session_entry*) malloc(sizeof(struct session_entry));
    p_se->gid = g;
    p_se->uid = u;
    p_se->last_contact = time(NULL);
    pthread_mutex_lock(&smap_mutex);
    sessionmap.insert(std::pair<ClientSessionId,struct session_entry*>(scid,p_se));
    pthread_mutex_unlock(&smap_mutex);
    return scid;            
}

ClientSessionId MdsSessionManager::getNewId()
{
    ClientSessionId scid;
    pthread_mutex_lock(&idgenmutex);
    scid = idgen->get_next_id();
    pthread_mutex_unlock(&idgenmutex);
    return scid;
}