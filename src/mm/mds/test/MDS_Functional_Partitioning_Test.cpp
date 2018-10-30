#include "mm/mds/test/AbstractMDSTest.h"

/**
 * @file MDS_Functional_Partitioning_Test.cpp
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
 * @class MDS_Functional_Partitioning_Test
 * @brief Constructor. Creates all the necessary modules needed by
 * the mds.
 * */
class MDS_Functional_Partitioning_Test : public AbstractMDSTest
{
public:
    MDS_Functional_Partitioning_Test()
    {
        objname = "MDS_Functional_Partitioning_Test";
    }
    ~MDS_Functional_Partitioning_Test()
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
 * @brief Test creation of new partition
 * 
 * Creates a new journal instance for the new partition. If the 
 * new Journal was created and is accessible by the Journal manager the 
 * operation was successful.
 * */
TEST_F(MDS_Functional_Partitioning_Test,scenario_01)
{
    int32_t rc=0;
    InodeNumber inum = generate_random_inodenumber();
    
    // create new directory on the fs root
    MDSTEST_mkdir(1, 1, &inum);
    ASSERT_NE(0, inum);
    
    // migrate 
    rc = mds->handle_migration_create_partition(&inum);
    ASSERT_EQ(0,rc);
    
    // get the resulting journal instance
    Journal *j = jm->get_instance()->get_journal(inum);
    if (j==NULL)
    {
        printf("No journal received.\n");
        FAIL();
    }    
    // check if journals id equals the expected one
    ASSERT_EQ(inum, j->get_journal_id());
}
 

/**
 * @brief Test update of ganesha configureation
 * 
 * Commands the MDS server to create the export configuration and send a 
 * sighup to the ganesha daemon.
 * */
TEST_F(MDS_Functional_Partitioning_Test,scenario_02)
{
    int32_t rc=0;
    rc = mds->handle_ganesha_configuration_update();
    ASSERT_EQ(0,rc);
}


/**
 * @brief Test quit partition.
 * 
 * Creates a partition and then quits handling it.
 * */
TEST_F(MDS_Functional_Partitioning_Test, scenario_03)
{
    int32_t rc=0;
    FsObjectName newdir;
    generate_fsobjectname(&newdir, 64);
    std::string path = "/"+string(newdir);
    InodeNumber inum = generate_random_inodenumber();
    
    // create new directory on the fs root
    MDSTEST_mkdir(1, 1, &inum);
    ASSERT_NE(0, inum);
    
    // migrate 
    rc = mds->handle_migration_create_partition(&inum);
    ASSERT_EQ(0,rc);
    
    // get the resulting journal instance
    Journal *j = jm->get_instance()->get_journal(inum);
    if (j==NULL)
    {
        printf("No journal received.\n");
        FAIL();
    }    
    // check if journals id equals the expected one
    ASSERT_EQ(inum, j->get_journal_id());

    // quit handling this partition
    rc = mds->handle_migration_quit_partition(&inum);
    ASSERT_EQ(0,rc);
    
    // get the resulting journal instance
    j = jm->get_instance()->get_journal(inum);
    if (j!=NULL)
    {
        printf("Journal received, it should have been destroyed.\n");
        FAIL();
    }
} 



/*
class MDS_Functional_CreateFile_Test: public MetadataserverTest
{
    public:
        MDS_Functional_CreateFile_Test():MetadataserverTest(){};
};
INSTANTIATE_TEST_CASE_P(
    blablubb,
    MDS_Functional_CreateFile_Test,
    ::testing::Values("ZmqFsal_LookupByName"));
/**
 * @brief Test stub
 * *
TEST_F(MDS_Functional_CreateFile_Test, Func)
{
    int32_t rc=0;
    ASSERT_EQ(0,rc);
} */
