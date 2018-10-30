#include "gtest/gtest.h"

#include <string>
#include <sstream>
#include <fstream>

#include "client/Client.h"
#include "tools/parity.h"

static int gen_string(size_t s, char *out)
{
    
}

struct thread_data {
    InodeNumber         inum;
    size_t              start;
    size_t              end;
    void                *data;
    Client              *p_cl;
    int                 success;
};

static char generate_random_fsname_char()
{
    char arr[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int ran = rand()% sizeof(arr);
    char c = arr[ran];
    return c;
}

static int generate_fsobjectname(FsObjectName *ptr, int len)
{
    FsObjectName name;
    if (len==0)
    {
        len = (rand()%MAX_NAME_LEN);
    }

    int i=0;
    while (i<=len)
    {
        name[i] = generate_random_fsname_char();
        i++;
    }
    name[len]='\0';
    memmove(ptr,&name,sizeof(FsObjectName));
    return 0;
}



void *write1(void *data)
{
        struct thread_data *tdata = (struct thread_data*)data;
        Client *p_cl = tdata->p_cl;
        int rc=0;
        //printf("inum:%llu.start:%llu, end:%llu\n",tdata->inum,tdata->start,tdata->end);
        rc = p_cl->acquireSID(0);
        rc = p_cl->getDataserverLayouts(0);
        InodeNumber root = 1;
        InodeNumber parent = 1;

        struct EInode einode;
        rc = p_cl->p_pnfs_cl->meta_get_file_inode(&root, &tdata->inum , &einode);
      //  printf("get einode:%d,inum:%llu.start:%llu\n",rc,tdata->inum,tdata->start);
        
        size_t opsize = tdata->end-tdata->start;
        void *dataread = malloc(opsize+1);
        
        for (int i=0; i<10;i++)
        {
        //    printf("inum:%llu.start:%llu, opsize:%llu\n",tdata->inum,tdata->start,opsize);
            rc = p_cl->handle_write(&einode,tdata->start,opsize,(char*)tdata->data);  
            if (rc) 
            {
      //          printf("ERROR.\n");
                exit(1);   
            }
            
            memset(dataread,0,opsize+1);
            rc = p_cl->handle_read(&einode,tdata->start,opsize,&dataread);
            rc = strncmp((char*)tdata->data,(char*)dataread, opsize);
            if (rc) 
            {
    //            printf("ERROR:%s!=%s.\n",(char*)tdata->data,(char*)dataread);
                exit(1);                
            }
//            printf("inum:%llu\n",tdata->inum);
  //          printf("Operation %u done:%s.\n",i,(char*)dataread);
        }
        tdata->success=1;
}

namespace
{

class ClientTest : public ::testing::Test
{
public:
    Client *p_cl;
    Logger *log;
    
    ClientTest()
    {
        log = new Logger();
        string s = string("/tmp/ClientTest.log");
	log->set_log_location(s);
        log->set_console_output(false);
    }
    ~ClientTest()
    {
        free(log);
    }

protected:
    void SetUp()
    {
        p_cl = new Client();       
        log->debug_log("cl created");
    }

    void TearDown()
    {
        delete p_cl;
        log->debug_log("cl deleted.");
     //   sleep(2);
    }
};


TEST_F(ClientTest, acquire_csid)
{       
        int rc=0;
        log->warning_log("start");    
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
}

TEST_F(ClientTest, get_dataserver_layouts)
{       
        int rc=0;
        log->warning_log("start 1");  
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
        
        log->warning_log("start 2");  
        rc = p_cl->getDataserverLayouts(0);
        ASSERT_EQ(rc,0);
        log->debug_log("Done.");
}


TEST_F(ClientTest, get_byterangelock)
{       
        int rc=0;
        log->warning_log("start 1");  
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
        
        log->warning_log("start 2");  
        rc = p_cl->getDataserverLayouts(0);
        ASSERT_EQ(rc,0);

        serverid_t mds = 0;
        InodeNumber inum = 2;
        uint64_t start=0;
        uint64_t end = 32*1024;
        rc = p_cl->acquireByteRangeLock(&mds,&inum,&start,&end);
        ASSERT_EQ(rc,0);
        log->warning_log("done");  
}

TEST_F(ClientTest, meta_create_file)
{       
        int rc=0;
        log->warning_log("start 1");  
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
        
        log->warning_log("start 2");  
        rc = p_cl->getDataserverLayouts(0);
        ASSERT_EQ(rc,0);

        InodeNumber result = 0;
        InodeNumber root = 1;
        InodeNumber parent = 1;
        FsObjectName name;
        rc = generate_fsobjectname(&name, 64);
        
        rc = p_cl->p_pnfs_cl->meta_create_file(&root,&parent,&name,&result);
        ASSERT_EQ(rc,0);
        ASSERT_NE(result,0);
        log->warning_log("start 3");  
}


TEST_F(ClientTest, meta_get_file)
{       
        int rc=0;
        log->warning_log("start 1");  
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
        
        log->warning_log("start 2");  
        rc = p_cl->getDataserverLayouts(0);
        ASSERT_EQ(rc,0);

        InodeNumber result = 0;
        InodeNumber root = 1;
        InodeNumber parent = 1;
        FsObjectName name;
        rc = generate_fsobjectname(&name, 64);
        
        rc = p_cl->p_pnfs_cl->meta_create_file(&root,&parent,&name,&result);
        ASSERT_EQ(rc,0);
        ASSERT_NE(result,0);
        log->warning_log("start 3");  

        struct EInode einode;
        rc = p_cl->p_pnfs_cl->meta_get_file_inode(&root, &result , &einode);
        ASSERT_EQ(rc,0);
        ASSERT_EQ(einode.inode.inode_number,result);        
        log->warning_log("start 4");  
}


TEST_F(ClientTest, write_n_read_s1_su1)
{       
        int rc=0;
        log->warning_log("start 1");  
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
        
        uint64_t a = 1;
        uint64_t b = 2;
        uint64_t c = 0;
        calc_parity(&a,&b,&c,1);
        ASSERT_EQ(c,3);
        
        log->warning_log("start 2");  
        rc = p_cl->getDataserverLayouts(0);
        ASSERT_EQ(rc,0);

        InodeNumber result = 0;
        InodeNumber root = 1;
        InodeNumber parent = 1;
        FsObjectName name;
        rc = generate_fsobjectname(&name, 64);
        ASSERT_EQ(rc,0);
        rc = p_cl->p_pnfs_cl->meta_create_file(&root,&parent,&name,&result);
        ASSERT_EQ(rc,0);
        ASSERT_NE(result,0);
        log->warning_log("start 3");  

        struct EInode einode;
        rc = p_cl->p_pnfs_cl->meta_get_file_inode(&root, &result , &einode);
        ASSERT_EQ(rc,0);
        ASSERT_EQ(einode.inode.inode_number,result);        
        log->warning_log("start 4");  
        
         serverid_t mds = 0;
        InodeNumber inum = 2;
        uint64_t start=0;
        uint64_t end = 32*1024;
        rc = p_cl->acquireByteRangeLock(&mds,&inum,&start,&end);
        ASSERT_EQ(rc,0);
        log->warning_log("start 2");  
        
        
        char *realdata = "abcdefghij";
        size_t opsize = 10;
        void *data = malloc(opsize+1);
        snprintf((char*)data,opsize+1,realdata);
        rc = p_cl->handle_write(&einode,0,opsize,data);  
        ASSERT_EQ(rc,0);
        log->warning_log("start 5");  
        
        void *dataread = malloc(opsize+1);
        memset(dataread,0,opsize+1);
        rc = p_cl->handle_read(&einode,0,opsize,&dataread);
        
        ASSERT_EQ(rc,0);
        ASSERT_STREQ(realdata,(char*)dataread);
        log->debug_log("Done.");
}

TEST_F(ClientTest, write_n_read_singlestripe_1)
{       
        int rc=0;
        log->warning_log("start 1");  
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
        
        log->warning_log("start 2");  
        rc = p_cl->getDataserverLayouts(0);
        ASSERT_EQ(rc,0);

        InodeNumber result = 0;
        InodeNumber root = 1;
        InodeNumber parent = 1;
        FsObjectName name;
        rc = generate_fsobjectname(&name, 64);
        
        rc = p_cl->p_pnfs_cl->meta_create_file(&root,&parent,&name,&result);
        ASSERT_EQ(rc,0);
        ASSERT_NE(result,0);
        log->warning_log("start 3");  

        struct EInode einode;
        rc = p_cl->p_pnfs_cl->meta_get_file_inode(&root, &result , &einode);
        ASSERT_EQ(rc,0);
        ASSERT_EQ(einode.inode.inode_number,result);        
        log->warning_log("start 4");  
        
        serverid_t mds = 0;
        InodeNumber inum = 2;
        uint64_t start=0;
        uint64_t end = 32*1024;
        rc = p_cl->acquireByteRangeLock(&mds,&inum,&start,&end);
        ASSERT_EQ(rc,0);
        log->warning_log("start 2");          
        
        char *realdata = "abcdefghijABCDEFGHIJ";
        size_t opsize = 20;
        void *data = malloc(opsize+1);
        snprintf((char*)data,opsize+1,realdata);
        rc = p_cl->handle_write(&einode,0,opsize,data);  
        ASSERT_EQ(rc,0);
        log->warning_log("start 5");  
        
        void *dataread = malloc(opsize+1);
        memset(dataread,0,opsize+1);
        rc = p_cl->handle_read(&einode,0,opsize,&dataread);
        
        ASSERT_EQ(rc,0);
        ASSERT_STREQ(realdata,(char*)dataread);
        log->debug_log("Done.");
}
 

TEST_F(ClientTest, write_n_read_single_s1_su2_num1)
{       
        int rc=0;
        log->warning_log("start 1");  
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
        
        log->warning_log("start 2");  
        rc = p_cl->getDataserverLayouts(0);
        ASSERT_EQ(rc,0);

        InodeNumber result = 0;
        InodeNumber root = 1;
        InodeNumber parent = 1;
        FsObjectName name;
        rc = generate_fsobjectname(&name, 64);
        
        rc = p_cl->p_pnfs_cl->meta_create_file(&root,&parent,&name,&result);
        ASSERT_EQ(rc,0);
        ASSERT_NE(result,0);
        log->warning_log("start 3");  

        struct EInode einode;
        rc = p_cl->p_pnfs_cl->meta_get_file_inode(&root, &result , &einode);
        ASSERT_EQ(rc,0);
        ASSERT_EQ(einode.inode.inode_number,result);        
        log->warning_log("start 4");  
        
        serverid_t mds = 0;
        InodeNumber inum = 2;
        uint64_t start=0;
        uint64_t end = 32*1024;
        rc = p_cl->acquireByteRangeLock(&mds,&inum,&start,&end);
        ASSERT_EQ(rc,0);
        log->warning_log("start 2");          
        
        char *realdata = "abcdefghijABCDEFGHIJ";
        size_t opsize = 20;
        void *data = malloc(opsize+1);
        snprintf((char*)data,opsize+1,realdata);
        for (int i=0; i < 1; i++)
        {
            rc = p_cl->handle_write(&einode,0,opsize,data);  
            ASSERT_EQ(rc,0);
            log->debug_log("write: %d iteration successful ",i);
            //usleep(10000);    
        }
        
        void *dataread = malloc(opsize+1);
        memset(dataread,0,opsize+1);
        rc = p_cl->handle_read(&einode,0,opsize,&dataread);
        
        ASSERT_EQ(rc,0);
        ASSERT_STREQ(realdata,(char*)dataread);
        log->debug_log("Done.");
}


TEST_F(ClientTest, write_n_read_single_s2_su2)
{       
        int rc=0;
        log->warning_log("start 1");  
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
        
        log->warning_log("start 2");  
        rc = p_cl->getDataserverLayouts(0);
        ASSERT_EQ(rc,0);

        InodeNumber result = 0;
        InodeNumber root = 1;
        InodeNumber parent = 1;
        FsObjectName name;
        rc = generate_fsobjectname(&name, 64);
        
        rc = p_cl->p_pnfs_cl->meta_create_file(&root,&parent,&name,&result);
        ASSERT_EQ(rc,0);
        ASSERT_NE(result,0);
        log->warning_log("start 3");  

        struct EInode einode;
        rc = p_cl->p_pnfs_cl->meta_get_file_inode(&root, &result , &einode);
        ASSERT_EQ(rc,0);
        ASSERT_EQ(einode.inode.inode_number,result);        
        log->warning_log("start 4");  
        
        serverid_t mds = 0;
        InodeNumber inum = 2;
        uint64_t start=0;
        uint64_t end = 32*1024;
        rc = p_cl->acquireByteRangeLock(&mds,&inum,&start,&end);
        ASSERT_EQ(rc,0);
        log->warning_log("start 2");          
        
        char *realdata = "abcdefghijABCDEFGHIJ0123456789";
        size_t opsize = 30;
        void *data = malloc(opsize+1);
        snprintf((char*)data,opsize+1,realdata);
        rc = p_cl->handle_write(&einode,0,opsize,data);  
        ASSERT_EQ(rc,0);
        log->warning_log("start 5");  
        
        void *dataread = malloc(opsize+1);
        memset(dataread,0,opsize+1);
        rc = p_cl->handle_read(&einode,0,opsize,&dataread);
        
        ASSERT_EQ(rc,0);
        ASSERT_STREQ(realdata,(char*)dataread);
        log->debug_log("Done.");
}

/*
TEST_F(ClientTest, write_n_read_singlestripe_1_clients_2)
{       
        int rc=0;
        log->warning_log("start 1");  
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
        
        log->warning_log("start 2");  
        rc = p_cl->getDataserverLayouts(0);
        ASSERT_EQ(rc,0);

        InodeNumber result = 0;
        InodeNumber root = 1;
        InodeNumber parent = 1;
        FsObjectName name;
        rc = generate_fsobjectname(&name, 64);
        
        rc = p_cl->p_pnfs_cl->meta_create_file(&root,&parent,&name,&result);
        ASSERT_EQ(rc,0);
        ASSERT_NE(result,0);
        log->warning_log("start 3");  

        std::vector<struct thread_data*> vec;
        struct thread_data *p1 = new struct thread_data;
        p1->inum=result;
        p1->start=0;
        p1->data= (void*)"abcdefghij";
        p1->end=10;
        p1->p_cl = p_cl;
        p1->success=0;
        vec.push_back(p1);
        struct thread_data *p2 = new struct thread_data;
        p2->inum=result;
        p2->start=10;
        p2->data=(void*)"1234567890";
        p2->end=20;
        p2->success=0;
        p2->p_cl = p_cl;
        vec.push_back(p2);
        
        std::vector<struct thread_data*>::iterator it = vec.begin();
        for (it ; it !=vec.end() ; it++)
        {
            pthread_t worker_thread;
            struct thread_data *d = *it;

            rc = pthread_create(&worker_thread, NULL,write1 , d );
            if (rc)
            {
                log->error_log("thread creation failed.");
            }
            //printf("Thread created.\n");
        }
        
        sleep(5);
        it = vec.begin();
        for (it ; it !=vec.end() ; it++)
        {
            struct thread_data *d = *it;
            ASSERT_EQ(d->success,1);
        }
        log->debug_log("Done.");
}
 * */
 


/*
TEST_F(ClientTest, fail)
{       
        int rc=0;
        
        ASSERT_TRUE(false);
}


TEST_F(ClientTest, acquire_csid)
{       
        int rc=0;
        log->warning_log("start");    
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
}
*/

}//namespace
