/**
 * @brief Main Server starting up all components
 * @author Markus Mäsker, maesker@gmx.net
 * @date 25.7.11
 * @file main.cpp
 * @version 0.2
 */

#include <list>
#include <string>
#include <sstream>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "custom_protocols/pnfs/PnfsProtocol.h"
#include "message_types.h"
#include "logging/Logger.h"
#include "mm/mds/MetadataServer.h"
#include "mm/mds/MessageRouter.h"
#include "mm/storage/storage.h"
#include "mm/storage/MountHelper.h"
#include "ConfigurationManager.h"
#include "mm/journal/JournalManager.h"
#include "mm/journal/JournalException.h"
#include "coco/coordination/MltHandler.h"
#include "coco/communication/RecCommunicationServer.h"
#include "coco/loadbalancing/MDistributionManager.h"
#include "coco/loadbalancing/LBConf.h"
#include "coco/administration/AdminCommunicationHandler.h"
#include "coco/prefix_perm/PrefixPermCommunicationHandler.h"
#include "coco/dao/DistributedAtomicOperations.h"
#include "exceptions/AbstractException.h"
#include "exceptions/MDSException.h"

using namespace std;

int fsal_worker_killswitch = 0;
int pnfs_worker_killswitch = 0;
int pnfs_worker_threads = 40;
void* metadata_server_worker_pnfs(void *p_v);
/** 
 * @var config relative path to config file
 * */
#define config "conf/main.conf"


const size_t size_fsal_lookup_inode_number_by_name = sizeof(struct FsalLookupInodeNumberByNameResponse);
const size_t size_fsal_readdir_response       = sizeof(struct FsalReaddirResponse);
const size_t size_fsal_file_einode_response   = sizeof(struct FsalFileEInodeResponse);
const size_t size_fsal_create_inode_response  = sizeof(struct FsalCreateInodeResponse);
const size_t size_fsal_delete_inode_response  = sizeof(struct FsalDeleteInodeResponse);
const size_t siete_fsal_fallback_response     = sizeof(struct FsalFallbackErrorMessage);
const size_t size_fsal_update_attributes_response = sizeof(struct FsalUpdateAttributesResponse);
const size_t size_fsal_parent_inode_number_lookup_response = sizeof(struct FsalParentInodeNumberLookupResponse);
const size_t size_fsal_inode_number_hierarchy_response = sizeof(struct FsalInodeNumberHierarchyResponse);
const size_t size_fsal_unknown_request_response = sizeof(struct FsalUnknownRequestResponse);
const size_t size_fsal_move_einode_response = sizeof(struct FsalMoveEinodeResponse);
const size_t size_fsal_prefix_perm_populate_response = sizeof(struct FsalPrefixPermPopulateRespone);
const size_t size_fsal_prefix_perm_update_response = sizeof(struct FsalPrefixPermUpdateRespone);
const size_t size_mds_populate_prefix_perm = sizeof(struct MDSPopulatePrefixPermission);
const size_t size_mds_update_prefix_perm = sizeof(struct MDSUpdatePrefixPermission);
const size_t size_populate_prefix_perm = sizeof(prefix_perm_populate_t);
const size_t size_update_prefix_perm = sizeof(prefix_perm_update_t);


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

    cm->register_option("port","Port of the MDS");
    cm->register_option("address","IP address of the MDS");
    
    cm->register_option("storage_type", "PARTITION or FILE");
    cm->register_option("storage_dir","Directory to write the journal and Storage when storage_type=FILE");
    cm->register_option("storage_devices", "Comma separated list of devices used for PARTITION storage_type");
    cm->register_option("mount_dir", "Directory to mount storage partitions");
    cm->register_option("fs_type", "File-system of storage partitions");
    cm->register_option("ds.pw", "Dataserver ssh password");    

    cm->parse();
    return cm;
}

/**
 * @brief Start the RecCommunicationServer
 * @param[in] pntr unused void pointer
 * */
void* run_receive_server(void* pntr)
{
    RecCommunicationServer::get_instance().receive();
    return NULL;
}

/**
 * @brief Create and start MDistributionManager
 * @param[in] psal pointer to storage abstraction layer
 * */
void* run_loadbalancer(void* psal)
{    
    MDistributionManager::create_instance((StorageAbstractionLayer*) psal);
    MDistributionManager::get_instance()->run();
    return NULL;
}

/**
 * @brief Create list of devices from comma separated configuration string
 *
 * @param[in] devices_string Comma separated list of devices
 * @retval List of devices
 */
std::list<std::string> *create_devices_list(char *devices_string)
{
	std::list<std::string> *result = new std::list<std::string>();
	char *device;
	device = strtok(devices_string, ", ");

	while(device != NULL)
	{
		result->push_back(std::string(device));
		device = strtok(NULL, ", ");
	}

	return result;
}

/**
 * @brief Create partitions for existing subtrees, if not done
 *
 * @param[in] mlt MltHandler instance
 * @param[in] pm PartitionManager instance
 */
void create_partitions(MltHandler *mlt, PartitionManager *pm)
{
	std::vector<InodeNumber> my_partitions;
	mlt->get_my_partitions(my_partitions);

	for(std::vector<InodeNumber>::iterator it = my_partitions.begin(); it != my_partitions.end(); it++)
	{
		try
		{
			Partition *p = pm->get_partition(*it);
			if(p == NULL)
			{
				// Partition not found, create it!
				Partition *partition = pm->get_free_partition();
				char *address = (char *) mlt->get_my_address().address.data();
				partition->set_owner(address);
				partition->set_root_inode(*it);
				partition->mount_rw();
			}
			// Partition still existing, do nothing!
		}
		catch(StorageException)
		{
			// Partition not found, create it!
			Partition *partition = pm->get_free_partition();
			char *address = (char *) mlt->get_my_address().address.data();
			partition->set_owner(address);
			partition->set_root_inode(*it);
			partition->mount_rw();
		}
	}
}

/**
 * @brief Create StorageAbstractionLayer instance
 *
 * @param[in] cm ConfigurationManager instance
 * @param[in] log Logger instance
 * @param[in] mlt HltHandler instance
 * @retval StorageAbstractionLayer instance
 */
StorageAbstractionLayer *create_sal_instance(ConfigurationManager *cm, Logger *log, MltHandler *mlt)
{
	StorageAbstractionLayer *result;

	if (strcasecmp(cm->get_value("storage_type").data(), "PARTITION") == 0)
	{
		log->debug_log("Creating partition-based storage");
		char *host = (char *) mlt->get_my_address().address.data();
		vector<Server> server_list;
		mlt->get_server_list(server_list);
		int total_hosts = server_list.size();
		int rank = mlt->get_my_rank();

		char *devices_string = (char *) cm->get_value("storage_devices").data();
		if (devices_string == NULL || strcmp("", devices_string) == 0)
		{
			log->error_log("No storage devices given");
			return NULL;
		}
		std::list<std::string> *devices = create_devices_list(devices_string);

		char *mount_dir = (char *) cm->get_value("mount_dir").data();
		if (mount_dir == NULL || strcmp("", mount_dir) == 0)
		{
			log->error_log("No mount_dir given");
			return NULL;
		}

		char *fs_type = (char *) cm->get_value("fs_type").data();
		if (fs_type == NULL || strcmp("", fs_type) == 0)
		{
			log->error_log("No fs_type given");
			return NULL;
		}

		MountHelper *mh = new MountHelper(fs_type, 0);
		result = new StorageAbstractionLayer(devices, mount_dir, mh, host, rank,
				total_hosts);

		create_partitions(mlt, result->get_partition_manager());
	}
	else
	{
		char *device_identifier;
		device_identifier = (char *) cm->get_value("storage_dir").data();

		if (device_identifier == NULL || strcmp(device_identifier, "") == 0)
		{
			log->error_log("Invalid device, using /tmp/storage for metadata storage");
			device_identifier = strdup("/tmp/storage/");
		}

		struct stat st;
		if (!(stat(device_identifier, &st) == 0))
			mkdir(device_identifier, 700);

		result = new StorageAbstractionLayer(device_identifier);
	}

	return result;
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
        system ("export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib");
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
        std::string abspath = basedir;
        abspath.append("/");
        abspath.append(config);
        // initialize the configuration manager
        ConfigurationManager *cm = init_cm(argc,argv,abspath);
        std::string logfile = cm->get_value("logfile");
        int loglevel = atoi(cm->get_value("loglevel").c_str());
        int cmdoutput = atoi(cm->get_value("cmdoutput").c_str());
        //storage_dir = cm->get_value('storage_dir');
        
        // create logger instance and initialize
        Logger *log = new Logger();
        log->set_log_level(loglevel);
        log->set_console_output((bool)cmdoutput);

        std::string log_location = std::string(DEFAULT_LOG_DIR) + "/" + logfile; 
        log->set_log_location( log_location );
        log->debug_log( "Logging to location: %s" , log_location.c_str() );

        // get MltHandler instance
        MltHandler *p_mlt_handler = &(MltHandler::get_instance());
        if (p_mlt_handler == NULL)
        {
            log->error_log("MltHandler instance not received.");
            return -1;
        }
        log->debug_log("MltHandler instance received.");

        // TODO Test if mlt needs to be read.
        rc=p_mlt_handler->read_from_file(MLT_PATH);
        if (rc)
        {
            log->error_log("Error reading MLT from %s.",MLT_PATH);
            return -1;
        }
        // write server address to mlt
        uint16_t port = atoi(cm->get_value("port").c_str());
        std::string address = cm->get_value("address");
        const Server this_box = {address,port};
        p_mlt_handler->set_my_address(this_box);


        //initialize sockets of SendCommunicationServer
        rc = SendCommunicationServer::get_instance().set_up_external_sockets();
        if(rc)
        {
            log->error_log("Error while initializing sockets for SendCommunicationServer.");
            return -1;
        }
        // initialize socket of RecCommunicationServer
        rc = RecCommunicationServer::get_instance().set_up_receive_socket();
        if(rc)
        {
            log->error_log("Error while initializing socket for RecCommunicationServer.");
            return -1;
        }
        log->debug_log("Sockets for external sending and receiving created");


        // create journal manager instance
        JournalManager *jm = JournalManager::get_instance();
        if (!jm)
        {
            log->error_log("Error while creating journal manager object.");
            return -1;
        }

        // create and initialize storage abstraction layer
        StorageAbstractionLayer *storage_abstraction_layer = create_sal_instance(cm, log, p_mlt_handler);
        if (!storage_abstraction_layer)
        {
            log->error_log("Error while creating StorageAbstractionLayer object");
            return -1;
        }
        log->debug_log("StorageAbstractionLayer object created.");

        // register storage abstraction layer at journal manager
        if (jm->get_instance()->set_storage_abstraction_layer(storage_abstraction_layer))
        {
            log->error_log("Unable to register sal at journal manager");
        }

        // create server journal
        jm->get_instance()->create_server_journal();

        //start communication server threard
        pthread_t thread1;
        if (pthread_create(&thread1, NULL, run_receive_server, NULL) != 0)
        {
            log->error_log("Error while creating communication server thread");
            return -1;
        }
        log->debug_log("CommunicationServer thread started");

	
	DistributedAtomicOperations* dao = &(DistributedAtomicOperations::get_instance());
        dao->do_recovery();

        //start loadbalancer thread
        pthread_t thread2;
        if (pthread_create(&thread2, NULL, run_loadbalancer, storage_abstraction_layer) != 0)
        {
            log->error_log("Error while creating loadbalancing thread");
            return -1;
        }

	//wait until loadbalancing.conf was read
	while(LOAD_BALANCER_ENABLED == -1) usleep(50000); 

	if (LOAD_BALANCER_ENABLED)	
	{
		log->debug_log("Load balancer started");	
	}
	else
	{
		log->debug_log("Load balancer is disabled!!! You can enable it in 'loadbalancing.conf.'");
	}
	

        AdminCommunicationHandler::create_instance();
        AdminCommunicationHandler *com_handler;
        com_handler = AdminCommunicationHandler::get_instance();
        if (com_handler == NULL)
        {
            log->error_log("Error while creating AdminCommunicationHandler object");
            return -1;
        }

        //start Administration daemon
        com_handler->init(COM_AdminOp);
        com_handler->start();
        log->debug_log("AdminCommunicationHandler object created.");

        PermCommunicationHandler::create_instance();
        PermCommunicationHandler *perm_com_handler;
        perm_com_handler = PermCommunicationHandler::get_instance();
        if (perm_com_handler == NULL)
        {
            log->error_log("Error while creating PermCommunicationHandler object");
            return -1;
        }

        //start Administration daemon
        perm_com_handler->init(COM_PrefixPerm);
        perm_com_handler->start();
        log->debug_log("PermCommunicationHandler object created.");

        MessageRouter *p_mr = new MessageRouter(basedir);
        p_mr->run();
        
        //printf("Basedir:%s.\n",basedir.c_str());
        // create metadata server instance
        MetadataServer *mds = MetadataServer::create_instance(jm->get_instance(),p_mlt_handler,basedir);
        if (!mds)
        {
            log->error_log("Error while creating MetadataServer object");
            return -1;
        }
        log->debug_log("Metadataserver object created.");

        // create some worker threads, 'worker_thread' defined in mds.conf
        int i=0;
        for(i = 0; i < pnfs_worker_threads; i++)
        {
            pthread_t worker_thread;
            rc = pthread_create(&worker_thread, NULL,metadata_server_worker_pnfs , mds );
            if (rc)
            {
                // throw an exception on fail
                log->error_log("mds thread not created:rc=%d.",rc);
                throw MDSException("worker thread not created.");
            }
        }
        log->debug_log("mds thread creation done. %d treads created.",i);

        rc = mds->set_storage_abstraction_layer(storage_abstraction_layer);
        rc = mds->set_dataserver_sshpw(cm->get_value("ds.pw"));
        // start metadata server
        rc = mds->run();
        if (rc)
        {
            log->error_log("Failed to start mds routines. rc:%d. System exit.",rc);
            shutdown=1;
        }
        log->debug_log("Metadataserver running.");
        

        char input;
        while(!shutdown)
        {
            printf ("'y' to quit:\n");
            input = getchar();
            if (input =='y') 
            {
                break;
            }
            sleep(60);
            
        }
        printf("Shutting down.\n");
        exit(0);
        // deleting created instances.
        pnfs_worker_killswitch=1;
        //delete cm;
        delete jm;
        //mds->stop();
        delete storage_abstraction_layer;
        delete com_handler;
        //TODO kill loadbalancer and receiveserver
        log->debug_log("system shutdown.");
        delete log;
    }
    catch(SubtreeManagerException e)
    {
        printf("SubtreemanagerException:\n%s\n",e.what());
    }
    catch(JournalException e)
    {
        printf("JournalException:\n%s\n",e.get_msg().c_str());
    }
    catch(MDSException e)
    {
        printf("MDSException raised:%s.\n",e.what());
    }
    return 0;
}



/** 
 * @brief Free zmq message data
 * @param[in] data pointer to the zmq message data to free
 * @param[in] hint dont know, in my case always  null.
 * 
 * This function is used by the zmq library to free the data after sending a 
 * message.
 * 
 * 
 * */
inline void my_free_zmq_message(void *data, void *hint){}


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
void* metadata_server_worker_pnfs(void *p_v)
{
    Pc2fsProfiler *p_profiler = Pc2fsProfiler::get_instance();
    p_profiler->function_start();
    // zmq messaging context, to connect to message router
    zmq::context_t context(1);

    PnfsProtocol *protocol = new PnfsProtocol();
    uint32_t maxmsglen = protocol->get_max_msg_len();
    
    struct custom_protocol_reqhead_t head;
    size_t headsize = sizeof(custom_protocol_reqhead_t);
    size_t respsize;
    
    struct fixed_fsalmsg_attrs_t {enum MsgType type; Fsal_Msg_Sequence_Number seqnum;};
    struct fixed_fsalmsg_attrs_t fixed_fsalmsg_attrs;
    const size_t sizeof_fixed_fsal_attrs = sizeof(fixed_fsalmsg_attrs);
    
    
    // metadata objects pointer to access the handler functions
    MetadataServer* p_obj = (MetadataServer*)p_v;
    
    p_obj->log->debug_log( "protocolled worker thread running." );

    // create zmq objects
    zmq::socket_t responder( context, ZMQ_REP );

    // connect to message router
    responder.connect( ZMQ_ROUTER_BACKEND );
    // create request and response message objects
    zmq_msg_t request;
    // initialize request message with provided message size
    zmq_msg_init_size( &request, maxmsglen );

    int32_t rc;

    void *response_structure_mem = malloc(maxmsglen);

    //used to identify traced messages
    uint32_t message_id;
    srand((unsigned)time(0));   
                
    //struct mallinfo mi_start;
    while( !pnfs_worker_killswitch )
    {    
        zmq_msg_t response;
        // receive new message
        memset(zmq_msg_data(&request),'\0', headsize);
        p_profiler->function_sleep();
        rc = zmq_recv( responder, &request, 0 );
        p_profiler->function_wakeup();
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
                memcpy( &fixed_fsalmsg_attrs, zmq_msg_data(&request), sizeof_fixed_fsal_attrs);
            }            
            switch(head.protocol_id)
            {
                case pnfs_id:
                {
                    p_obj->log->debug_log( "Pnfsdummy request received." );
                    respsize = protocol->handle_request(&head, zmq_msg_data(&request), response_structure_mem);
                    rc = zmq_msg_init_data(&response, response_structure_mem,respsize,my_free_zmq_message,NULL);
                    break;
                }
                default:
                {
                    //printf("default fsal msg\n");
                        switch(fixed_fsalmsg_attrs.type)
                        {
                        case einode_request:
                        {
                            /* Returns the requested einode or an appropriate error msg.*/
                            p_obj->log->debug_log( "EInode request received." );
                            struct FsalFileEInodeResponse *p_fsal_resp = (struct FsalFileEInodeResponse*) response_structure_mem;
                            // process request
                            p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                            p_obj->handle_einode_request( &request, p_fsal_resp );
                            // write result to zmq response message
                            rc = zmq_msg_init_data(&response, p_fsal_resp, size_fsal_file_einode_response,my_free_zmq_message,NULL);
                            p_obj->log->debug_log( "EInode request rc:%d", rc);
                            break;
                        }
                        case create_file_einode_request:
                        {
                            /* Request to create a new file_inode object */
                            p_obj->log->debug_log( "Create EInode request received." );
                            struct FsalCreateInodeResponse *p_fsal_resp = (struct FsalCreateInodeResponse*) response_structure_mem;
                            // process request
                            p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                            p_obj->handle_create_file_einode_request( &request, p_fsal_resp );
                            // write result to zmq response message
                            rc = zmq_msg_init_data(&response, p_fsal_resp, size_fsal_create_inode_response,my_free_zmq_message,NULL);
                            p_obj->log->debug_log( "Create EInode %u, rc:%d.",p_fsal_resp->inode_number ,rc);
                            break;
                        }
                        case delete_inode_request:
                        {
                            /* Request tp delete an object received */
                            p_obj->log->debug_log( "Delete einode request received" );
                            struct FsalDeleteInodeResponse *p_fsal_resp = (struct FsalDeleteInodeResponse*) response_structure_mem;
                            // process request
                            p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                            p_obj->handle_delete_inode_request( &request, p_fsal_resp );
                            // write result to zmq response message
                            rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_delete_inode_response,my_free_zmq_message,NULL);
                            p_obj->log->debug_log( "Delete einode request rc:%d.", rc );
                            break;
                        }
                        case read_dir_request:
                        {
                            /* Request to return the inode number representing the file
                                system object name based in directory represented by
                                the supplied inode number   */
                            p_obj->log->debug_log( "Readdir request received." );
                            struct FsalReaddirResponse *p_fsal_resp = (struct FsalReaddirResponse*) response_structure_mem;
                            // process request
                            p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                            p_obj->handle_read_dir_request( &request, p_fsal_resp );
                            // write result to zmq response message                
                            rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_readdir_response,my_free_zmq_message,NULL);
                            p_obj->log->debug_log( "Readdir reqeust rc:%d", rc );
                            break;
                        }
                        case lookup_inode_number_request:
                        {
                            /* Request to return the inode number representing the file
                                system object name based in directory represented by
                                the supplied inode number   */
                            printf("lookup inode number fsal msg\n");
                            p_obj->log->debug_log( "Lookup inode number request received" );
                            struct FsalLookupInodeNumberByNameResponse *p_fsal_resp = (struct FsalLookupInodeNumberByNameResponse*) response_structure_mem;
                            // process request
                            p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                            p_obj->handle_lookup_inode_number_request( &request, p_fsal_resp );
                            // write result to zmq response message
                            rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_lookup_inode_number_by_name,my_free_zmq_message,NULL);
                            p_obj->log->debug_log( "Lookup inode number rc:%d", rc );
                            break;
                        }
                        case update_attributes_request:
                        {
                            /* Update attributes request received */
                            p_obj->log->debug_log( "Update attributes request received." );
                            struct FsalUpdateAttributesResponse *p_fsal_resp = (struct FsalUpdateAttributesResponse*) response_structure_mem;
                            // process request
                            p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                            p_obj->handle_update_attributes_request(&request, p_fsal_resp);
                            // write result to zmq response message
                            rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_update_attributes_response,my_free_zmq_message,NULL);
                            p_obj->log->debug_log( "Update attributes rc:%d", rc );
                            break;
                        }
                        case parent_inode_number_lookup_request:
                        {
                            /* Lookup parent inode numbers of the provided inodes */
                            p_obj->log->debug_log( "Parent inode number lookup request received." );
                            struct FsalParentInodeNumberLookupResponse *p_fsal_resp = (struct FsalParentInodeNumberLookupResponse*) response_structure_mem;
                            // process request
                            p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                            p_obj->handle_parent_inode_number_lookup_request( &request, p_fsal_resp );
                            // write result to zmq response message
                            rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_parent_inode_number_lookup_response,my_free_zmq_message,NULL);
                            p_obj->log->debug_log( "Parent inode number lookup rc:%d", rc );
                            break;
                        }
                        case populate_prefix_permission:
                        {
                            p_obj->log->debug_log( "prefix_perm populate request received." );
                            struct FsalPrefixPermPopulateRespone *p_fsal_resp  = (struct FsalPrefixPermPopulateRespone*) response_structure_mem;
                            p_obj->handle_populate_prefix_perm(&request);
                            p_fsal_resp->type = populate_prefix_permission_rsp;
                            p_fsal_resp->error = 0;

                            rc = zmq_msg_init_data(&response, p_fsal_resp, size_fsal_prefix_perm_populate_response, my_free_zmq_message, NULL);
                            p_obj->log->debug_log( "prefix_perm populate rc: %d", rc );
                            break;
                        }
                        case update_prefix_permission:
                                    {
                                            p_obj->log->debug_log( "prefix_perm update request received." );
                                            struct FsalPrefixPermUpdateRespone *p_fsal_resp  = (struct FsalPrefixPermUpdateRespone*) response_structure_mem;
                                            p_obj->handle_update_prefix_perm(&request);
                                            p_fsal_resp->type = update_prefix_permission_rsp;
                                            p_fsal_resp->error = 0;

                                            rc = zmq_msg_init_data(&response, p_fsal_resp, size_fsal_prefix_perm_update_response, my_free_zmq_message, NULL);
                            p_obj->log->debug_log( "prefix_perm update rc: %d", rc );
                                            break;
                                    }
                        case parent_inode_hierarchy_request:
                        {
                            /* Lookup parent inode hierarchy of the provided inode */
                            p_obj->log->debug_log("Parent inode hierarchy request: received.");
                            struct FsalInodeNumberHierarchyResponse *p_fsal_resp = (struct FsalInodeNumberHierarchyResponse*) response_structure_mem;
                            // process request
                            p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                            p_obj->handle_parent_inode_hierarchy_request(&request, p_fsal_resp);
                            // write result to zmq response message
                            rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_inode_number_hierarchy_response,my_free_zmq_message,NULL);
                            p_obj->log->debug_log("Parent inode hierarchy request: rc:%d",rc);
                            break;
                        }
                        case move_einode_request:
                        {
                            /* Move an EInode within a partition*/
                            p_obj->log->debug_log("Move Einode request: received.");
                            struct FsalMoveEinodeResponse *p_fsal_resp = (struct FsalMoveEinodeResponse*) response_structure_mem;
                            // process request
                            p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                            p_obj->handle_move_einode_request(&request, p_fsal_resp);
                            // write result to zmq response message
                            rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_move_einode_response,my_free_zmq_message,NULL);
                            p_obj->log->debug_log("Move einode request: rc:%d",rc);
                            break;
                        }
                        default:
                        {
                            /* Unknown message received*/
                            p_obj->log->warning_log( "Unknown request received." );
                            struct FsalUnknownRequestResponse *p_fsal_resp = (struct FsalUnknownRequestResponse*) response_structure_mem;
                            p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                            p_fsal_resp->type = unknown_request_response;
                            p_fsal_resp->error = fsal_unknown_request;
                            rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_unknown_request_response,my_free_zmq_message,NULL);
                            break;
                        }
                        }
                }
            }
        }
        catch (MJIException e)
        {
            p_obj->log->error_log( "MJIException raised:%s", e.what() );
//            zmq_fallback_error_message(&response, head.sequence_number);
        }
        catch (ZMQMessageException)
        {
            p_obj->log->error_log( "ZMQ recv error raised. Send fallback error message" );
  //          zmq_fallback_error_message(&response, 0) ;
        }
        catch (...)
        {
            p_obj->log->error_log( "MDS workerthread exception raised. Send fallback error message" );
    //        zmq_fallback_error_message(&response, head.sequence_number) ;
        }

        #ifdef TRACE_ZMQ_MESSAGES
        p_obj->trace_outgoing_message( &response, message_id );
        #endif
        p_obj->log->debug_log("sending response:%p",&responder);
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
    //free(response_structure_mem);
}


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
void* metadata_server_worker(void *p_v)
{
    Pc2fsProfiler *p_profiler = Pc2fsProfiler::get_instance();
    p_profiler->function_start();
    zmq::context_t context(1);
    short errorcnt=0;
    //printf("Sizeof: readdir response %d.\n",size_fsal_readdir_response);
    struct fixed_fsalmsg_attrs_t {enum MsgType type; Fsal_Msg_Sequence_Number seqnum;};
    struct fixed_fsalmsg_attrs_t fixed_fsalmsg_attrs;
    const size_t sizeof_fixed_fsal_attrs = sizeof(fixed_fsalmsg_attrs);
    
    // metadata objects pointer to access the handler functions
    MetadataServer* p_obj = (MetadataServer*)p_v;
    p_obj->log->debug_log( "worker thread running." );

    // create zmq objects
    zmq::socket_t responder( context, ZMQ_REP );

    // connect to message router
    responder.connect( ZMQ_ROUTER_BACKEND );
    // create request and response message objects
    zmq_msg_t request;
    // initialize request message with provided message size
    zmq_msg_init_size( &request, FSAL_MSG_LEN );

    int32_t rc;

    void *response_structure_mem = malloc(FSAL_MSG_LEN);

    //used to identify traced messages
    uint32_t message_id;
    srand((unsigned)time(0)); 
                
    //struct mallinfo mi_start;
    while( !fsal_worker_killswitch )
    {    
        zmq_msg_t response;
        // receive new message
        memset(zmq_msg_data(&request),'\0', sizeof_fixed_fsal_attrs);
        p_profiler->function_sleep();
        rc = zmq_recv( responder, &request, 0 );
        p_profiler->function_wakeup();
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
                errorcnt+=1;
                throw ZMQMessageException( "zmq_recv returned an error.");
            }
            else
            {
                errorcnt=0;
                memcpy( &fixed_fsalmsg_attrs, zmq_msg_data(&request), sizeof_fixed_fsal_attrs);
            }
            
            switch(fixed_fsalmsg_attrs.type)
            {
            case einode_request:
            {
                /* Returns the requested einode or an appropriate error msg.*/
                p_obj->log->debug_log( "EInode request received." );
                struct FsalFileEInodeResponse *p_fsal_resp = (struct FsalFileEInodeResponse*) response_structure_mem;
                // process request
                p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                p_obj->handle_einode_request( &request, p_fsal_resp );
                // write result to zmq response message
                rc = zmq_msg_init_data(&response, p_fsal_resp, size_fsal_file_einode_response,my_free_zmq_message,NULL);
                p_obj->log->debug_log( "EInode request rc:%d", rc);
                break;
            }
            case create_file_einode_request:
            {
                /* Request to create a new file_inode object */
                p_obj->log->debug_log( "Create EInode request received." );
                struct FsalCreateInodeResponse *p_fsal_resp = (struct FsalCreateInodeResponse*) response_structure_mem;
                // process request
                p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                p_obj->handle_create_file_einode_request( &request, p_fsal_resp );
                // write result to zmq response message
                rc = zmq_msg_init_data(&response, p_fsal_resp, size_fsal_create_inode_response,my_free_zmq_message,NULL);
                p_obj->log->debug_log( "Create EInode %u, rc:%d.",p_fsal_resp->inode_number ,rc);
                break;
            }
            case delete_inode_request:
            {
                /* Request tp delete an object received */
                p_obj->log->debug_log( "Delete einode request received" );
                struct FsalDeleteInodeResponse *p_fsal_resp = (struct FsalDeleteInodeResponse*) response_structure_mem;
                // process request
                p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                p_obj->handle_delete_inode_request( &request, p_fsal_resp );
                // write result to zmq response message
                rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_delete_inode_response,my_free_zmq_message,NULL);
                p_obj->log->debug_log( "Delete einode request rc:%d.", rc );
                break;
            }
            case read_dir_request:
            {
                /* Request to return the inode number representing the file
                    system object name based in directory represented by
                    the supplied inode number   */
                p_obj->log->debug_log( "Readdir request received." );
                struct FsalReaddirResponse *p_fsal_resp = (struct FsalReaddirResponse*) response_structure_mem;
                // process request
                p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                p_obj->handle_read_dir_request( &request, p_fsal_resp );
                // write result to zmq response message                
                rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_readdir_response,my_free_zmq_message,NULL);
                p_obj->log->debug_log( "Readdir reqeust rc:%d", rc );
                break;
            }
            case lookup_inode_number_request:
            {
                /* Request to return the inode number representing the file
                    system object name based in directory represented by
                    the supplied inode number   */
                p_obj->log->debug_log( "Lookup inode number request received" );
                struct FsalLookupInodeNumberByNameResponse *p_fsal_resp = (struct FsalLookupInodeNumberByNameResponse*) response_structure_mem;
                // process request
                p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                p_obj->handle_lookup_inode_number_request( &request, p_fsal_resp );
                // write result to zmq response message
                rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_lookup_inode_number_by_name,my_free_zmq_message,NULL);
                p_obj->log->debug_log( "Lookup inode number rc:%d", rc );
                break;
            }
            case update_attributes_request:
            {
                /* Update attributes request received */
                p_obj->log->debug_log( "Update attributes request received." );
                struct FsalUpdateAttributesResponse *p_fsal_resp = (struct FsalUpdateAttributesResponse*) response_structure_mem;
                // process request
                p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                p_obj->handle_update_attributes_request(&request, p_fsal_resp);
                // write result to zmq response message
                rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_update_attributes_response,my_free_zmq_message,NULL);
                p_obj->log->debug_log( "Update attributes rc:%d", rc );
                break;
            }
            case parent_inode_number_lookup_request:
            {
                /* Lookup parent inode numbers of the provided inodes */
                p_obj->log->debug_log( "Parent inode number lookup request received." );
                struct FsalParentInodeNumberLookupResponse *p_fsal_resp = (struct FsalParentInodeNumberLookupResponse*) response_structure_mem;
                // process request
                p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                p_obj->handle_parent_inode_number_lookup_request( &request, p_fsal_resp );
                // write result to zmq response message
                rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_parent_inode_number_lookup_response,my_free_zmq_message,NULL);
                p_obj->log->debug_log( "Parent inode number lookup rc:%d", rc );
                break;
            }
            case populate_prefix_permission:
            {
            	p_obj->log->debug_log( "prefix_perm populate request received." );
            	struct FsalPrefixPermPopulateRespone *p_fsal_resp  = (struct FsalPrefixPermPopulateRespone*) response_structure_mem;
            	p_obj->handle_populate_prefix_perm(&request);
            	p_fsal_resp->type = populate_prefix_permission_rsp;
            	p_fsal_resp->error = 0;

            	rc = zmq_msg_init_data(&response, p_fsal_resp, size_fsal_prefix_perm_populate_response, my_free_zmq_message, NULL);
                p_obj->log->debug_log( "prefix_perm populate rc: %d", rc );
            	break;
            }
            case update_prefix_permission:
			{
				p_obj->log->debug_log( "prefix_perm update request received." );
				struct FsalPrefixPermUpdateRespone *p_fsal_resp  = (struct FsalPrefixPermUpdateRespone*) response_structure_mem;
				p_obj->handle_update_prefix_perm(&request);
				p_fsal_resp->type = update_prefix_permission_rsp;
				p_fsal_resp->error = 0;

				rc = zmq_msg_init_data(&response, p_fsal_resp, size_fsal_prefix_perm_update_response, my_free_zmq_message, NULL);
                p_obj->log->debug_log( "prefix_perm update rc: %d", rc );
				break;
			}
            case parent_inode_hierarchy_request:
            {
                /* Lookup parent inode hierarchy of the provided inode */
                p_obj->log->debug_log("Parent inode hierarchy request: received.");
                struct FsalInodeNumberHierarchyResponse *p_fsal_resp = (struct FsalInodeNumberHierarchyResponse*) response_structure_mem;
                // process request
                p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                p_obj->handle_parent_inode_hierarchy_request(&request, p_fsal_resp);
                // write result to zmq response message
                rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_inode_number_hierarchy_response,my_free_zmq_message,NULL);
                p_obj->log->debug_log("Parent inode hierarchy request: rc:%d",rc);
                break;
            }
            case move_einode_request:
            {
                /* Move an EInode within a partition*/
                p_obj->log->debug_log("Move Einode request: received.");
                struct FsalMoveEinodeResponse *p_fsal_resp = (struct FsalMoveEinodeResponse*) response_structure_mem;
                // process request
                p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                p_obj->handle_move_einode_request(&request, p_fsal_resp);
                // write result to zmq response message
                rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_move_einode_response,my_free_zmq_message,NULL);
                p_obj->log->debug_log("Move einode request: rc:%d",rc);
                break;
            }
            default:
            {
                /* Unknown message received*/
                p_obj->log->warning_log( "Unknown request received." );
                struct FsalUnknownRequestResponse *p_fsal_resp = (struct FsalUnknownRequestResponse*) response_structure_mem;
                p_fsal_resp->seqnum = fixed_fsalmsg_attrs.seqnum;
                p_fsal_resp->type = unknown_request_response;
                p_fsal_resp->error = fsal_unknown_request;
                rc = zmq_msg_init_data(&response, p_fsal_resp,size_fsal_unknown_request_response,my_free_zmq_message,NULL);
                break;
            }
            }
        }
        catch (MJIException e)
        {
            p_obj->log->error_log( "MJIException raised:%s", e.what() );
    //        zmq_fallback_error_message(&response, fixed_fsalmsg_attrs.seqnum);
        }
        catch (ZMQMessageException)
        {
            p_obj->log->error_log( "ZMQ recv error raised. Send fallback error message" );
  //          zmq_fallback_error_message(&response, 0) ;
        }
        catch (...)
        {
            p_obj->log->error_log( "MDS workerthread exception raised. Send fallback error message" );
//            zmq_fallback_error_message(&response, fixed_fsalmsg_attrs.seqnum) ;
        }

        #ifdef TRACE_ZMQ_MESSAGES
        p_obj->trace_outgoing_message( &response, message_id );
        #endif

        rc = zmq_send(responder, &response, 0);
        if (rc)
        {
           p_obj->log->error_log( "MDS workerthread: error sending message:rc=%d.",rc );
           if (errorcnt >3)
           {
                fsal_worker_killswitch=1;
           }
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
    //free(response_structure_mem);
}
