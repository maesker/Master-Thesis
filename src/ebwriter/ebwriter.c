/**
 * @file ebwriter.c
 * @author Markus Mäsker, maesker@gmx.net
 * @brief Reads the MLT and creates an Export configuration for the Ganesha 
 * server. 
 * 
 * Once the file is written, it sends a SIGHUP to the Ganesha daemon to
 * signal configuration reload.
 * */
#include "ebwriter.h"

/**
 * @brief checks whether the given source string does exceed the valid
 * path length and writes it to the given path pointer.
 * @param[out] p_path pointer to the paths destination
 * @param[in] p_source source of the paths string
 * @returns zero if ok, 1 if given path is longer than MAX_PATH_LEN
 * */
static char* set_path(char *p_source)
{
    int pathlen=MAX_PATH_LEN;
    if (strlen(p_source)<=MAX_PATH_LEN)
    {
        pathlen =strlen(p_source)+1;
    }
    else
    {
        return NULL;
    }
    char *p_path = (char *) malloc(pathlen);
    memset(p_path,0,pathlen);
    memcpy(p_path, p_source,pathlen-1);
    return p_path;
}

/** 
 * @brief Allocate memory, write string represented by p_source to it and 
 * return pointer to allocated space.
 * @param[in] p_source input string
 * @return char pointer to new string
 * */
static char* set_char(char *p_source)
{
    int len =strlen(p_source)+1;
    char *p = (char *) malloc(len);
    memset(p,0,len);
    memcpy(p, p_source,len-1);
    return p;
}



static void get_file_from_path(char *path, char *file)
{
    char *start=NULL;
    char *oldstart=NULL;
    if (!strcmp(path,"/"))
    {
        memcpy(file,"/fs\0",5);
        return;
    }
    while (strlen(path)>0)
    {
        start = strpbrk(path,"/");
        if (start==oldstart)
        {
            break;
        }
        else if (start==NULL)
        {
            start=oldstart;
            break;
        }
        path=start;
        path++;
        oldstart=start;
    }
    memset(file,'\0',strlen(start)+1);
    memcpy(file,start,strlen(start));
        
}

/** 
 * @brief Ceck if pointer is allocated and free it
 * */
static void checked_free(void *p)
{
    if (p!=NULL)
    {
        free(p);
    }
}

/**
 * @brief recursively frees all memory allocated by the given export
 * blocks
 * @parem[in] p_eb pointer to the first export block to free
 * */
static void free_export_blocks(struct export_block *p_eb)
{
    struct export_block *p_n = NULL;
    struct export_block *p_tmp = NULL;
    p_n = p_eb;
    while(p_n != NULL)
    {
        p_tmp=(*p_n).next;
        checked_free((*p_n).path);
        checked_free((*p_n).pseudo);
        checked_free((*p_n).access);
        checked_free((*p_n).root_access);
        checked_free((*p_n).access_type);
        checked_free((*p_n).nfs_protocols);
        checked_free((*p_n).transport_protocols);
        checked_free((*p_n).sec_type);
        checked_free((*p_n).fs_specific);
        checked_free((*p_n).tag);
        checked_free((*p_n).referral);
        checked_free(p_n->cache_data);
        checked_free(p_n);
        p_n=p_tmp;
    }
}

/**
 * @brief writes a single option-value pair of type sting
 *  to the given file
 * @param[in] p_fp file pointer to write to
 * @param[in] p_optionname option string
 * @param[in] p_value value of the given option
 * @param[in] quotes write value with quotes
 * */
static int write_string_opt(FILE *p_fp, char *p_optionname, char *p_value, int quotes)
{
    int rc=0;
    int written;
    if ((p_optionname!=NULL ) && (p_value != NULL))
    {
        char *p_str=NULL;
        p_str=(char*)malloc(strlen(p_value)+strlen(p_optionname)+16);
        if (quotes)
        {
            written = sprintf(p_str,"%s = \"%s\";\n",p_optionname,p_value);
        }
        else
        {
            written = sprintf(p_str,"%s = %s;\n",p_optionname,p_value);
        }
        if (written<=0)
        {
            rc=1;
        }
        else
        {
            fputs(p_str,p_fp);
        }
        free(p_str);
    }
    return rc;
}


/**
 * @brief writes a single option-value pair of type float
 *  to the given file
 * @param[in] p_fp file pointer to write to
 * @param[in] p_optionname option string
 * @param[in] value value of the given option
 * */
static int write_float_opt(FILE *p_fp, char *p_optionname, double value)
{
    int rc=0;
    char *p_str=NULL;
    if ((p_optionname!=NULL ) )
    {
        p_str=(char*)malloc(strlen(p_optionname)+32);
        int written = snprintf(p_str,(size_t) strlen(p_optionname)+32,"%s = %f;\n",p_optionname,value);
        if (written <= 0)
        {
            rc=1;
        }
        else
        {
            fputs(p_str,p_fp);
        }
        free(p_str);
    }
    else
    {
        rc=2;
    }
    return rc;
}

/**
 * @brief writes a single option-value pair of type int to the given file
 * @param[in] p_fp file pointer to write to
 * @param[in] p_optionname option string
 * @param[in] value value of the given option
 * */
static int write_int_opt(FILE *p_fp, char *p_optionname, int value)
{
    int rc=0;
    int len = (sizeof(char)*32)+strlen(p_optionname);
    if (p_optionname!=NULL )
    {
        char *p_str = (char*)malloc((size_t) len);
        int written = snprintf(p_str,(size_t) len,"%s = %i;\n",p_optionname,value);
        if (written=0)
        {
            rc=1;
        }
        else
        {
            fputs(p_str,p_fp);
        }
        free(p_str);
    }
    else
    {
        rc=2;
    }
    return rc;
}

/**
 * @brief Writes a config file.
 * @param[in] p_eb export block pointer
 * @param[in] p_filename name of the config file
 * @return integer, zero means ok.
 * */
static int write_file(struct export_block *p_eb, const char *p_filename)
{
    int rc=2;
    FILE *p_fp;
    int exportid = 0;
    p_fp=fopen(p_filename,"w");
    if(p_fp==NULL)
    {
        fprintf(stderr,"Could not write target file.\n");
        rc=1;
    }
    else
    {
        while (p_eb!=NULL)
        {
          // skip referrals to the root object
          //if (!(p_eb->referral!=NULL  )) //&& !strcmp((*p_eb).path,"/")))
          //{
            exportid++;
            fputs("\nEXPORT\n{\n",p_fp); 	// open block
            rc=write_int_opt(p_fp,"Export_Id",exportid);
            

            rc = write_string_opt(p_fp,"Pseudo",p_eb->pseudo,1);
            if (rc!=0) break;
            
            rc=write_string_opt(p_fp,"Path","/",1);
            if (rc!=0) break;
            rc = write_string_opt(p_fp,"Root_Access",(*p_eb).root_access,1);
            if (rc!=0) break;
            rc = write_string_opt(p_fp,"Access_Type",(*p_eb).access_type,0);
            if (rc!=0) break;
            rc = write_string_opt(p_fp,"Access",(*p_eb).access,1);
            if (rc!=0) break;
            rc = write_int_opt(p_fp,"Anonymous_root_uid",(*p_eb).anonymous_root_uid);
            if (rc!=0) break;
            rc = write_int_opt(p_fp,"Nosuid",(int)(*p_eb).nosuid);
            if (rc!=0) return ;
            rc = write_int_opt(p_fp,"Nosgid",(int)(*p_eb).nosgid);
            if (rc!=0) break;
            rc = write_string_opt(p_fp,"NFS_Protocols",(*p_eb).nfs_protocols,1);
            if (rc!=0) break;
            rc = write_string_opt(p_fp,"Transport_Protocols",(*p_eb).transport_protocols,1);
            if (rc!=0) break;
            rc = write_string_opt(p_fp,"SecType",(*p_eb).sec_type,1);
            if (rc!=0) break;
            rc = write_int_opt(p_fp,"MaxOffsetRead",(*p_eb).maxoffsetread);
            if (rc!=0) break;
            rc = write_int_opt(p_fp,"MaxOffsetWrite",(*p_eb).maxoffsetwrite);
            if (rc!=0) break;
            rc = write_int_opt(p_fp,"MaxRead",(*p_eb).maxread);
            if (rc!=0) break;
            rc = write_int_opt(p_fp,"MaxWrite",(*p_eb).maxwrite);
            if (rc!=0) break;
            rc = write_int_opt(p_fp,"PrefRead",(*p_eb).prefread);
            if (rc!=0) break;
            rc = write_int_opt(p_fp,"PrefWrite",(*p_eb).prefwrite);
            if (rc!=0) break;
            rc = write_int_opt(p_fp,"PrefReaddir",(*p_eb).prefreaddir);
            if (rc!=0) break;
            rc = write_float_opt(p_fp,"Filesystem_id",(float)(*p_eb).filesystem_id);
            if (rc!=0) break;
            rc = write_int_opt(p_fp,"PrivilegedPort",(int)(*p_eb).privilegedport);
            if (rc!=0) break;
            rc = write_string_opt(p_fp,"Cache_Data",(*p_eb).cache_data,0);
            if (rc!=0) break;
            rc = write_int_opt(p_fp,"MaxCacheSize",(*p_eb).max_cache_size);
            if (rc!=0) break;
            rc = write_string_opt(p_fp,"FS_Specific",(*p_eb).fs_specific,1);
            if (rc!=0) break;
            
            rc = write_string_opt(p_fp,"Referral",p_eb->referral,1);
            if (rc!=0) break;
            
            //write_string_opt(p_fp,"Tag",(*p_eb).tag);*/
            fputs("\n}\n",p_fp); 			// close block.
          //}
          p_eb = (*p_eb).next;           
        }
        fclose(p_fp);
        rc=0;
    }
    return rc;
}

/**
 * @brief Acquire pid of the given process
 * @param p_processname name of the process
 * @return pid of the process.
 * */
pid_t get_pid(char *p_processname)
{
    pid_t pid = -1;
    if (p_processname != NULL)
    {
        char *p_cmd = (char *)malloc(64*sizeof(char));
        char *p_pidchar = (char *)malloc(64*sizeof(char));
        int written=snprintf(p_cmd,(size_t)64,"pidof %s\0",p_processname);
        if (written>0)
        {        
            FILE * pFile;
            pFile = popen(p_cmd,"r");
            int read = fread(p_pidchar, sizeof(char) ,64 , pFile);
            if (read>0)
            {
                pid = atoi(p_pidchar);
            }
            fclose(pFile);
        }        
        free(p_pidchar);
        free(p_cmd);
    }
    return pid;
}

int send_signal(const char *p_processname, short signal_number)
{
    int rc=0;
    pid_t pid = get_pid( (char *) p_processname);
    if (pid != -1)
    {
        if (signal_number == SIGHUP)
        {
            rc=kill(pid,SIGHUP);
        }
        else
        {
            //fprintf(stderr, "No method for signal %d available",signal_number) ;
            rc=1;
        }
    }
    else
    {
        //fprintf(stderr, "No pid for process %s found",p_processname) ;
        rc=2;
    }
    return rc;
}

/**
 * @brief extract data from mlt_entry and form a ner export block
 * structure
 * @param[in] p_entry mlt entry pointer
 * @param[out] p_eb pointer to export block to create
 * */
static int handle_entry(struct mlt_entry *p_entry,struct export_block *p_eb, 
        struct asyn_tcp_server *p_mds)
{
    int rc=0;
    if (p_entry==NULL)
    {
        rc=1;
    }
    else
    {
        size_t size = 2*1024;
        p_eb->pseudo = (char *) malloc(size);
        snprintf(p_eb->pseudo,size,"%s%s",EXPORT_DEF_PSEUDO,p_entry->path); 
        p_eb->next = NULL;
        p_eb->export_id = p_entry->export_id;
        p_eb->path = set_path(p_entry->path);
        p_eb->access = set_char(EXPORT_DEF_ACCESS);
        p_eb->root_access = set_char(EXPORT_DEF_ROOT_ACCESS);
        p_eb->access_type = set_char(EXPORT_DEF_ACCESS_TYPE);
        p_eb->anonymous_root_uid = EXPORT_DEF_ANON_ROOT_UID;
        p_eb->nosuid = EXPORT_DEF_NOSUID;
        p_eb->nosgid = EXPORT_DEF_NOSGID;
        p_eb->nfs_protocols = set_char(EXPORT_DEF_NFS_PROTOCOLS);
        p_eb->transport_protocols = set_char(EXPORT_DEF_TRANSPORT_PROTOCOL);
        p_eb->sec_type = set_char(EXPORT_DEF_SEC_TYPE);
        p_eb->maxoffsetread = EXPORT_DEF_MAXOFFSETREAD;
        p_eb->maxoffsetwrite = EXPORT_DEF_MAXOFFSETWRITE;
        p_eb->maxread = EXPORT_DEF_MAXREAD;
        p_eb->maxwrite = EXPORT_DEF_MAXWRITE;
        p_eb->prefread = EXPORT_DEF_PREFREAD;
        p_eb->prefwrite = EXPORT_DEF_PREFWRITE;
        p_eb->prefreaddir = EXPORT_DEF_PREFREADDIR; 
        p_eb->filesystem_id = EXPORT_DEF_FILESYSTEM_ID;
        p_eb->privilegedport = EXPORT_DEF_PRIVILEGEDPORT;
        p_eb->cache_data = set_char(EXPORT_DEF_CACHE_DATA);
        p_eb->max_cache_size = EXPORT_DEF_MAX_CACHE_SIZE;
        p_eb->fs_specific = (char *) malloc(32);
        sprintf(p_eb->fs_specific, "%llu\0", p_entry->root_inode);
        p_eb->tag = set_char("ganesha");
        
        if  (strcmp(p_entry->mds->address, p_mds->address))
        {
            p_eb->referral = (char *) malloc(size);
            snprintf(p_eb->referral,size,"%s:%s@%s",p_eb->pseudo,p_eb->pseudo,p_entry->mds->address); 
        }        
        else
        {
            p_eb->referral=NULL;
        } 
    }
    return rc;
}


int create_export_block_conf(struct mlt *p_mlt, const char *p_target, struct asyn_tcp_server *mds)
{
    int rc=0;
    struct export_block *p_list_entry = NULL;
    struct export_block *p_start = NULL;
    struct mlt_entry *p_mlt_entry;
    if (p_mlt != NULL)
    {
        p_mlt_entry = p_mlt->root;
        do
        {
            struct export_block *p_eb = malloc(sizeof(struct export_block));
            p_eb->next = NULL;
            int ret = handle_entry(p_mlt_entry, p_eb, mds);
            if (!ret)
            {
                if (p_list_entry==NULL)
                {
                    p_list_entry = p_eb;
                }
                else
                {
                    p_list_entry->next=p_eb;
                    p_list_entry=p_eb;
                }
                if (p_start==NULL)
                {
                    p_start=p_eb;
                }
                p_mlt_entry=get_successor(p_mlt_entry);
            }
        }
        while(p_mlt_entry!=NULL);
        rc = write_file(p_start, p_target);
    }
    else
    {
        rc=1;
    }
    free_export_blocks(p_start);
    return rc;
}
