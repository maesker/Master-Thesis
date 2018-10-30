#include  "mm/mds/test/dump_data.h"
#include <string>

void dump_einode(Logger *p_log, EInode *p_einode)
{
    p_log->debug_log("Einode.name:%s\n",p_einode->name);
    p_log->debug_log("Einode.ctime:%u\n",p_einode->inode.ctime);
    p_log->debug_log("Einode.mtime:%s\n",p_einode->inode.mtime);
    p_log->debug_log("Einode.atime:%s\n",p_einode->inode.atime);
    p_log->debug_log("Einode.uid:%u\n",p_einode->inode.uid);
    p_log->debug_log("Einode.gid:%u\n",p_einode->inode.gid);
    p_log->debug_log("Einode.mode:%u\n",p_einode->inode.mode);
    p_log->debug_log("Einode.size:%u\n",p_einode->inode.size);
    p_log->debug_log("Einode.has_acl:%u\n",p_einode->inode.has_acl);
    p_log->debug_log("Einode.link_count:%s\n",p_einode->inode.link_count);
    p_log->debug_log("Einode.inode_number:%u\n",p_einode->inode.inode_number);

}



/**
 * @brief helperfunction to print content of an inode.
 * */
void print_inode(Logger *p_log, mds_inode_t *inode)
{
    // prints the single values of an provided inode
    if (p_log != NULL)
    {
        p_log->debug_log( "inode inode_number:%u.", inode->inode_number );
        p_log->debug_log( "inode uid:%d.", inode->uid );
        p_log->debug_log( "inode gid:%d.", inode->gid );
        p_log->debug_log( "inode accessmode:%u.", inode->mode );
        p_log->debug_log( "inode acl:%d.", inode->has_acl );
        p_log->debug_log( "inode size:%d.", inode->size );
        p_log->debug_log( "inode link cnt:%d.", inode->link_count );
        p_log->debug_log( "inode file type:%s.", inode->file_type );
    }
} // end of MetadataServer::print_inode


/**
 * @brief helperfunction to print content of an einode
 * */
void print_einode(Logger *p_log, struct EInode *p_einode)
{
    // prints the data of an provided einode
    if (p_log != NULL)
    {
        p_log->debug_log( "Einode name:%s.",p_einode->name );
        print_inode( &(p_einode->inode) );
    }
} // end of MetadataServer::print_einode

