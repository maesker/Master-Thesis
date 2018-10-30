/*
 * Sebastian Moors <mauser@smoors.de> 
 * 
 * Created on 23.08.2011
 *
 */


#include "fsal_zmq_mds_library.h"  
#include "fsal_debug.h"
#include <stdio.h>



/* Some debugging helpers... */

void dump_einode(struct EInode *p_einode)
{
      printf("\nEinode.name:%s\n",p_einode->name);
      printf("Einode.ctime:%lld\n",(long long int)p_einode->inode.ctime);
      printf("Einode.mtime:%lld\n",(long long int)p_einode->inode.mtime);
      printf("Einode.atime:%lld\n",(long long int)p_einode->inode.atime);
      printf("Einode.uid:%u\n",p_einode->inode.uid);
      printf("Einode.gid:%u\n",p_einode->inode.gid);
      printf("Einode.mode:%u\n",p_einode->inode.mode);
      printf("Einode.size:%u\n",p_einode->inode.size);
 //     printf("Einode.has_acl:%u\n",p_einode->inode.has_acl);
      printf("Einode.link_count:%u\n",p_einode->inode.link_count);
      printf("Einode.inode_number:%lld\n\n",p_einode->inode.inode_number);
 }


void trace_incoming(zmq_msg_t * request)
{
	printf("Incoming!!! \n");
}
