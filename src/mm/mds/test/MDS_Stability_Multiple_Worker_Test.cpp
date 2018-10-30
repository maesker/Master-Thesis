#include "mm/mds/test/AbstractMDSTest.h"

/**
 * @file MDS_Stability_Multiple_Worker_Test.cpp
 * @author Markus Mäsker, <maesker@gmx.net>
 * @date 12.08.2011
 *
 * @brief Tests the stability of the MetadataServer when running with multiple
 * worker threads.
 *
 * */


struct thread_attrs_01
{
    InodeNumber partition_root;
    InodeNumber parent;
    int iterations;
}thread_attrs_01_t;



/**
 * @class MDS_Stability_Multiple_Worker_Test
 * @brief Constructor. Creates all the necessary modules needed by
 * the mds.
 * */
class MDS_Stability_Multiple_Worker_Test : public AbstractMDSTest 
{
public:
    MDS_Stability_Multiple_Worker_Test()
    {
        objname = "MDS_Stability_Multiple_Worker_Test";
    }
    ~MDS_Stability_Multiple_Worker_Test()
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

void* run_creates(void *p_attrs)
{
    struct thread_attrs_01 *attrs = (struct thread_attrs_01*)p_attrs;
    InodeNumber partition_root = attrs->partition_root;
    InodeNumber parent = attrs->parent;
    InodeNumber res;
    ZmqFsalHelper *zh = new ZmqFsalHelper();
    int i=0;
    for (i; i<attrs->iterations; i++)
    {
        res = zh->create_file(partition_root, parent);
    }
}

/**
 * @brief Create a directory and all threads create files within that directory
 * */
TEST_F(MDS_Stability_Multiple_Worker_Test,scenario_01)
{    
    InodeNumber partition_root = 1;
    InodeNumber parent_inum ;
    MDSTEST_mkdir(partition_root, partition_root, &parent_inum);

    int iterations = 1000;
    int threads = 30;
    stack<pthread_t*> pthreads;  
    int i;
    for(i = 1; i <= threads; i++)
    {
        pthread_t *worker_thread = (pthread_t*) malloc(sizeof(pthread_t));
        pthreads.push(worker_thread);
        struct thread_attrs_01 attrs;
        attrs.partition_root = partition_root;
        attrs.parent = parent_inum;
        attrs.iterations=iterations;
        pthread_create(worker_thread, NULL, run_creates, &attrs );
    }
    log->debug_log("threads created");
    int total = iterations*threads;
    while (!pthreads.empty())
    {
        log->debug_log("waiting for thead join.");
        pthread_t *worker_thread = pthreads.top();
        pthread_join( *worker_thread, NULL);
        pthreads.pop();
        free(worker_thread);
        log->debug_log("joined.");
    }
    log->debug_log("Readdir starting");
    ReaddirOffset offset = 0;
    ReadDirReturn *p_rdir = new ReadDirReturn;
    MDSTEST_readdir(partition_root, parent_inum, offset,p_rdir);
    log->debug_log("Readdir done");
    ASSERT_EQ(total, p_rdir->dir_size);
    
    // fsal response states that new file was created. check that
    delete p_rdir;
}

/**
 * @brief Create threads and all threads have a directory to create files in
 * */
TEST_F(MDS_Stability_Multiple_Worker_Test,scenario_02)
{    
    InodeNumber partition_root = 1;
    InodeNumber parent_inum ;
    int iterations = 1000;
    int threads = 30;
    stack<pthread_t*> pthreads;  
    int i;
    for(i = 1; i <= threads; i++)
    {
        
        MDSTEST_mkdir(partition_root, partition_root, &parent_inum);
        pthread_t *worker_thread = (pthread_t*) malloc(sizeof(pthread_t));
        pthreads.push(worker_thread);
        struct thread_attrs_01 attrs;
        attrs.partition_root = partition_root;
        attrs.parent = parent_inum;
        attrs.iterations=iterations;
        pthread_create(worker_thread, NULL, run_creates, &attrs );
    }
    log->debug_log("threads created");

    while (!pthreads.empty())
    {
        log->debug_log("waiting for thead join.");
        pthread_t *worker_thread = pthreads.top();
        pthread_join( *worker_thread, NULL);
        pthreads.pop();
        free(worker_thread);
        log->debug_log("joined.");
    }
    log->debug_log("Readdir starting");
    ReaddirOffset offset = 0;
    ReadDirReturn *p_rdir = new ReadDirReturn;
    MDSTEST_readdir(partition_root, parent_inum, offset,p_rdir);
    log->debug_log("Readdir done");
    ASSERT_EQ(iterations, p_rdir->dir_size);
    
    // fsal response states that new file was created. check that
    delete p_rdir;
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
