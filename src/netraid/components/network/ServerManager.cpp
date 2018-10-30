/* 
 * File:   ServerManager.cpp
 * Author: markus
 * 
 * Created on 13. Januar 2012, 20:16
 */

#include <map>

#include "components/network/ServerManager.h"


ServerManager::ServerManager(Logger *p_log, uint16_t port, bool dynport) {
    this->p_smap = new std::map<serverid_t,server_map_entry*>;
    this->server_cnt = 0;
    this->baseport = port;
    this->log = p_log;
    this->dynport=dynport;
    mutex = PTHREAD_MUTEX_INITIALIZER;
}

ServerManager::ServerManager(Logger *p_log, uint16_t port) {
    this->p_smap = new std::map<serverid_t,server_map_entry*>;
    this->server_cnt = 0;
    this->baseport = port;
    this->log = p_log;
    this->dynport=true;
    mutex = PTHREAD_MUTEX_INITIALIZER;
}

ServerManager::ServerManager(const ServerManager& orig) {
}

ServerManager::~ServerManager() 
{
    log->debug_log("Shutting down");
    std::map<serverid_t,server_map_entry*>::iterator it = p_smap->begin();
    for(it;it!=p_smap->end();it++)
    {
        //free(it->second->asio_sock);
        delete it->second;
    }
    delete p_smap;
    pthread_mutex_destroy(&mutex);
    log->debug_log("done");
}


int ServerManager::register_server(serverid_t id)
{
    return register_server(id, "\0", 1);
}
int ServerManager::register_server(serverid_t id, char *ip)
{
    return register_server(id, ip, 2);
}

int ServerManager::register_server(serverid_t id, char *ip, uint8_t status)
{
    log->debug_log("id:%u, ip:%s, status:%u...",id,ip,status);
    int rc=-1;
    pthread_mutex_lock(&this->mutex);
    log->debug_log("got mutex");
    if (!this->exists(id))
    {
        struct server_map_entry *new_entry = new struct server_map_entry;
        new_entry->status = status;    
        new_entry->asio_sock = NULL;
        new_entry->sockmutex = PTHREAD_MUTEX_INITIALIZER;
        snprintf(&new_entry->ip[0], sizeof(new_entry->ip), "%s",ip);

        std::pair<std::map<serverid_t,struct server_map_entry*>::iterator,bool> ret;

        ret = this->p_smap->insert(std::pair<serverid_t,server_map_entry*>(id, new_entry));
        if (ret.second)
        {
            rc=0;
            server_cnt++;
            log->debug_log("insert ok.");
        }
        else
        {
            log->error_log("ret second:%u",ret.second);
            rc=0;
        }
    }
    else
    {
        log->debug_log("id:%u already registered.",id);
        rc=0;
    }
    pthread_mutex_unlock(&this->mutex);
    log->debug_log("rc=%d",rc);
    return rc;
}

uint32_t ServerManager::get_server_count(uint32_t *count)
{
    *count = server_cnt;
    log->debug_log("cnt:%u",server_cnt);
    return 0;
}

uint32_t ServerManager::get_server_address(serverid_t *id, ipaddress_t *dest, uint16_t *port)
{
    uint32_t rc =1;
    log->debug_log("start: get address of id %u",*id);
    pthread_mutex_lock(&mutex);
    std::map<serverid_t,server_map_entry*>::iterator it = this->p_smap->find(*id);
    if (it != this->p_smap->end())
    {
            strncpy(dest[0],&it->second->ip[0],sizeof(ipaddress_t));
            if (this->dynport)
            {
                *port = this->baseport+(*id);
            }
            else
            {
                *port = this->baseport;
            }
            rc = 0;
            log->debug_log("address:%s.",dest[0]);
    }
    else
    {
        log->debug_log("no such address.");
    }
    pthread_mutex_unlock(&mutex);
    log->debug_log("rc:%u",rc);
    return rc;
}

uint32_t ServerManager::get_ids(std::vector<serverid_t> *asyn_tcp_server)
{
    uint32_t rc=1;
    log->debug_log("start");
    pthread_mutex_lock(&mutex);
    for (std::map<serverid_t,server_map_entry*>::iterator it=this->p_smap->begin(); it!=this->p_smap->end(); it++)
    {
        log->debug_log("serverid:%u\n",it->first);
        asyn_tcp_server->push_back(it->first);
        rc=0;
    }
    pthread_mutex_unlock(&mutex);
    log->debug_log("rc:%u",rc);
    return rc;
}

uint32_t ServerManager::print_server()
{
    log->debug_log("start");
    for (std::map<serverid_t,server_map_entry*>::iterator it=this->p_smap->begin(); it!=this->p_smap->end(); it++)
    {
        log->debug_log("Serverid:%u, Address:%s",it->first, it->second->ip);
    }
    log->debug_log("done");
}


uint32_t ServerManager::register_socket(serverid_t *servid, boost::asio::ip::tcp::socket *sock)
{    
    uint32_t rc = -1;
    pthread_mutex_lock(&mutex);
    //log->debug_log("servid:%u, sockid:%u",*servid);
    std::map<serverid_t,server_map_entry*>::iterator it = this->p_smap->find(*servid);
    
    server_map_entry *p_sment = NULL; 
    if (it == this->p_smap->end())
    {
        log->debug_log("new entry for %u.",*servid);
        p_sment = new server_map_entry;
        p_sment->sockmutex = PTHREAD_MUTEX_INITIALIZER;
        this->p_smap->insert(std::pair<serverid_t,server_map_entry*>(*servid,p_sment));
        rc=0;
    }
    else
    {
        p_sment = it->second;
        log->debug_log("replace existing for %u.",*servid);
        rc=0;
    }
    p_sment->asio_sock = sock;
    pthread_mutex_unlock(&mutex);
    log->debug_log("rc:%d",rc);
    return rc;
}

server_map_entry* ServerManager::get_server_entry(serverid_t *id)
{
    server_map_entry *p_res = NULL;
    pthread_mutex_lock(&mutex);
    std::map<serverid_t,server_map_entry*>::iterator it = this->p_smap->find(*id);
    if (it != this->p_smap->end())
    {
        p_res = it->second;
        log->debug_log("id:%u found",*id);
    }
    else
    {
            log->debug_log("not found");
    }
    
    pthread_mutex_unlock(&mutex);
    return p_res;
}

boost::asio::ip::tcp::socket* ServerManager::get_socket(serverid_t *servid)
{
    log->debug_log("servid:%u",*servid);
    boost::asio::ip::tcp::socket *p_socket = NULL;
    server_map_entry *p_res = this->get_server_entry(servid);
    if (p_res!=NULL) 
    {
        p_socket=p_res->asio_sock;
        log->debug_log("entry found.");
    }
    return p_socket;
}

bool ServerManager::exists(serverid_t id)
{
    bool rc=true;
    std::map<serverid_t,server_map_entry*>::iterator it;
    it = this->p_smap->find(id);    
    if (it == this->p_smap->end())
    {
        rc=false;
    }
    else
    {        
        if (it->second->status==2)
        {
           // rc=false;
        }
    }
    log->debug_log("exists id:%u = %s",id,(rc)?"true":"false");
    return rc;
}