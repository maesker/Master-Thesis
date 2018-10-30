/**
 * @file MessageRouter.cpp
 * @author Markus Mäsker, maesker@gmx.net
 * @date 20.12.11
 */
#include  "components/network/MessageRouter.h"

static int kill = 0;

void* router(void *v);
void* router_device(void *v);



/**
 * @class MessageRouter
 * @brief Empty constructor.
 * @param[in] basedir std::string representing the base directory
 * */
MessageRouter::MessageRouter(std::string basedir): AbstractComponent(basedir,MR_CONF_FILE)
{
    p_profiler = Pc2fsProfiler::get_instance();
    front = std::string("tcp://*:");
    back = std::string("ipc:///tmp/backend");    
}


void MessageRouter::set_id(serverid_t id)
{
    this->id = id;
}

void MessageRouter::set_frontend(std::string s)
{
    this->front = s;
}

void MessageRouter::set_backend(std::string s)
{
    this->back = s;
}

void MessageRouter::set_port(uint16_t port)
{
    this->port = port;
}

/**
 * @brief start worker thread.
 * @return return the return code of pthread_create
 * */
int32_t MessageRouter::run()
{
    p_profiler->function_start();
    int32_t rc = 0;
    kill = 0;
    try
    {   
        char *c = (char *) malloc(8);
        memset(c,0,8);
        sprintf(c,"%d",this->port);
        front.append(c);

        memset(c,0,8);
        sprintf(c,"%d",this->id);
        back.append(c);
        free(c);
        
        printf("Setting up:\n%s\n%s\n",front.c_str(),back.c_str());
        // create worker thread and start it with 'router' method
        //rc = pthread_create(&worker_thread, NULL, router, NULL );
        rc = pthread_create(&worker_thread, NULL, router_device, this );
        set_status(started);
    }
    catch (...)
    {
        p_profiler->function_end();
        throw MRException("Message Router exception thrown.");
    }
    p_profiler->function_end();
    return rc;
}

/** 
 * @brief Stop the MessageRouter
 * */
int32_t MessageRouter::stop()
{
    p_profiler->function_start();
    kill = 1;
    set_status(stopped);
    p_profiler->function_end();
    return 0;
}


/**
 * @brief actual worker thread method. Creates two zmq sockets and
 * distributes messages between them.
 * @return never returns...
 * */
void* router(void *v)
{
    Pc2fsProfiler *p_profiler = Pc2fsProfiler::get_instance();
    p_profiler->function_start();
    MessageRouter *mr = (MessageRouter*) v;
    try
    {
        //  Prepare our context and sockets
        zmq::context_t context(1);
        zmq::socket_t frontend_sock (context, ZMQ_ROUTER);
        zmq::socket_t backend_sock (context, ZMQ_DEALER);

        frontend_sock.bind(mr->front.c_str());
        backend_sock.bind(mr->back.c_str());

        // specify high water mark
        /*uint64_t hwm_f = 1000;
        frontend.setsockopt( ZMQ_HWM, &hwm_f, sizeof (hwm_f));
        uint64_t hwm_b = 1000;
        backend.setsockopt( ZMQ_HWM, &hwm_b, sizeof (hwm_b));
         // Specify swap space in bytes, this covers all subscribers
        uint64_t swap_f = 16*1024*1024;
        frontend.setsockopt( ZMQ_SWAP, &swap_f, sizeof (swap_f));
        uint64_t swap_b = 16*1024*1024;
        backend.setsockopt( ZMQ_SWAP, &swap_b, sizeof (swap_b));
*/
        
        //  Initialize poll set
        zmq::pollitem_t items [] =
        {
            { frontend_sock, 0, ZMQ_POLLIN, 0 },
            { backend_sock,  0, ZMQ_POLLIN, 0 }
        };
        kill = 0;
        zmq::message_t message;
        //  Switch messages between sockets
        while (!kill)
        {   
            int64_t more;           //  Multipart detection
            p_profiler->function_sleep();
            zmq::poll (&items [0], 2, -1);
            p_profiler->function_wakeup();
            if (items [0].revents & ZMQ_POLLIN)
            {
                while (1)
                {
                    //  Process all parts of the message
                    p_profiler->function_sleep();
                    frontend_sock.recv(&message);
                    p_profiler->function_wakeup();
                    //std::cout << "frontend:5559 got something" << std::endl;
                    size_t more_size = sizeof (more);
                    frontend_sock.getsockopt(ZMQ_RCVMORE, &more, &more_size);
                    p_profiler->function_sleep();
                    backend_sock.send(message, more? ZMQ_SNDMORE: 0);
                    p_profiler->function_wakeup();
                    if (!more)
                        break;      //  Last message part
                }
            }
            if (items [1].revents & ZMQ_POLLIN)
            {
                while (1)
                {
                    //  Process all parts of the message
                    p_profiler->function_sleep();
                    backend_sock.recv(&message);
                    p_profiler->function_wakeup();
                    //std::cout << "backend:5560 got something" << std::endl;
                    size_t more_size = sizeof (more);
                    backend_sock.getsockopt(ZMQ_RCVMORE, &more, &more_size);
                    p_profiler->function_sleep();
                    frontend_sock.send(message, more? ZMQ_SNDMORE: 0);
                    p_profiler->function_wakeup();
                    if (!more)
                        break;      //  Last message part
                }
            }
        }
    }
    catch (MRException e)
    {
        p_profiler->function_end();
        throw MRException("Message Router exception thrown while running.");
    }
    //std::cout << "Message router end." << std::endl;
    p_profiler->function_end();
}



/**
 * @brief actual worker thread method. Creates two zmq sockets and
 * distributes messages between them.
 * 
 * Based on the "Extended request-reply" Pattern described here http://zguide.zeromq.org/page:all#A-Request-Reply-Broker.
 * 
 * @return never returns...
 * */
void* router_device(void *v)
{
    Pc2fsProfiler *p_profiler = Pc2fsProfiler::get_instance();
    p_profiler->function_start();
    MessageRouter *mr = (MessageRouter*) v;
    try
    {
        void *context = zmq_init (1);

        // Socket facing clients
        void *frontend_sock = zmq_socket (context, ZMQ_REP);
        //void *frontend_sock = zmq_socket (context, ZMQ_XREP);
        zmq_bind (frontend_sock, mr->front.c_str());
        
        // Socket facing services
        void *backend_sock = zmq_socket (context, ZMQ_REQ);
        zmq_bind (backend_sock, mr->back.c_str());
        //printf("MR: connected to backend %s.\n",mr->back.c_str());

        uint64_t hwm_f = 1000;
        zmq_setsockopt(frontend_sock, ZMQ_HWM, &hwm_f, sizeof (hwm_f));
        uint64_t hwm_b = 1000;
        zmq_setsockopt(backend_sock, ZMQ_HWM, &hwm_b, sizeof (hwm_b));
         // Specify swap space in bytes, this covers all subscribers
        uint64_t swap_f = 64*1024*1024;
        zmq_setsockopt(frontend_sock, ZMQ_SWAP, &swap_f, sizeof (swap_f));
        uint64_t swap_b = 64*1024*1024;
        zmq_setsockopt(backend_sock, ZMQ_SWAP, &swap_b, sizeof (swap_b));


        // Start built-in device
        p_profiler->function_sleep();
        zmq_device (ZMQ_QUEUE, frontend_sock, backend_sock);
        p_profiler->function_wakeup();
        // We never get here…
        zmq_close (frontend_sock);
        zmq_close (backend_sock);
        zmq_term (context);
    }
    catch (MRException e)
    {
        p_profiler->function_end();
        throw MRException("Message Router exception thrown while running.");
    }
    //std::cout << "Message router end." << std::endl;
    p_profiler->function_end();
}
