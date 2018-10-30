/* 
 * File:   SimpleBenchmarker.h
 * Author: markus
 *
 * Created on 4. Juni 2012, 17:59
 */

#ifndef SIMPLEBENCHMARKER_H
#define	SIMPLEBENCHMARKER_H

#include <time.h>
#include <stdlib.h>

#include "global_types.h"
#include "EmbeddedInode.h"
#include "tools/sys_tools.h"
#include "tools/parity.h"
#include "time.h"
#include "logging/Logger.h"
#include "components/raidlibs/Libraid4.h"
#include "components/network/sshwrapper.h"
#include "client/Client.h"
#include "components/configurationManager/ConfigurationManager.h"


enum benchtype
{
    stripe,
    stripeunit,
    stripeunitlocked,
    lockroundtrip,
    pingpongbench,
    readsubench,
    diskiobench,
};

struct resultdata 
{
    double opspersec;
    double latency_min;
    double latency_max;
    double latency_av;
};


struct thread_data {
    int                 id;
    std::string         dir;
    
    size_t              size;
    bool                sync;
    bool                lockmode;
    size_t              lockstart;
    size_t              lockend;
    size_t              start;
    size_t              end;
    void                *data;
    Client              *p_cl;
    int                 success;
    int                 iterations;
    struct EInode       *einode;
    struct timertimes   starttime;
    std::map<uint32_t, struct resultdata*>  *results;
};

class SimpleBenchmarker
{
public:
    std::string sshuser;
    std::string sshserver;
    std::string sshpw;
    std::string sshtarget;
    std::string mdsuser;
    std::string mdspw;
    std::string mdsbin;
    std::string mdsip;
    bool sync;
    size_t size;
    
    Logger *log;
    SimpleBenchmarker(int iter, int threads, std::string logloc);
    ~SimpleBenchmarker();    
    
    int fullbench();
    int eval_StripeUnit();
    int eval_Stripe();
    int eval_Stripe_lockmode();
    int eval_lock_roundtrip();
    int eval_ReadSU();
    int eval_pingpong();
    int parity_calc();
    int device_diskio(int threads, size_t bytes, int iterations, std::string dir,bool sync);
    int tcpTest();
    void write_result(const char *path, std::stringstream& ss);
    void send_results();
    void boot_system(std::string mdsip);
    
private:
    Client *cl;
    Libraid4 *raid;
    int iterations;
    int threads;
    sshwrapper *p_ssh;
    InodeNumber root;

    std::vector<std::string> *result_files;
    
    int setup();
    int createRandomFile(InodeNumber *inum);
    struct thread_data* newThreadDataSU(struct EInode *einode, StripeUnitId sid);
    struct thread_data* newThreadDataS(struct EInode *einode, StripeId sid);
    int perform(std::vector<struct thread_data*> *v, void* cb(void*));
    
    int analyse(std::vector<struct thread_data*> *vec, enum benchtype bench, int threads);
    int systeminfo();
};

#endif	/* SIMPLEBENCHMARKER_H */

