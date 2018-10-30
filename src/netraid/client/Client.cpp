#ifndef CLIENT_C
#define	CLIENT_C
/** 
 * @file   Client.cpp
 * @author Markus Maesker  maesker@gmx.net
 * 
 * @date 21. Dezember 2011, 01:55
 */

#include "client/Client.h"

/** 
 * @var config relative path to config file
 * */
#define config "/etc/r2d2/client.conf"


ConfigurationManager* init_cm(int argc, char **argv, std::string abspath)
{
    ConfigurationManager *cm = new ConfigurationManager(argc, argv,abspath);
    cm->register_option("logfile","path to logfile");
    cm->register_option("loglevel","Specify loglevel");
    cm->register_option("cmdoutput","Activate commandline output");
    cm->register_option("mds","Metadataserver address");
    cm->parse();
    return cm;
}


Client::Client() 
{
    std::string configfile = std::string(config);
    p_cm = init_cm(0,NULL,configfile);
    
    log = new Logger();
    std::string logfile = p_cm->get_value("logfile");
    //printf("logfile:%s\n",logfile.c_str());
    atoi(p_cm->get_value("cmdoutput").c_str())==1 ? log->set_console_output(true) : log->set_console_output(false);
    log->set_log_location(logfile);
    log->set_log_level(atoi(p_cm->get_value("loglevel").c_str()));    
    mds_address = p_cm->get_value("mds");
    log->debug_log("MDS address is %s.",mds_address.c_str());
    
    csid = 0;
    ds_count  = 0;
    p_pnfs_cl   = new Pnfsdummy_client(log);
    p_spn_cl    = new SPNetraid_client(log);    
    p_brlman =    new ByterangeLockManager(log);    
    
    
    serverid_t mdsid = 0;
    p_pnfs_cl->addMDS(mdsid,mds_address,DEF_MDS_PORT+0);
    log->debug_log("Done");
}       

Client::~Client() 
{
    delete p_spn_cl;
    delete p_pnfs_cl;    
    delete p_brlman; 
    delete log;
    delete p_cm;
}

int Client::acquireSID(serverid_t id)
{
    int rc=0;
    if (this->csid==0)
    {
         rc = p_pnfs_cl->handle_pnfs_send_createsession(id,&this->csid);
    }
    if (rc)
    {
        log->warning_log("error occured:rc=%d.",rc);
    }
    log->debug_log("Got SID:%u, rc=%d",this->csid, rc);
    return rc;
}

int Client::releaselock(serverid_t *id, InodeNumber *inum, uint64_t *start, uint64_t *end)
{
    log->debug_log("id:%u,inum:%llu,start:%llu,end:%llu",*id,*inum,*start,*end);
    int ret = -1;
    if (this->csid != 0)
    {
        ret = p_pnfs_cl->handle_pnfs_send_releaselock(id,&this->csid,inum,start,end);
        if (!ret)
        {
            log->debug_log("lock manager ...");
            ret = this->p_brlman->releaseLockObject(*inum,*start);
            if (ret)
            {
                log->warning_log("error while locking object:%d",ret);
            }
            else
            {
                log->debug_log("lock registered.");
            }
        }
        else
        {
            log->warning_log("error occured:rc=%d.",ret);
        }
    }
    log->debug_log("rc:%u",ret);
    return ret;
}


int Client::acquireByteRangeLock(serverid_t *id, InodeNumber *inum, uint64_t *start, uint64_t *end)
{
    log->debug_log("id:%u,inum:%llu,start:%llu,end:%llu",*id,*inum,*start,*end);
    int ret = -1;
    if (this->csid != 0)
    {
        time_t expires;
        bool loop=true;
        while (loop)
        {                   
            ret = p_pnfs_cl->handle_pnfs_send_byterangelock(id,&this->csid,inum,start,end,&expires);
            if (!ret)
            {
                log->debug_log("lock manager ...");
                ret = this->p_brlman->lockObject(inum,&this->csid,start,end,&expires);
                if (ret)
                {
                    log->warning_log("error while locking object:%d",ret);
                }
                else
                {
                    log->debug_log("lock registered.");
                }
                loop=false;
            }
            else
            {
                log->warning_log("error occured:rc=%d.",ret);
                usleep(100);
            }
        }
    }
    log->debug_log("rc:%u",ret);
    return ret;
}

int Client::getDataserverLayouts(serverid_t id)
{
    log->debug_log("get ds layout from mds:%u",id);
    
    int rc = p_pnfs_cl->handle_pnfs_send_dataserverlayout(id, &this->ds_count);
    if (!rc)
    {
        //log->debug_log("ok:dscount=%u.",this->ds_count)
        vector<serverid_t> vec;
        for (serverid_t i=0; i<this->ds_count; i++)
        {
            log->debug_log("serverid_t:%u",i);
            vec.push_back(i);
        }
        rc = this->getDataserverAddresses(&id, &vec);
        if (rc)
        {
            log->warning_log("get ds address failed:%d",rc);
        }
    }
    else
    {
        rc = -2;
    }
    log->debug_log("rc:%d",rc);
    return rc;
}

int Client::getDataserverAddresses(serverid_t *mds, vector<serverid_t> *dsids)
{
    log->debug_log("mds:%u",*mds);
    int rc;
    for (vector<serverid_t>::iterator it = dsids->begin(); it!= dsids->end(); it++)
    {
        log->debug_log("getAddress:%u.",*it);
        ipaddress_t address;
        rc = p_pnfs_cl->handle_pnfs_send_dataserver_address( mds, &(*it), &address);    
        if (!rc)
        {
            log->debug_log("getAddress:%s.",&address[0]);
            rc = p_spn_cl->register_ds_address(*it, &address[0]);
            if (rc)
            {
                log->warning_log("error occured:%d",rc);
            }
        }
        else
        {
            log->warning_log("Error occured:%d",rc);
        }
    }
    log->debug_log("rc:%u",rc);
    return rc;
}
        
int Client::handle_write(struct EInode *p_einode, size_t offset, size_t length, void *data)
{
    log->debug_log("handle write start: size:%u",length);
    int rc = p_spn_cl->handle_write(this->csid,p_einode,offset,length,data);
    log->debug_log("rc=%d",rc);
    return rc;
}

int Client::handle_write_lock(struct EInode *p_einode, size_t offset, size_t length, void *data)
{
    log->debug_log("handle write start: size:%u",length);
    int rc = p_spn_cl->handle_write_lock(this->csid,p_einode,offset,length,data);
    log->debug_log("rc=%d",rc);
    return rc;
}

int Client::handle_read(struct EInode *p_einode, size_t offset, size_t length, void **data)
{
    log->debug_log("handle read start");
    int rc = p_spn_cl->handle_read(this->csid,p_einode,offset,length,data);
    log->debug_log("rc=%d",rc);
    return rc;
}

int Client::pingpong()
{
    return p_spn_cl->handle_pingpong(this->csid);
}

void* Client::run()
{
    int rc=0;
    InodeNumber root=1;
    log->warning_log("start");    
    serverid_t mdsid = 0;
    rc = p_pnfs_cl->addMDS(mdsid,mds_address,DEF_MDS_PORT+0);
    if (rc) exit(1);
        
    size_t opsize = 10;
    void *data = malloc(opsize);
    snprintf((char*)data,opsize,"HelloHelloHelloHell\0");
    struct timertimes start = timer_start();
    double diff;
    
    log->debug_log("Acuired start");
    rc = this->acquireSID(mdsid);
    if (rc) exit(1);
    log->debug_log("Acuired");
    
    rc = getDataserverLayouts(mdsid);
    if (rc) exit(1);

    FsObjectName name;
    snprintf(&name[0],2,"a\0");
    InodeNumber newfile;
    struct EInode einode;
    rc = p_pnfs_cl->meta_create_file(&root,&name, &newfile);  
    if (rc) 
    {
        log->debug_log("Error occured, exit");
        exit(1);
    }
    rc = p_pnfs_cl->meta_get_file_inode(&root, &newfile , &einode);
    if (rc) 
    {
        log->debug_log("Error occured, exit");
        exit(1);
    }
    uint64_t startb, end;
    startb = 0;
    end = 600;
    rc = acquireByteRangeLock(&mdsid,&newfile,&startb,&end);
    if (rc) 
    {
        log->debug_log("Error occured, exit");
        exit(1);
    }
    log->debug_log("XXX");
    rc = handle_write(&einode,0,opsize,data);
    if (rc) 
    {
        log->debug_log("Error occured, exit");
        exit(1);
    }
    
    void *dataread = malloc(opsize);
    rc = handle_read(&einode,0,opsize,&dataread);
    log->debug_log("read:%s",(char*)dataread);
    diff = timer_end(start);
    
    
/*    filelayout f;
    filelayout_raid5 *fl = (filelayout_raid5*) &f;
    struct file_op_data fop;
    p_raid->create_initial_filelayout((InodeNumber)4,&f);
    p_raid->printFL(fl);
    struct FileStripeList *p_sl = p_raid->get_stripes(fl,startb,end);
    log->debug_log("-------------");
    p_raid->printFileStripeList(p_sl);
  */  
    log->debug_log("Done: rc:%d, time=%f.",rc,diff);
}



#endif