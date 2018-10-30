#include "mm/mds/test/AbstractMDSTest.h"

/**
 * @file MDS_Functional_Readdir_Test.cpp
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
 * @class MDS_Functional_Readdir_Test
 * @brief Constructor. Creates all the necessary modules needed by
 * the mds.
 * */
class MDS_Functional_Readdir_Test : public AbstractMDSTest
{
public:
    MDS_Functional_Readdir_Test()
    {
        objname = "MDS_Functional_Readdir_Test";
    }
    ~MDS_Functional_Readdir_Test()
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
 * @brief Create 20 Testfiles directly on the Root object. This means,
 * that both partition_root_inode_number and parent_inode_number refere
 * to the root object 1. 
 * 
 * This is a functional test for the FsalReaddirRequest
 * handling. The effects on the metadata are validated by succeeding 
 * requests to the MDS and comparison of the results.
 * */
TEST_F(MDS_Functional_Readdir_Test,scenario_01)
{
    InodeNumber partition_root = 1;
    InodeNumber parent_inum;
    InodeNumber inum;
    int32_t files_cnt = 500;
    int32_t i = 0;
    MDSTEST_create_file(partition_root,1,&parent_inum);
    for (i=0; i<files_cnt; i++)
    {
        MDSTEST_create_file(partition_root,parent_inum,&inum);
        // create and initialize the fsal request structure
    }
    // fsal response states that new file was created. check that
    int32_t recv_dircnt = 0;
    int32_t total_dircnt = 0;
    ReaddirOffset offset = 0;
    ReadDirReturn *p_rdir = new ReadDirReturn;
    while (1)
    {        
        MDSTEST_readdir(1,1,offset,p_rdir);
        total_dircnt = p_rdir->dir_size;
        recv_dircnt += p_rdir->nodes_len;
        if (recv_dircnt >= p_rdir->dir_size)
        {
            break;
        }
        offset += FSAL_READDIR_EINODES_PER_MSG;
        //ASSERT_EQ( , p_rdir->nodes_len);
    }
    ASSERT_EQ(total_dircnt, recv_dircnt);
    delete p_rdir;
}
 


/*
class MDS_Functional_Readdir_Test: public MetadataserverTest
{
    public:
        MDS_Functional_Readdir_Test():MetadataserverTest(){};
};
INSTANTIATE_TEST_CASE_P(
    blablubb,
    MDS_Functional_Readdir_Test,
    ::testing::Values("ZmqFsal_LookupByName"));
/**
 * @brief Test stub
 * *
TEST_F(MDS_Functional_Readdir_Test, Func)
{
    int32_t rc=0;
    ASSERT_EQ(0,rc);
} */
