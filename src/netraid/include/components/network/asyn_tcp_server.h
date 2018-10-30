/* 
 * File:   asyn_tcp_server.h
 * Author: markus
 *
 * Created on 29. Januar 2012, 14:53
 */

#ifndef ASYN_TCP_SERVER_H
#define	ASYN_TCP_SERVER_H

#include <cstdlib>
#include "pthread.h"
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include "logging/Logger.h"

#include "coco/communication/ConcurrentQueue.h"
#include "custom_protocols/global_protocol_data.h"

using boost::asio::ip::tcp;

static ConcurrentQueue<void*> *p_workerqueue= new ConcurrentQueue<void*>;

struct sockdata
{
    void (*cb)(void *);
    tcp::socket *p_socket;
    bool killthread;
    bool threaded;
};

template<class data2>class session
{
public:
    
  void (*pushCallback)(void*);
  session(boost::asio::io_service& io_service ,
          void (*cb)(void *),
          Logger *p_log)
    : socket_(io_service)
  {
        pushCallback = cb;
        log=p_log;
        p_s = new sockdata;
        p_s->cb = pushCallback;
        p_s->p_socket = &socket_;
        p_s->threaded=false;
        p_s->killthread = false;
        
        /*struct timeval tv;
        tv.tv_sec  = 1; 
        tv.tv_usec = 0;         
        setsockopt(socket().native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(socket().native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
*/
  }

  ~session()
  {
      log->debug_log("Shuting down session");
      p_s->killthread=true;
      log->debug_log("done.");
      if (p_s->p_socket->is_open()) p_s->p_socket->close();
      log->debug_log("done.");
      if (p_s->threaded)
      {
          //pthread_cancel(worker_thread);
          //pthread_join(worker_thread,NULL);
      }
      log->debug_log("done.");
      delete p_s;
      log->debug_log("done.");
  }
  
  tcp::socket& socket()
  {
      return socket_;
  }

  void start()
  {    
      data2 *p_head = new data2;
      boost::asio::read(socket_, boost::asio::buffer(p_head, sizeof(data2)));
      struct custom_protocol_reqhead_t  *p_customhead = (struct custom_protocol_reqhead_t*)p_head;

      boost::asio::ip::tcp::endpoint endpoint = socket_.remote_endpoint();
      p_customhead->ip = (char *)malloc(32);
      sprintf(p_customhead->ip,"%s",endpoint.address().to_string().c_str());

      p_customhead->datablock = malloc(p_customhead->datalength);
      boost::asio::read(
            socket_, 
            boost::asio::buffer(p_customhead->datablock,p_customhead->datalength));

      pushCallback(p_head);
      log->debug_log("done reading");      
      pthread_create(&worker_thread, NULL,handle_write ,p_s);
  }
  
  
  static void* handle_write(void*obj)
  {
      struct sockdata *p_s = (struct sockdata * ) obj;
      p_s->threaded=true;
      try
      {
          while(!p_s->killthread)
          {
            //printf("session handle write\n");
            //if (!error)
            //{
                
                data2 *p_head = new data2;
                boost::asio::read(*p_s->p_socket, boost::asio::buffer(p_head, sizeof(data2)));
                struct custom_protocol_reqhead_t  *p_customhead = (struct custom_protocol_reqhead_t*)p_head;

                p_customhead->datablock = malloc(p_customhead->datalength);
                p_customhead->ip=NULL;
                boost::asio::read(
                        *p_s->p_socket, 
                        boost::asio::buffer(p_customhead->datablock,p_customhead->datalength));
//                socket_.async_read_some(boost::asio::buffer(p_customhead->datablock,p_customhead->datalength),
//                    boost::bind(&session::handle_read, this,
//                    boost::asio::placeholders::error,
//                    boost::asio::placeholders::bytes_transferred));
            //    log->debug_log("Received: stored at:%p.\n",p_customhead->datablock);
                //session::pushCallback(p_head);
                //if (p_customhead->datalength==4) printf("Result:%u\n",*(uint32_t*)p_customhead->datablock);
                    
                p_s->cb(p_head);
                //p_workerqueue->push((void*)p_head);
                //printf("CB:done\n");
           /*/ }
            else
            {
              delete this;
            }**/
          }
      }
      catch (boost::system::system_error &e) 
      { 
        /*boost::system::error_code ec = e.code(); 
        std::cerr << ec.value() << std::endl; 
        std::cerr << ec.category().name() << std::endl; **/
        /*if (p_s->p_socket->is_open())
          {
              p_s->p_socket->cancel();
              p_s->p_socket->close();        
          }*/
      } /*
      catch(...)
      {
          
          if (p_s->p_socket->is_open())
          {
              p_s->p_socket->cancel();
              p_s->p_socket->close();        
          }
      }*/
      pthread_exit(0);
  }

private:
  tcp::socket socket_;
  pthread_t worker_thread;
  struct sockdata *p_s; 
  Logger *log;
};

template<class data> class asyn_tcp_server
{
public:
  void (*pushCallback)(void *);
  asyn_tcp_server(boost::asio::io_service& io_service, short port, void (*cb)(void *), Logger *p_log)
    : io_service_(io_service),
      acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
    {
        shutdown=true;
        mutex = PTHREAD_MUTEX_INITIALIZER;
        p_vec = new std::vector< session<data>*>();
        //pthread_t worker_thread;
        //pthread_create(&worker_thread, NULL,queueconnect , cb );
        this->log=p_log;
        boost::asio::socket_base::reuse_address option(true);
        acceptor_.set_option(option);
        pushCallback = cb;
        session<data> *new_session = new session<data>(io_service_,pushCallback,log);
        p_vec->push_back(new_session);
       // printf("pushed:%p\n",new_session);
        acceptor_.async_accept(new_session->socket(),
            boost::bind(&asyn_tcp_server::handle_accept, this, new_session,
              boost::asio::placeholders::error));
    }
    
  void handle_accept(session<data>* new_session,
      const boost::system::error_code& error)
  {
    if (!error)
    {
      log->debug_log("session pointer %p.",new_session);
      new_session->start();
      new_session = new session<data>(io_service_, pushCallback,log);
      p_vec->push_back(new_session);
     // printf("pushed:%p\n",new_session);
      acceptor_.async_accept(new_session->socket(),
          boost::bind(&asyn_tcp_server::handle_accept, this, new_session,
            boost::asio::placeholders::error));      
    }
    else
    {
      //log->debug_log("delete session");
      //delete new_session;
    }
  }
  
  void setLogger(Logger *p_log){
      this->log = p_log;
  }
  
  ~asyn_tcp_server()
  {
      pthread_mutex_lock(&mutex);
      if (shutdown)
      {
          shutdown=false;
         // printf("Shuting down.: vec:%u\n",p_vec->size());
          log->debug_log("Shuting down.: size:%u",p_vec->size());
          while (!p_vec->empty())
          {
              session<data>* s = p_vec->back();
              p_vec->pop_back();
              //printf("vec:%p\n",s);
              delete s;
              log->debug_log("Shuting down session done.");
          }
          log->debug_log("Sessions deleted.");
          delete p_vec;
        //  acceptor_.close();
      //    io_service_.stop();
      }
      pthread_mutex_unlock(&mutex);
      pthread_mutex_destroy(&mutex);
      log->debug_log("deleted");
  }

private:
  boost::asio::io_service& io_service_;
  tcp::acceptor acceptor_;
  Logger *log;
  std::vector< session<data>*> *p_vec;  
  pthread_mutex_t mutex;
  bool shutdown;
};


#endif	/* ASYN_TCP_SERVER_H */

