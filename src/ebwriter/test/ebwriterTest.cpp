#include "gtest/gtest.h"
#include <string.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>

#include "global_types.h"
#include "mm/mds/MockupObjects.h"


#ifdef __cplusplus
extern "C"
{
#endif
#include "ebwriter.h"
#include "mlt/mlt.h"
#include "mlt/mlt_file_provider.h"
#ifdef __cplusplus
}
#endif

namespace
{

void sighup(int c)
{
    signal(SIGHUP,sighup); /* reset signal */
    exit(0);
}

void create_fork()
{ 
    if ( fork() == 0)
    {   /* child */
        signal(SIGHUP,sighup); /* set functon calls */
        sleep(5);
    }
}


class ebwriterTest : public ::testing::Test
{
protected:
    struct mallinfo mi_start;
    struct mallinfo mi_end;
    std::string path;
    struct mlt *p_mymlt;
    ebwriterTest()
    {
        path = "/tmp/export.conf";
        p_mymlt = create_dummy_mlt();
    }

    ~ebwriterTest()
    {
    }

    virtual void SetUp()
    {
        init_memleak_check_start();
    }

    virtual void TearDown()
    {
        init_memleak_check_end();
    }
    
    void init_memleak_check_start()
    {
        mi_start = mallinfo();
    }

    void init_memleak_check_end()
    {
        mi_end = mallinfo();
        int diff = mi_end.uordblks-mi_start.uordblks;
        if (diff)
        {
            printf("Diff:%d.\n",diff);
        }
        ASSERT_EQ(mi_start.uordblks, mi_end.uordblks);
    }
};


TEST_F(ebwriterTest, CreateExportBlock)
{
    struct server mds_server =  { "127.0.0.1", (unsigned short) 9000};
    int rc = create_export_block_conf(p_mymlt, path.c_str(), &mds_server);
    ASSERT_EQ(0,rc);
}
    
TEST_F(ebwriterTest, SendSighup)
{
    int rc;
    std::string proc = "ebwriterTest";
    create_fork();
    rc = send_signal(proc.c_str() , SIGHUP);
    ASSERT_EQ(0,rc);
}
    

} // namespace
