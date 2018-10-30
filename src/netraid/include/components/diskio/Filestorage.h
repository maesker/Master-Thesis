/* 
 * File:   Filestorage.h
 * Author: markus
 *
 * Created on 24. April 2012, 19:16
 */

#ifndef Filestorage_H
#define	Filestorage_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <vector>
#include <iostream>
#include <string>

#include "logging/Logger.h"
#include "components/OperationManager/OpData.h"

#include "tools/sys_tools.h"
#include "tools/parity.h"
#include "global_types.h"
#include "components/raidlibs/raid_data.h"
#include "components/raidlibs/Libraid4.h"

using namespace std;


class Filestorage
{
public:
    bool no_sync;
    Filestorage(Logger *p_log, const char *p_base, serverid_t myid,bool dosync);
    Filestorage(const Filestorage& orig);
    virtual ~Filestorage();
    
    int write_file(struct operation_participant *p_part);
    int write_file(struct OPHead *p_head, struct datacollection_primco* p_dcol);
    int write_file(struct OPHead *p_head,std::map<StripeId,struct dataobject_collection*>*datamap);
    int read_object(struct OPHead *p_head, std::map<StripeId,struct dataobject_collection*> *p_map);
    int read_stripe_object(InodeNumber inum, StripeId sid, struct data_object **p_out);
    int remove_lower_than(InodeNumber inum, StripeId sid, uint32_t version);
    
private:
    Logger *log;
    std::string basedir;
    serverid_t id;
    Libraid4 *p_raid;
    uint32_t size_metadata;
    

    int create_block_object_prty(struct OPHead *p_head,struct dataobject_collection  *p_dcol, struct data_object *p_do);
    int create_block_object_recv(struct OPHead *p_head,struct dataobject_collection  *p_dcol, struct data_object *p_do);
    int _create_block_object(struct OPHead *p_head,struct dataobject_collection  *p_dcol, struct data_object *p_do);
    
    int write_object(struct data_object *p_do, const char *file, int *fh);
    void create_checksum(struct data_object *p_do);
    
    int get_max_version(std::string& dir,uint32_t *version);
    int get_min_version(std::string& dir, uint32_t *version);
    std::string getPath( InodeNumber inum);
    std::string getPath( InodeNumber inum, StripeId sid);
    std::string getPath( InodeNumber inum, StripeId sid, uint32_t version);
    
    int remove_File(std::string path, const char *filename);
};


#endif	/* Filestorage_H */

