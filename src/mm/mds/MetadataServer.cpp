/**
 * @file MetadataServer.cpp
 * @class MetadataServer
 * @author Markus Maesker, maesker@gmx.net
 * @date 28.07.11
 *
 * @brief Main Metadataserver source, doing the actual work.
 *
 * 
     * This Class does the usual Metadata work. It can be considered
     * to be the connecting instance between the Ganesha server and the
     * Metadata storage backend.
     * At first it creates the MessageRouter instance. This router 
     * receives ZMQ messages from our Ganeshas FSAL-ZMQ layer. This MDS 
     * creates several worker threads that connect to the MessageRouter,
     * receive a zmq message, process it and return the response.
 *
 * Inside the worker threads the ZMQ messages are decoded and passed to
 * the appropriate handler function.
 * The handler function extracts the required arguments from the fsal
 * message and issues the provided (Journal-) callback. The result is
 * written to the provided fsal response structure defined in the worker
 * thread.
 * Control returns to the worker thread, where the fsal response
 * structure is written to a zmq message and sent back via the
 * message router.
 *
 * The common options are defined in conf/mds.conf:
 *  "logfile=mds.log" defines the logfiles name. Logdir is a global option
 *  "loglevel=1" defines the loglevel, 1=debug, 2=warning, 3=error
 *  "cmdoutput=0" enable/disable cmd output
 *  "ganesha_bin=zmq.ganesha.nfsd" name of the ganesha daemon
 *  "worker_threads=1" number of worker threads
 * 
 * 
 * @todo Sandeeps access counter needs information after partition quit.
 * */

#include "mm/mds/MetadataServer.h"
#include "global_types.h"

const size_t size_mds_populate_prefix_perm = sizeof(struct MDSPopulatePrefixPermission);
const size_t size_mds_update_prefix_perm = sizeof(struct MDSUpdatePrefixPermission);
const size_t size_populate_prefix_perm = sizeof(prefix_perm_populate_t);
const size_t size_update_prefix_perm = sizeof(prefix_perm_update_t);


// zmq messaging context, to connect to message router
static zmq::context_t context(1);



// declaration of the worker thread function
void* metadata_server_worker(void *p_v);
void* metadata_server_worker_protocolled(void *p_v);
/** @brief fsal_worker_killswitch = true will cause the threads to stop
 *  processing zmq messages and terminate.
 * */

/**
 * @brief Return the singleton instance. Creates the instance if called
 * for the first time.
 * */
MetadataServer *MetadataServer::p_instance=NULL;

MetadataServer *MetadataServer::create_instance(JournalManager *p_jm, MltHandler *p_mlt_handler, std::string basedir)
{
    if(p_instance == NULL)
    {
        // if no instance existing, create one
        std::string conf = string(MDS_CONFIG_FILE);
        basedir=std::string("");
        p_instance = new MetadataServer(p_jm, p_mlt_handler,basedir,conf);
    }
    // return MetadataServer instance
    return p_instance;
} // end of get_instance


/** 
 * @brief returns the MetadataServer instance
 * @return MetadataServer instance
 */
MetadataServer *MetadataServer::get_instance()
{
    return p_instance;
}


/**
 * @brief MDS Constructor
 * @param[in] p_jm JournalManager instance
 * @param[in] p_mlt_handler MltHandler instance
 * 
 * @todo mds handle xxx functions threadsave?
 * */
MetadataServer::MetadataServer(
    JournalManager *p_jm,
    MltHandler *p_mlt_handler,
    std::string basedir,
    std::string conffile): AbstractMdsComponent(basedir,conffile)
{
    p_profiler->function_start();
    p_profiler->function_sleep();
    p_profiler->function_wakeup();
    p_profiler->function_sleep();
    p_profiler->function_wakeup();
    
    p_cm->register_option( "ganesha_bin", "Process name of the ganesha daemon");
    p_cm->register_option( "worker_threads", "Number of worker threads");
    //p_cm->register_option("groupsize", "raid group size, e.g. 9 for a 8+1 configuration");
    p_cm->register_option("susize", "Stripe unit size in Byte, e.g. 256");
    p_cm->register_option("dsuser", "User account to run the data server");
    p_cm->register_option("dspw", "Password of the data server user");    
    p_cm->register_option("ds.bin", "Dataserver binary to execute");    
    p_cm->register_option("nuttcp", "1 to run a nuttcp server");    
    
    char c[256];
    for (int i=0; i<256; i++)
    {
        memset(&c[2],0,254);
        sprintf(&c[0],"ds%i",i);
        p_cm->register_option(&c[0], "ip of the dataserver");
        log->debug_log("registered:%u",i);
    }
    p_cm->parse();
    root_inode_number = FS_ROOT_INODE_NUMBER;
    try
    {
        // define ganesha daemon name and number of worker threads
        ganesha_process_name = p_cm->get_value( "ganesha_bin" );
        worker_threads = (uint16_t) atoi( p_cm->get_value( "worker_threads" ).c_str() );
        //uint8_t groupsize = (uint8_t) atoi(p_cm->get_value("groupsize").c_str());
        size_t susize = atoi(p_cm->get_value("susize").c_str())*1024;
        user = p_cm->get_value("dsuser");
        

        // set mlt handler instance
        this->p_mlt_handler = p_mlt_handler;
 
        // defines whether this MDS is responsible for the fs root object
        fs_root_flag = false;
        // killswitch to stop worker threads
        mutex_write_export_block = PTHREAD_MUTEX_INITIALIZER;

        //For tracing messages (just for debugging)
        setup_message_tracing();

        // get Subtreemanager instance and initialize it
        p_subtree_manager = SubtreeManager::get_instance(this->basedir);
        p_subtree_manager->set_journal_manager( p_jm );
        p_subtree_manager->set_mlt_handler( p_mlt_handler );
        // create Message router instance
        
        p_sessionman = new MdsSessionManager();
        p_byterangelock = new ByterangeLockManager(log);
        p_cpclient = new CPNetraid_client(log);
        p_dataserver = new ServerManager(log,MR_CONTROL_PORT_FRONTEND); 
        p_raid = new Libraid4(log);
        p_raid->register_server_manager(p_dataserver);
        p_raid->setStripeUnitSize(susize);
        log->debug_log("Stripeunit size:%llu",susize);
        
        if (!strcmp(p_cm->get_value("nuttcp").c_str(),"1"))
        {
            system("nuttcp -S &");
        }
        //p_raid->setGroupsize(groupsize);        
    }
    catch (MDSException e)
    {
        // Catch a metadata server exception
        if (log != NULL)
        {
            log->error_log( "Initialization failed: %s.", e.what() );
        }
        p_profiler->function_end();
        throw e;
    }
    catch (SubtreeManagerException e)
    {
        // catch a SubtreeManager exception
        if (log != NULL)
        {
            log->error_log( "Subtreemanager raised error: %s.", e.what() );
        }
        p_profiler->function_end();
        throw MDSException(e.what());
    }
    catch (...)
    {
        // unknown exception catched
        if (log != NULL)
        {
            log->error_log( "Unknown exception thrown." );
        }
        p_profiler->function_end();
        throw MDSException( "Unknown exception thrown." );
    }
    p_profiler->function_end();
} // end mds constructor

/**
 * @brief Deconstructor of the MDS. Subtreemanager, journal manager and
 * mlt handler singleton instances are not deleted.
 * */
MetadataServer::~MetadataServer()
{
    trace_file.close();
    log->debug_log(" Stopping metadata server" );
    pthread_mutex_destroy( &mutex_write_export_block );
    
    // delete created objects
    //delete p_inumdist;    
    //delete p_subtree_manager;
    printf("Metadats destroyer. instance = null\n");
    delete log;
    delete p_cm;
    delete p_profiler;
} // end if deconstructor


/** 
 * @brief stops the MetadataServer
 * @return 0 on success
 */
int32_t MetadataServer::stop()
{
    int32_t rc = 0;
    p_profiler->function_start();
    set_status(stopped);
    log->debug_log("stopping metadata server");

    // send one last message to the message router. This is needed 
    // because the message router waits for a message and checks the 
    // killswitch after that
    /*
    zmq_msg_t request;
    zmq::context_t context(1);
    zmq::socket_t responder(context, ZMQ_REQ);
    responder.connect(ZMQ_ROUTER_FRONTEND);
    rc = zmq_msg_init_size(&request, 8);
    if (!rc)
    {
        // write data to zmq message
        rc = zmq_send( responder , &request, 0);
        //zmq_recv(responder, &request, 0);
    } 
    zmq_msg_close(&request);*/
    p_profiler->function_end();
    return rc;
}

/**
 * @brief Start the fsal-zmq worker threads. The number of threads is
 * defined by the 'threads' object variable.
 * @return integer representing pthread_create rc.
 * */
int32_t MetadataServer::run()
{
    p_profiler->function_start();
    log->debug_log( "Starting metadata server threads." );
    int32_t rc = -1;
    try
    {
        int i;
        if (get_status()!=started)
        {
            register_dataserver();            
            p_cpclient->setupDS(user,p_cm->get_value("ds.bin"),this->dataserver_sshpw);
            uint32_t cnt;
            p_dataserver->get_server_count(&cnt);
            p_raid->setServercnt(cnt);
            
            if (this->p_mlt_handler==NULL) throw MDSException("No JournalManager registered.");
            // initialize InodeNumberDistributor
            if (this->p_sal==NULL) throw MDSException("Missing StorageAbstractionLayer");

            int rank = this->p_mlt_handler->get_my_rank();
            vector<Server> server_list;
            this->p_mlt_handler->get_server_list(server_list);
            log->debug_log("Register rank:%d.",rank);
            // TODO: Set InodeNumber to writable partition
            p_inumdist = new InodeNumberDistributor(rank ,this->p_sal, 1);

            // create ganesha config
            this->handle_ganesha_configuration_update();

            // start the subtree manager, it throw an exception on fail
            if (this->p_subtree_manager==NULL) throw MDSException("Missing SubtreeManager");  
            p_subtree_manager->run();
            // after the subtreemanager read the mlt, the mds asks whether
            // it should switch to "fs_root" mode.
            fs_root_flag = p_subtree_manager->am_i_the_fs_root();
            log->debug_log("Is this server responsible for the root object:%i",(int)fs_root_flag);
            rc=0;
        }
        else
        {
            log->debug_log("Server already running...");
        }
        log->debug_log( "Mds thread creation done. %d treads created.", i );
    }
    catch (MDSException e)
    {
        log->error_log( "Startup failed: %s.", e.what() );
        throw e;
    }
    catch (MRException e)
    {
        log->error_log( "Message Router exception:%s.", e.what() );
        throw e;
    }
    catch (SubtreeManagerException e)
    {
        log->error_log( "SubtreeManager exception:%s.", e.what() );
        throw e;
    }
    set_status(started);
    p_profiler->function_end();
    return rc;
} // end MetadataServer::run()

int MetadataServer::set_dataserver_sshpw(std::string pw)
{
    this->dataserver_sshpw = pw;
}
    
uint32_t MetadataServer::register_dataserver()
{
    uint32_t rc=1;
    std::string s = std::string();
    for (serverid_t i=0; i<256; i++)
    {
        stringstream ss("");
        s.clear();
        ss << "ds" << i << "\0";
        s = p_cm->get_value(ss.str());
        if (s.length() > 2)
        {
            p_dataserver->register_server(i,(char*)s.c_str(),i+MR_CONTROL_PORT_FRONTEND);
            p_cpclient->addDS(i,s,i+MR_CONTROL_PORT_FRONTEND);
            log->debug_log("added %u at %s",i,s.c_str());
            rc=0;
        }
    }
    log->debug_log("rc=%u",rc);
    return rc;
}

/** 
 * @brief Sanity check for the needed components
 * 
 * @details Call the Inherited verify method to test the inherited components
 * and performs sanity check on the instantiated components.
 * 
 * @return 0 if one check fails, 1 on success
 * */
int16_t MetadataServer::verify()
{
    p_profiler->function_start();
    int rc = 1;
    if (!AbstractMdsComponent::verify())
    {
        log->error_log("AbstractMdsComponent verify failed.");
        rc = 0;
    }
    if (!p_subtree_manager->verify())
    {
        log->error_log("SubtreeManager verify failed.");
        rc = 0;
    }
    if (get_status()!=started)
    {
        log->error_log("Invalid status detected.");
        rc = 0;
    }
    p_profiler->function_end();
    return rc;
}

/**
 * @brief Dummy Inode Number generator. This musst be implemented once
 * there is a inode number pool provided.
 * @param[out] p_inum returned Inodenumber that has be determined by some
 * registration mechanism.
 * @return mds_global_ok
 * */
int32_t  MetadataServer::generateNewInodeNumber (InodeNumber *p_inum)
{
    p_profiler->function_start();
    // method for inode registration must be implemented and called here.
    int rc = mds_global_ok;
    if (p_inumdist != NULL)
    {
        // the new inode number is received here.
        *p_inum = p_inumdist->get_free_inode_number();
    }
    else
    {
        // error occured.
        rc = mds_no_inode_number_distributor_registered;
    }
    log->debug_log( "Generated inode:%u.", *p_inum );
    p_profiler->function_end();
    return rc;
}


/**
 * @brief request the journal responsible for the provided inode from
 * the subtree manager
 * @param[in] p_partition_root_inode_number root inode number to
 * identify the partition.
 * @return Pointer to the journal instance responsible for the
 * inode, or null if none found.
 * */
Journal* MetadataServer::get_responsible_journal(
    InodeNumber *p_partition_root_inode_number)
{
    p_profiler->function_start();
    log->debug_log( "Lookup Journal for inode %u;", *p_partition_root_inode_number );
    Journal *j = NULL;
    // check whether the inode number is provided
    if (p_partition_root_inode_number != NULL)
    {
        // check whether the subtree manager is registered
        //if (p_subtree_manager != NULL)
        //{
            // get the journal that is responsible for the given inode number
            j = p_subtree_manager->get_responsible_journal( p_partition_root_inode_number );
           // if (j == NULL)
           // {
                // no journal received
           //     log->error_log( "SubtreeManager has no responsible journal for inode %u",*p_partition_root_inode_number );
           // }
        //}
        //else
        //{
            // no subtree manager registered
        //    log->error_log( "SubtreeManager not instantiated." );
        //}
    }
    else
    {
        // needed input missing
        log->error_log( "No partition root inode number provided." );
    }
    p_profiler->function_end();
    return j;
}   // end of MetadataServer::get_responsible_journal()

/**
 * @brief Returns the intercepted einode request messages. This message
 * contains the filesystems root einode objekt.
 * @param[in] p_fsal pointer to the fsal response structure.
 *  
 * Creates a dummy einode structure that represents the file-
 * systems root object. This is needed if the MDS instance is the one
 * responsible for the fs root. The root einode is not stored at the
 * storage backend, because its not going to be changed or deleted
 * anyway. 
 * 
 * @return 0 on success, fail otherwise
 * */
int32_t MetadataServer::get_directory_inode_fsal_response(void *p_fsal, InodeNumber root)
{
    p_profiler->function_start();
    int rc = 0;
    // create needed structure
    try
    {
        struct FsalFileEInodeResponse *p_fsal_resp =(struct FsalFileEInodeResponse *) p_fsal;
        time_t t = time(0);
        p_fsal_resp->einode.inode.size = 0;
        p_fsal_resp->einode.inode.ctime = t;
        p_fsal_resp->einode.inode.atime = t;
        p_fsal_resp->einode.inode.mtime = t;
        p_fsal_resp->einode.inode.mode = 17407;        
        p_fsal_resp->einode.inode.inode_number = root;
        p_fsal_resp->einode.inode.link_count = 1;
        p_fsal_resp->einode.inode.has_acl = 0;
        p_fsal_resp->einode.inode.uid = (uid_t)0;
        p_fsal_resp->einode.inode.gid = (uid_t)0;
        memcpy(&p_fsal_resp->einode.name,"/\0",2);
        log->debug_log(" Rootobject create:root=%u.",root );
        // write root einode data to fsal_response structure        
        p_fsal_resp->type = file_einode_response;
        p_fsal_resp->error = 0;
    }
    catch(...)
    {
        log->error_log( "Failed to create root einode." );
        rc = -1;
    }
    // clean up data structures
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::get_directory_inode_fsal_response

/** @brief Handle incomming einode requests. The incomming message and
 * return pointer will be forwarded to the responsible generic method
 * handler: 'generic_handle_einode_request'.
 * @param[in] p_msg pointer to the received zmq request message
 * @param[out] p_fsal_resp_ref The resulting FSAL structure that will be
 * transformed into a zmq message und sent back
 * @return Errorcode of the generic_handle_einode_request, mds_no_matching_journal_found
 * or zmq_response_to_fsal error code
 * */
int32_t MetadataServer::handle_einode_request(
    zmq_msg_t *p_msg, void *p_fsal_resp_ref)
{
    p_profiler->function_start();
    struct FsalEInodeRequest *p_fsalmsg = (struct FsalEInodeRequest *) zmq_msg_data(p_msg);
    int32_t rc = -1;
    try
    {
        // if this MDS is responsible for the file systems root object
        // and the current request handles the inode number, return a
        // built in dummy root inode object
        if(    (p_fsalmsg->inode_number == FS_ROOT_INODE_NUMBER)
            || (p_subtree_manager->is_subtree_root_inode_number( &p_fsalmsg->inode_number)))
        {
            log->debug_log( "Request for root inode detected." );
            get_directory_inode_fsal_response( p_fsal_resp_ref , p_fsalmsg->inode_number);
            rc = 0;
        }
        else
        {
            // else handle normal einode request
            log->debug_log( "fsalmsg:partition_root:%u",p_fsalmsg->partition_root_inode_number );
            // get journal responsible for the requested einode
            Journal *j = get_responsible_journal( &p_fsalmsg->partition_root_inode_number );
            if (j != NULL)
            {
                log->debug_log( "Einode request received for %u. Journal found.", p_fsalmsg->inode_number );

                // specify the callback interface and acquire the journal
                // objects function pointer
                EInodeRequestCbIf p_cb_func;
                p_cb_func = &Journal::handle_mds_einode_request;

                // perform journal request and write result to p_fsal_resp_ref
                rc = generic_handle_einode_request( p_fsalmsg, p_fsal_resp_ref, p_cb_func,j );
                if (rc)
                {
                    // error occured
                    log->error_log( "Einode request not successful for %u.", p_fsalmsg->inode_number );
                    rc = mds_journal_backend_einode_request_error;
                }
                else
                {
                    // operation performed successfully.
                    log->debug_log( "Einode request successful for %s.", ((struct FsalFileEInodeResponse*)p_fsal_resp_ref)->einode.name );
                    // This print is debug stuff
                    //print_einode( &(((struct FsalFileEInodeResponse*)p_fsal_resp_ref)->einode) );
                }
            }
            else
            {
                // journal manager could not provide responsible journal
                rc = 0;//mds_no_matching_joural_found;
                log->debug_log( "fsalmsg:partition_root:%u",p_fsalmsg->partition_root_inode_number );
                log->error_log( "No journal found." );
                get_directory_inode_fsal_response( p_fsal_resp_ref , p_fsalmsg->partition_root_inode_number);
            }
            // mark journal operation as done
            p_subtree_manager->remove_working_on_flag();

        }
    }
    catch (StorageException e)
    {
        // storage backend threw exception. Return fsal response with error code
        log->error_log( "StorageException:%s.",e.get_message() );
        rc = mds_storage_exception_thrown;
        struct FsalFileEInodeResponse *p_fsal_resp =
            (struct FsalFileEInodeResponse *) p_fsal_resp_ref;
        p_fsal_resp->error = rc;
    }
    catch (MJIException e)
    {
        // mds journal interface threw exception. Return fsal response with error code
        log->error_log( "MJIException:%s.", e.what() );
        rc = mds_mji_exception_thrown;
        struct FsalFileEInodeResponse *p_fsal_resp =
            (struct FsalFileEInodeResponse *) p_fsal_resp_ref;
        p_fsal_resp->error = rc;
    }
    log->debug_log( "Einode request result:%d", rc );
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::handle_einode_request

/** @brief Handle incomming create_file_einode requests. The incomming
 * message and eturn pointer will be forwarded to the responsible
 * generic method
 * handler: 'generic_handle_create_file_einode_request'.
 * @param[in] p_msg pointer to the received zmq request message
 * @param[out] p_fsal_resp_ref The resulting FSAL structure that will be
 * transformed into a zmq message und sent back
 * @return Errorcode of the generic_handle_create_file_einode_request,
 * no_matching_journal_found or zmq_response_to_fsal error code
 * */
int32_t MetadataServer::handle_create_file_einode_request(
    zmq_msg_t *p_msg, void *p_fsal_resp_ref)
{
    p_profiler->function_start();
    struct FsalCreateEInodeRequest *p_fsalmsg = (struct FsalCreateEInodeRequest*) zmq_msg_data(p_msg);
    int32_t rc = -1;
    try
    {
        // request responsible journal from journal manager
        Journal *j = get_responsible_journal( &p_fsalmsg->partition_root_inode_number);
        if (j != NULL)
        {
            // get new inode number
            InodeNumber inum;
            rc = generateNewInodeNumber( &inum );
            // define callback function and get journal object function ptr
            if (!rc)
            {
                filelayout *p_fl = (filelayout *) malloc(sizeof(filelayout));
                rc = p_raid->create_initial_filelayout(inum,p_fl);
                CreateFileEInodeRequestCbIf p_cb_func;
                p_cb_func = &Journal::handle_mds_create_einode_request;
                log->debug_log( "Create file einide request received for parent %u with new inode number:%u.", p_fsalmsg->parent_inode_number, inum );
                // process fsal request and write data to fsal response structure
                rc = generic_handle_create_file_einode_request(
                         p_fsalmsg,p_fsal_resp_ref, &inum,p_fl, p_cb_func,j);
                free(p_fl);
            }
            else
            {
                log->warning_log("Error generating inode number.");
            }
        }
        else
        {
            // no journal could be provided
            rc = mds_no_matching_joural_found;
            log->error_log( "No journal found." );
        }
        // mark journal operation as done
        p_subtree_manager->remove_working_on_flag();
    }
    catch (StorageException e)
    {
        // storage backend threw an exception
        // return fsal error message
        log->error_log( "StorageException:%s.", e.get_message() );
        rc = mds_storage_exception_thrown;
        struct FsalCreateInodeResponse *p_fsal_resp =
            (struct FsalCreateInodeResponse *) p_fsal_resp_ref;
        p_fsal_resp->error = rc;
    }
    log->debug_log( "Create einode result:%d", rc );
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::handle_create_file_einode_request

/** @brief Handle incomming_update_attributes requests.
 * The incomming message and
 * return pointer will be forwarded to the responsible generic method
 * handler: 'generic_handle_update_attributes_request'.
 * @param[in] p_msg pointer to the received zmq request message
 * @param[out] p_fsal_resp_ref The resulting FSAL structure that will be
 * transformed into a zmq message und sent back
 * @return Errorcode of the generic_handle_update_attributes_request,
 * no_matching_journal_found or zmq_response_to_fsal error code
 * */
int32_t MetadataServer::handle_update_attributes_request(
    zmq_msg_t *p_msg, void *p_fsal_resp_ref)
{
    p_profiler->function_start();
    struct FsalUpdateAttributesRequest *p_fsalmsg = (struct FsalUpdateAttributesRequest *) zmq_msg_data(p_msg);
    int32_t rc=-1;
    try
    {
        // check whether the operation intends to modify the root inode
        if(p_fsalmsg->inode_number != (InodeNumber) FS_ROOT_INODE_NUMBER )
        {
            // if not, get the responsible journal object
            Journal *j = get_responsible_journal( &p_fsalmsg->partition_root_inode_number );
            if (j != NULL)
            {
                // create the journals callback pointer
                UpdateAttributesRequestCbIf p_cb_func;
                p_cb_func = &Journal::handle_mds_update_attributes_request;
                log->debug_log( "Update attributes inode number %u.", p_fsalmsg->inode_number );

                // process request. Result will be written to p_fsalmsg
                rc = generic_handle_update_attributes_request(
                         p_fsalmsg,p_fsal_resp_ref, p_cb_func,j );
            }
            else
            {
                rc = mds_no_matching_joural_found;
                log->error_log( "No journal found." );
            }
            // mark journal operation as done
            p_subtree_manager->remove_working_on_flag();
        }
        else
        {
            rc = mds_wont_change_root_einode_attributes_error;
            log->error_log( "Won't change root einode object attributes." );
        }
    }
    catch (StorageException e)
    {
        log->error_log( "StorageException:%s.", e.get_message() );
        rc = mds_storage_exception_thrown;
        struct FsalUpdateAttributesResponse *p_fsal_resp =
            (struct FsalUpdateAttributesResponse *) p_fsal_resp_ref;
        p_fsal_resp->error=rc;
    }
    log->debug_log( "Update attributes result:%d" , rc);
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::handle_update_attributes_request


/** 
 * @brief identify referral  
 * @return 0 on success, fail otherwise
 * @todo check if the returned partition root is valid
 * @todo check if this function can be removed compeltely
 * */
int32_t MetadataServer::handle_referral_request(FsalLookupInodeNumberByNameRequest *fsalmsg, InodeNumber *result)
{
    int32_t rc=-1;
    std::string name(fsalmsg->name);
    log->debug_log("Referral:name:%s.",name.c_str());
    //InodeNumber inum = this->p_mlt_handler->get_inode_from_path(name);
    InodeNumber inum = fsalmsg->partition_root_inode_number;
    *result=inum;
    if (inum != -1)
    {
        rc=0;
        log->error_log("Mlthandler path:%s.has inodenumber:%u",name.c_str(),inum);
    }
    else
    {
        log->error_log("Mlthandler could not find a InodeNumber corresponding to path:%s.",name.c_str());
    }
    return rc;
}

/**
 * @brief This MDS is assigned to a new subtree and now has to create
 * a journal for that subtree and start accepting request.
 * @param[in] p_inum pointer to the inode number of the subtrees root
 * object
 * @return 0 on success, failure otherwise.
 * */
int32_t MetadataServer::handle_migration_create_partition(InodeNumber *p_inum)
{
    p_profiler->function_start();
    int32_t rc=-1;
    try
    {
        log->debug_log("handle migration create partition:id:%u",*p_inum);
        rc = p_subtree_manager->handle_migration_create_partition(p_inum);
        log->debug_log( "Done. rc = %d" , rc);
    }
    catch (SubtreeManagerException e)
    {
        // catch a SubtreeManager exception
        if (log != NULL)
        {
            log->error_log( "Subtreemanager raised error: %s.", e.what() );
        }
        p_profiler->function_end();
        throw MDSException(e.what());
    }
    p_profiler->function_end();
    return rc;
} // end of handle_migration_create_partition

/**
 * @brief MDS quits to serve the subtree represented by the provided
 * inode number
 * @param[in]  p_inum pointer to the inodenumber object, representing
 * the subtree.
 * @return 0 on success, failure otherwise
 * */
int32_t MetadataServer::handle_migration_quit_partition(InodeNumber *p_inum)
{
    p_profiler->function_start();
    int32_t rc=-1;
    try
    {
        log->debug_log("handle migration quit partition:id:%u",*p_inum);
        rc = p_subtree_manager->handle_migration_quit_partition(p_inum);
        log->debug_log( "Done. rc = %d", rc );
    }
    catch (SubtreeManagerException e)
    {
        // catch a SubtreeManager exception
        if (log != NULL)
        {
            log->error_log( "Subtreemanager raised error: %s.", e.what() );
        }
        p_profiler->function_end();
        throw MDSException(e.what());
    }
    p_profiler->function_end();
    return rc;
} // end of handle_migration_quit_partition


/**
 * @brief Issued to check whether the current system has potential to
 * merge to subtrees because both are served by the same mds.
 * @return 0 if no change has been done, 1 if subtrees were merged and
 * the mlt has been updated.
 * */
int32_t MetadataServer::investigate_subtree_merge_potential()
{
    p_profiler->function_start();
    int32_t rc = -1;
    log->debug_log( "Analyse merge potential." );
    try
    {
        rc = p_subtree_manager->investigate_subtree_merge_potential();
        log->debug_log( "Done. rc = %d", rc );
    }
    catch (SubtreeManagerException e)
    {
        // catch a SubtreeManager exception
        if (log != NULL)
        {
            log->error_log( "Subtreemanager raised error: %s.", e.what() );
        }
        p_profiler->function_end();
        throw MDSException(e.what());
    }
    p_profiler->function_end();
    return rc;
}


/** @brief Handle incomming delete_inode requests.
 * The incomming message and
 * return pointer will be forwarded to the responsible generic method
 * handler: 'generic_handle_delete_einode_request'.
 * @param[in] p_msg pointer to the received zmq request message
 * @param[out] p_fsal_resp_ref The resulting FSAL structure that will be
 * transformed into a zmq message und sent back
 * @return Errorcode of the generic handle request
 * */
int32_t MetadataServer::handle_delete_inode_request(
    zmq_msg_t *p_msg, void *p_fsal_resp_ref)
{
    p_profiler->function_start();
    struct FsalDeleteInodeRequest *p_fsalmsg = (struct FsalDeleteInodeRequest*) zmq_msg_data(p_msg);
    int32_t rc = -1;
    try
    {
        // check whether the request tries to delete the root inode
        if(p_fsalmsg->inode_number != (InodeNumber) FS_ROOT_INODE_NUMBER)
        {
            // if not, get responsible journal
            Journal *j = get_responsible_journal(&p_fsalmsg->partition_root_inode_number);
            if (j != NULL)
            {
                // get journals callback function
                DeleteEInodeRequestCbIf p_cb_func;
                p_cb_func = &Journal::handle_mds_delete_einode_request;
                log->debug_log( "Delete einode inode number %d.", p_fsalmsg->inode_number );

                if( p_fsalmsg->inode_number < FS_ROOT_INODE_NUMBER )
                {
                        log->error_log( "Failed to delete inode: Attempting to delete inode 0" );
                }
                // process request and write response to provided pointer
                rc = generic_handle_delete_einode_request( p_fsalmsg,p_fsal_resp_ref,p_cb_func, j );
            }
            else
            {
                rc = mds_no_matching_joural_found;
                log->error_log( "No journal found." );
            }
            // mark journal operation as done
            p_subtree_manager->remove_working_on_flag();
        }
        else
        {
            rc = mds_wont_delete_root_einode_error;
            log->error_log( "Won't delete root einode object." );
        }
    }
    catch (StorageException e)
    {
        log->error_log( "StorageException:%s.", e.get_message() );
        rc = mds_storage_exception_thrown;
        struct FsalDeleteInodeResponse *p_fsal_resp =
            (struct FsalDeleteInodeResponse *) p_fsal_resp_ref;
        p_fsal_resp->error=rc;
    }
    log->debug_log( "Delete einode result:%d", rc );
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::handle_delete_inode_request

/** @brief Handle incomming parent_lookup_inode_numbers requests.
 * @param[in] p_msg pointer to the received zmq request message
 * @param[out] p_fsal_resp_ref The resulting FSAL structure that will be
 * transformed into a zmq message und sent back
 * @return Errorcode of the generic handle request
 * */
int32_t MetadataServer::handle_parent_inode_number_lookup_request(
    zmq_msg_t *p_msg, void *p_fsal_resp_ref)
{
    p_profiler->function_start();
    struct FsalParentInodeNumberLookupRequest *p_fsalmsg = (struct FsalParentInodeNumberLookupRequest *) zmq_msg_data(p_msg);
    int32_t rc = -1;
    try
    {
        // get responsible journal
        Journal *j = get_responsible_journal(&p_fsalmsg->partition_root_inode_number);
        if (j != NULL)
        {
            // create required journals function pointer fsal resp pointer
            ParentInodeNumbersLookupRequestCbIf p_cb_func;
            p_cb_func = &Journal::handle_mds_parent_inode_number_lookup_request;
            log->debug_log( "Parent inode number lookup." );
            // process request and write response to
            rc = generic_handle_parent_inode_numbers_lookup( p_fsalmsg, p_fsal_resp_ref, p_cb_func, j );
        }
        else
        {
            rc = mds_no_matching_joural_found;
            log->error_log( "No journal found." );
        }
        // mark journal operation as done
        p_subtree_manager->remove_working_on_flag();
    }
    catch (StorageException e)
    {
        log->error_log( "StorageException:%s.", e.get_message() );
        rc = mds_storage_exception_thrown;
        struct FsalParentInodeNumberLookupResponse *p_fsal_resp =
            (struct FsalParentInodeNumberLookupResponse *) p_fsal_resp_ref;
        p_fsal_resp->error=rc;
    }
    log->debug_log( "Delete einode result:%d", rc) ;
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::handle_parent_inode_number_lookup_request

/** 
 * @brief Handle incomming parent_inode_hierarchy_requests.
 * @param[in] p_msg pointer to the received zmq request message
 * @param[out] p_fsal_resp_ref The resulting FSAL structure that will be
 * transformed into a zmq message und sent back
 * @return Errorcode of the generic handle request
 * */
int32_t MetadataServer::handle_parent_inode_hierarchy_request(
    zmq_msg_t *p_msg, void *p_fsal_resp_ref)
{
    p_profiler->function_start();
    struct FsalInodeNumberHierarchyRequest *p_fsalmsg = (struct FsalInodeNumberHierarchyRequest *) zmq_msg_data(p_msg);
    int32_t rc = -1;
    try
    {
        // get responsible journal
        Journal *j = get_responsible_journal(&p_fsalmsg->partition_root_inode_number);
        if (j != NULL)
        {
            // create required journals function pointer fsal resp pointer
            ParentInodeHierarchyCbIf p_cb_func;
            p_cb_func = &Journal::handle_mds_parent_inode_hierarchy_request;
            log->debug_log( "Parent inode number hierarchy." );
            // process request and write response to
            rc = generic_handle_parent_inode_hierarchy(p_fsalmsg,
                                            p_fsal_resp_ref, p_cb_func, j );
        }
        else
        {
            rc = mds_no_matching_joural_found;
            log->error_log( "No journal found." );
        }
        // mark journal operation as done
        p_subtree_manager->remove_working_on_flag();
    }
    catch (StorageException e)
    {
        log->error_log( "StorageException:%s.", e.get_message() );
        rc = mds_storage_exception_thrown;
        struct FsalInodeNumberHierarchyResponse *p_fsal_resp =
            (struct FsalInodeNumberHierarchyResponse *) p_fsal_resp_ref;
        p_fsal_resp->error=rc;
    }
    log->debug_log( "Delete einode result:%d", rc) ;
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::handle_parent_inode_number_lookup_request



/** @brief Handle incomming read dir requests.
 * The incomming message and
 * return pointer will be forwarded to the responsible generic method
 * handler: 'generic_handle_read_dir_request'.
 * @param[in] p_msg pointer to the received zmq request message
 * @param[out] p_fsal_resp_ref The resulting FSAL structure that will be
 * transformed into a zmq message und sent back
 * @return Errorcode of the generic_handle_request
 * */
int32_t MetadataServer::handle_read_dir_request(
    zmq_msg_t *p_msg, void *p_fsal_resp_ref)
{
    p_profiler->function_start();
    struct FsalReaddirRequest *p_fsalmsg = (struct FsalReaddirRequest *) zmq_msg_data(p_msg);
    int32_t rc = -1;
    try
    {
        // get responsible journal
        Journal *j = get_responsible_journal(&p_fsalmsg->partition_root_inode_number);
        if (j != NULL)
        {
            // create journals responsible function pointer
            ReadDirRequestCbIf p_cb_func;
            p_cb_func = &Journal::handle_mds_readdir_request;
            log->debug_log( "Read dir inode number %d, offset:%d.",
                           p_fsalmsg->inode_number, p_fsalmsg->offset);

            // process request and write result to fsal_response pointer
            rc = generic_handle_readdir_request(
                     p_fsalmsg,p_fsal_resp_ref,p_cb_func,j );
        }
        else
        {
            rc = mds_no_matching_joural_found;
            log->error_log( "No journal found." );
        }
        // mark journal operation as done
        p_subtree_manager->remove_working_on_flag();
    }
    catch (StorageException e)
    {
        // return an error response
        log->error_log( "StorageException:%s.",e.get_message() );
        rc = mds_storage_exception_thrown;
        struct FsalReaddirResponse *p_fsal_resp =
            (struct FsalReaddirResponse *) p_fsal_resp_ref;
        p_fsal_resp->error=rc;
    }
    log->debug_log( "Read dir result:%d", rc );
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::handle_read_dir_request

/** @brief Handle incomming lookup inode requests.
 * The incomming message and
 * return pointer will be forwarded to the responsible generic method
 * handler: 'generic_handle_lookup_inode_request'.
 * @param[in] p_msg pointer to the received zmq request message
 * @param[out] p_fsal_resp_ref The resulting FSAL structure that will be
 * transformed into a zmq message und sent back
 * @return Errorcode of the generic_handle_request
 * */
int32_t MetadataServer::handle_lookup_inode_number_request(
    zmq_msg_t *p_msg, void *p_fsal_resp_ref)
{
    p_profiler->function_start();
    struct FsalLookupInodeNumberByNameRequest *p_fsalmsg = (struct FsalLookupInodeNumberByNameRequest*) zmq_msg_data(p_msg);
    int32_t rc = -1;
    try
    {
        log->debug_log("lookup name:%s.",p_fsalmsg->name);
        log->debug_log("partition_root_inode_number:%u",p_fsalmsg->partition_root_inode_number);
        log->debug_log("parent_inode_number:%u",p_fsalmsg->parent_inode_number);
        if (p_fsalmsg->parent_inode_number == p_fsalmsg->partition_root_inode_number)
        {
            string path = p_mlt_handler->get_path(p_fsalmsg->partition_root_inode_number);
            log->debug_log( "Path of inode:%llu is %s.",p_fsalmsg->partition_root_inode_number,path.c_str());
            log->debug_log( "requested Path is %s.",p_fsalmsg->name);
            string p = string(p_fsalmsg->name);
            size_t found = path.rfind("/");

            log->debug_log("found:%d.",found);
            string x = path.substr(found+1);
            log->debug_log("found:%s.",x.c_str());

            if (!p.compare(x))
            {
                log->debug_log( "Request for subtree root inode detected." );
                struct FsalLookupInodeNumberByNameResponse *p_fsal_resp = (struct FsalLookupInodeNumberByNameResponse *)p_fsal_resp_ref;
                p_fsal_resp->inode_number = p_fsalmsg->partition_root_inode_number;
                p_fsal_resp->error = 0;
                p_fsal_resp->type = lookup_inode_number_response;
                rc = 0;
            }
        }
        if (rc)
        {
            // get responsible journal
            Journal *j = get_responsible_journal(&p_fsalmsg->partition_root_inode_number);
            if (j != NULL)
            {
                // create journals responsible function pointer
                LookupInodeNumberByNameCbIf p_cb_func;
                p_cb_func = &Journal::handle_mds_lookup_inode_number_by_name;
                log->debug_log( "Lookup file %s.", p_fsalmsg->name );

                // process request and write result to fsal response pointer
                rc = generic_handle_lookup_inode_number_request(
                         p_fsalmsg,p_fsal_resp_ref, p_cb_func,j );
            }
            else
            {
                //rc = mds_no_matching_joural_found;
                log->debug_log( "No journal found. Check for referral" );
                InodeNumber inum;
                rc = handle_referral_request(p_fsalmsg,&inum);
                struct FsalLookupInodeNumberByNameResponse *p_fsal_resp = (struct FsalLookupInodeNumberByNameResponse *)p_fsal_resp_ref;
                p_fsal_resp->inode_number = inum;
                p_fsal_resp->error = 0;
                p_fsal_resp->type = lookup_inode_number_response;

            }
            // mark journal operation as done
            p_subtree_manager->remove_working_on_flag();
        }
    }
    catch (StorageException e)
    {
        // return an error response message
        log->error_log( "StorageException:%s.",e.get_message() );
        rc = mds_storage_exception_thrown;
        struct FsalLookupInodeNumberByNameResponse *p_fsal_resp =
            (struct FsalLookupInodeNumberByNameResponse *) p_fsal_resp_ref;
        p_fsal_resp->error=rc;
    }
    log->debug_log( "Lookup inode number result:%d", rc );
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::handle_lookup_inode_number_request



/** @brief Handle incomming move einode requests.
 * The incomming message and
 * return pointer will be forwarded to the responsible generic method
 * handler: 'generic_handle_move_einode_request'.
 * @param[in] p_msg pointer to the received zmq request message
 * @param[out] p_fsal_resp_ref The resulting FSAL structure that will be
 * transformed into a zmq message und sent back
 * @return Errorcode of the generic_handle_request
 * */
int32_t MetadataServer::handle_move_einode_request(
    zmq_msg_t *p_msg, void *p_fsal_resp_ref)
{
    p_profiler->function_start();
    struct FsalMoveEinodeRequest *p_fsalmsg = (struct FsalMoveEinodeRequest *) zmq_msg_data(p_msg);
    int32_t rc = -1;
    try
    {
        // extract zmq data and write them to fsal structure
        //rc = zmq_response_to_fsal_structure(&fsalmsg, sizeof(fsalmsg), p_msg);
        if (!rc || 1)
        {
            // get responsible journal
            Journal *j = get_responsible_journal(&p_fsalmsg->partition_root_inode_number);
            if (j != NULL)
            {
                // create journals responsible function pointer
                MoveEinodeCbIf p_cb_func;
                p_cb_func = &Journal::handle_mds_move_einode_request;
                log->debug_log( "Move %s, in %u.",&p_fsalmsg->old_name, p_fsalmsg->old_parent_inode_number);

                // process request and write result to fsal_response pointer
                rc = generic_handle_move_einode(p_fsalmsg,p_fsal_resp_ref,p_cb_func,j );
            }
            else
            {
                rc = mds_no_matching_joural_found;
                log->error_log( "No journal found." );
            }
            // mark journal operation as done
            p_subtree_manager->remove_working_on_flag();
        }
        else
        {
            log->error_log( "Zmq message and FSAL structure do not match" );
        }
    }
    catch (StorageException e)
    {
        // return an error response
        log->error_log( "StorageException:%s.",e.get_message() );
        rc = mds_storage_exception_thrown;
        struct FsalMoveEinodeResponse *p_fsal_resp =
            (struct FsalMoveEinodeResponse *) p_fsal_resp_ref;
        p_fsal_resp->error=rc;
    }
    log->debug_log( "Move Einode result:%d", rc );
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::handle_move_einode_request



/** 
 * @brief Triggers the ganesha config file update procedure
 * @return 0 if successful, error code otherwise
 * 
 * The function checks whether the ganesha config dir is existing and creates it
 * if needed. Then it gets the mlt from the MltHandler and passes it to the 
 * write_export_conf function. 
 * */
int32_t MetadataServer::handle_ganesha_configuration_update()
{
    p_profiler->function_start();
    int32_t rc = -1;
    try
    {
        // get the root mlt entry
        struct mlt *p_mlt = p_mlt_handler->get_mlt();
        bool success = systools_fs_mkdir(GANESHA_CONF_DIR);
        if (!success)
        {
            rc = mds_ganesha_conf_dir_error;
            log->error_log("Could not create ganesha config dir.");
        }
        else
        {
            systools_fs_cpfile(DEFAULT_GANESHA_CONF , GANESHA_MAIN_CONF, 1);
            std::string fullpath = std::string(GANESHA_CONF_DIR);
            fullpath += '/';
            fullpath.append(GANESHA_EXPORT_CONFIG_FILE_NAME);
            rc = write_export_conf(p_mlt,fullpath.c_str());
            log->debug_log("write export conf rc=%d",rc);
            systools_fs_chmod(GANESHA_CONF_DIR, "777");
            send_sighup();
        }
    }
    catch (MDSException e)
    {
        log->error_log("MDSexception cought: %s.",e.what());
        rc = mds_write_export_block_config_error;
    }
    log->debug_log("Return rc=%d",rc);
    p_profiler->function_end();
    return rc;
}

/**
 * @brief Forwards the prefix permission received from ganesha to MDS on another server
 * handler: handle_populate_prefix_perm.
 * @param p_msg [IN] data containing the prefix permissions
 * @return Errorcode of the generic_handle_request
 */
int32_t MetadataServer::handle_populate_prefix_perm(zmq_msg_t * p_msg)
{
	p_profiler->function_start();
	struct FsalPopulatePrefixPermission *p_fsalmsg = (struct FsalPopulatePrefixPermission *) zmq_msg_data(p_msg);
	struct MDSPopulatePrefixPermission mdsmsg;
	int32_t rc;
	try
	{
		mdsmsg.type = POPULATE_PREFIX_PERM;
		memcpy(&mdsmsg.pperm_data, &p_fsalmsg->pperm_data, size_populate_prefix_perm);

		string addr;
		memcpy(&addr, p_fsalmsg->server_address, sizeof(p_fsalmsg->server_address));

		uint64_t message_id;
		vector<string> server_list;
		vector<ExtractedMessage> message_list;
		PermCommunicationHandler * perm_obj = PermCommunicationHandler::get_instance();

		server_list.push_back(addr);
		int ret = perm_obj->request_reply(&message_id, (void *)&mdsmsg, size_mds_populate_prefix_perm, COM_PrefixPerm, server_list, message_list);
		if(!ret)
		{
			log->error_log("request_reply failed");
			p_profiler->function_end();
			return -1;
		}
		if(!(perm_obj->receive_reply(message_id, 1000)))
		{
			log->debug_log("No reply received");
			p_profiler->function_end();
			return -1;
		}
	}
	catch(StorageException e)
	{
		log->error_log( "StorageException:%s.",e.get_message() );
	}
    log->debug_log( "Populdate prefix perm result:%d", rc );
    p_profiler->function_end();
    return rc;
}


/**
 * @brief Forwards the update permission received from ganesha to MDS on another server
 * handler: handle_update_prefix_perm.
 * @param p_msg [IN] data containing the prefix permissions
 * @return Errorcode of the generic_handle_request
 */
int32_t MetadataServer::handle_update_prefix_perm(zmq_msg_t * p_msg)
{
	p_profiler->function_start();
	struct FsalUpdatePrefixPermission *p_fsalmsg = (struct FsalUpdatePrefixPermission *) zmq_msg_data(p_msg);
	struct MDSUpdatePrefixPermission mdsmsg;
	int32_t rc;
	try
	{
		mdsmsg.type = UPDATE_PREFIX_PERM;
		memcpy(&mdsmsg.uperm_data, &p_fsalmsg->uperm_data, size_update_prefix_perm);

		string addr;
		memcpy(&addr, p_fsalmsg->server_address, sizeof(p_fsalmsg->server_address));

		uint64_t message_id;
		vector<string> server_list;
		vector<ExtractedMessage> message_list;
		PermCommunicationHandler * perm_obj = PermCommunicationHandler::get_instance();

		server_list.push_back(addr);
		int ret = perm_obj->request_reply(&message_id, (void *)&mdsmsg, size_mds_update_prefix_perm, COM_PrefixPerm, server_list, message_list);
		if(!ret)
		{
			log->error_log("request_reply failed");
			p_profiler->function_end();
			return -1;
		}
		if(!(perm_obj->receive_reply(message_id, 1000)))
		{
			log->debug_log("No reply received");
			p_profiler->function_end();
			return -1;
		}
	}
	catch(StorageException e)
	{
		log->error_log( "StorageException:%s.",e.get_message() );
	}
    log->debug_log( "Update prefix perm result:%d", rc );
    p_profiler->function_end();
    return rc;
}

/**
 * @brief Write the export.conf. Uses the ebwriter functions
 * @param[in] p_mlt pointer to mlt structure to turn into export.conf
 * @param[in] p_path path to store the config file as.
 * @return Zero in success, non-zero if an error occures.
 * */
int32_t MetadataServer::write_export_conf(struct mlt *p_mlt, const char *p_path)
{
    p_profiler->function_start();
    int32_t rc = -1;
    // acquire export.conf mutex
    pthread_mutex_lock(&mutex_write_export_block);
    try
    {
        // use external library to create export.conf
        const Server mds = p_mlt_handler->get_my_address();
        struct asyn_tcp_server c_mds = {(char *) mds.address.c_str(), mds.port};
        rc = create_export_block_conf(p_mlt, p_path, &c_mds);
        if (rc)
        {
            log->error_log( "Mds write export config file failed.rc=%d", rc );
            rc = mds_write_export_block_config_error;
        }
        else
        {
            log->debug_log( "Mds export block written." );
        }
    }
    catch (...)
    {
        log->error_log( "Error while writing export config." );
        
        pthread_mutex_unlock(&mutex_write_export_block);
        p_profiler->function_end();
        throw MDSException("Error while writing export config.");
    }
    // release mutex and quit
    pthread_mutex_unlock(&mutex_write_export_block);
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::write_export_conf

/**
 * @brief Send a sighup to the Ganesha process which results in a
 * configuration reload. 'send_signal' is implemented in ebwriter.c.
 *
 * @return zero on success, non-zero if signaling fails.
 * */
int32_t MetadataServer::send_sighup()
{
    p_profiler->function_start();
    int32_t rc = -1;
    try
    {
        // end a sighup to the ganesha process. Use external library
        log->debug_log("Ganesha bin name is %s.",ganesha_process_name.c_str());
        p_profiler->function_sleep();
        rc = send_signal( ganesha_process_name.c_str(), (short) 1 );
        p_profiler->function_wakeup();
        if (rc)
        {
            log->error_log( "Sending SIGHUP failed. rc=%d", rc );
            rc = mds_send_sighup_error;
        }
        else
        {
            log->debug_log( "SIGHUP signal sent." );
        }
    }
    catch(...)
    {
        log->error_log( "Exception raised while sending SIGHUP" );
        rc = mds_send_sighub_exception;
    }
    p_profiler->function_end();
    return rc;
} // end of MetadataServer::send_sighup


/** 
 * @brief Set a reference to the storage abstractoin layer
 * @param[in] p_sal pointer to the storage abstraction layer instance
 * @return 0 if successful, failure else
 * */
int32_t MetadataServer::set_storage_abstraction_layer(StorageAbstractionLayer *p_sal)
{
    p_profiler->function_start();
    int rc=1;
    if (p_sal != NULL)
    {
        this->p_sal = p_sal;
        rc = 0;
    }
    p_profiler->function_end();
    return rc;
}

int32_t MetadataServer::handle_pnfs_create_session(gid_t g, uid_t u, ClientSessionId *scid)
{
    *scid = p_sessionman->addSession(g,u);
    //printf("CreateSession %llu\n",*scid);
    return 0;
}

int32_t MetadataServer::handle_pnfs_byterangelock(ClientSessionId csid, InodeNumber inum, uint64_t start, uint64_t end, time_t *expires)
{
    log->debug_log("brl request detected:inum:%llu",inum);
    //printf("Byyyterangelock request \nSession:%llu\nInum:%llu\n%llu:%llu.\n",csid,inum,start,end);
    if (! p_byterangelock->lockObject(&inum, &csid, &start, &end,expires))
    {
        //std::list<socketid_t> *p_sockets = new std::list<socketid_t>;
        lock_exchange_struct lock;
        //p_sockets.push_back(0);
        //p_sockets.push_back(1);        
        lock.inum = inum;
        lock.start = start;
        lock.end = end;
        lock.owner = csid;
        lock.expires = *expires;
        log->debug_log("bla");
        //this->p_cpclient->multicast_distribute_Lock(p_sockets,&lock);
        log->debug_log("rc:0");
        return 0;
    }
    log->debug_log("rc:-1");
    return -1;
}

/**
 * @TODO check if release came from sender
 * @param csid
 * @param inum
 * @param start
 * @param end
 * @return 
 */
int32_t MetadataServer::handle_pnfs_releaselock(ClientSessionId csid, InodeNumber inum, uint64_t start, uint64_t end)
{
    log->debug_log("brl release detected:inum:%llu",inum);
    //printf("Byyyterangelock request \nSession:%llu\nInum:%llu\n%llu:%llu.\n",csid,inum,start,end);
    if (! p_byterangelock->releaseLockObject(inum,start))
    {
        /*std::list<socketid_t> *p_sockets = new std::list<socketid_t>;
        lock_exchange_struct lock;
        //p_sockets.push_back(0);
        //p_sockets.push_back(1);        
        lock.inum = inum;
        lock.start = start;
        lock.end = end;
        lock.owner = csid;
        log->debug_log("bla");**/
        //this->p_cpclient->multicast_distribute_Lock(p_sockets,&lock);
        log->debug_log("rc:0");
        return 0;
    }
    log->debug_log("rc:-1");
    return -1;
}

int32_t MetadataServer::handle_pnfs_dataserver_layout(uint32_t *count)
{
    int32_t rc = this->p_dataserver->get_server_count(count);
    log->debug_log("cnt:%u",count);
    return rc;
}

int32_t MetadataServer::handle_pnfs_dataserver_address(serverid_t *dsid, ipaddress_t *address, uint16_t *port)
{
    log->debug_log("received: request for id:%llu.",*dsid);
    return this->p_dataserver->get_server_address(dsid,address,port);
}

int32_t MetadataServer::get_filelayout(InodeNumber inum, filelayout *fl)
{
    int rc = -1;
    struct EInode ei;
    if (!get_einode(inum,&ei))
    {
        memcpy(fl,&ei.inode.layout_info[0],sizeof(filelayout));
    }
    return rc;    
}

/** 
 @todo support more than one journal...
 */
int32_t MetadataServer::get_einode(InodeNumber inum, struct EInode *p_einode)
{
    int rc=-1;
    Journal *j = get_responsible_journal(&root_inode_number);
    if (j!=NULL)
    {
        rc = j->handle_mds_einode_request(&inum, p_einode);
    }
    return rc;
}


/** 
 * @brief Convert attributes of the create message to string
 * @param[out] str the string value that holds the string representation of the attributes
 * @param[in] p_attrs A pointer to the list of attributes
 * */

void einode_create_attrs_to_string(std::string &str, inode_create_attributes_t *p_attrs)
{
    std::stringstream out;
    char stringbuffer[255];

    std::string name =  p_attrs->name;
    
    std::string uid;
    snprintf( stringbuffer, 255 ,"%lu",(long unsigned int) p_attrs->uid); 
    uid = stringbuffer;
    memset(stringbuffer, ' ', 255); 

    std::string gid; 
    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int) p_attrs->gid); 
    gid = stringbuffer;
    memset(stringbuffer, ' ', 255); 

    std::string mode; 
    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int)p_attrs->mode); 
    mode = stringbuffer;
    memset(stringbuffer, ' ', 255); 

    std::string size; 
    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int)p_attrs->size); 
    size = stringbuffer;
    memset(stringbuffer, ' ', 255); 

 
    str =  "\t\t name: "  + name + "\n";
    str += "\t\t gid: " + gid + "\n";
    str += "\t\t uid: " + uid + "\n";
    str += "\t\t mode: " + mode + "\n";
    str += "\t\t size: " + size + "\n";
}


/** 
 * @brief Convert attributes an EInode to string
 * @param[out] str the string value that holds the string representation of the attributes
 * @param[in] p_attrs A pointer to the einode
 * */

void einode_to_string(std::string &str, EInode *p_einode)
{
    std::stringstream out;
    char stringbuffer[255];

    std::string name =  p_einode->name;
    
    std::string uid;
    snprintf( stringbuffer, 255 ,"%lu",(long unsigned int) p_einode->inode.uid); 
    uid = stringbuffer;
    memset(stringbuffer, ' ', 255); 

    std::string gid; 
    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int) p_einode->inode.gid); 
    gid = stringbuffer;
    memset(stringbuffer, ' ', 255); 

    std::string mode; 
    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int)p_einode->inode.mode); 
    mode = stringbuffer;
    memset(stringbuffer, ' ', 255); 

    std::string size; 
    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int)p_einode->inode.size); 
    size = stringbuffer;
    memset(stringbuffer, ' ', 255); 
   
    std::string link_count;
    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int)p_einode->inode.link_count); 
    link_count = stringbuffer;
    memset(stringbuffer, '\0', 255); 
    
    std::string inode_number;
    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int)p_einode->inode.inode_number); 
    inode_number = stringbuffer;
    memset(stringbuffer, '\0', 255); 

    std::string atime;
    snprintf( stringbuffer, 255 ,"%lld",(long long int) p_einode->inode.atime);
    atime = stringbuffer;
    memset(stringbuffer, ' ', 255); 
    
    std::string mtime;
    snprintf( stringbuffer, 255 ,"%lld", (long long int)p_einode->inode.mtime); 
    mtime = stringbuffer;
    memset(stringbuffer, '\0', 255); 


	
 
    str +=  "\t\t name: "  + name + "\n";
    str += "\t\t inode_number: " + inode_number + "\n";
    str += "\t\t gid: " + gid + "\n";
    str += "\t\t uid: " + uid + "\n";
    str += "\t\t mode: " + mode + "\n";
    str += "\t\t size: " + size + "\n";
    str += "\t\t st_nlink: " + link_count + "\n";
    str += "\t\t atime: " + atime + "\n";
    str += "\t\t mtime: " + mtime + "\n";

}


/** 
 * @brief Convert attributes of the update message to string
 * @param[out] str the string value that holds the string representation of the attributes
 * @param[in] p_attrs A pointer to the list of attributes
 * @param[in] bitfield A bitfield used the check which attributes are set
 * */

void einode_update_attrs_to_string(std::string &str, inode_update_attributes_t *p_attrs, int  bitfield)
{
    std::stringstream out;
    char stringbuffer[255]; 
    
    int attribute_bitfield = bitfield;

    std::string atime; 
    std::string mtime; 
    std::string uid;
    std::string gid; 
    std::string mode; 
    std::string size; 
    std::string link_count; 

    atime = mtime = uid = gid = mode = size = link_count = "Not supplied";

    if(IS_ATIME_SET(attribute_bitfield))
    {
	    snprintf( stringbuffer, 255 ,"%lld",(long long int) p_attrs->atime);
	    atime = stringbuffer;
	    memset(stringbuffer, ' ', 255); 
    }

    if(IS_MTIME_SET(attribute_bitfield))
    {
	    snprintf( stringbuffer, 255 ,"%lld", (long long int)p_attrs->mtime); 
	    mtime = stringbuffer;
	    memset(stringbuffer, '\0', 255); 
    }

    if(IS_UID_SET(attribute_bitfield))
    {
	    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int)p_attrs->uid); 
	    uid = stringbuffer;
	    memset(stringbuffer, '\0', 255); 
    }
    
    if(IS_GID_SET(attribute_bitfield))
    {
	    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int)p_attrs->gid); 
	    gid = stringbuffer;
	    memset(stringbuffer, '\0', 255); 
    }

    if(IS_MODE_SET(attribute_bitfield))
    {
	    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int)p_attrs->mode); 
	    mode = stringbuffer;
	    memset(stringbuffer, '\0', 255); 
    }
    
    if(IS_SIZE_SET(attribute_bitfield))
    {
	    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int)p_attrs->size); 
	    size = stringbuffer;
	    memset(stringbuffer, '\0', 255); 
    }
    
    if(IS_NLINK_SET(attribute_bitfield))
    {
	    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int)p_attrs->st_nlink); 
	    link_count = stringbuffer;
	    memset(stringbuffer, '\0', 255); 
    }
	    

    str += "\t\t mtime: " + mtime + "\n";
    str += "\t\t atime: " + atime + "\n";
    str += "\t\t gid: " + gid + "\n";
    str += "\t\t uid: " + uid + "\n";
    str += "\t\t mode: " + mode + "\n";
    str += "\t\t size: " + size + "\n";
    str += "\t\t link_count: " + link_count + "\n\n";
}


/** 
 * @brief Convert the readdir return msg to string
 * @param[out] str the string value that holds the string representation of the attributes
 * @param[in] p_readdir A pointer to the ReadDirReturn structure
 * */

void readdir_return_to_string(std::string &str, ReadDirReturn *p_readdir)
{
    std::stringstream out;
    char stringbuffer[255];
    
    std::string nodes_len;
    snprintf( stringbuffer, 255 ,"%lu",(long unsigned int) p_readdir->nodes_len); 
    nodes_len = stringbuffer;
    memset(stringbuffer, ' ', 255); 

    std::string dir_len; 
    snprintf( stringbuffer, 255 ,"%lu", (long unsigned int) p_readdir->dir_size); 
    dir_len = stringbuffer;
    memset(stringbuffer, ' ', 255); 

    str += "\t\t nodes_len: " + nodes_len + "\n";
    str += "\t\t dir_size: " + dir_len + "\n";

    int i = 0;
    for(i = 0; i < p_readdir->nodes_len; i++)
    {
        str += "\t\t * * * * * * * *\n";
   	einode_to_string( str, &p_readdir->nodes[i] ); 
    }
}



/** 
 * @brief Initialises the ressources which are needed for the tracing of messages.
 * */

void MetadataServer::setup_message_tracing()
{
	mutex_trace_file = PTHREAD_MUTEX_INITIALIZER;
	trace_file.open( "/tmp/mds_tracefile.log", ios::out);	
}


/** 
 * @brief Traces incoming message.
 * @param[in] request The request that was issued to the mds
 * 
 * */

void MetadataServer::trace_incoming_message( zmq_msg_t* request, uint32_t message_id)
{
    uint32_t dispatch;
    memcpy( &dispatch, zmq_msg_data( request), sizeof(dispatch) );
    
    std::stringstream out;
    
    switch(dispatch)
    {

    case move_einode_request:
    {
    	struct FsalMoveEinodeRequest fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), request );
 
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Move Einode request received" << std::endl;	
		out << "Arguments:" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Root inode number: " << fsalmsg.partition_root_inode_number << std::endl;	
		out << "\t Old parent inode number: " << fsalmsg.old_parent_inode_number << std::endl;	
		out << "\t New parent inode number: " << fsalmsg.new_parent_inode_number << std::endl;	
		out << "\t Old name: " << fsalmsg.old_name << std::endl;	
		out << "\t New name: " << fsalmsg.new_name << std::endl;	
		out << "======================" << std::endl;
	}

	break;
    }
    
    case einode_request:
    {
    	struct FsalEInodeRequest fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), request );
 
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Einode request received" << std::endl;	
		out << "Arguments:" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Inode Number: " << fsalmsg.inode_number << std::endl;	
		out << "\t Root Inode Number: " << fsalmsg.partition_root_inode_number << std::endl;	
		out << "======================" << std::endl;
	}

	break;
    }
    case create_file_einode_request:
    {
	struct FsalCreateEInodeRequest fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), request );
 	
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Create einode request received" << std::endl;	
		out << "Arguments:" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Parent Inode Number: " << fsalmsg.parent_inode_number << std::endl;	
		out << "\t Root Inode Number: " << fsalmsg.partition_root_inode_number << std::endl;	
		out << "\t Attributes:" << std::endl;	


		std::string tmp;
		einode_create_attrs_to_string(tmp, &fsalmsg.attrs);
		out << tmp;
		
		out << "======================" << std::endl;
	}
	break;
    }
    case delete_inode_request:
    {
	struct FsalDeleteInodeRequest fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), request );
 
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Delete Einode request received" << std::endl;	
		out << "Arguments:" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Inode Number: " << fsalmsg.inode_number << std::endl;	
		out << "\t Root Inode Number: " << fsalmsg.partition_root_inode_number << std::endl;	
		out << "======================" << std::endl;
	}

	break;
    }
    case read_dir_request:
    {
	struct FsalReaddirRequest fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), request );
 
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Readdir request received" << std::endl;	
		out << "Arguments:" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Inode Number: " << fsalmsg.inode_number << std::endl;	
		out << "\t Root Inode Number: " << fsalmsg.partition_root_inode_number << std::endl;	
		out << "\t Readdir Offset: " << fsalmsg.offset << std::endl;	
		out << "======================" << std::endl;
	}
	break;
    }
    case lookup_inode_number_request:
    {
	struct FsalLookupInodeNumberByNameRequest fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), request );
 
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Lookup request received" << std::endl;	
		out << "Arguments:" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Parent Inode Number: " << fsalmsg.parent_inode_number << std::endl;	
		out << "\t Root Inode Number: " << fsalmsg.partition_root_inode_number << std::endl;	
		out << "\t Name: " << fsalmsg.name << std::endl;	
		out << "======================" << std::endl;
	}

	break;
    }
    case update_attributes_request:
    {
	struct FsalUpdateAttributesRequest fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), request );
 	
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out <<  "Update einode request received" << std::endl;	
		out << "Arguments:" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Inode Number: " << fsalmsg.inode_number << std::endl;	
		out << "\t Root Inode Number: " << fsalmsg.partition_root_inode_number << std::endl;	
		out << "Attributes:" << std::endl;	

		std::string tmp;
		einode_update_attrs_to_string(tmp, &fsalmsg.attrs, fsalmsg.attribute_bitfield);
		out << tmp;
		out << "======================" << std::endl;
	}
	break;
    }
    case parent_inode_number_lookup_request:
    {
	break;
    }
    default:
    {
	/* Unknown message received*/
	break;
    }

    }

    trace_file << out.str();
    trace_file.flush();
}

/** 
 * @brief Traces outgoing messages.
 * @param[in] request The request that was issued to the mds
 * 
 * */

void MetadataServer::trace_outgoing_message( zmq_msg_t* response, uint32_t message_id)
{
    uint32_t dispatch;
    memcpy( &dispatch, zmq_msg_data( response), sizeof(dispatch) );
    
    std::stringstream out;
    
    switch(dispatch)
    {

    case file_einode_response:
    {
    	struct FsalFileEInodeResponse fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), response );
 
	if(!rc)
	{
		std::string einode_string;

		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Sending Einode response" << std::endl;	
		out << "Arguments:" << std::endl;
		out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		
		einode_to_string(einode_string, &fsalmsg.einode);
		
		out << einode_string << std::endl;
		out << "\t Error: " << (int) fsalmsg.error << std::endl;	
		out << "======================" << std::endl;
	}

	break;
    }
    case create_inode_response:
    {
	struct FsalCreateInodeResponse fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), response );
 	
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Sending create einode response" << std::endl;	
		out << "Arguments:" << std::endl;	
		out << "\t New inode Number: " << fsalmsg.inode_number << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Error: " << (int)fsalmsg.error << std::endl;	
		out << "======================" << std::endl;
	}
	break;
    }
    case delete_inode_response:
    {
	struct FsalDeleteInodeResponse fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), response );
 
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Sending Delete einode response" << std::endl;	
		out << "Arguments:" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Inode Number: " << fsalmsg.inode_number << std::endl;	
		out << "\t Error: " << (int)fsalmsg.error << std::endl;	
		out << "======================" << std::endl;
	}

	break;
    }

    case move_einode_response:
    {
	struct FsalMoveEinodeResponse fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), response );
 
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Sending Move einode response" << std::endl;	
		out << "Arguments:" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Error: " << (int)fsalmsg.error << std::endl;	
		out << "======================" << std::endl;
	}

	break;
    }
 

    case read_dir_response:
    {
	struct FsalReaddirResponse fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), response );
 
	if(!rc)
	{
		std::string read_values;
	
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Sending readdir response" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "Arguments:" << std::endl;	
		out << "\t Error: " << (int) fsalmsg.error << std::endl;	
		
		if( (int)fsalmsg.error == 0){
			readdir_return_to_string(read_values, &fsalmsg.directory_content);
		}

		
		out << read_values;
		out << "======================" << std::endl;
	}
	break;
    }
    case lookup_inode_number_response:
    {
	struct FsalLookupInodeNumberByNameResponse fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), response );
 
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out << "Sending Lookup response" << std::endl;	
		out << "Arguments:" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Inode Number: " << fsalmsg.inode_number << std::endl;	
		out << "\t Error: " << (int)fsalmsg.error << std::endl;	
		out << "======================" << std::endl;
	}

	break;
    }
    case update_attributes_response:
    {
	struct FsalUpdateAttributesResponse fsalmsg;
	int rc = 0;
        rc = zmq_response_to_fsal_structure( &fsalmsg, sizeof(fsalmsg), response );
 	
	if(!rc)
	{
		out << std::endl;
		out << std::endl;
		out << "======================" << std::endl;
		out << "Message id: " << message_id << std::endl;
		out <<  "Sending Update einode response" << std::endl;	
		out << "Arguments:" << std::endl;	
                out << "\t Seqnum: " << fsalmsg.seqnum << std::endl;	
		out << "\t Inode Number: " << fsalmsg.inode_number << std::endl;	
		out << "\t Error: " << (int) fsalmsg.error << std::endl;	
		out << "======================" << std::endl;
	}
	break;
    }
    case parent_inode_number_lookup_request:
    {
	break;
    }
    default:
    {
	/* Unknown message received*/
        out << "unknown" << std::endl;
	break;
    }

    }

    trace_file << out.str();
    trace_file.flush();
}


