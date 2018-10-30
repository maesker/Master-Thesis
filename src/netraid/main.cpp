/**
 * @brief Main DataServer starting
 * @author Markus Mäsker, maesker@gmx.net
 * @date 20.12.11
 * @file main.cpp
 * @version 0.2
 */

#include <zmq.h>
#include <list>
#include <string>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "global_types.h"
#include "tools/sys_tools.h"
#include "abstracts/AbstractComponent.h"
#include "logging/Logger.h"
#include "components/configurationManager/ConfigurationManager.h"
#include "customExceptions/AbstractException.h"
#include "components/network/MessageRouter.h"

#include "coco/communication/ConcurrentQueue.h"
#include "custom_protocols/storage/SPNetraid.h"
#include "custom_protocols/storage/SPdata.h"
#include "custom_protocols/control/CPNetraid.h"
#include "custom_protocols/cluster/CCCNetraid.h"
#include "server/DataServer/DataServer.h"
#include "custom_protocols/global_protocol_data.h"

using namespace std;

/** 
 * @var config relative path to config file
 * */
//#define config "../conf/main_ds.conf"

/**
 * @brief initialize, configure and run config manager
 * @param[in] argc number of command line arguments the process received
 * @param[in] argv arguments the process received
 * @return ConfigurationManager pointer
 * */
ConfigurationManager* init_cm(int argc, char **argv, std::string abspath)
{
    ConfigurationManager *cm = new ConfigurationManager(argc, argv,abspath);
    cm->register_option("logfile","path to logfile");
    cm->register_option("loglevel","Specify loglevel");
    cm->register_option("cmdoutput","Activate commandline output");
    
    
    cm->parse();
    return cm;
}


inline void free_zmq_msgmem(void *data, void *hint){}

int worker_killswitch = 0;
int worker_threads_storage = 0;
int worker_threads_control = 0;


/**
 * @brief Metadataserver worker thread.
 * @details Handles the communication with
 * the ganesha FSAL layer via ZMQ. ATM loops infinitly and waits for
 * incomming messages. These messages will be transformed into the
 * corresponding FSAL structures and passed to the appropriate handler.
 *
 * @param[in] p_v void pointer to the metadataserver object. Used to call
 * the MDS handler functions.
 * */
void* data_server_worker_control(void *p_v)
{
    Pc2fsProfiler *p_profiler = Pc2fsProfiler::get_instance();
    p_profiler->function_start();
        
    struct custom_protocol_reqhead_t head;
    size_t headsize = sizeof(custom_protocol_reqhead_t);
    size_t respsize;
    // metadata objects pointer to access the handler functions
    DataServer* p_obj = (DataServer*)p_v;
    CPNetraid *protocol = new CPNetraid();
    protocol->register_dataserver(p_obj);
    uint32_t maxmsglen = protocol->get_max_msg_len();

    p_obj->log->debug_log( "protocolled worker thread running." );

    // create zmq objects
    
    // zmq messaging context, to connect to message router
    zmq::context_t context(1);
    zmq::socket_t responder( context, ZMQ_REP );

    // connect to message router
    std::string backend = std::string(MR_CONTROL_PREFIX_BACKEND);
    char *c = (char *) malloc(8);
    memset(c,0,8);
    sprintf(c,"%d",p_obj->getId());
    backend.append(c);
    free(c);
    responder.connect( backend.c_str() );

    // create request and response message objects
    zmq_msg_t request;
    // initialize request message with provided message size
    zmq_msg_init_size( &request, maxmsglen );

    int32_t rc;

    void *response_structure_mem = malloc(maxmsglen);

    //used to identify traced messages
    uint32_t message_id;
    srand((unsigned)time(0)     ); 
                
    //struct mallinfo mi_start;
    while( !worker_killswitch )
    {    
        zmq_msg_t response;
        // receive new message
        memset(zmq_msg_data(&request),'\0', headsize);
        p_profiler->function_sleep();
        rc = zmq_recv( responder, &request, 0 );
        p_profiler->function_wakeup();
        //printf("DS:control recved.\n");
        try
        {  
            #ifdef TRACE_ZMQ_MESSAGES
	    message_id = rand();
	    p_obj->trace_incoming_message( &request, message_id );
	    #endif	

            /*  read message type and copy it to dispatch. This is used
                to identify the message type and switch to the responsible
                message handler. */
            if (rc)
            {
                p_obj->log->debug_log( "Incomplete  rc:%d.",rc);
                throw ZMQMessageException( "zmq_recv returned an error.");
            }
            else
            {
                memcpy( &head, zmq_msg_data(&request), headsize);
            }            
            switch(head.protocol_id)
            {
                case cpn_id:
                {
                    p_obj->log->debug_log( "SPNetraid request received." );
                    printf( "SPNetraid request received.\n" );
                    respsize = protocol->handle_request(&head, zmq_msg_data(&request), response_structure_mem);
                    rc = zmq_msg_init_data(&response, response_structure_mem,respsize,free_zmq_msgmem,NULL);
                    break;
                }
                default:
                {
                    printf("Unknown protocol received\n.");
                }
            }
        }
        catch (...)
        {
            p_obj->log->error_log( "MDS workerthread exception raised. Send fallback error message" );
           // zmq_fallback_error_message(&response, head.sequence_number) ;
        }

        #ifdef TRACE_ZMQ_MESSAGES
        p_obj->trace_outgoing_message( &response, message_id );
        #endif

        rc = zmq_send(responder, &response, 0);
        if (rc)
        {
           p_obj->log->error_log( "MDS workerthread: error sending message:rc=%d.",rc );
        }

        zmq_msg_close(&response);
        //p_obj->log->debug_log( "Sending response done." );   
        //printf("Message processed: type %d,",dispatch);
        //MEMDIFF(mallinfo(),mi_start);
        //p_profiler->function_end();

    }
    p_obj->log->debug_log( "Worker terminated." );
    p_profiler->function_end();
    zmq_msg_close(&request);
    free(response_structure_mem);
}


/**
 * @brief main function
 * @param[in] argc argument count
 * @param[in] argv arguments
 * */
int main(int argc, char **argv)
{
    int shutdown = 0;
    int rc;
    try
    { 
        serverid_t id = 0;
        if (argc>0)
        {
            id = atoi(argv[1]);
        }
        else
        {
            printf("Please specify the id of the data server.\n");
            exit(1);
        }
        
        std::string basedir;
        std::string relpath = string(argv[0]);
        if (relpath[0]=='/')
        {
            basedir = relpath.substr(0,relpath.find_last_of("/"));
        }
        else
        {
            char cCurrentPath[FILENAME_MAX];
            GetCurrentDir(cCurrentPath, sizeof(cCurrentPath));
            basedir = string(cCurrentPath);        
            basedir+="/";
            basedir.append(relpath.substr(0,relpath.find_last_of("/")));
        }
        //basedir += string(id);
        //basedir += "/";
        std::string abspath = basedir;
        abspath.append("/");
        abspath = std::string(DS_CONFIG_FILE);
        // initialize the configuration manager
        ConfigurationManager *cm = init_cm(argc,argv,abspath);
        std::string logfile = cm->get_value("logfile");
        int loglevel = atoi(cm->get_value("loglevel").c_str());
        int cmdoutput = atoi(cm->get_value("cmdoutput").c_str());
        //storage_dir = cm->get_value('storage_dir');
        
        std::string logdir = std::string(DEFAULT_LOG_DIR) + "/" + argv[1] + "/";
        xsystools_fs_mkdir(logdir);
        
        // create logger instance and initialize
        Logger *log = new Logger();
        log->set_log_level(loglevel);
        log->set_console_output((bool)cmdoutput);
        std::string log_location = logdir + logfile; 
        log->set_log_location( log_location );
        log->debug_log( "Logging to location: %s" , log_location.c_str() );

        MessageRouter *mr_control = new MessageRouter(abspath);
        mr_control->set_id(id);
        mr_control->set_port( id+MR_CONTROL_PORT_FRONTEND);
        mr_control->set_frontend(MR_CONTROL_PREFIX_FRONTEND);
        std::string backend = std::string(MR_CONTROL_PREFIX_BACKEND) + argv[1] + "/control_";
        mr_control->set_backend(backend.c_str());
        mr_control->run();
        
        /*MessageRouter *mr_storage = new MessageRouter(abspath);
        mr_storage->set_id(id);
        mr_storage->set_port( id+MR_STORAGE_PORT_FRONTEND);
        mr_storage->set_backend(MR_STORAGE_PREFIX_BACKEND);
        mr_storage->set_frontend(MR_STORAGE_PREFIX_FRONTEND);
        mr_storage->run();*/
        

        DataServer *ds = DataServer::create_instance(logdir,basedir,id);
        ds->run();
        log->debug_log("Dataserver %d running.",id);
            
            // create some worker threads, 'worker_thread' defined in mds.conf
            for(int i = 0; i < worker_threads_control; i++)
            {
                pthread_t worker_thread;
                rc = pthread_create(&worker_thread, NULL, data_server_worker_control, ds );
                if (rc)
                {
                    // throw an exception on fail
                    log->error_log("mds thread not created:rc=%d.",rc);
                    throw DataServerException("control worker thread not created.");
                }
                log->debug_log("thread created:%d",rc);
            }
        log->debug_log("thread creation done.");
    
        SPNetraid *p_prot_spn  = new SPNetraid(logdir, id,&ds->push_spn_in);
        CCCNetraid *p_prot_ccc = new CCCNetraid(logdir, id, &ds->push_ccc_in);
    
        flush_sysbuffer();
        char input;
        
        //daemonize();
        while(!shutdown)
        {
            printf ("'y' to quit:\n");
            input = getchar();
            if (input =='y') break;
            sleep(100);
        }
        // deleting created instances.
        //delete cm;
        //log->debug_log("system shutdown.");
        //delete log;
        
        
        
    }
    catch(...)
    {
        printf("Exception raised.\n");
        throw;
    }
    return 0;
}
