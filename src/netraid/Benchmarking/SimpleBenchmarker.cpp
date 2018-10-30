
#include <iosfwd>
#include <sstream>

#include "Benchmarking/SimpleBenchmarker.h"

void *diskwrite(void *data);

#define INTERVAL 50
static const int TEST_REPEATS=2;

/*/
static double __device_diskio(int threads, size_t bytes, int iterations, std::string dir,bool sync)
{
    int rc=-1;
    //printf("threads:%d,bytes:%d,iter:%d,file:%s",threads,bytes,iterations,dir.c_str());
    std::vector<pthread_t> *threadvec = new  std::vector<pthread_t>();
    std::vector<struct thread_data*> *vec = new std::vector<struct thread_data*>();
    for (int i=0; i<threads; i++)
    {
        thread_data *dd = new thread_data();
        dd->iterations=iterations;
        dd->size=bytes;
        dd->data = malloc(bytes);
        dd->id = i;
        dd->sync=sync;
        dd->dir = dir;
        memset(dd->data,1,bytes);
        dd->results = new std::map<uint32_t, struct resultdata*>();
        vec->push_back(dd);
    }    

    struct timertimes  start = timer_start() ;
    std::vector<struct thread_data*>::iterator it = vec->begin();
    for (it ; it !=vec->end() ; it++)
    {
        pthread_t worker_thread;
        struct thread_data *d = *it;
        rc = pthread_create(&worker_thread, NULL,diskwrite , d );
        threadvec->push_back(worker_thread);
    }

    std::vector<pthread_t>::iterator it2 = threadvec->begin();
    for(it2; it2!=threadvec->end(); it2++)
    {
        pthread_join(*it2,NULL);
    }
    double diff = timer_end(start);    
    size_t total = bytes*iterations*threads/(1024*1024);
    double rate = total/(diff);
//    std::cout << "Run took " << diff << " seconds." << std::endl;
//    std::cout << "Throughput: Total "<< total << "MBs at " << rate << "MB/sec." << std::endl;
    //analyse();
    
    it = vec->begin();
    for (it ; it !=vec->end() ; it++)
    {
        delete *it;
    }    
    delete threadvec;
    delete vec;
    return rate;
}
**/

static std::string get_time()
{      
    struct tm *zeit;
    time_t sekunde;
    char s[32];
    time(&sekunde);
    zeit = localtime(&sekunde);
    strftime(s,20,"%y%m%d_%H%M%S",zeit);
    return std::string(s);
}

static const char* get_fullpath(char *name, int iterations, int threads, size_t bytes, bool sync)
{
    std::stringstream ss("");
    ss << name << "_iter_" << iterations << "_threads_" << threads;
    ss << "_bytes_" << bytes;
    if (sync)
    {
        ss << "_sync";
    }
    ss << get_time();    
    ss << ".csv";
    return ss.str().c_str();
}

double format_result(std::map<int,double> *res, std::stringstream& ss)
{
    double av=0;
    std::map<int,double>::iterator it = res->begin();
    for (it; it!=res->end(); it++)
    {
        ss << "Run:" << it->first << " rate " << it->second << ";\n";
        av += it->second;
    }
    av = av / res->size();
    ss << "Average: " << av << "MB/sec\n";
    return av;
}

/*
void *xxx_diskwrite(void *data)
{
    struct thread_data *tdata = (struct thread_data*) data;
    tdata->success=0;
    double res;
    double diff;
    struct timertimes tt = timer_start();
    
    double latency_av = 0.0;
    double latency_min = 1000.0;
    double latency_max = 0.0;
    int x=0;
    int operation_iter=0;
    std::stringstream ss("");
    ss << tdata->dir.c_str();
    ss << "/file_" << tdata->id;
    for (int i=0; i<tdata->iterations; i++)
    {
        std::stringstream tempss ("");
        tempss << ss.str().c_str();
        tempss << "_iteration_" << i;
        int fh = open(tempss.str().c_str() ,O_WRONLY); 
        if(fh == -1)
        {
            fh = creat(tempss.str().c_str(), 0600);
        }
        size_t count = pwrite(fh, tdata->data, tdata->size , 0);
        if (count!=tdata->size)
        {
            tdata->success=-1;
            break;
        }  
        if (tdata->sync) fsync(fh);
        close(fh);
        
        operation_iter++;
        if (operation_iter%INTERVAL == 0)
        {
            struct resultdata *r = new struct resultdata;
            r->
            //diff = timer_end(tt);
            res = (operation_iter*1.0)/(diff);
            printf("Ops:%u, Average Ops/sec:%f.\n",operation_iter,res);
            tdata->results->insert(std::pair<uint32_t,double>(operation_iter,diff));            
            
            struct resultdata *resdata = new struct resultdata;
            //resdata->opspersec = (operation_iter*1.0)/(diff);
            resdata->opspersec = 0;
            resdata->latency_av = latency_av/INTERVAL;
            resdata->latency_max=latency_max;
            resdata->latency_min=latency_min;
            printf("Ops:%u, Average Ops/sec:%f, latency_av:%f.\n",operation_iter,resdata->opspersec, resdata->latency_av);
            tdata->results->insert(std::pair<uint32_t,struct resultdata*>(operation_iter,resdata));   
            
            latency_av = 0.0;
            latency_min = 100.0;
            latency_max = 0.0;
        } 
    }
    pthread_exit(0);
}
**/


void *diskwrite(void *data)
{
    struct thread_data *tdata = (struct thread_data*)data;
    int rc=0;
    uint32_t operation_iter=0;
   
    std::stringstream ss("");
    ss << tdata->dir.c_str();
    ss << "/file_" << tdata->id;
    struct timertimes  start_latencytime;
    double latency;
    double diff;
    tdata->starttime = timer_start();    

    double latency_av = 0.0;
    double latency_min = 1000.0;
    double latency_max = 0.0;
    int x=0;
    for (int i=0; i<tdata->iterations;i++)
    {
//        printf("i:%d",i);
        start_latencytime=timer_start();
        
        std::stringstream tempss ("");
        tempss << ss.str().c_str();
        tempss << "_iteration_" << i;
        int fh = open(tempss.str().c_str() ,O_WRONLY); 
        if(fh == -1)
        {
            fh = creat(tempss.str().c_str(), 0600);
        }
        //printf("i:%d",i);
        size_t count = pwrite(fh, tdata->data, tdata->size , 0);
        //printf("i:%d:wrote:%d\n",i,count);
        if (count!=tdata->size)
        {
            //printf("failure...\n");
            tdata->success=-1;
            exit(1); 
        }  
       // printf("i:%d\n",i);
        if (tdata->sync) fsync(fh);
        close(fh);
        //printf("i 55:%d\n",i);
        
        latency = timer_end(start_latencytime);
        
        if (latency<latency_min) latency_min=latency;
        if (latency>latency_max) latency_max=latency;
        latency_av += latency;
        if (rc) 
        {
            printf("ERROR write operation.\n");
            exit(1);   
        }
        
        operation_iter++;
        if (operation_iter%INTERVAL == 0)
        {
            diff = timer_end(tdata->starttime);   
            struct resultdata *resdata = new struct resultdata;
            //resdata->opspersec = (operation_iter*1.0)/(diff);
            resdata->opspersec = (operation_iter*1.0)/(diff);
            resdata->latency_av = latency_av/INTERVAL;
            resdata->latency_max=latency_max;
            resdata->latency_min=latency_min;
            printf("Ops:%u, Average Ops/sec:%f, latency_av:%f.\n",operation_iter,resdata->opspersec, resdata->latency_av);
            tdata->results->insert(std::pair<uint32_t,struct resultdata*>(operation_iter,resdata));   
            
            latency_av = 0.0;
            latency_min = 100.0;
            latency_max = 0.0;
         //   printf("i:%d\n",i);
        }                
        //printf("i:%d\n",i);
        //rc = p_cl->handle_read(tdata->einode,tdata->start,opsize,&dataread);
        //rc = strncmp(randdata,(char*)dataread, opsize);
        //if (rc) 
        //{
        //    printf("ERROR:exsp:%s!=%s read.\n",randdata,(char*)dataread);
        //    exit(1);
        //}
//            printf("inum:%llu\n",tdata->inum);
//          printf("Operation %u done:%s.\n",i,(char*)dataread);
        
    }
    tdata->success=1;
}


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


void *acquire_release_bench(void *data)
{
    struct thread_data *tdata = (struct thread_data*)data;
    Client *p_cl = tdata->p_cl;
    int rc=0;
    serverid_t mds=0;
    uint32_t operation_iter=0;
   
    struct timertimes  start_latencytime;
    double latency;
    //double diff;
    //tdata->starttime = timer_start();
    

    double latency_av = 0.0;
    double latency_min = 1000.0;
    double latency_max = 0.0;
    int x=0;
    int limit=10;
    for (int i=0; i<tdata->iterations;i++)
    {
        //gen_string(opsize,randdata);
        //memset(dataread,0,opsize+1);
        //sprintf(randdata,"%s:%d.",tdata->data,i);
        //printf("inum:%llu.start:%llu, opsize:%llu\n",tdata->einode->inode.inode_number,tdata->start,opsize);
        start_latencytime=timer_start();
        for (x=0; x<limit;x++)
        {
            p_cl->acquireByteRangeLock(&mds,&tdata->einode->inode.inode_number,&tdata->lockstart,&tdata->lockend);   
            p_cl->releaselock(&mds,&tdata->einode->inode.inode_number,&tdata->lockstart,&tdata->lockend);        
        }
        latency = 1000.0*timer_end(start_latencytime)/(limit*1.0);
        
        if (latency<latency_min) latency_min=latency;
        if (latency>latency_max) latency_max=latency;
        latency_av += latency;
        if (rc) 
        {
            printf("ERROR write operation.\n");
            exit(1);   
        }
        
        operation_iter++;
        if (operation_iter%INTERVAL == 0)
        {
            //diff = timer_end(tdata->starttime);   
            
            struct resultdata *resdata = new struct resultdata;
            //resdata->opspersec = (operation_iter*1.0)/(diff);
            resdata->opspersec = 0;
            resdata->latency_av = latency_av/INTERVAL;
            resdata->latency_max=latency_max;
            resdata->latency_min=latency_min;
            printf("Ops:%u, Average Ops/sec:%f, latency_av:%f.\n",operation_iter,resdata->opspersec, resdata->latency_av);
            tdata->results->insert(std::pair<uint32_t,struct resultdata*>(operation_iter,resdata));   
            
            latency_av = 0.0;
            latency_min = 100.0;
            latency_max = 0.0;
        }                
        //rc = p_cl->handle_read(tdata->einode,tdata->start,opsize,&dataread);
        //rc = strncmp(randdata,(char*)dataread, opsize);
        //if (rc) 
        //{
        //    printf("ERROR:exsp:%s!=%s read.\n",randdata,(char*)dataread);
        //    exit(1);
        //}
//            printf("inum:%llu\n",tdata->inum);
//          printf("Operation %u done:%s.\n",i,(char*)dataread);
        
    }
    tdata->success=1;
}


void *ccc_pingpong_bench(void *data)
{
    struct thread_data *tdata = (struct thread_data*)data;
    Client *p_cl = tdata->p_cl;
    int rc=0;
    serverid_t mds=0;
    uint32_t operation_iter=0;
   
    struct timertimes  start_latencytime;
    double latency;
    //double diff;
    //tdata->starttime = timer_start();
    

    double latency_av = 0.0;
    double latency_min = 100.0;
    double latency_max = 0.0;
    for (int i=0; i<tdata->iterations;i++)
    {
        //gen_string(opsize,randdata);
        //memset(dataread,0,opsize+1);
        //sprintf(randdata,"%s:%d.",tdata->data,i);
        //printf("inum:%llu.start:%llu, opsize:%llu\n",tdata->einode->inode.inode_number,tdata->start,opsize);
        start_latencytime=timer_start();
        p_cl->pingpong();
        latency = timer_end(start_latencytime);
        
        if (latency<latency_min) latency_min=latency;
        if (latency>latency_max) latency_max=latency;
        latency_av += latency;
        if (rc) 
        {
            printf("ERROR write operation.\n");
            exit(1);   
        }
        
        operation_iter++;
        if (operation_iter%INTERVAL == 0)
        {
            //diff = timer_end(tdata->starttime);   
            
            struct resultdata *resdata = new struct resultdata;
            //resdata->opspersec = (operation_iter*1.0)/(diff);
            resdata->opspersec = 0;
            resdata->latency_av = latency_av/INTERVAL;
            resdata->latency_max=latency_max;
            resdata->latency_min=latency_min;
            printf("Ops:%u, Average Ops/sec:%f, latency_av:%f.\n",operation_iter,resdata->opspersec, resdata->latency_av);
            tdata->results->insert(std::pair<uint32_t,struct resultdata*>(operation_iter,resdata));   
            
            latency_av = 0.0;
            latency_min = 100.0;
            latency_max = 0.0;
        }                
        //rc = p_cl->handle_read(tdata->einode,tdata->start,opsize,&dataread);
        //rc = strncmp(randdata,(char*)dataread, opsize);
        //if (rc) 
        //{
        //    printf("ERROR:exsp:%s!=%s read.\n",randdata,(char*)dataread);
        //    exit(1);
        //}
//            printf("inum:%llu\n",tdata->inum);
//          printf("Operation %u done:%s.\n",i,(char*)dataread);
        
    }
    tdata->success=1;
}


void *write_only(void *data)
{
    struct thread_data *tdata = (struct thread_data*)data;
    Client *p_cl = tdata->p_cl;
    int rc=0;
    serverid_t mds=0;
    uint32_t operation_iter=0;
   
    struct timertimes  start_latencytime;
    double latency;
    double diff;
    tdata->starttime = timer_start();
    
    size_t opsize = tdata->end-tdata->start;
    void *dataread = malloc(opsize+1);

    char *randdata=(char*)malloc(opsize+1);
    memset(randdata,0,opsize+1);
    sprintf(randdata,"%s",tdata->data);
    
    double latency_av = 0.0;
    double latency_min = 100.0;
    double latency_max = 0.0;
    for (int i=0; i<tdata->iterations;i++)
    {
        //gen_string(opsize,randdata);
        //memset(dataread,0,opsize+1);
        //sprintf(randdata,"%s:%d.",tdata->data,i);
        //printf("inum:%llu.start:%llu, opsize:%llu\n",tdata->einode->inode.inode_number,tdata->start,opsize);
        start_latencytime=timer_start();
        
        rc = p_cl->handle_write(tdata->einode,tdata->start,opsize,randdata);  
        
        latency = timer_end(start_latencytime);
        //printf("Latency:%f\n",latency);
        if (latency<latency_min) latency_min=latency;
        if (latency>latency_max) latency_max=latency;
        latency_av += latency;
        if (rc) 
        {
            printf("ERROR write operation.\n");
            exit(1);   
        }
        
        operation_iter++;
        if (operation_iter%INTERVAL == 0)
        {
            diff = timer_end(tdata->starttime);   
            
            struct resultdata *resdata = new struct resultdata;
            resdata->opspersec = (operation_iter*1.0)/(diff);
            resdata->latency_av = latency_av/INTERVAL;
            resdata->latency_max=latency_max;
            resdata->latency_min=latency_min;
            printf("Ops:%u, Average Ops/sec:%f, latency_av:%f.\n",operation_iter,resdata->opspersec, resdata->latency_av);
            tdata->results->insert(std::pair<uint32_t,struct resultdata*>(operation_iter,resdata));   
            
            latency_av = 0.0;
            latency_min = 100.0;
            latency_max = 0.0;
        }                
        //rc = p_cl->handle_read(tdata->einode,tdata->start,opsize,&dataread);
        //rc = strncmp(randdata,(char*)dataread, opsize);
        //if (rc) 
        //{
        //    printf("ERROR:exsp:%s!=%s read.\n",randdata,(char*)dataread);
        //    exit(1);
        //}
//            printf("inum:%llu\n",tdata->inum);
//          printf("Operation %u done:%s.\n",i,(char*)dataread);
        
    }
    tdata->success=1;
    free(dataread);
    free(randdata);
}


void *write_locked(void *data)
{
    struct thread_data *tdata = (struct thread_data*)data;
    Client *p_cl = tdata->p_cl;
    int rc=0;
    serverid_t mds=0;
    uint32_t operation_iter=0;
   
    struct timertimes  start_latencytime;
    double latency;
    double diff;
    tdata->starttime = timer_start();
    
    size_t opsize = tdata->end-tdata->start;
    void *dataread = malloc(opsize+1);
    void *datadump = malloc(opsize+1);

    char *randdata=(char*)malloc(opsize+1);
    memset(randdata,0,opsize+1);
    sprintf(randdata,"%s",tdata->data);
    
    double latency_av = 0.0;
    double latency_min = 100.0;
    double latency_max = 0.0;
    for (int i=0; i<tdata->iterations;i++)
    {
        //gen_string(opsize,randdata);
        //memset(dataread,0,opsize+1);
        //sprintf(randdata,"%s:%d.",tdata->data,i);
        //printf("inum:%llu.start:%llu, opsize:%llu\n",tdata->einode->inode.inode_number,tdata->start,opsize);
        start_latencytime=timer_start();
        
        if (tdata->lockmode) p_cl->acquireByteRangeLock(&mds,&tdata->einode->inode.inode_number,&tdata->lockstart,&tdata->lockend);   
        p_cl->handle_read(tdata->einode,tdata->start,opsize,&dataread);
        calc_parity (dataread, datadump, datadump, opsize);
        rc = p_cl->handle_write_lock(tdata->einode,tdata->start,opsize,randdata);  
        if (tdata->lockmode) p_cl->releaselock(&mds,&tdata->einode->inode.inode_number,&tdata->lockstart,&tdata->lockend);
        
        latency = timer_end(start_latencytime);
        //printf("Latency:%f\n",latency);
        if (latency<latency_min) latency_min=latency;
        if (latency>latency_max) latency_max=latency;
        latency_av += latency;
        if (rc) 
        {
            printf("ERROR write operation.\n");
            exit(1);   
        }
        
        operation_iter++;
        if (operation_iter%INTERVAL == 0)
        {
            diff = timer_end(tdata->starttime);   
            
            struct resultdata *resdata = new struct resultdata;
            resdata->opspersec = (operation_iter*1.0)/(diff);
            resdata->latency_av = latency_av/INTERVAL;
            resdata->latency_max=latency_max;
            resdata->latency_min=latency_min;
            printf("Ops:%u, Average Ops/sec:%f, latency_av:%f.\n",operation_iter,resdata->opspersec, resdata->latency_av);
            tdata->results->insert(std::pair<uint32_t,struct resultdata*>(operation_iter,resdata));   
            
            latency_av = 0.0;
            latency_min = 100.0;
            latency_max = 0.0;
        }                
        //rc = p_cl->handle_read(tdata->einode,tdata->start,opsize,&dataread);
        //rc = strncmp(randdata,(char*)dataread, opsize);
        //if (rc) 
        //{
        //    printf("ERROR:exsp:%s!=%s read.\n",randdata,(char*)dataread);
        //    exit(1);
        //}
//            printf("inum:%llu\n",tdata->inum);
//          printf("Operation %u done:%s.\n",i,(char*)dataread);
        
    }
    tdata->success=1;
    free(dataread);
    free(randdata);
}


void *readonly(void *data)
{
    printf("start.\n");
    struct thread_data *tdata = (struct thread_data*)data;
    Client *p_cl = tdata->p_cl;
    int rc=0;
    serverid_t mds=0;
    uint32_t operation_iter=0;
   
    struct timertimes  start_latencytime;
    double latency;
    double diff;
    tdata->starttime = timer_start();    
    size_t opsize = tdata->end-tdata->start;
    
    
    void *randdata=malloc(opsize+1);
    memset(randdata,0,opsize+1);  
    //printf("x\n");
    rc = p_cl->handle_write(tdata->einode,tdata->start,opsize,(void*)randdata);  
    //printf("x\n");
    void *dataread = malloc(opsize+1);
    
    double latency_av = 0.0;
    double latency_min = 100.0;
    double latency_max = 0.0;
    //int limit = 100;
    //printf("loop\n");
    for (int i=0; i<tdata->iterations;i++)
    {
        //gen_string(opsize,randdata);
        //memset(dataread,0,opsize+1);
        //sprintf(randdata,"%s:%d.",tdata->data,i);
        //printf("inum:%llu.start:%llu, opsize:%llu\n",tdata->einode->inode.inode_number,tdata->start,opsize);
        start_latencytime=timer_start();
        //for (int x = 0; x<limit; x++)
        //{
            p_cl->handle_read(tdata->einode,tdata->start,opsize,&dataread);
        //}
        latency = timer_end(start_latencytime);
        //printf("done.\n");
        //printf("Latency:%f\n",latency);
        if (latency<latency_min) latency_min=latency;
        if (latency>latency_max) latency_max=latency;
        latency_av += latency;
        if (rc) 
        {
            printf("ERROR write operation.\n");
            exit(1);   
        }
        
        operation_iter++;
        if (operation_iter%INTERVAL == 0)
        {
            diff = timer_end(tdata->starttime);   
            
            struct resultdata *resdata = new struct resultdata;
            resdata->opspersec = (operation_iter*1.0)/(diff);
            resdata->latency_av = latency_av/INTERVAL;
            resdata->latency_max=latency_max;
            resdata->latency_min=latency_min;
            printf("Ops:%u, Average Ops/sec:%f, latency_av:%f.\n",operation_iter,resdata->opspersec, resdata->latency_av);
            tdata->results->insert(std::pair<uint32_t,struct resultdata*>(operation_iter,resdata));   
            
            latency_av = 0.0;
            latency_min = 100.0;
            latency_max = 0.0;
        }                
        //rc = p_cl->handle_read(tdata->einode,tdata->start,opsize,&dataread);
        //rc = strncmp(randdata,(char*)dataread, opsize);
        //if (rc) 
        //{
        //    printf("ERROR:exsp:%s!=%s read.\n",randdata,(char*)dataread);
        //    exit(1);
        //}
//            printf("inum:%llu\n",tdata->inum);
//          printf("Operation %u done:%s.\n",i,(char*)dataread);        
    }
    tdata->success=1;
    free(dataread);
    //printf("exit\n");
}

SimpleBenchmarker::SimpleBenchmarker(int iter, int threads, std::string logloc)
{
    log = new Logger();
    log->set_log_level(LOG_LEVEL_DEBUG);
    log->set_console_output(false);
    log->set_log_location(logloc);
    cl = new Client();
    raid = new Libraid4(log);
    root = 1;       
    result_files = new std::vector<std::string>();
    p_ssh = new sshwrapper();
        
    iterations = iter;
    this->threads = threads;
    this->sync=true;
}

SimpleBenchmarker::~SimpleBenchmarker()
{
    delete log;
    delete cl;
}

int SimpleBenchmarker::setup()
{
    log->debug_log("setup");
    int rc = cl->acquireSID(0);
    if (rc) return rc;
    log->debug_log("setup");
    rc = cl->getDataserverLayouts(0);
    log->debug_log("setup");
    if (rc) return rc;
    log->debug_log("setup performed successfully.");
}

struct thread_data* SimpleBenchmarker::newThreadDataSU(struct EInode *einode, StripeUnitId sid)
{
    struct thread_data *p1 = new struct thread_data;
    struct filelayout_raid4 *flr4 = (struct filelayout_raid4 *) &einode->inode.layout_info[0];
    
    p1->einode = einode;
    p1->start=flr4->stripeunitsize*sid;
    p1->data = malloc(flr4->stripeunitsize);
    gen_string(flr4->stripeunitsize, (char*) p1->data);
    p1->end=flr4->stripeunitsize+p1->start;
    p1->p_cl = cl;
    p1->lockmode=false;
    p1->success=0;
    p1->iterations=iterations;
    p1->results = new std::map<uint32_t, struct resultdata*>();
    return p1;
}

struct thread_data* SimpleBenchmarker::newThreadDataS(struct EInode *einode, StripeId sid)
{
    struct thread_data *p1 = new struct thread_data;
    struct filelayout_raid4 *flr4 = (struct filelayout_raid4 *) &einode->inode.layout_info[0];
    size_t stripesize = (flr4->groupsize-1)*flr4->stripeunitsize;
    p1->einode = einode;
    p1->start=stripesize*sid;
    p1->data = malloc(stripesize);
    gen_string(stripesize, (char*) p1->data);
    p1->end=stripesize+p1->start;
    p1->p_cl = cl;
    p1->success=0;
    p1->results = new std::map<uint32_t, struct resultdata*>();
    p1->iterations=iterations;
    return p1;
}

void SimpleBenchmarker::write_result(const char *path, std::stringstream& ss)
{
    log->debug_log("Writing:%s.",path);
    ofstream myfile;
    std::string sfile = std::string(path);
    myfile.open (sfile.c_str());
    myfile.write(ss.str().c_str(), ss.str().size());
    myfile.flush();
    myfile.close();
    result_files->push_back(sfile);
    log->debug_log("Done");
}

void SimpleBenchmarker::send_results()
{
    log->debug_log("sending results...");
    std::vector<std::string>::iterator it = result_files->begin();
    for (it; it!= result_files->end(); it++)
    {   
        log->debug_log("file:%s",it->c_str());
        std::string target = std::string(this->sshtarget);
        target.append("/");
        std::string filename;
        size_t pos = it->find_last_of("/");
        if(pos != std::string::npos)
            filename.assign(it->begin() + pos + 1, it->end());
        else
            filename = *it;
        target.append(filename);

        log->debug_log("host:%s, user:%s, ps:%s, file:%s, %s",this->sshserver.c_str(),this->sshuser.c_str(), this->sshpw.c_str(), it->c_str(),target.c_str());
        p_ssh->scp_write(this->sshserver,this->sshuser,this->sshpw, *it, target);
        log->debug_log("sent");
    }
    log->debug_log("done.");
}

int SimpleBenchmarker::createRandomFile(InodeNumber *inum)
{
    int rc;
    InodeNumber parent = 1;
    FsObjectName name;
    rc = generate_fsobjectname(&name, 64);
    if (rc) return rc;  
    log->debug_log("send create request:root=%llu",root);
    rc = cl->p_pnfs_cl->meta_create_file(&root,&parent,&name,inum);
    log->debug_log("Created:name:%s,inum:%llu",&name[0],*inum);
    return rc;
}

int SimpleBenchmarker::eval_StripeUnit()
{
    log->debug_log("starting");
    int rc=0;
    setup();    
    /*std::stringstream name("");    
    std::string tt = get_time();
    name << "/tmp/SingleStripeUnit_";
    name << iterations;
    name << "_" << tt.c_str();
    name << ".log";
    log->set_log_location(name.str().c_str());*/
    InodeNumber inum;
    for (int i=1; i<=threads; i++)
    {
        log->debug_log("Run:i=%d",i);
        rc = createRandomFile(&inum);
        log->debug_log("file created.");
        struct EInode einode;
        rc = cl->p_pnfs_cl->meta_get_file_inode(&root, &inum , &einode);
        log->debug_log("get einode:%d,inum:%llu.\n",rc,inum);
        std::vector<struct thread_data*> *vec = new std::vector<struct thread_data*>();
        for (int j=0; j<i; j++)
        {
            vec->push_back(newThreadDataSU(&einode,j));
        }
        log->debug_log("created file... inum:%llu",inum);  
        rc = perform(vec, &write_only);

        analyse(vec, stripeunit, i);

        std::vector<struct thread_data*>::iterator it = vec->begin();
        //while (it !=vec->end())
        //{
            //delete it;
        //}
        delete vec;
    }
    log->debug_log("Done");
    return rc;
}

int SimpleBenchmarker::eval_Stripe()
{
    log->debug_log("Start");
    setup();/*
    std::stringstream name("");    
    name << "/tmp/SingleStripe_";
    name << iterations;
    std:string tt =  get_time();
    name << "_" << tt.c_str();
    name << ".log";
    log->set_log_location(name.str().c_str());*/
    InodeNumber inum;
    int rc = createRandomFile(&inum);
    
    struct EInode einode;
    rc = cl->p_pnfs_cl->meta_get_file_inode(&root, &inum , &einode);
    log->debug_log("get einode:%d,inum:%llu.\n",rc,inum);

    std::vector<struct thread_data*> *vec = new std::vector<struct thread_data*>();
    for (int i=0; i<threads; i++)
    {
        vec->push_back(newThreadDataS(&einode,i));
    }    
    log->debug_log("created file... inum:%llu",inum);  
    rc = perform(vec, &write_only);
     
    analyse(vec,stripe,this->threads);
    
    std::vector<struct thread_data*>::iterator it = vec->begin();
    /*while (it !=vec->end())
    {
        vec->erase(it++);
    }*/
    log->debug_log("Done");
    return rc;
}


int SimpleBenchmarker::eval_Stripe_lockmode()
{
    log->debug_log("starting lockmode");
    setup();    
    /*std::stringstream name("");    
    std::string tt = get_time();
    name << "/tmp/SingleStripeUnit_";
    name << iterations;
    name << "_" << tt.c_str();
    name << ".log";
    log->set_log_location(name.str().c_str());*/
    int rc=0;
    InodeNumber inum;
    size_t lockstart= 0;
    size_t lockend = this->threads*this->size;
    for (int t=1; t<=threads; t++)
    {       
        log->debug_log("Run:t=%d",t);
        rc = createRandomFile(&inum);
        log->debug_log("file created.");
        struct EInode einode;
        rc = cl->p_pnfs_cl->meta_get_file_inode(&root, &inum , &einode);
        log->debug_log("get einode:%d,inum:%llu.\n",rc,inum);       
        std::vector<struct thread_data*> *vec = new std::vector<struct thread_data*>();
        for (int i=0; i<t; i++)
        {
            struct thread_data* p = newThreadDataSU(&einode,i);
            p->lockmode=true;
            p->lockstart = lockstart;
            p->lockend = lockend;
           // p->p_cl = new Client();
            //p->p_cl->acquireSID(0);
            //p->p_cl->getDataserverLayouts(0);    
            vec->push_back(p);
        }   
        log->debug_log("created file... inum:%llu",inum);  
        rc = perform(vec, &write_locked);

        analyse(vec, stripeunitlocked, t);

        std::vector<struct thread_data*>::iterator it = vec->begin();
        //while (it !=vec->end())
        //{
            //delete it;
        //}
        delete vec;
    }
    log->debug_log("Done");
    return rc;
}

int SimpleBenchmarker::eval_ReadSU()
{
    log->debug_log("Start");
    setup();
    InodeNumber inum;
    int rc ;
    for (int t=1; t<threads; t++)
    {
        rc= createRandomFile(&inum);

        struct EInode einode;
        rc = cl->p_pnfs_cl->meta_get_file_inode(&root, &inum , &einode);
        log->debug_log("get einode:%d,inum:%llu.\n",rc,inum);

        std::vector<struct thread_data*> *vec = new std::vector<struct thread_data*>();
        for (int i=0; i<t; i++)
        {
            vec->push_back(newThreadDataSU(&einode,i));
        }    
        log->debug_log("created file... inum:%llu",inum);  
        rc = perform(vec, &readonly);

        analyse(vec,readsubench,t);

        std::vector<struct thread_data*>::iterator it = vec->begin();
        /*while (it !=vec->end())
        {
            vec->erase(it++);
        }*/
        delete vec;
    }
    log->debug_log("Done");
    return rc;
}


int SimpleBenchmarker::eval_lock_roundtrip()
{
    log->debug_log("starting lock roundtrip");
    setup();    

    int rc=0;
    size_t lockstart= 0;
    size_t lockend = this->threads*this->size;
    for (int t=1; t<=threads; t++)
    {       
        log->debug_log("Run:t=%d",t);
        struct EInode einode;
        einode.inode.inode_number=2;
        
        std::vector<struct thread_data*> *vec = new std::vector<struct thread_data*>();
        for (int i=0; i<t; i++)
        {
            struct thread_data* p = newThreadDataSU(&einode,i);
            p->lockmode=true;
            p->lockstart = lockstart;
            p->lockend = lockend;
            vec->push_back(p);
        }   
        //log->debug_log("created file... inum:%llu",inum);  
        rc = perform(vec, &acquire_release_bench);

        analyse(vec, lockroundtrip, t);

        std::vector<struct thread_data*>::iterator it = vec->begin();
        //while (it !=vec->end())
        //{
            //delete it;
        //}
        delete vec;
    }
    log->debug_log("Done");
    return rc;
}


int SimpleBenchmarker::eval_pingpong()
{
    log->debug_log("starting lock roundtrip");
    setup();    

    int rc=0;
    size_t lockstart= 0;
    size_t lockend = this->threads*this->size;
    for (int t=1; t<2; t++)
    {       
        log->debug_log("Run:t=%d",t);
        struct EInode einode;
        einode.inode.inode_number=2;
        
        std::vector<struct thread_data*> *vec = new std::vector<struct thread_data*>();
        for (int i=0; i<t; i++)
        {
            struct thread_data* p = newThreadDataSU(&einode,i);
            p->lockmode=true;
            p->lockstart = lockstart;
            p->lockend = lockend;
            vec->push_back(p);
        }   
        //log->debug_log("created file... inum:%llu",inum);  
        rc = perform(vec, &ccc_pingpong_bench);

        analyse(vec, pingpongbench, t);

        std::vector<struct thread_data*>::iterator it = vec->begin();
        //while (it !=vec->end())
        //{
            //delete it;
        //}
        delete vec;
    }
    log->debug_log("Done");
    return rc;
}


int SimpleBenchmarker::perform(std::vector<struct thread_data*> *v, void* cb(void*))
{ 
    int rc;
    log->debug_log("Perform:threads:%d,iterations:%d",threads,iterations);
    struct timertimes  start = timer_start();
    std::vector<pthread_t> *threadvec = new std::vector<pthread_t>(); 
    std::vector<struct thread_data*>::iterator it = v->begin();
    for (it ; it !=v->end() ; it++)
    {
        pthread_t worker_thread;
        struct thread_data *d = *it;

        rc = pthread_create(&worker_thread, NULL,cb , d );
        if (rc)
        {
            log->error_log("thread creation failed.");
        }
        threadvec->push_back(worker_thread);
        //printf("Thread created.\n");
    }
    log->debug_log("Threads:%u, created...",v->size());
    std::vector<pthread_t>::iterator it2 = threadvec->begin();
    for(it2; it2!=threadvec->end(); it2++)
    {
        pthread_join(*it2,NULL);
    }
    double diff = timer_end(start);    
    log->debug_log("Run took %f secs.",diff);
    std::cout << "Run took " << diff << " seconds." << std::endl;
   
    log->debug_log("Done.");
    return 0;
}

int SimpleBenchmarker::analyse(std::vector<struct thread_data*> *vec, enum benchtype bench, int threads)
{
    log->debug_log("Start");
    double rate;
    std::string latency_unit = std::string("sec");
    std::string optype = std::string("");
    if (bench==stripeunit)
    {
        optype.append("SU_");
    }
    else if (bench==stripe)
    {
        optype.append("S_");
    }
    else if (bench==stripeunitlocked)
    {
        optype.append("SLK_");
    }
    else if (bench==lockroundtrip)
    {
        optype.append("LRT_");
        latency_unit = std::string("ms");
    }
    else if (bench==pingpongbench)
    {
        optype.append("PP_");
        latency_unit = std::string("ms");
    }    
    else if (bench==readsubench)
    {
        optype.append("RDSU_");
    }    
    else if (bench==diskiobench)
    {
        optype.append("DIO_");
    }    

    log->debug_log("Analyse...\n");
    std::map<uint32_t,struct resultdata*> res;
    std::map<uint32_t,struct resultdata*>::iterator resit;
    int numinst = vec->size();
    struct EInode *einode = NULL;
    std::vector<struct thread_data*>::iterator it = vec->begin();
    for (it ; it !=vec->end() ; it++)
    {
        std::map<uint32_t,struct resultdata*>::iterator it2 = (*it)->results->begin();
        for (it2; it2!=(*it)->results->end(); it2++)
        {
            resit = res.find(it2->first);
            if (resit==res.end())
            {
                struct resultdata *resdata = new struct resultdata;
                resdata->latency_av=0.0;
                resdata->latency_max=0.0;
                resdata->latency_min=100.0;
                resdata->opspersec=0;
                res.insert(std::pair<uint32_t,struct resultdata*>(it2->first,resdata));
                resit = res.find(it2->first);
            }
            resit->second->latency_av += it2->second->latency_av ; 
            resit->second->latency_min = min(it2->second->latency_min, resit->second->latency_min);
            resit->second->latency_max = max(it2->second->latency_max, resit->second->latency_max);
            resit->second->opspersec += it2->second->opspersec;
        }  
        if ((*it)->einode!=NULL) einode = (*it)->einode;
    }
    std::stringstream ss("");
    log->debug_log("start...");
    ss << "Ops,secs,Ops/Sec,MB/sec,Latencymin,Latencyav,Latencymax in" << latency_unit.c_str() << ";\n"; 
    for (resit=res.begin();resit!=res.end();resit++)
    {
        double latency_av = resit->second->latency_av/(1.0*numinst);
        double opspersec = resit->second->opspersec/(1.0);
        double totaltime = (resit->first*numinst)/opspersec;
        log->debug_log("latency_av=%f",latency_av);
        rate=0;
        if (einode!=NULL)
        {
            filelayout_raid *fl = (filelayout_raid *) &einode->inode.layout_info[0];
            size_t blocksize = fl->raid4.stripeunitsize;
            uint8_t groupsize = fl->raid4.groupsize-1;

            if (bench==stripeunit || bench==stripeunitlocked)
            {
                rate = opspersec*blocksize/(1024*1024);
            }
            else if (bench==stripe)
            {
                rate = opspersec*groupsize*blocksize/(1024*1024);
            }
        }
        else 
        {
            rate = opspersec*numinst*size/(1024*1024);
        }
        ss << resit->first << "," << totaltime << "," << opspersec << "," << rate << "," << resit->second->latency_min << "," << latency_av << "," << resit->second->latency_max << ";\n" ;
        //log->debug_log("First Ops:%u (total:%u) took %3f secs. Total ops/sec:%f",resit->first,resit->first*numinst,x,t);
        printf("First Ops:%u (total:%u) took %3f secs. Total ops/sec:%f, Rate:%f MB/sec, Latency min,av,max:%f, %f, %f\n",resit->first,resit->first*numinst,totaltime,opspersec,rate,resit->second->latency_min,latency_av,resit->second->latency_max);
    }
    std::stringstream filename("");
    filename << "/tmp/SiBe_" << optype << iterations << "_"<< threads << "_" << get_time() << ".csv";
    write_result(filename.str().c_str(),ss);
    log->debug_log("done");
}

int SimpleBenchmarker::device_diskio(int threads, size_t bytes, int iterations, std::string dir,bool sync)
{
    log->debug_log("start");
    double result;
    iterations = min(iterations,250);
    int rc=-1;
    //printf("threads:%d,bytes:%d,iter:%d,file:%s",threads,bytes,iterations,dir.c_str());
    std::vector<pthread_t> *threadvec = new  std::vector<pthread_t>();
    std::vector<struct thread_data*> *vec = new std::vector<struct thread_data*>();
    for (int i=0; i<threads; i++)
    {
        thread_data *dd = new thread_data();
        dd->iterations=iterations;
        dd->size=bytes;
        dd->data = malloc(bytes);
        dd->id = i;
        dd->sync=sync;
        dd->einode=NULL;
        dd->dir = dir;
        memset(dd->data,1,bytes);
        dd->results = new std::map<uint32_t, struct resultdata*>();
        vec->push_back(dd);
    }    
        
    xsystools_fs_mkdir(dir);
    xsystools_fs_flush(dir);

    struct timertimes  start = timer_start() ;
    std::vector<struct thread_data*>::iterator it = vec->begin();
    for (it ; it !=vec->end() ; it++)
    {
        pthread_t worker_thread;
        struct thread_data *d = *it;
        rc = pthread_create(&worker_thread, NULL,diskwrite , d );
        threadvec->push_back(worker_thread);
    }

    std::vector<pthread_t>::iterator it2 = threadvec->begin();
    for(it2; it2!=threadvec->end(); it2++)
    {
        pthread_join(*it2,NULL);
    }
    
    //double diff = timer_end(start);    
    //size_t total = bytes*iterations*threads/(1024*1024);
    //double rate = total/(diff);
//    std::cout << "Run took " << diff << " seconds." << std::endl;
//    std::cout << "Throughput: Total "<< total << "MBs at " << rate << "MB/sec." << std::endl;
    analyse(vec, diskiobench,threads);
    
    it = vec->begin();
    for (it ; it !=vec->end() ; it++)
    {
        delete *it;
    }    
    delete threadvec;
    delete vec;
    return 0;
}

int SimpleBenchmarker::parity_calc()
{
    std::stringstream ss("");
    ss << "1GB,time,GB/s;\n";
    size_t s = 4*1024*1024;
    int its = 256;

    void *a = malloc(s);
    void *b = malloc(s);
    void *res = malloc(s);
    
    double diff=0.0;
    double result = 0.0;
    for (int i=1; i<20; i++)
    {   
        struct timertimes  start = timer_start();
        for (int k=0;k<its;k++)
        {
            calc_parity (a, b, res, s);
        }
        diff = timer_end(start);    
        //printf("Diff:%f\n",diff);
        result = (its*s*1.0/(1024*1024))/diff;
        ss << i << "," << diff << "," << result << ";\n";            
    }
    std::stringstream path("");    
    path << "/tmp/Paritybench_" << get_time() << ".csv";
    
    write_result(path.str().c_str(),ss);
    free(a);
    free(b);
    free(res);
    return 0;
}

int SimpleBenchmarker::systeminfo()
{
    std::string filename = std::string("/tmp/Systeminfo_");
    char hostname[1024];
    gethostname(hostname, 1024);
    filename.append(&hostname[0]);
    filename.append(".txt");

    
    std::vector<std::string> *v = new std::vector<std::string>();
    v->push_back(std::string("hdparm -I /dev/sda1 "));
    v->push_back(std::string("cat /proc/cpuinfo "));
    v->push_back(std::string("cat /proc/meminfo "));
    v->push_back(std::string("lspci "));
    v->push_back(std::string("ifconfig "));
    v->push_back(std::string("cat /etc/mtab "));
    v->push_back(std::string("cat /proc/version "));
        
        
    std::stringstream cmdblank("");
    cmdblank << "echo ------------------------------- >> " << filename.c_str();
    
    std::stringstream cmdintro("");
    cmdintro << "echo ---- SYSTEM INOFORMATION ---- > " << filename.c_str();
    system(cmdintro.str().c_str());
    
    std::vector<std::string>::iterator it = v->begin();
    for (it; it!=v->end(); it++)
    {   
        std::stringstream cmdhead("");
        cmdhead << "echo " << it->c_str() << " >> " << filename.c_str();
        system(cmdhead.str().c_str());
        system(cmdblank.str().c_str());

        std::stringstream cmd("");
        cmd << it->c_str() << " >> " << filename.c_str();
        system(cmd.str().c_str());
        system(cmdblank.str().c_str());
        system(cmdblank.str().c_str());
    }
    result_files->push_back(filename);
}

int SimpleBenchmarker::tcpTest()
{
    std::string filename = std::string("/tmp/Nuttcp_");
    char hostname[1024];
    gethostname(hostname, 1024);
    filename.append(&hostname[0]);
    filename.append(".txt");
    
    std::stringstream cmd("");
    cmd << "nuttcp -i1 -w8m " << this->mdsip.c_str() ;
    std::stringstream cmd2("");
    cmd2 << "nuttcp -i1 -w1m " << this->mdsip.c_str();

    std::vector<std::string> *v = new std::vector<std::string>();
    v->push_back(cmd.str());
    v->push_back(cmd2.str());
        
    std::stringstream cmdblank("");
    cmdblank << "echo ------------------------------- >> " << filename.c_str();
    
    
    std::stringstream cmdintro("");
    cmdintro << "echo ---- Networkbenchmark to MDS ---- > " << filename.c_str();
    system(cmdintro.str().c_str());
    
    std::vector<std::string>::iterator it = v->begin();
    for (it; it!=v->end(); it++)
    {   
        std::stringstream cmdhead("");
        cmdhead << "echo " << it->c_str() << " >> " << filename.c_str();
        system(cmdhead.str().c_str());
        system(cmdblank.str().c_str());

        std::stringstream cmd("");
        cmd << it->c_str() << " >> " << filename.c_str();
        system(cmd.str().c_str());
        system(cmdblank.str().c_str());
        system(cmdblank.str().c_str());
    }
    result_files->push_back(filename);
}

int SimpleBenchmarker::fullbench()
{
    std::string dddir = std::string("/tmp/devicetest1");
    int iterations = this->iterations;
    bool sync = true;
    int threads=this->threads;
    log->debug_log("Do fullbench");
    systeminfo();
    int rc = parity_calc();
    rc = eval_pingpong();
    rc = eval_lock_roundtrip();
    rc = tcpTest();
    for (int i=0; i<threads; i++)
    {
        rc = device_diskio(i,this->size,iterations,dddir, sync);
    }
    //rc = device_diskio(1,64, 1, dddir, sync);
    rc = eval_StripeUnit();
    rc = eval_Stripe_lockmode();
    this->threads=1;
    rc = eval_Stripe();
    rc = eval_ReadSU();
    log->debug_log("Done.");
}


void SimpleBenchmarker::boot_system(std::string mdsip)
{    
    std::string ss("");
    log->debug_log("booting mds:%s.",mdsip.c_str());
    std::string target = std::string(MDS_CONFIG_FILE);
    std::string src = std::string("../conf/mds.conf");
    std::string target2 = std::string(DS_CONFIG_FILE);
    std::string src2 = std::string("../conf/main_ds.conf");
    
    log->debug_log("user:%s,pw:%s\n",this->mdsuser.c_str(),this->mdspw.c_str());
    
    std::string mkdircmd = std::string("mkdir /etc/r2d2");    
    p_ssh->send_cmd(mdsip, this->mdsuser,this->mdspw,mkdircmd.c_str(),ss);
    log->debug_log("mkdir done");
    p_ssh->scp_write(mdsip,this->mdsuser,this->mdspw, src, target);
    log->debug_log("copied:%s",src.c_str());
    p_ssh->scp_write(mdsip,this->mdsuser,this->mdspw, src2, target2);
    log->debug_log("copied:%s",src2.c_str());

    char *cmd = (char *) malloc(512);
    sprintf(cmd, "cd %s && python mdsstart.py %s ",this->mdsbin.c_str(), this->mdspw.c_str());
    log->debug_log("cmd:%s",cmd);
    p_ssh->send_cmd(mdsip, this->mdsuser,this->mdspw,cmd,ss);
    log->debug_log("done");
}

int main(int argc, char **argv)
{
    int rc;
    
    std::string abspath("../conf/simplebench.conf");
    ConfigurationManager *cm = new ConfigurationManager(argc,argv,abspath);
    cm->register_option("log.loc","logfile location");
    cm->register_option("benchmark","Benchmark to execute [s,su,slk,dio,cpu,tcp,pp,lrt,rsu] stripe,stripeunit,stripelock,diskio,cpu parity,tcp,pingpong,lockroundtrip,readsu");
    cm->register_option("iterations","Number of iterations");
    cm->register_option("threads", "Number of threads");
    cm->register_option("bytes", "Measure disc device speed, write Kibytes per block, in MB for parity calc");
    cm->register_option("dir", "Target dir for disk benchmark");
    cm->register_option("sync", "Force sync. 0/1 [default:1]");
    cm->register_option("ssh.user","User login at the server to storage the results at.");
    cm->register_option("ssh.pw","Password for the result server user login");
    cm->register_option("ssh.target", "Directory at the result server");
    cm->register_option("ssh.server", "Address of the server to store the results at.");
    cm->register_option("mds.ip", "Address of the metadata server");
    cm->register_option("mds.user", "username of the metadata server");
    cm->register_option("mds.pw", "Password of the mds user");
    cm->register_option("mds.bin", "Path of the mds binary");
    cm->register_option("setup", "setup mds");
    
    
    cm->parse();

    int iterations = atoi(cm->get_value("iterations").c_str());
    int threads    = atoi(cm->get_value("threads").c_str());
    SimpleBenchmarker *sb = new SimpleBenchmarker(iterations,threads,cm->get_value("log.loc"));
    sb->sshtarget = cm->get_value("ssh.target");
    sb->sshuser = cm->get_value("ssh.user");
    sb->sshserver = cm->get_value("ssh.server");
    sb->sshpw = cm->get_value("ssh.pw");
    sb->mdspw=cm->get_value("mds.pw");
    sb->mdsuser=cm->get_value("mds.user");
    sb->mdsbin=cm->get_value("mds.bin");
    sb->mdsip=cm->get_value("mds.ip");
        
    if (!strcmp(cm->get_value("sync").c_str(),"0")) sb->sync=false;    
    sb->size = atol(cm->get_value("bytes").c_str())*1024;
    
    if (!strcmp(cm->get_value("setup").c_str(),"1"))
    {
        sb->boot_system(cm->get_value("mds.ip"));
        sleep(20);
        printf("System up\n\n");
    }
    if (!strcmp(cm->get_value("benchmark").c_str(),"su"))
    {
        rc = sb->eval_StripeUnit();
    }
    else if (!strcmp(cm->get_value("benchmark").c_str(),"s"))
    {
        rc = sb->eval_Stripe();
    }
    else if (!strcmp(cm->get_value("benchmark").c_str(),"slk"))
    {
        rc = sb->eval_Stripe_lockmode();
    }
    else if (!strcmp(cm->get_value("benchmark").c_str(),"dio"))
    { 
        std::string dir = cm->get_value("dir");
        sb->device_diskio(threads,sb->size,iterations, dir, sb->sync);
    }
    else if (!strcmp(cm->get_value("benchmark").c_str(),"cpu"))
    {
        sb->parity_calc();
    }
    else if (!strcmp(cm->get_value("benchmark").c_str(),"tcp"))
    {
        sb->tcpTest();
    }
    else if (!strcmp(cm->get_value("benchmark").c_str(),"pp"))
    {
        sb->eval_pingpong();
    }
    else if (!strcmp(cm->get_value("benchmark").c_str(),"lrt"))
    {
        sb->eval_lock_roundtrip();
    }
    else if (!strcmp(cm->get_value("benchmark").c_str(),"rsu"))
    {
        sb->eval_ReadSU();
    }
    else if (!strcmp(cm->get_value("benchmark").c_str(),"full"))
    {
        rc = sb->fullbench();
    }           
    else
    {
        printf("Unknown benchmark:%s\n",cm->get_value("benchmark").c_str());
    }
    
    printf("Successful\n");
    sb->send_results();    
    printf("Results sent.\n");
}