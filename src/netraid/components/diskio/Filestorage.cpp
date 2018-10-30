
#include "components/diskio/Filestorage.h"

using namespace std;

static int getdir (string dir, vector<string> &files)
{
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) {
        cout << "Error(" << errno << ") opening " << dir << endl;
        return errno;
    }

    while ((dirp = readdir(dp)) != NULL) 
    {
        if (dirp->d_name[0]!='.')
        {
            if (strncmp(&dirp->d_name[0],"..",2)!=0)
            {
                files.push_back(string(dirp->d_name));
            }
        }
    }
    closedir(dp);
    return 0;
}



Filestorage::Filestorage(Logger *p_log, const char *p_base, serverid_t myid, bool dosync)
{
    log = p_log;
    ostringstream relpath;
    relpath << p_base;
    xsystools_fs_mkdir(relpath.str());
    relpath << "/serverid_" << myid;
    basedir = std::string(relpath.str());    
    xsystools_fs_mkdir(basedir);
    
    id = myid;
    p_raid = new Libraid4(log);
    no_sync=!dosync;
    int on= no_sync ? 0 : 1;
    log->debug_log("Fsync activated:%d",on);
    log->debug_log("created:%s",basedir.c_str());
    size_metadata = sizeof(struct dataobject_metadata);
}

Filestorage::Filestorage(const Filestorage& orig)
{
}

Filestorage::~Filestorage()
{    
    delete p_raid;
}

void Filestorage::create_checksum(struct data_object *p_do)
{
    calc_parity(&p_do->metadata, sizeof(p_do->metadata), (void*) &p_do->checksum);
}


int Filestorage::create_block_object_recv(struct OPHead *p_head,struct dataobject_collection  *p_dcol, struct data_object *p_do)
{
    p_do->data                          = p_dcol->recv_data;
    return _create_block_object(p_head,p_dcol, p_do);
}

int Filestorage::create_block_object_prty(struct OPHead *p_head,struct dataobject_collection  *p_dcol, struct data_object *p_do)
{
    p_do->data                          = p_dcol->parity_data;
    return _create_block_object(p_head,p_dcol, p_do);
}

int Filestorage::_create_block_object(struct OPHead *p_head,struct dataobject_collection  *p_dcol, struct data_object *p_do)
{
    int rc=0;
    p_do->metadata.ccoid                = p_head->cco_id;
    p_do->metadata.offset               = p_head->offset;
    p_do->metadata.operationlength      = p_head->length;
    memcpy(&p_do->metadata.filelayout[0], &p_head->filelayout[0], sizeof(p_do->metadata.filelayout));
    memcpy(&p_do->metadata.versionvector[0], &p_dcol->versionvector[0], sizeof(p_do->metadata.versionvector));
    p_do->metadata.datalength           = p_dcol->recv_length;    
    create_checksum(p_do);
    return rc;
}

int Filestorage::write_file(struct OPHead *p_head,std::map<StripeId,struct dataobject_collection*>*datamap)
{
    int rc=-1;
    ostringstream relpath;
    relpath <<  basedir.c_str() << "/" << p_head->inum;

    log->debug_log("Write file...%llu, relpath:%s",p_head->inum,relpath.str().c_str());
    xsystools_fs_mkdir(relpath.str());
    
    std::map<StripeId,struct dataobject_collection*>::iterator it = datamap->begin();
    for (it; it!= datamap->end(); it++)
    {
        ostringstream s;
        s << relpath.str().c_str();
        s << "/" << it->first;
        log->debug_log("path:%s",s.str().c_str());
        if (!xsystools_fs_mkdir(s.str()))
        {
            log->debug_log("Error occured while trying to create stripe dir.");
            return -1;
        }
        struct data_object p_do;
        create_block_object_recv(p_head, it->second, &p_do);
        log->debug_log("data block:%u",p_do.metadata.ccoid);

        filelayout_raid *p_fl = (filelayout_raid*) &p_do.metadata.filelayout;
        serverid_t suid = p_raid->get_my_stripeunitid(p_fl,p_head->offset, id);
        uint32_t *p_myversion = &p_do.metadata.versionvector[suid];
        s << "/" << *p_myversion;
        //int fh;
        rc = write_object(&p_do, s.str().c_str(),&it->second->filehandle);
        if (rc==0)
        {
            it->second->fhvalid=true;
            log->debug_log("Filehandle valid.");
        }
        //if(!no_sync)
        //{
            //fsync(it->second->filehandle);
        //}
        //close(fh);**/
    }    
    log->debug_log("rc:%d",rc);
    return rc;
}

int Filestorage::write_file(struct OPHead *p_head, struct datacollection_primco * p_dcol)
{
    int rc=0;
    //log->debug_log("start\nvec0:%u,vec1:%u,vec2:%u",p_dcol->newobject->metadata.versionvector[0],p_dcol->newobject->metadata.versionvector[1],p_dcol->newobject->metadata.versionvector[2]);
                    
    filelayout_raid *p_fl = (filelayout_raid*) &p_head->filelayout[0];
    StripeId sid = p_raid->getStripeId(p_fl, p_head->offset);
    std::string stripe_path = getPath(p_head->inum);
    xsystools_fs_mkdir(stripe_path);
    stripe_path = getPath(p_head->inum,sid);
    log->debug_log("Write file...%llu, path:%s",p_head->inum,stripe_path.c_str());
    if (!xsystools_fs_mkdir(stripe_path))
    {
        log->debug_log("Error occured while trying to create stripe dir.");
        return -1;
    }
    stripe_path = getPath(p_head->inum,sid,p_dcol->newobject->metadata.versionvector[default_groupsize]);
    rc = write_object(p_dcol->newobject, stripe_path.c_str(), &p_dcol->filehandle);
    if (rc==0)
    {
        p_dcol->fhvalid=true;
        log->debug_log("file handle true:%u",p_dcol->newobject->metadata.versionvector[default_groupsize]);
    }
    log->debug_log("data block:%u, rc:%d",p_dcol->newobject->metadata.ccoid.csid,rc);
    return rc;
}

int Filestorage::write_file(struct operation_participant *p_part)
{
    int rc=0;
    log->debug_log("start");
    filelayout_raid *p_fl = ( filelayout_raid*) &p_part->ophead.filelayout[0];
    StripeId sid = p_raid->getStripeId(p_fl, p_part->ophead.offset);
    std::string stripe_path = getPath( p_part->ophead.inum);
    xsystools_fs_mkdir(stripe_path);
    stripe_path = getPath( p_part->ophead.inum,sid);
    log->debug_log("Write file...%llu, path:%s", p_part->ophead.inum,stripe_path.c_str());
    if (!xsystools_fs_mkdir(stripe_path))
    {
        log->debug_log("Error occured while trying to create stripe dir.");
        return -1;
    }
    std::map<StripeId,struct dataobject_collection*>::iterator it = p_part->datamap->begin();
    for (it; it!=p_part->datamap->end(); it++)
    {
        log->debug_log("current version:%u",it->second->mycurrentversion);
        //log->debug_log("vec0:%u,vec1:%u,vec2:%u",it->second->versionvector[0],it->second->versionvector[1],it->second->versionvector[2]);
        stripe_path = getPath( p_part->ophead.inum,sid, it->second->mycurrentversion+1);
                
        struct data_object *newobj = new struct data_object;
        newobj->data = it->second->recv_data;
        newobj->metadata.ccoid=p_part->ophead.cco_id;
        newobj->metadata.datalength = it->second->recv_length;
        memcpy(&newobj->metadata.filelayout[0],&p_part->ophead.filelayout[0], sizeof(newobj->metadata.filelayout));
        newobj->metadata.offset = p_part->ophead.offset;
        newobj->metadata.operationlength = p_part->ophead.length;
        memcpy(&newobj->metadata.versionvector[0],&it->second->versionvector[0],sizeof(newobj->metadata.versionvector));    

        rc = write_object(newobj, stripe_path.c_str(), &it->second->filehandle);
        log->debug_log("write object returned:%d",rc);
        if (rc==0)
        {
            it->second->fhvalid=true;
            log->debug_log("Filehandle valid");
        }
        /*if(!no_sync)
        {
            fsync(fh);
            log->debug_log("flushed");
        }
        close(fh);
        log->debug_log("closed...");**/
        it->second->newblock= newobj;
        log->debug_log("data block:%u, rc:%d",newobj->metadata.ccoid.csid,rc);
    }
    return rc;
}

int Filestorage::write_object(struct data_object *p_do, const char *file, int *fh)
{
    int rc=0;    
    log->debug_log("Writing file:%s",file);
    log->debug_log("metadata:%u,length:%u",size_metadata,p_do->metadata.datalength);
    create_checksum(p_do);    
    
    //std::stringstream ss;
    //print_dataobject(p_do, ss);
    //log->debug_log("Writing:%s ",ss.str().c_str());
    //int fh = open(file, O_WRONLY);
    size_t total = size_metadata+p_do->metadata.datalength+sizeof(p_do->checksum);
    void *buffer = malloc(total);

    if (buffer!=NULL)
    {
        log->debug_log("Totala size is:%u",total);
        void *tmp = buffer;
        memcpy(tmp,&p_do->metadata, size_metadata);
        log->debug_log("Totala size is:%u",total);
        tmp+=size_metadata;
        memcpy(tmp,p_do->data,  p_do->metadata.datalength);
        log->debug_log("Totala size is:%u",total);
        tmp+=p_do->metadata.datalength;
        memcpy(tmp,&p_do->checksum, sizeof(p_do->checksum));
        log->debug_log("Total size is:%u",total);
        *fh = open(file,O_WRONLY | O_CREAT,  S_IRUSR | S_IWUSR );
        if(*fh == NULL)
        {
            log->debug_log("could not create file handle...");
        }
        else
        {
            size_t count = write(*fh, buffer, total);
            log->debug_log("metadata written:%d",count);
            if (count!=total)
            {
                log->error_log("error writing file. count=%u",count);
                rc=-1;
            }
            else
            {
                rc=0;
            }            
        }  
        free(buffer);
    }
    else
    {
        log->debug_log("could not allocate buffer");
    }
    //if(!no_sync)
    //{
    //    fsync(*fh);
    //}
    //close(*fh);
    log->debug_log("rc:%d",rc);
    return rc;
}

int Filestorage::read_object(struct OPHead *p_head, std::map<StripeId,struct dataobject_collection*> *p_map)
{
    int rc=-1;    
    uint32_t version;
    std::string stripe_path;
    std::map<StripeId,struct dataobject_collection*>::iterator it_map;
    filelayout_raid *p_fl = ( filelayout_raid*) &p_head->filelayout[0];
    struct FileStripeList *p_fsl = p_raid->get_stripes(p_fl, p_head->offset, p_head->length);
    std::map<StripeId,struct Stripe_layout*>::iterator it = p_fsl->stripemap->begin();
    for (it; it!= p_fsl->stripemap->end(); it++)
    {
        stripe_path = getPath(p_head->inum, it->first);
        rc = get_max_version(stripe_path, &version);
        log->debug_log("open dir %s: version:%u", stripe_path.c_str(), version);
        if (version==0)
        {
            it_map = p_map->find(it->first);
            it_map->second->existing=NULL;
            log->debug_log("No object found.");
            it_map->second->mycurrentversion=0;
        }
        else
        {
            //it_map = p_map->find();
            serverid_t suid = p_raid->get_my_stripeunitid(p_fl,p_head->offset, id);
            uint32_t *p_myversion = &it_map->second->versionvector[suid];
            it_map->second->mycurrentversion=*p_myversion;        
        }        
        log->debug_log("current version %u.",it_map->second->mycurrentversion);
    }    
    free_FileStripeList(p_fsl);
    return rc;
}

int Filestorage::get_max_version(std::string& dir, uint32_t *version)
{
    vector<string> files = vector<string>();
    int rc = getdir(dir,files);
    *version=0;
    uint32_t tmp=0;
    log->debug_log("dir:%s",dir.c_str());
    vector<string>::iterator it = files.begin();
    for (it; it!=files.end(); it++)
    {
        log->debug_log("files:%s",it->c_str());
        try
        {
            tmp = atoi(it->c_str());
            if (tmp>*version)
            {
                *version=tmp;
            }
        }
        catch (...)
        {
        }
    }
    return rc;
}


int Filestorage::get_min_version(std::string& dir, uint32_t *version)
{
    vector<string> files = vector<string>();
    int rc = getdir(dir,files);
    *version=UINT_MAX;
    uint32_t tmp;
    log->debug_log("dir:%s",dir.c_str());
    vector<string>::iterator it = files.begin();
    for (it; it!=files.end(); it++)
    {
        log->debug_log("files:%s",it->c_str());
        try
        {
            tmp = atoi(it->c_str());
            if (tmp<*version)
            {
                *version=tmp;
            }
        }
        catch (...)
        {
        }
    }
    return rc;
}

int Filestorage::read_stripe_object(InodeNumber inum, StripeId sid, struct data_object **p_out)
{
    int rc=-1;
    uint32_t version;
    std::string path = getPath(inum, sid);
    get_max_version(path, &version);
    log->debug_log("inum:%llu,stripeid:%u, version:%u",inum,sid,version);
    if (version>0)
    {
        int fh;
        path = getPath(inum,sid,version);
        log->debug_log("path:%s.",path.c_str());
        try
        {
            if((fh = open(path.c_str(), O_RDONLY)) != -1)
            {
                size_t count;
                *p_out = new data_object;
                count = pread(fh, &(*p_out)->metadata, sizeof((*p_out)->metadata), 0);
                (*p_out)->data = malloc((*p_out)->metadata.datalength);
                count = pread(fh, (*p_out)->data, (*p_out)->metadata.datalength,sizeof((*p_out)->metadata));
                count = pread(fh, &(*p_out)->checksum, sizeof((*p_out)->checksum),(*p_out)->metadata.datalength+sizeof((*p_out)->metadata));
                close(fh);
                //log->debug_log("Object read:%s",(char*)(*p_out)->data);
                rc=0;
            }
            else
            {
                log->debug_log("Error opening file");
                rc=-2;
            }
        }
        catch(...)
        {
            rc=-3;
        }
    }
    else
    {
        log->debug_log("No such file found.");
    }
    return rc;
}

std::string Filestorage::getPath( InodeNumber inum)
{
    ostringstream relpath;
    relpath  <<  basedir.c_str() << "/" << inum;
    return relpath.str();
}

std::string Filestorage::getPath( InodeNumber inum, StripeId sid)
{
    ostringstream relpath;
    relpath  <<  basedir.c_str() << "/" << inum << "/" << sid;
    return relpath.str();
}

std::string Filestorage::getPath( InodeNumber inum, StripeId sid, uint32_t version)
{
    ostringstream relpath;
    relpath  <<  basedir.c_str() << "/" << inum << "/" << sid << "/" << version;
    return relpath.str();
}

int Filestorage::remove_lower_than(InodeNumber inum, StripeId sid, uint32_t version)
{
    string stripe_path = getPath(inum, sid);
    vector<string> files = vector<string>();
    int rc = getdir(stripe_path,files);
    uint32_t tmp=0;
    vector<string>::iterator it = files.begin();
    for (it; it!=files.end(); it++)
    {
        tmp = atoi(it->c_str());
        if (tmp<version)
        {
            remove_File(stripe_path, it->c_str());
        }
    }    
    return rc;
}

int Filestorage::remove_File(std::string path, const char  *filename)
{
    int rc=0;
    std::string fullpath = std::string(path);
    fullpath.append("/");
    fullpath.append(filename);
    log->debug_log("Removing:%s",fullpath.c_str());
    if( remove( fullpath.c_str() ) != 0 )
    {
        rc=-1;
    }
    return rc;    
}