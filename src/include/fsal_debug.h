/*
 * Sebastian Moors <mauser@smoors.de> 
 * 
 * Created on 23.08.2011
 *
 */


#include <zmq.h>

#include "fsal_zmq_mds_library.h"  

/* Some debugging helpers... */
void dump_einode(struct EInode *p_einode);
