/* 
 * File:   SocketManager.h
 * Author: markus
 *
 * Created on 9. Januar 2012, 16:59
 */

#ifndef SOCKETMANAGER_H
#define	SOCKETMANAGER_H

#include <map>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include "pthread.h"

#include "custom_protocols/global_protocol_data.h"


struct serverstruct_t
{
    char ip[32];
    uint16_t port;
};

template <typename sock>
class SocketManager {  
    
public: 
    
    std::map<serverid_t,sock*> socketmap;      
    SocketManager()
    {
        globallock_mutex = PTHREAD_MUTEX_INITIALIZER;
    };
    
    ~SocketManager()
    {
        pthread_mutex_destroy(&globallock_mutex);        
        std::map<serverid_t,struct serverstruct_t*>::iterator it2 = servermap.begin();
        for (it2; it2!=servermap.end(); it2++)
        {
            free(it2->second);
        }
    };
    
    int addServer(serverid_t id, std::string ip, uint16_t port)
    {  
        struct serverstruct_t *s = (struct serverstruct_t*) malloc(sizeof(struct serverstruct_t));
        memset(&s->ip[0],0,32);
        memcpy(&s->ip[0], ip.c_str(),ip.size());
        s->port = port;
        servermap.insert(std::pair<serverid_t,struct serverstruct_t*>(id,s) );
        return 0;
    };
    
    uint64_t getServerNumber()
    {
        return servermap.size();
    }
    
    int getServer(serverid_t id, struct serverstruct_t *t)
    {
        std::map<serverid_t,struct serverstruct_t*>::iterator it = servermap.find(id);
        *t = *it->second;
        return 0;
    }
    
    void addSocket(serverid_t id, sock *s) 
    {  
        //std::pair<std::map<socketid_t,T>::iterator,bool> ret;
        socketmap.insert(std::pair<serverid_t,sock*>(id,s) );
    };
    
    sock* getSocket(serverid_t id) {
        //id = socketmap.find(id);
        pthread_mutex_lock(&globallock_mutex);
        //std::map<serverid_t,sock*>::iterator it = socketmap.find(id);
        //printf("Get:%u\n",id);
        sock *p = socketmap.find(id)->second;
        if (p)
        {
                pthread_mutex_lock(&p->mutex);
        }
        else
        {
            p=NULL;
        }
        pthread_mutex_unlock(&globallock_mutex);
        return p;
    }    

    std::string getTcpAddress(serverid_t id)
    {
        std::string address = std::string("tcp://");
        struct serverstruct_t *s = servermap.find(id)->second;
        address.append(&s->ip[0]);
        address.append(":");
        
        char *c = (char *) malloc(8);
        memset(c,0,8);
        sprintf(c,"%d",s->port);
        address.append(c);
        free(c);
        return address;
    }
    
private:
    
    std::map<serverid_t,struct serverstruct_t*> servermap;
    
    pthread_mutex_t globallock_mutex;
};



#endif	/* SOCKETMANAGER_H */

