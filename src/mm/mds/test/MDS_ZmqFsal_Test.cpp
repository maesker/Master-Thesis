#include "mm/mds/test/AbstractMDSTest.h"

/**
 * @file MDS_ZmqFsal_Test.cpp
 * @author Markus Mäsker, <maesker@gmx.net>
 * @date 26.08.2011
 *
 * @brief Tests the main functionality of the MetadataServer.
 *
 *
     * Distributed operations involving more than one Metadataserver are
     * not covered here. That means except for the ZMQ communication all
     * other communication handler are omitted here. The loadbalancer is
     * also not part of the test scenarios.
     * Among the used Modules are the MltHander, ConfigurationManager, Logger,
     * JournalManager and according journals, the complete Storage
     * infrastructure, the MDS itself and the Messagerouter.
     *
     * ZMQ-FSAL tests: These tests only focus on the Zmq and FSAL
     * message handling. Test messages are created and send to the MDS.
     * The response messages are validated. The effects on the FS are
     * not validated. This means a FSAL_Delete Test succeeds if the
     * fsal response received is valid. It is not checked whether the
     * object actually got deleted on the MDS side.
     *
 * */


/**
 * @class MDS_ZmqFsal_Test
 * @brief Constructor. Creates all the necessary modules needed by
 * the mds.
 * */
class MDS_ZmqFsal_Test : public AbstractMDSTest
{
public:
    MDS_ZmqFsal_Test()
    {
        objname = "MDS_ZmqFsal_Test";
    }
    ~MDS_ZmqFsal_Test()
    {
    }

protected:
    void SetUp()
    {
        AbstractMDSTest::SetUp();
        archived = 0;
        ASSERT_EQ(1,mds->verify());
        ASSERT_EQ(0,startup_failure);
        init_memleak_check_start();
    }

    void TearDown()
    {
        AbstractMDSTest::TearDown();
    }
};



/**
 * @brief Perform readdir on empty Root object.
 *
 * This is a unittest for the FsalLookupInodeNumberByNameRequest (delete file)
 * handling. Only the return values of the FSAL responses are checked.
 * The actual filesystem operations are not verified. This means that
 * e.g. a create file operation might pass even though a later lookup
 * reveals that the file never was created.
 * */
TEST_F(MDS_ZmqFsal_Test,  emptyRoot_Readdir)
{
    log->debug_log("Start_scenario01");
    ReadDirReturn *p_rdir = new ReadDirReturn;
    MDSTEST_readdir(1,1,0,p_rdir);
    ASSERT_EQ(0, p_rdir->dir_size);
    ASSERT_EQ(0, p_rdir->nodes_len);
    free(p_rdir);
    log->debug_log("end_scenario01");
}

/**
 * @brief Sends a zmq_request for the fs root objects and checks the
 * values. The values must match the ones defined in the MetadataServer
 * module.
 *
 * This is a unittest for the FsalEInodeRequest handling. Only the
 * return values of the FSAL responses are checked. The actual file-
 * system operations are not verified. This means that e.g. a
 * create file operation might pass even though a later lookup reveals
 * that the file never was created.
 * */
TEST_F(MDS_ZmqFsal_Test, Read_Root)
{
    log->debug_log("start");
    int rc;
    // create and initialize the FSAL request structure
    FsalEInodeRequest *p_fsal = get_mock_EInodeRequestStructure(1,1);
    struct FsalFileEInodeResponse fsal_resp;
    // send the request to the MDS message router
    rc = p_zmqhelper->send_msg_and_recv(p_fsal ,sizeof(FsalEInodeRequest), &fsal_resp,sizeof(fsal_resp));
    // send and receive must return 0.
    ASSERT_EQ(rc,0);

    // transform zmq response message in FSAL response
    //rc = p_zmqhelper->get_fsal_response(&fsal_resp,sizeof(fsal_resp));
    // check attributes of the received einode
    struct stat buffer;
    stat("/tmp", &buffer);
    ASSERT_EQ(fsal_resp.type, file_einode_response);
    ASSERT_EQ(0, fsal_resp.error);
    ASSERT_EQ(1, fsal_resp.einode.inode.inode_number);
    //ASSERT_EQ(32000, fsal_resp.einode.inode.size);
    ASSERT_EQ(0, fsal_resp.einode.inode.size);
    ASSERT_LE(time(0), fsal_resp.einode.inode.ctime);
    ASSERT_LE(time(0), fsal_resp.einode.inode.atime);
    ASSERT_LE(time(0), fsal_resp.einode.inode.mtime);
    ASSERT_EQ(buffer.st_mode, fsal_resp.einode.inode.mode);
    ASSERT_EQ(1, fsal_resp.einode.inode.link_count);
    ASSERT_EQ(0, fsal_resp.einode.inode.has_acl);
    ASSERT_EQ((uid_t)0, fsal_resp.einode.inode.uid);
    ASSERT_EQ((uid_t)0, fsal_resp.einode.inode.gid);
    ASSERT_STREQ("/",fsal_resp.einode.name);
    free(p_fsal);
    log->debug_log("end");
}


/**
 * @brief Create a Testfile directly on the Root object. This means,
 * that both partition_root_inode_number and parent_inode_number refere
 * to the root object 1.
 *
 * This is a unittest for the FsalCreateEInodeRequest (create file)
 * handling. Only the return values of the FSAL responses are checked.
 * The actual filesystem operations are not verified. This means that
 * e.g. a create file operation might pass even though a later lookup
 * reveals that the file never was created.
 * */
TEST_F(MDS_ZmqFsal_Test, CreateFile_Root)
{
    // create and initialize the fsal request structure
    log->debug_log("start");
    InodeNumber inum;
    MDSTEST_create_file(1,1, &inum);
    log->debug_log("scenrati0 03 checking.");
    ASSERT_NE(0, inum);
    log->debug_log("end");
}


/**
 * @brief Creates a Directory on the root fs object. This means that
 * both partition_root_inode_number and parent_inode_number refere to
 * the root object 1.
 *
 * This is a unittest for the FsalCreateEInodeRequest (mkdir)
 * handling. Only the return values of the FSAL responses are checked.
 * The actual filesystem operations are not verified. This means that
 * e.g. a create file operation might pass even though a later lookup
 * reveals that the file never was created.
 * */
TEST_F(MDS_ZmqFsal_Test, CreateDir_Root)
{
    InodeNumber inum;
    MDSTEST_mkdir(1, 1, &inum);
    ASSERT_NE(0, inum);
}

/**
 * @brief This test creates a request with an invalid message type. The
 * expected response is an FsalUnknownRequestResponse.
 *
 * This is a unittest for the FsalUnknownRequestResponse Only the
 * return values of the FSAL responses are checked.
 * The actual filesystem operations are not verified. This means that
 * e.g. a create file operation might pass even though a later lookup
 * reveals that the file never was created.
 * */
TEST_F(MDS_ZmqFsal_Test, UnknownFsalRequest)
{
    int rc;
    // create and initialize the FSAL request structure
    FsalEInodeRequest *p_fsal = get_mock_EInodeRequestStructure(1,1);
    // screw up the message type
    p_fsal->type= (MsgType) 9999;

    struct FsalUnknownRequestResponse fsal_resp;

    // send the request to the MDS message router
    rc = p_zmqhelper->send_msg_and_recv(p_fsal ,sizeof(FsalEInodeRequest), &fsal_resp,sizeof(fsal_resp));
    delete p_fsal;

    // send and receive must return 0.
    ASSERT_EQ(rc,0);
    // check attributes of the received einode
    // request type was invalid therefore the response type should be
    // unknown request response
    ASSERT_EQ(fsal_resp.type, unknown_request_response);

    // request type was invalid therefore the error flag shpould be set
    ASSERT_NE(0, fsal_resp.error);
}

/**
 * @brief Create a Testfile directly on the Root object. If successful
 * a request to delete the file is sent.
 *
 * This is a unittest for the FsalDeleteInodeRequest (delete file)
 * handling. Only the return values of the FSAL responses are checked.
 * The actual filesystem operations are not verified. This means that
 * e.g. a create file operation might pass even though a later lookup
 * reveals that the file never was created.
 * */
TEST_F(MDS_ZmqFsal_Test, Delete_File_at_Root)
{
    int32_t rc;
    InodeNumber new_file;
    MDSTEST_create_file(1, 1, &new_file);
    ASSERT_NE(new_file,0);
    // test file created.

    // create and initialize the fsal request structure
    FsalDeleteInodeRequest *ptr = new FsalDeleteInodeRequest;
    ptr = get_mock_FsalDeleteInodeRequestStructure(1,new_file);
    struct FsalDeleteInodeResponse fsal_resp;
    // transform fsal request into zmq message, send it to the message-
    // router and receive the response. Returnvalue of the function
    // must be 0.
    rc = p_zmqhelper->send_msg_and_recv(ptr ,sizeof(FsalDeleteInodeRequest), &fsal_resp,sizeof(fsal_resp));
    ASSERT_EQ(rc,0);

    // check the returned attributes of the FSAL request.
    ASSERT_EQ(delete_inode_response, fsal_resp.type);
    ASSERT_EQ(0, fsal_resp.error);
    ASSERT_EQ(new_file, fsal_resp.inode_number);
    delete ptr;
}

/**
 * @brief Deletes a file that does not exist.
 *
 * This is a unittest for the FsalDeleteInodeRequest (delete file)
 * handling. Only the return values of the FSAL responses are checked.
 * The actual filesystem operations are not verified. This means that
 * e.g. a create file operation might pass even though a later lookup
 * reveals that the file never was created.
 * */
TEST_F(MDS_ZmqFsal_Test, Delete_Nonexisting_File)
{
    int32_t rc;
    // create and initialize the fsal request structure
    FsalDeleteInodeRequest *ptr = new FsalDeleteInodeRequest;
    InodeNumber inum = generate_random_inodenumber();
    ptr = get_mock_FsalDeleteInodeRequestStructure(1,inum);
    struct FsalDeleteInodeResponse fsal_resp;
    // transform fsal request into zmq message, send it to the message-
    // router and receive the response. Returnvalue of the function
    // must be 0.
    rc = p_zmqhelper->send_msg_and_recv(ptr ,sizeof(FsalDeleteInodeRequest), &fsal_resp,sizeof(fsal_resp));
    ASSERT_EQ(rc,0);

    // check the returned attributes of the FSAL request.
    ASSERT_EQ(delete_inode_response, fsal_resp.type);
    ASSERT_NE(0, fsal_resp.error);
    delete ptr;
}

/**
 * @brief Create a Testfile directly on the Root object. Then the Inodenumber
 * is requested based on the fs object name
 *
 * This is a unittest for the FsalLookupInodeNumberByNameRequest (delete file)
 * handling. Only the return values of the FSAL responses are checked.
 * The actual filesystem operations are not verified. This means that
 * e.g. a create file operation might pass even though a later lookup
 * reveals that the file never was created.
 * */
TEST_F(MDS_ZmqFsal_Test, Inodenumber_Lookup_By_Name)
{
    InodeNumber new_file;
    MDSTEST_create_file(1,1, &new_file);
    ASSERT_NE(new_file,0);
    EInode *einode = new EInode;
    MDSTEST_read_einode(einode, 1,new_file);
    if (einode == NULL)
    {
        FAIL();
    }
    // test file created.
    int32_t rc;
    // create and initialize the fsal request structure
    FsalLookupInodeNumberByNameRequest *ptr = new FsalLookupInodeNumberByNameRequest;
    ptr = get_mock_FsalLookupInodeNumberByNameRequestStructure(1,1,&einode->name);

    struct FsalLookupInodeNumberByNameResponse fsal_resp;
    // transform fsal request into zmq message, send it to the message-
    // router and receive the response. Returnvalue of the function
    // must be 0.
    rc = p_zmqhelper->send_msg_and_recv(ptr ,sizeof(FsalLookupInodeNumberByNameRequest),&fsal_resp,sizeof(fsal_resp));
    ASSERT_EQ(rc,0);

    // check the returned attributes of the FSAL request.
    ASSERT_EQ(0, fsal_resp.error);
    ASSERT_EQ(lookup_inode_number_response, fsal_resp.type);
    ASSERT_EQ(new_file, fsal_resp.inode_number);
    delete ptr;
    delete einode;
}


/**
 * @brief Create a Testfile directly on the Root object. If successful
 * reads it and compares received inode number
 *
 * This is a unittest for the FsalEInodeRequest
 * handling. Only the return values of the FSAL responses are checked.
 * The actual filesystem operations are not verified. This means that
 * e.g. a create file operation might pass even though a later lookup
 * reveals that the file never was created.
 * */
TEST_F(MDS_ZmqFsal_Test, Request_EInode)
{
    InodeNumber new_file;
    MDSTEST_create_file(1,1, &new_file);
    ASSERT_NE(new_file,0);
    EInode *einode = new EInode;
    MDSTEST_read_einode(einode, 1,new_file);
    ASSERT_EQ(einode->inode.inode_number, new_file);
    delete einode;
}

/**
 * @brief Create a Testfile directly on the Root object. Then changes
 * the attributes of the file.
 *
 * This is a unittest for the FsalUpdateAttributesRequest
 * handling. Only the return values of the FSAL responses are checked.
 * The actual filesystem operations are not verified. This means that
 * e.g. a create file operation might pass even though a later lookup
 * reveals that the file never was created.
 * */
TEST_F(MDS_ZmqFsal_Test, Empty_Update_Attributes)
{
    InodeNumber new_file;
    MDSTEST_create_file(1,1, &new_file);
    ASSERT_NE(new_file,0);

    inode_update_attributes_t *attrs = new inode_update_attributes_t;
    int attribute_bitfield = 0;
    // register mode as changed
    MDSTEST_update(1,new_file,attrs,attribute_bitfield);
    delete attrs;
    
}

/**
 * @brief test Inodes parent hierarchy request
 *
 * This is a unittest for the FsalUpdateAttributesRequest
 * handling. Only the return values of the FSAL responses are checked.
 * The actual filesystem operations are not verified. This means that
 * e.g. a create file operation might pass even though a later lookup
 * reveals that the file never was created.
 * 
 * @todo Activate test
 * */
TEST_F(MDS_ZmqFsal_Test, Parent_Hierarchy)
{
    // Request a hierarchy 
    // create the file to test the attribute change
    inode_number_list_t inode_number_list;
    //MDSTEST_parent_hierarchy(1,2,&inode_number_list);
    
    //FAIL();
}

/*
TEST_F(MDS_ZmqFsal_Test, Prefix_Perm_Populate)
{
    FAIL();
    struct FsalPopulatePrefixPermission fsal_req;
    int rc;

    fsal_req.type = populate_prefix_permission;
    memset(&fsal_req.pperm_data, 0, sizeof(fsal_req.pperm_data));
    fsal_req.pperm_data.root_inode = 2;
    fsal_req.pperm_data.entry[0].inode = 1;
    fsal_req.pperm_data.entry[1].inode = 2;
    fsal_req.pperm_data.num_entries = 2;

    FsalPrefixPermPopulateRespone fsal_resp;

	 rc = p_zmqhelper->send_msg_and_recv(&fsal_req ,sizeof(FsalPopulatePrefixPermission), &fsal_resp,sizeof(fsal_resp));

	//rc = p_zmqhelper->get_fsal_response(&fsal_resp,sizeof(fsal_resp));
	ASSERT_EQ(0,rc);
	// check attributes of the fsal response
	ASSERT_EQ(0, fsal_resp.error);
	ASSERT_EQ(populate_prefix_permission_rsp, fsal_resp.type);

	log->debug_log("end");

}
*/