#include "mm/mds/test/AbstractMDSTest.h"

/**
 * @file MDS_Functional_UpdateFile_Test.cpp
 * @author Markus Mäsker, <maesker@gmx.net>
 * @date 12.08.2011
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
     * 1. ZMQ-FSAL tests: These tests only focus on the Zmq and FSAL
     * message handling. Test messages are created and send to the MDS.
     * The response messages are validated. The effects on the FS are
     * not validated. This means a FSAL_Delete Test succeeds if the
     * fsal response received is valid. It is not checked whether the
     * object actually got deleted on the MDS side.
     *
     * 2. Functional tests: These tests check the effects on the
     * metadata. The Zqm-FSAL tests of section 1 did not consider the
     * real effects. Now for example a update attributes operation is
     * valided by performing two lookups and compare the received
     * einodes.
     *
 * */


/**
 * @class MDS_Functional_CreateFile_Test
 * @brief Constructor. Creates all the necessary modules needed by
 * the mds.
 * */
class MDS_Functional_UpdateFile_Test : public AbstractMDSTest
{
public:
    MDS_Functional_UpdateFile_Test()
    {
        objname = "MDS_Functional_UpdateFile_Test";
    }
    ~MDS_Functional_UpdateFile_Test()
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
 * @brief Create a Testfile directly on the Root object. Then changes  
 * the Mode of the file.
 * 
 * This is a functional test for the FsalUpdateAttributesRequest
 * handling. The effects on the metadata are validated by succeeding 
 * requests to the MDS and comparison of the old/new values.
 * */
TEST_F(MDS_Functional_UpdateFile_Test, scenario_11)
{
    // create the file to test the attribute change
    InodeNumber new_file;
    MDSTEST_create_file(1,1, &new_file);
    ASSERT_NE(new_file,0);
    
    // read attributes of the created file. These are needed to test 
    // the unchanged attributes for unwanted manipulation
    EInode *p_old_einode = new EInode;
    MDSTEST_read_einode(p_old_einode, 1,new_file);
    ASSERT_EQ(p_old_einode->inode.inode_number, new_file);
    
    // the new set mode
    mode_t newmode = generate_mode_t();
    inode_update_attributes_t *attrs = new inode_update_attributes_t;
    int attribute_bitfield = 0;
    // register mode as changed
    SET_MODE(attribute_bitfield);
    attrs->mode = newmode;

    MDSTEST_update(1,new_file,attrs,attribute_bitfield);
    
    // read attributes of the updated file.
    EInode *p_new_einode = new EInode;
    MDSTEST_read_einode(p_new_einode, 1,new_file);
    ASSERT_EQ(p_new_einode->inode.inode_number, new_file);
    
    // verify if all attributes match and mode differs.
    validate_einode_mode_equal(p_old_einode, p_new_einode, attribute_bitfield);
    delete p_old_einode;
    delete p_new_einode;
    delete attrs;
}

/**
 * @brief Create a Testfile directly on the Root object. Then changes  
 * the mtime of the file.
 * 
 * This is a functional test for the FsalUpdateAttributesRequest
 * handling. The effects on the metadata are validated by succeeding 
 * requests to the MDS and comparison of the old/new values.
 * */
TEST_F(MDS_Functional_UpdateFile_Test,scenario_12)
{
    // create the file to test the attribute change
    InodeNumber new_file;
    MDSTEST_create_file(1,1, &new_file);
    ASSERT_NE(new_file,0);
    
    // read attributes of the created file. These are needed to test 
    // the unchanged attributes for unwanted manipulation
    EInode *p_old_einode = new EInode;
    MDSTEST_read_einode(p_old_einode, 1,new_file);
    ASSERT_EQ(p_old_einode->inode.inode_number, new_file);
    
    // the new set mtime
    time_t newmtime = generate_time()+1;
    inode_update_attributes_t *attrs = new inode_update_attributes_t;
    int attribute_bitfield = 0;
    // register mtime as changed
    SET_MTIME(attribute_bitfield);
    attrs->mtime = newmtime;

    MDSTEST_update(1,new_file,attrs,attribute_bitfield);
    
    // read attributes of the updated file.
    EInode *p_new_einode = new EInode;
    MDSTEST_read_einode(p_new_einode, 1,new_file);
    ASSERT_EQ(p_new_einode->inode.inode_number, new_file);
    
    // verify if all attributes match and mode differs.
    validate_einode_mode_equal(p_old_einode, p_new_einode, attribute_bitfield);
    delete p_old_einode;
    delete p_new_einode;
    delete attrs;
}

/**
 * @brief Create a Testfile directly on the Root object. Then changes  
 * the atime of the file.
 * 
 * This is a functional test for the FsalUpdateAttributesRequest
 * handling. The effects on the metadata are validated by succeeding 
 * requests to the MDS and comparison of the old/new values.
 * */
TEST_F(MDS_Functional_UpdateFile_Test,scenario_13)
{
    // create the file to test the attribute change
    InodeNumber new_file;
    MDSTEST_create_file(1,1, &new_file);
    ASSERT_NE(new_file,0);
    
    // read attributes of the created file. These are needed to test 
    // the unchanged attributes for unwanted manipulation
    EInode *p_old_einode = new EInode;
    MDSTEST_read_einode(p_old_einode, 1,new_file);
    ASSERT_EQ(p_old_einode->inode.inode_number, new_file);
    
    // the new set mtime
    time_t newatime = generate_time()+1;
    inode_update_attributes_t *attrs = new inode_update_attributes_t;
    int attribute_bitfield = 0;
    // register mtime as changed
    SET_ATIME(attribute_bitfield);
    attrs->atime = newatime;

    MDSTEST_update(1,new_file,attrs,attribute_bitfield);

    // read attributes of the updated file.
    EInode *p_new_einode = new EInode;
    MDSTEST_read_einode(p_new_einode, 1,new_file);
    ASSERT_EQ(p_new_einode->inode.inode_number, new_file);
    
    // verify if all attributes match and mode differs.
    validate_einode_mode_equal(p_old_einode, p_new_einode, attribute_bitfield);
    delete p_new_einode;
    delete p_old_einode;
    delete attrs;
}


/**
 * @brief Create a Testfile directly on the Root object. Then changes  
 * the size of the file.
 * 
 * This is a functional test for the FsalUpdateAttributesRequest
 * handling. The effects on the metadata are validated by succeeding 
 * requests to the MDS and comparison of the old/new values.
 * */
TEST_F(MDS_Functional_UpdateFile_Test,scenario_14)
{
    // create the file to test the attribute change
    InodeNumber new_file;
    MDSTEST_create_file(1,1, &new_file);
    ASSERT_NE(new_file,0);
    
    // read attributes of the created file. These are needed to test 
    // the unchanged attributes for unwanted manipulation
    EInode *p_old_einode = new EInode;
    MDSTEST_read_einode(p_old_einode, 1,new_file);
    ASSERT_EQ(p_old_einode->inode.inode_number, new_file);
    
    // the new set size
    time_t newsize = generate_size_tx();
    inode_update_attributes_t *attrs = new inode_update_attributes_t;
    int attribute_bitfield = 0;
    // register size as changed
    SET_SIZE(attribute_bitfield);
    attrs->size = newsize;

    MDSTEST_update(1,new_file,attrs,attribute_bitfield);

    // read attributes of the updated file.
    EInode *p_new_einode = new EInode;
    MDSTEST_read_einode(p_new_einode, 1,new_file);
    ASSERT_EQ(p_new_einode->inode.inode_number, new_file);
    
    // verify if all attributes match and mode differs.
    validate_einode_mode_equal(p_old_einode, p_new_einode, attribute_bitfield);
    delete p_old_einode;
    delete p_new_einode;
    delete attrs;
}


/**
 * @brief Create a Testfile directly on the Root object. Then changes  
 * the link_count of the file.
 * 
 * This is a functional test for the FsalUpdateAttributesRequest
 * handling. The effects on the metadata are validated by succeeding 
 * requests to the MDS and comparison of the old/new values.
 * */
TEST_F(MDS_Functional_UpdateFile_Test, scenario_15)
{
    // create the file to test the attribute change
    InodeNumber new_file;
    MDSTEST_create_file(1,1, &new_file);
    ASSERT_NE(new_file,0);
    
    // read attributes of the created file. These are needed to test 
    // the unchanged attributes for unwanted manipulation
    EInode *p_old_einode = new EInode;
    MDSTEST_read_einode(p_old_einode, 1,new_file);
    ASSERT_EQ(p_old_einode->inode.inode_number, new_file);
    
    // the new set size
    nlink_t newlinkcount = p_old_einode->inode.link_count+1;
    inode_update_attributes_t *attrs = new inode_update_attributes_t;
    int attribute_bitfield = 0;
    // register size as changed
    SET_NLINK(attribute_bitfield);
    attrs->st_nlink = newlinkcount;

    MDSTEST_update(1,new_file,attrs,attribute_bitfield);

    // read attributes of the updated file.
    EInode *p_new_einode = new EInode;
    MDSTEST_read_einode(p_new_einode, 1,new_file);
    ASSERT_EQ(p_new_einode->inode.inode_number, new_file);
    
    // verify if all attributes match and mode differs.
    validate_einode_mode_equal(p_old_einode, p_new_einode, attribute_bitfield);
    delete p_old_einode;
    delete p_new_einode;
    delete attrs;
}


/**
 * @brief Create a Testfile directly on the Root object. Then changes  
 * the has_acl of the file.
 * 
 * This is a functional test for the FsalUpdateAttributesRequest
 * handling. The effects on the metadata are validated by succeeding 
 * requests to the MDS and comparison of the old/new values.
 * */
TEST_F(MDS_Functional_UpdateFile_Test,scenario_16)
{
    // create the file to test the attribute change
    InodeNumber new_file;
    MDSTEST_create_file(1,1, &new_file);
    ASSERT_NE(new_file,0);
    
    // read attributes of the created file. These are needed to test 
    // the unchanged attributes for unwanted manipulation
    EInode *p_old_einode = new EInode;
    MDSTEST_read_einode(p_old_einode, 1,new_file);
    ASSERT_EQ(p_old_einode->inode.inode_number, new_file);
    
    // the new set size
    int newhas_acl = p_old_einode->inode.has_acl+1;
    inode_update_attributes_t *attrs = new inode_update_attributes_t;
    int attribute_bitfield = 0;
    // register size as changed
    SET_HAS_ACL(attribute_bitfield);
    attrs->has_acl = newhas_acl;

    MDSTEST_update(1,new_file,attrs,attribute_bitfield);
    
    // read attributes of the updated file.
    EInode *p_new_einode = new EInode;
    MDSTEST_read_einode(p_new_einode, 1,new_file);
    ASSERT_EQ(p_new_einode->inode.inode_number, new_file);
    
    // verify if all attributes match and mode differs.
    validate_einode_mode_equal(p_old_einode, p_new_einode, attribute_bitfield);
    delete p_old_einode;
    delete p_new_einode;
    delete attrs;
}


/**
 * @brief Create a Testfile directly on the Root object. Then changes  
 * the uid of the file.
 * 
 * This is a functional test for the FsalUpdateAttributesRequest
 * handling. The effects on the metadata are validated by succeeding 
 * requests to the MDS and comparison of the old/new values.
 * */
TEST_F(MDS_Functional_UpdateFile_Test,scenario_17)
{
    // create the file to test the attribute change
    InodeNumber new_file;
    MDSTEST_create_file(1,1, &new_file);
    ASSERT_NE(new_file,0);
    
    // read attributes of the created file. These are needed to test 
    // the unchanged attributes for unwanted manipulation
    EInode *p_old_einode = new EInode;
    MDSTEST_read_einode(p_old_einode, 1,new_file);
    ASSERT_EQ(p_old_einode->inode.inode_number, new_file);
    
    // the new set size
    int new_uid = generate_uid();
    if (new_uid == p_old_einode->inode.uid)
    {
        new_uid++;
    }
    inode_update_attributes_t *attrs = new inode_update_attributes_t;
    int attribute_bitfield = 0;
    // register size as changed
    SET_UID(attribute_bitfield);
    attrs->uid = new_uid;

    MDSTEST_update(1,new_file,attrs,attribute_bitfield);
    
    // read attributes of the updated file.
    EInode *p_new_einode = new EInode;
    MDSTEST_read_einode(p_new_einode, 1,new_file);
    ASSERT_EQ(p_new_einode->inode.inode_number, new_file);
    
    // verify if all attributes match and mode differs.
    validate_einode_mode_equal(p_old_einode, p_new_einode, attribute_bitfield);
    delete p_old_einode;
    delete p_new_einode;
    delete attrs;
}

/**
 * @brief Create a Testfile directly on the Root object. Then changes  
 * the gid of the file.
 * 
 * This is a functional test for the FsalUpdateAttributesRequest
 * handling. The effects on the metadata are validated by succeeding 
 * requests to the MDS and comparison of the old/new values.
 * */
TEST_F(MDS_Functional_UpdateFile_Test,scenario_23)
{
    // create the file to test the attribute change
    InodeNumber new_file;
    MDSTEST_create_file(1,1, &new_file);
    ASSERT_NE(new_file,0);
    
    // read attributes of the created file. These are needed to test 
    // the unchanged attributes for unwanted manipulation
    EInode *p_old_einode = new EInode;
    MDSTEST_read_einode(p_old_einode, 1,new_file);
    ASSERT_EQ(p_old_einode->inode.inode_number, new_file);
    
    // the new set size
    int new_gid = generate_gid();
    if (new_gid == p_old_einode->inode.gid)
    {
        new_gid++;
    }
    inode_update_attributes_t *attrs = new inode_update_attributes_t;
    int attribute_bitfield = 0;
    // register size as changed
    SET_GID(attribute_bitfield);
    attrs->gid = new_gid;

    MDSTEST_update(1,new_file,attrs,attribute_bitfield);
        
    // read attributes of the updated file.
    EInode *p_new_einode = new EInode;
    MDSTEST_read_einode(p_new_einode, 1,new_file);
    ASSERT_EQ(p_new_einode->inode.inode_number, new_file);
    
    // verify if all attributes match and mode differs.
    validate_einode_mode_equal(p_old_einode, p_new_einode, attribute_bitfield);
    delete attrs;
    delete p_new_einode;
    delete p_old_einode;
}



/*
class MDS_Functional_UpdateFile_Test: public MetadataserverTest
{
    public:
        MDS_Functional_UpdateFile_Test():MetadataserverTest(){};
};
INSTANTIATE_TEST_CASE_P(
    blablubb,
    MDS_Functional_UpdateFile_Test,
    ::testing::Values("ZmqFsal_LookupByName"));
/**
 * @brief Test stub
 * *
TEST_F(MDS_Functional_UpdateFile_Test, Func)
{
    int32_t rc=0;
    ASSERT_EQ(0,rc);
} */
