/**
 * @file MessageRouter.cpp
 * @author Sebastian Moors <mauser@smoors.de>, Markus Mäsker, maesker@gmx.net
 * @date 28.7.11
 */
#include  "mm/mds/MessageRouter.h"

static int kill = 0;

void* router(void *v);
void* router_device(void *v);


/**
 * @class MessageRouter
 * @brief Empty constructor.
 * @param[in] basedir std::string representing the base directory
 * */
MessageRouter::MessageRouter(std::string basedir): AbstractMdsComponent(basedir,MR_CONF_FILE)
{
   // p_profiler = Pc2fsProfiler::get_instance();
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
        // create worker thread and start it with 'router' method
        //rc = pthread_create(&worker_thread, NULL, router, NULL );
        rc = pthread_create(&worker_thread, NULL, router_device, NULL );
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
    try
    {
        //  Prepare our context and sockets
        zmq::context_t context(1);
        zmq::socket_t frontend (context, ZMQ_ROUTER);
        zmq::socket_t backend (context, ZMQ_DEALER);

        frontend.bind(ZMQ_ROUTER_FRONTEND);
        backend.bind(ZMQ_ROUTER_BACKEND);

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
            { frontend, 0, ZMQ_POLLIN, 0 },
            { backend,  0, ZMQ_POLLIN, 0 }
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
                    frontend.recv(&message);
                    p_profiler->function_wakeup();
                    //std::cout << "frontend:5559 got something" << std::endl;
                    size_t more_size = sizeof (more);
                    frontend.getsockopt(ZMQ_RCVMORE, &more, &more_size);
                    p_profiler->function_sleep();
                    backend.send(message, more? ZMQ_SNDMORE: 0);
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
                    backend.recv(&message);
                    p_profiler->function_wakeup();
                    //std::cout << "backend:5560 got something" << std::endl;
                    size_t more_size = sizeof (more);
                    backend.getsockopt(ZMQ_RCVMORE, &more, &more_size);
                    p_profiler->function_sleep();
                    frontend.send(message, more? ZMQ_SNDMORE: 0);
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
    try
    {
        void *context = zmq_init (5);

        // Socket facing clients
        void *frontend = zmq_socket (context, ZMQ_REP);
        zmq_bind (frontend, ZMQ_ROUTER_FRONTEND);

        // Socket facing services
        void *backend = zmq_socket (context, ZMQ_REQ);
        zmq_bind (backend, ZMQ_ROUTER_BACKEND);

        uint64_t hwm_f = 1000;
        zmq_setsockopt(frontend, ZMQ_HWM, &hwm_f, sizeof (hwm_f));
        uint64_t hwm_b = 1000;
        zmq_setsockopt(backend, ZMQ_HWM, &hwm_b, sizeof (hwm_b));
         // Specify swap space in bytes, this covers all subscribers
        uint64_t swap_f = 64*1024*1024;
        zmq_setsockopt(frontend, ZMQ_SWAP, &swap_f, sizeof (swap_f));
        uint64_t swap_b = 64*1024*1024;
        zmq_setsockopt(backend, ZMQ_SWAP, &swap_b, sizeof (swap_b));


        // Start built-in device
        p_profiler->function_sleep();
        zmq_device (ZMQ_QUEUE, frontend, backend);
        p_profiler->function_wakeup();
        // We never get here…
        zmq_close (frontend);
        zmq_close (backend);
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
