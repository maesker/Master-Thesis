#ifndef MESSAGEROUTER_H
#define MESSAGEROUTER_H

/** 
 * @file MessageRouter.h
 * @author Markus Maesker, maesker@gmx.net
 * @date 15.07.11
 * */
#include <iostream>
#include <sstream>
#include <zmq.hpp>
#include <zmq.h>

#include "exceptions/MDSException.h"
#include "communication.h"
#include "mm/mds/AbstractMdsComponent.h"
#include "global_types.h"


#define MR_CONF_FILE DEFAULT_CONF_DIR "/" "messagerouter.conf"

class MessageRouter: public AbstractMdsComponent
{
public:
    explicit MessageRouter(std::string basedir);
    int32_t run();
    int32_t stop();

private:
    pthread_t worker_thread;
};

#endif // MESSAGEROUTER_H
