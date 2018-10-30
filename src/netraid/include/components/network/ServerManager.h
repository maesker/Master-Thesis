/* 
 * File:   ServerManager.h
 * Author: markus
 *
 * Created on 13. Januar 2012, 20:16
 */

#ifndef SERVERMANAGER_H
#define	SERVERMANAGER_H

#include <map>
#include <stdlib.h>
#include <vector>
#include <pthread.h>
#include <cstring>

#include "global_types.h"
#include "custom_protocols/global_protocol_data.h"
#include "logging/Logger.h"
#include <boost/asio.hpp>
#include <boost/array.hpp>

struct server_map_entry {
    ipaddress_t ip;
    //std::map<uint64_t id,boost::asio::ip::tcp::socket*> *asio_sock;
    uint8_t status;
    boost::asio::ip::tcp::socket *asio_sock;
    pthread_mutex_t sockmutex;
    //uint16_t port;
    //uint16_t storage_port;
    //uint16_t control_port;
    //uint16_t mds_port;
};


class ServerManager {
public:
    ServerManager(Logger *p_log,uint16_t port, bool dynport);
    ServerManager(Logger *p_log,uint16_t port);
    ServerManager(const ServerManager& orig);
    virtual ~ServerManager();
    uint32_t print_server();
    
    int register_server(serverid_t id);
    int register_server(serverid_t id, char *ip);
    int register_server(serverid_t id, char *ip, uint8_t status);
    uint32_t get_server_count(uint32_t *count);
    uint32_t get_server_address(serverid_t *id, ipaddress_t *dest, uint16_t *port);
    bool     exists(serverid_t id);
    uint32_t get_ids(std::vector<serverid_t> *asyn_tcp_server);
    uint32_t register_socket(serverid_t *servid, boost::asio::ip::tcp::socket *sock);
    boost::asio::ip::tcp::socket* get_socket(serverid_t *id);
    server_map_entry* get_server_entry(serverid_t *id);
    
private:
    Logger *log;
    std::map<serverid_t,struct server_map_entry*> *p_smap;
    uint32_t server_cnt;
    uint16_t baseport;
    bool dynport;    
    pthread_mutex_t mutex;
    
    
};

#endif	/* SERVERMANAGER_H */

