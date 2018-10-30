#include "mm/mds/test/AbstractMDSTest.h"

/**
 * @file AbstractMDSTest.cpp
 * @class AbstractMDSTest
 * @author Markus Mäsker, <maesker@gmx.net>
 * @date 24.08.2011
 *
 * @brief Abstract MDS Testclass.
 *
 * Sets up testenvironment, created configmanager and logger.
 * */

int MLT_FILE_READ = 0 ;
int TESTCASE_ITER = 0;

AbstractMDSTest::AbstractMDSTest()
{
    int archived=0;
    int startup_failure=0;
    std::string workdir = "";
    std::string test_environment_dir = "";
    std::string objname = "";
    std::string confdest = "";
    TESTCASE_ITER++;
}

AbstractMDSTest::~AbstractMDSTest()
{
    /*
    //mds->stop();
    log->debug_log("Destructor start.");
    delete device;
    log->debug_log("Abstract storage device deleted.");
    delete ddf;
    log->debug_log("AbstractDataDistributionFunction  deleted.");
    delete ddm;
    log->debug_log("DataDistributionManager deleted.");
    delete storage_abstraction_layer;
    log->debug_log("StorageAbstractionLayer deleted.");
    delete p_zmqhelper;
    log->debug_log("ZmqFsalHelper deleted.");
    delete cm;
    log->debug_log("cm deleted");
    delete p_mlt_handler;
    log->debug_log("deleted mlt handler.");

    log->debug_log("mds stopped");
    delete mds;
    log->debug_log("mds deleted");
    delete jm;
    log->debug_log("jm deleted");*/
    log->debug_log("end");
    //delete log;
    //archive_result();
}


void AbstractMDSTest::SetUp()
{
    int rc=0;
    startup_failure=0;
    try
    {
        init_testenvironment();
        cm = new ConfigurationManager(0, NULL,confdest+"/conf/main.conf");
        cm->register_option("logfile","path to logfile");
        cm->register_option("loglevel","Specify loglevel");
        cm->register_option("cmdoutput","Activate commandline output");
        cm->register_option("port","Port of the MDS");
        cm->register_option("address","IP address of the MDS");
        cm->parse();
        log = new Logger();
        log->set_log_level(LOG_LEVEL_DEBUG);
        log->set_console_output(false);
        std::string logdir = workdir + "/test.log";
        log->set_log_location(logdir);
    }
    catch (...)
    {
        printf("\nError occured while creating testinstance.\n");
    }
    log->debug_log("creating mlt handler");
    p_mlt_handler = &(MltHandler::get_instance());
    if (p_mlt_handler == NULL)
    {
        log->error_log("MltHandler instance not received.");
        startup_failure = 1;
    }
    log->debug_log("MltHandler instance received.");

    if (!MLT_FILE_READ)
    {
        log->debug_log("mlt file not read");
        rc=p_mlt_handler->read_from_file(MLT_PATH);

        MLT_FILE_READ = 1;
        if (rc)
        {
            log->error_log("Error reading MLT from %s.",MLT_PATH);
            startup_failure = 2;
        }
    }
    log->debug_log("creating mlt entry server object");
    uint16_t port = atoi(cm->get_value("port").c_str());
    std::string address = cm->get_value("address");
    const Server this_box = {address,port};
    p_mlt_handler->set_my_address(this_box);
    log->debug_log("mlt server address set");

    // create journal manager instance
    log->debug_log("creating journal manager instance");
    jm = jm->get_instance();
    if (!jm)
    {
        log->error_log("error while creating journal manager object.");
    }
    log->debug_log("JournalManager object created");
    // Create and initialize Storage device
    log->debug_log("abstract storage device created");

    storage_abstraction_layer = new StorageAbstractionLayer ((char *) workdir.c_str());
    if (!storage_abstraction_layer)
    {
        log->error_log("error while creating StorageAbstractionLayer object");
    }
    log->debug_log("StorageAbstractionLayer object created.");

    // register storage abstraction layer at journal manager
    if (jm->get_instance()->set_storage_abstraction_layer(storage_abstraction_layer))
    {
        log->error_log("unable to register sal at journal manager");
    }
    // create metadata server instance
    mds = MetadataServer::create_instance(jm->get_instance(),p_mlt_handler,workdir);
    if (!mds)
    {
        log->error_log("error while creating MetadataServer object");
    }
    if (!mds->verify())
    {
        log->error_log("Could not verify mds.");
    }
    log->debug_log("Metadataserver object created.");
    // start metadata server
    mds->set_storage_abstraction_layer(storage_abstraction_layer);
    log->debug_log("MDS storage abstraction layer set.");
    mds->run();
    log->debug_log("MDS run done.");
    p_zmqhelper = new ZmqFsalHelper();
    log->debug_log("ZmqFsalHelper created.");

    log->debug_log("Metadataserver running.");
    archived=0;
    ASSERT_EQ(0,startup_failure);
    log->debug_log("- - - - System OK. - - - -end.");
    //init_memleak_check_start();
}

void AbstractMDSTest::TearDown()
{
    log->debug_log("start.");
    //archive_result("");
    log->debug_log("end");
    //init_memleak_check_end();
}


void AbstractMDSTest::MDSTEST_lookup_by_name(
    InodeNumber partition_root_inode_number,
    InodeNumber parent_inode_number,
    FsObjectName *p_name,
    InodeNumber *p_result)
{
    log->debug_log("start");
    int32_t rc;
    if (p_name == NULL)
    {
        generate_fsobjectname(p_name, 64);
    }
    // create and initialize the fsal request structure
    struct FsalLookupInodeNumberByNameResponse fsal_resp;
    
    FsalLookupInodeNumberByNameRequest *ptr = get_mock_FsalLookupInodeNumberByNameRequestStructure(
            partition_root_inode_number,
            parent_inode_number,
            p_name);
            
    // sned zmq request to message router and receive the result.
    rc = p_zmqhelper->send_msg_and_recv(ptr ,sizeof(FsalLookupInodeNumberByNameRequest), &fsal_resp,sizeof(fsal_resp));

    //rc = p_zmqhelper->get_fsal_response(&fsal_resp,sizeof(fsal_resp));
    ASSERT_EQ(0,rc);
    // check attributes of the fsal response
    //ASSERT_EQ(0, fsal_resp.error);
    //ASSERT_NE(0, fsal_resp.inode_number);
    ASSERT_EQ(lookup_inode_number_response, fsal_resp.type);
    delete ptr;
    *p_result = fsal_resp.inode_number;
    log->debug_log("end");
}

void AbstractMDSTest::MDSTEST_create_file(
    InodeNumber partition_root_inode_number,
    InodeNumber parent_inode_number,
    InodeNumber *p_result)
{
    log->debug_log("start");
    int32_t rc;
    InodeNumber ret = 0;
    // create and initialize the fsal request structure
    struct FsalCreateInodeResponse fsal_resp;
    FsalCreateEInodeRequest *ptr = get_mock_FsalCreateFileInodeRequest();
    ptr->partition_root_inode_number = partition_root_inode_number;
    ptr->parent_inode_number = parent_inode_number;

    // sned zmq request to message router and receive the result.
    rc = p_zmqhelper->send_msg_and_recv(ptr ,sizeof(FsalCreateEInodeRequest), &fsal_resp,sizeof(fsal_resp));

    //rc = p_zmqhelper->get_fsal_response(&fsal_resp,sizeof(fsal_resp));
    ASSERT_EQ(0,rc);
    // check attributes of the fsal response
    ASSERT_EQ(0, fsal_resp.error);
    ASSERT_NE(0, fsal_resp.inode_number);
    ASSERT_EQ(create_inode_response, fsal_resp.type);
    delete ptr;
    *p_result = fsal_resp.inode_number;
    log->debug_log("end");
}



void AbstractMDSTest::MDSTEST_full_create_file(
    InodeNumber partition_root_inode_number,
    InodeNumber parent_inode_number,
    FsObjectName *p_name,
    InodeNumber *p_result)
{
    log->debug_log("start");
    int32_t rc;
    InodeNumber ret = 0;
    // create and initialize the fsal request structure    
    
    MDSTEST_lookup_by_name( partition_root_inode_number,
                            parent_inode_number,
                            p_name,
                            &ret);
    ASSERT_EQ(0,ret);
    // twice to emulate nfs client-server behaviour
    MDSTEST_lookup_by_name( partition_root_inode_number,
                            parent_inode_number,
                            p_name,
                            &ret);
    ASSERT_EQ(0,ret);
    
    MDSTEST_create_file(    partition_root_inode_number,
                            parent_inode_number,
                            &ret);    
    ASSERT_NE(0,ret);
    
    EInode *p_ei = new EInode;
    MDSTEST_read_einode(    p_ei,
                            partition_root_inode_number,
                            ret);
    
    MDSTEST_read_einode(    p_ei,
                            partition_root_inode_number,
                            ret);
    

    // the new set mode
    mode_t newmode = generate_mode_t();
    inode_update_attributes_t *attrs1 = new inode_update_attributes_t;
    int attribute_bitfield = 0;
    // register mode as changed
    SET_MODE(attribute_bitfield);
    attrs1->mode = newmode;
    
    MDSTEST_update(     partition_root_inode_number,
                        ret,
                        attrs1,
                        attribute_bitfield);
    delete attrs1;
    
    MDSTEST_read_einode(    p_ei,
                            partition_root_inode_number,
                            ret);
    
    MDSTEST_read_einode(    p_ei,
                            partition_root_inode_number,
                            ret);
    
    
    // the new set mtime
    time_t newmtime = generate_time()+1;
    time_t newatime = generate_time()+1;
    inode_update_attributes_t *attrs2 = new inode_update_attributes_t;
    attribute_bitfield = 0;
    // register mtime as changed
    SET_MTIME(attribute_bitfield);
    SET_ATIME(attribute_bitfield);
    attrs2->mtime = newmtime;
    attrs2->atime = newatime;
    MDSTEST_update(     partition_root_inode_number,
                        ret,
                        attrs2,
                        attribute_bitfield);
    delete attrs2;
    
    
    delete p_ei;
    log->debug_log("end");
}

void AbstractMDSTest::MDSTEST_mkdir(
    InodeNumber parition_root,
    InodeNumber parent,
    InodeNumber *p_result)
{
    log->debug_log("start");
    int rc;
    struct FsalCreateInodeResponse fsal_resp;
    FsalCreateEInodeRequest *ptr = get_mock_DIR_FsalCreateFileInodeRequest();
    ptr->partition_root_inode_number = 1;
    ptr->parent_inode_number = 1;

    // transform fsal request into zmq message, send it to the message-
    // router and receive the response. Returnvalue of the function
    // must be 0.
    rc = p_zmqhelper->send_msg_and_recv(ptr ,sizeof(FsalCreateEInodeRequest),&fsal_resp,sizeof(fsal_resp));
    ASSERT_EQ(rc,0);

    // check the returned attributes of the FSAL request.
    ASSERT_EQ(create_inode_response, fsal_resp.type);
    ASSERT_EQ(0, fsal_resp.error);
    ASSERT_NE(0, fsal_resp.inode_number);
    delete ptr;
    *p_result = fsal_resp.inode_number;
    log->debug_log("end");
}


void AbstractMDSTest::MDSTEST_read_einode(
    EInode *p_ei,
    InodeNumber partition_root_inode_number,
    InodeNumber inode_number)
{
    log->debug_log("start.");
    int32_t rc = -1;
    // create and initialize the fsal request structure
    struct FsalFileEInodeResponse fsal_resp;
    FsalEInodeRequest *ptr = get_mock_FsalEInodeRequestStructure(
                                 partition_root_inode_number,inode_number);

    // sned zmq request to message router and receive the result.
    rc = p_zmqhelper->send_msg_and_recv(ptr ,sizeof(FsalEInodeRequest), &fsal_resp,sizeof(fsal_resp));
    ASSERT_EQ(0, rc);

    // check attributes of the fsal response
    ASSERT_EQ(0, fsal_resp.error);
    ASSERT_EQ(file_einode_response, fsal_resp.type);
    memcpy(p_ei, &fsal_resp.einode,sizeof(EInode));
    delete ptr;
    log->debug_log("end.");
}



void AbstractMDSTest::MDSTEST_readdir(
    InodeNumber partition_root_inum,
    InodeNumber parent_inum,
    ReaddirOffset offset,
    ReadDirReturn *p_rdir)
{
    log->debug_log("start.");
    int32_t rc;
    // create and initialize the fsal request structure
    FsalReaddirRequest *ptr;
    ptr = get_mock_FsalReaddirRequestStructure(partition_root_inum,parent_inum,offset);
    struct FsalReaddirResponse fsal_resp;
    // transform fsal request into zmq message, send it to the message-
    // router and receive the response. Returnvalue of the function
    // must be 0.
    rc = p_zmqhelper->send_msg_and_recv(ptr ,sizeof(FsalReaddirRequest), &fsal_resp,sizeof(fsal_resp));
    ASSERT_EQ(0, rc);

    // check the returned attributes of the FSAL request.
    ASSERT_EQ(0, fsal_resp.error);
    ASSERT_EQ(read_dir_response, fsal_resp.type);
    memcpy(p_rdir,&fsal_resp.directory_content,sizeof(ReadDirReturn));
    delete ptr;
    log->debug_log("end.");
}


void AbstractMDSTest::MDSTEST_update(
    InodeNumber partition_root_inode_number,
    InodeNumber new_file,
    inode_update_attributes_t *attrs,
    int attribute_bitfield)
{
    log->debug_log("start.");
    int32_t rc;
    // create fsal request structure
    FsalUpdateAttributesRequest *ptr = get_mock_FSALUpdateAttrRequest(
                                           partition_root_inode_number,new_file, *attrs, attribute_bitfield);
    struct FsalUpdateAttributesResponse fsal_resp;
    // transform fsal request into zmq message, send it to the message-
    // router and receive the response. Returnvalue of the function
    // must be 0.
    rc = p_zmqhelper->send_msg_and_recv(ptr ,sizeof(FsalUpdateAttributesRequest), &fsal_resp,sizeof(fsal_resp));
    ASSERT_EQ(rc,0);

    // check the returned attributes of the FSAL request.
    ASSERT_EQ(0, fsal_resp.error);
    ASSERT_EQ(update_attributes_response, fsal_resp.type);
    ASSERT_EQ(new_file, fsal_resp.inode_number);
    delete ptr;
    log->debug_log("end.");
}

void AbstractMDSTest::MDSTEST_parent_hierarchy(
    InodeNumber partition_root_inode_number,
    InodeNumber inode_number,
    inode_number_list_t *p_inode_number_list )
{
    log->debug_log("start.");
    int32_t rc;
    // create fsal request structure
    FsalInodeNumberHierarchyRequest *ptr = get_mock_FsalParentHierarchyRequest();
    ptr->partition_root_inode_number = partition_root_inode_number;
    ptr->inode_number = inode_number;
    
    struct FsalInodeNumberHierarchyResponse fsal_resp;
    // transform fsal request into zmq message, send it to the message-
    // router and receive the response. Returnvalue of the function
    // must be 0.
    rc = p_zmqhelper->send_msg_and_recv(ptr,
                                        sizeof(FsalInodeNumberHierarchyRequest),
                                        &fsal_resp,
                                        sizeof(fsal_resp));
    ASSERT_EQ(rc,0);

    // check the returned attributes of the FSAL request.
    ASSERT_EQ(0, fsal_resp.error);
    ASSERT_EQ(parent_inode_hierarchy_response, fsal_resp.type);
    memcpy(p_inode_number_list, &fsal_resp.inode_number_list, sizeof(inode_number_list_t));
    delete ptr;
    log->debug_log("end.");
}


void AbstractMDSTest::init_memleak_check_start()
{
    mi_start = mallinfo();
}

void AbstractMDSTest::init_memleak_check_end()
{
    mi_end = mallinfo();
   printf("Diff:%d.\n",mi_end.uordblks-mi_start.uordblks);
    ASSERT_EQ(mi_start.uordblks, mi_end.uordblks);
}



void AbstractMDSTest::intermediate_memleak_check_start()
{
    mi_intermediate_start = mallinfo();
}

void AbstractMDSTest::intermediate_memleak_check_end()
{
    mi_intermediate_end = mallinfo();
    printf("Diff:%d.\n",mi_intermediate_end.uordblks-mi_intermediate_start.uordblks);
    ASSERT_GE(mi_intermediate_start.uordblks, mi_intermediate_end.uordblks);
}

void AbstractMDSTest::init_testenvironment()
{
    //objname = std::string(GetParam());
    workdir = string(BASE_TEMP);
    systools_fs_mkdir(workdir);
    workdir.append(objname);
    workdir+="/";
    systools_fs_mkdir(workdir);

    time_t rawtime;
    struct tm * timeinfo;
    char tbuffer [19];
    char tbuffer2 [19];
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    strftime (tbuffer,10,"%y_%m_%d",timeinfo);
    strftime (tbuffer2,10,"/%H_%M_%S",timeinfo);

    workdir+=tbuffer;
    systools_fs_mkdir(workdir);
    workdir+=tbuffer2;
    systools_fs_mkdir(workdir);
    //workdir+="/";

    char cCurrentPath[FILENAME_MAX];
    GetCurrentDir(cCurrentPath, sizeof(cCurrentPath));

    test_environment_dir = string(cCurrentPath);
    test_environment_dir+="/";
    test_environment_dir.append(BASE_ENVIRONMENT);
    test_environment_dir.append(objname);

//    systools_fs_mkdir(RESULT_ARCHIVE);

    system(CMD_BACKUP_MLT);
    std::string cmd = "cp " + test_environment_dir + "/mlt " + MLT_PATH;
    system(cmd.c_str());

    std::string confsrc = test_environment_dir;
    confsrc.append("/conf");
    confdest = workdir;

    std::string cmd_mkconf = "cp -r " + confsrc + " " + confdest;
    system(cmd_mkconf.c_str());
}


void AbstractMDSTest::archive_result()
{
    /*
    log->debug_log("start");
    if (archived==0)
    {
        time_t rawtime;
        struct tm * timeinfo;
        char tbuffer [19];
        char tbuffer2 [19];
        time ( &rawtime );
        timeinfo = localtime ( &rawtime );
        strftime (tbuffer,19,"%y_%m_%d",timeinfo);
        strftime (tbuffer2,19,"%H_%M_%S",timeinfo);

        systools_fs_mkdir(RESULT_ARCHIVE);
        std::string destination = std::string(RESULT_ARCHIVE);
        destination += tbuffer;
        systools_fs_mkdir(destination);
        destination += "/";
        destination += objname;
        systools_fs_mkdir(destination);
        char buf[8];
        memset(&buf[0],'0',8);
        sprintf(&buf[0],"/%d",TESTCASE_ITER);
        destination.append(&buf[0]);
        systools_fs_mkdir(destination);
        destination += "/";
        destination += tbuffer2;
        systools_fs_mkdir(destination);


        std::string mv_cmd = "mv ";
        mv_cmd += workdir;
        mv_cmd += " ";
        mv_cmd += destination;
        mv_cmd += "/*";
        system(mv_cmd.c_str());

        std::string mv_cmd2 = "mv ";
        mv_cmd2 += MLT_PATH;
        mv_cmd2 += " ";
        mv_cmd2 += destination;
        system(mv_cmd2.c_str());
        archived=1;
        system(CMD_RESTORE_MLT);
    }
    log->debug_log("end");
    * */
}



/*
int32_t AbstractMDSTest::start_fs_worker_thread(int operation_count)
{
    int32_t rc=0;
    
}


*/
