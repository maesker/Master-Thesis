#ifndef MESSAGEROUTER_H
#define MESSAGEROUTER_H

/** 
 * @file MessageRouter.h
 * @author Markus Maesker, maesker@gmx.net
 * @date 20.12.11
 * */

#include <iostream>
#include <sstream>
#include <zmq.hpp>
#include <zmq.h>
#include <string>
#include <stdlib.h>

#include "abstracts/AbstractComponent.h"
#include "global_types.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

#include "customExceptions/ComponentException.h"
#include "exceptions/MDSException.h"
#include "custom_protocols/global_protocol_data.h"

#define MR_CONF_FILE DEFAULT_CONF_DIR "/" "messagerouter.conf"




class MessageRouter: public AbstractComponent
{
public:
    explicit MessageRouter(std::string basedir);
    int32_t run();
    int32_t stop();
    void set_id(serverid_t id);
    void set_port(uint16_t port);
    void set_frontend(std::string s);
    void set_backend(std::string s);

    
    std::string front;
    std::string back;
    
private:
    pthread_t worker_thread;
    serverid_t id;
    uint16_t port;
};

#endif // MESSAGEROUTER_H
