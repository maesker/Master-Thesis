#include "gtest/gtest.h"

#include <string>
#include <sstream>
#include <fstream>

#include "client/Client.h"
#include "tools/parity.h"


struct thread_data {
    InodeNumber         inum;
    size_t              start;
    size_t              end;
    void                *data;
    Client              *p_cl;
    int                 success;
    int                 iterations;
};

static char generate_random_fsname_char()
{
    char arr[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ123456789";
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


static void gen_string(size_t s, char *out)
{
    for (int i =0; i<s; i++)
    {
        //snprintf(out,1,"%c",generate_random_fsname_char());
        *out = generate_random_fsname_char();
        out++;
    }
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

        struct EInode einode;
        rc = p_cl->p_pnfs_cl->meta_get_file_inode(&root, &tdata->inum , &einode);
      //  printf("get einode:%d,inum:%llu.start:%llu\n",rc,tdata->inum,tdata->start);
        
        size_t opsize = tdata->end-tdata->start;
        void *dataread = malloc(opsize+1);
        
        char *randdata=(char*)malloc(opsize+1);
        memset(randdata,0,opsize+1);
        for (int i=0; i<tdata->iterations;i++)
        {
            //gen_string(opsize,randdata);
            memset(dataread,0,opsize+1);
            sprintf(randdata,"%s:%d.",tdata->data,i);
        //    printf("inum:%llu.start:%llu, opsize:%llu\n",tdata->inum,tdata->start,opsize);
            rc = p_cl->handle_write(&einode,tdata->start,opsize,randdata);  
            if (rc) 
            {
                printf("ERROR write operation.\n");
                exit(1);   
            }
            
            rc = p_cl->handle_read(&einode,tdata->start,opsize,&dataread);
            rc = strncmp(randdata,(char*)dataread, opsize);
            if (rc) 
            {
                printf("ERROR:exsp:%s!=%s read.\n",randdata,(char*)dataread);
                exit(1);
                
            }
//            printf("inum:%llu\n",tdata->inum);
  //          printf("Operation %u done:%s.\n",i,(char*)dataread);
        }
        tdata->success=1;
}

namespace
{

class ClientStressTest : public ::testing::Test
{
public:
    Client *p_cl;
    Logger *log;
    
    ClientStressTest()
    {
        log = new Logger();
        string s = string("/tmp/ClientTest.log");
	log->set_log_location(s);
        log->set_console_output(false);
    }
    ~ClientStressTest()
    {
        delete log;
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


TEST_F(ClientStressTest, write_n_read_singlestripe_1_clients_2)
{       
      std::vector<pthread_t> threadvec;  
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
        
        int iterations = 10;
        std::vector<struct thread_data*> vec;
        struct thread_data *p1 = new struct thread_data;
        p1->inum=result;
        p1->start=0;
        p1->data= (void*)"T1";
        p1->end=10;
        p1->p_cl = p_cl;
        p1->success=0;
        p1->iterations=iterations;
        vec.push_back(p1);
        struct thread_data *p2 = new struct thread_data;
        p2->inum=result;
        p2->start=10;
        p2->data=(void*)"T2";
        p2->end=20;
        p2->success=0;
        p2->iterations = iterations;
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
            threadvec.push_back(worker_thread);
            //printf("Thread created.\n");
        }
        
        std::vector<pthread_t>::iterator it2 = threadvec.begin();
        for(it2; it2!=threadvec.end(); it2++)
        {
            pthread_join(*it2,NULL);
        }
        //sleep(iterations/5);
        it = vec.begin();
        for (it ; it !=vec.end() ; it++)
        {
            struct thread_data *d = *it;
            ASSERT_EQ(d->success,1);
            delete *it;
        }
        log->debug_log("Done.");
}
 
TEST_F(ClientStressTest, write_n_read__2files_2clients_each)
{       
      std::vector<pthread_t> threadvec;  
        int rc=0;
        log->warning_log("start 1");  
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
        
        log->warning_log("start 2");  
        rc = p_cl->getDataserverLayouts(0);
        ASSERT_EQ(rc,0);

        InodeNumber result_a = 0;
        InodeNumber result_b = 0;        
        InodeNumber root = 1;
        InodeNumber parent = 1;
        
        FsObjectName name_a;
        FsObjectName name_b;
        rc = generate_fsobjectname(&name_a, 64);
        rc = generate_fsobjectname(&name_b, 64);
        rc = p_cl->p_pnfs_cl->meta_create_file(&root,&parent,&name_a,&result_a);
        ASSERT_EQ(rc,0);
        ASSERT_NE(result_a,0);
        rc = p_cl->p_pnfs_cl->meta_create_file(&root,&parent,&name_b,&result_b);
        ASSERT_EQ(rc,0);
        ASSERT_NE(result_b,0);
        
        int iterations = 10;
        std::vector<struct thread_data*> vec;
        struct thread_data *p1 = new struct thread_data;
        p1->inum=result_a;
        p1->start=0;
        p1->data= (void*)"Ta1";
        p1->end=10;
        p1->p_cl = p_cl;
        p1->success=0;
        p1->iterations=iterations;
        vec.push_back(p1);
        struct thread_data *p2 = new struct thread_data;
        p2->inum=result_a;
        p2->start=10;
        p2->data=(void*)"Ta2";
        p2->end=20;
        p2->success=0;
        p2->iterations = iterations;
        p2->p_cl = p_cl;
        vec.push_back(p2);
        
        struct thread_data *p3 = new struct thread_data;
        p3->inum=result_b;
        p3->start=0;
        p3->data=(void*)"Tb1";
        p3->end=10;
        p3->success=0;
        p3->iterations = iterations;
        p3->p_cl = p_cl;
        vec.push_back(p3);
        struct thread_data *p4 = new struct thread_data;
        p4->inum=result_b;
        p4->start=10;
        p4->data=(void*)"Tb2";
        p4->end=20;
        p4->success=0;
        p4->iterations = iterations;
        p4->p_cl = p_cl;
        vec.push_back(p4);
        
        
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
            threadvec.push_back(worker_thread);
            //printf("Thread created.\n");
        }
        
        std::vector<pthread_t>::iterator it2 = threadvec.begin();
        for(it2; it2!=threadvec.end(); it2++)
        {
            pthread_join(*it2,NULL);
        }
        //sleep(iterations/5);
        it = vec.begin();
        for (it ; it !=vec.end() ; it++)
        {
            struct thread_data *d = *it;
            ASSERT_EQ(d->success,1);
        }
        log->debug_log("Done.");
}
 

/*
TEST_F(ClientStressTest, fail)
{       
        int rc=0;
        
        ASSERT_TRUE(false);
}

/*

TEST_F(ClientTest, acquire_csid)
{       
        int rc=0;
        log->warning_log("start");    
        rc = p_cl->acquireSID(0);
        ASSERT_EQ(rc,0);
}
*/

}//namespace
