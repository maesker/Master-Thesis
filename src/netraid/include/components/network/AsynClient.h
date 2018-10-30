/* 
 * File:   AsynClient.h
 * Author: markus
 *
 * Created on 24. Januar 2012, 11:39
 */

#ifndef ASYNCLIENT_H
#define	ASYNCLIENT_H

#include <iostream>
#include <pthread.h>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include "components/network/ServerManager.h"
#include "custom_protocols/global_protocol_data.h"
#include "logging/Logger.h"

using boost::asio::ip::tcp;

template<class data> class AsynClient {
public:
    AsynClient(Logger *p_log, ServerManager *sm)
    {
        log = p_log;
        p_sm = sm;
        sockmutex = PTHREAD_MUTEX_INITIALIZER;
    }
    
    AsynClient(const AsynClient& orig)
    {
    }

    virtual ~AsynClient()
    {
    }
    
    int send(data *p_msg, serverid_t id)
    {
        int rc=0;
        int retry=0;
        server_map_entry *p_entry ;
        //pthread_mutex_lock(&sockmutex);
        while (retry<4)
        {            
            try
            {
                //log->debug_log("send to id:%u.msg:%p",id,p_msg);
                struct custom_protocol_reqhead_t  *p_customhead = (struct custom_protocol_reqhead_t*)p_msg;
                //boost::asio::ip::tcp::socket* p_sock= this->p_sm->get_socket(&id);
                p_entry = p_sm->get_server_entry(&id);
                if (p_entry==NULL)
                {
                    log->debug_log("Error. no socket");
                    return -1;
                }
                if (p_entry->asio_sock==NULL)
                {
                    rc = connect(&id);
                    if (!rc)
                    {
                        p_entry = this->p_sm->get_server_entry(&id);
                        //log->debug_log("psock = %p.",p_sock);
                    }
                    else
                    {
                        log->debug_log("connect failded.");
                    }
                }
                else
                {
                    log->debug_log("socket already connected.");
                }
                if (p_entry==NULL || rc!=0)
                {
                    log->warning_log("Socket error.");
                }
                else
                {
                    log->debug_log("start sending...");
                    pthread_mutex_lock(&p_entry->sockmutex);
                    if (p_customhead->datablock==NULL) p_customhead->datalength=0;
                    size_t sent;
                    sent = boost::asio::write(*p_entry->asio_sock, boost::asio::buffer(p_msg, sizeof(data)));
                    log->debug_log("headsize: %u, sent size:%u.", sizeof(data),sent);
                    if (sent==sizeof(data))
                    {
                        if (p_customhead->datablock!=NULL)
                        {
                            log->debug_log("Send data block:size:%llu, pointer:%p.",p_customhead->datalength,p_customhead->datablock);
                            sent = boost::asio::write(*p_entry->asio_sock, boost::asio::buffer(p_customhead->datablock, p_customhead->datalength));
                            log->debug_log("Sent:%llu",sent);
                            rc = 0;
                            if (sent!=p_customhead->datalength)
                            {
                                rc=-1;
                                log->warning_log("send-length missmatch");
                            }
                            //else
                            //{
                            //    log->debug_log("sent:%u: OK",sent);
                            //}
                        }
                        else
                        {
                            log->debug_log("No datablock attached. return ok");
                            rc=0;
                        }
                    }
                    else
                    {
                        log->debug_log("Error, head sent length missmatch:%u-%u",sent,sizeof(data));
                        rc=-1;
                    }
                    pthread_mutex_unlock(&p_entry->sockmutex);
                    //p_sock->close();
                }
                break;
            }
            catch(...)
            {
                log->error_log("ERROR:send failed...");
                retry++;
                pthread_mutex_unlock(&p_entry->sockmutex);
                delete p_entry->asio_sock;
                p_entry->asio_sock=NULL;
            }
        }
        //pthread_mutex_unlock(&sockmutex);
        log->debug_log("done:%d.",rc);
        return rc;
    }

        
private:
        boost::asio::io_service io_service;
        ServerManager *p_sm;
        Logger *log;
        pthread_mutex_t sockmutex;
        
        int connect(serverid_t *servid)
        {
            int rc=0;
            try
            {                
                log->debug_log("sid:%u.",*servid);
                boost::asio::io_service io_service;
                tcp::resolver resolver(io_service);

                ipaddress_t dest;
                uint16_t port;
                this->p_sm->get_server_address(servid, &dest,&port);            
                char *port_str = (char *) malloc(8);
                snprintf(port_str,8, "%u", port);            
                log->debug_log("dest=%s, port:%s",dest,port_str);

                tcp::resolver::query query(tcp::v4(),&dest[0], port_str);
                tcp::resolver::iterator iterator = resolver.resolve(query);
                tcp::socket *s = new tcp::socket(io_service);
                s->connect(*iterator);
                this->p_sm->register_socket(servid,s);
                free(port_str);
                log->debug_log("done:rc=%d",rc);
            }
            catch(...)
            {
                log->debug_log("Could not connect to id :%u",*servid);
                rc=-1;
            }
                    
            return rc;
        }
};

#endif	/* ASYNCLIENT_H */

