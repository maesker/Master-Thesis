#include "EmbeddedInode.h"
#include "global_types.h"
#include "ReadDirReturn.h"
#include "logging/Logger.h"

void print_inode(Logger *p_log, mds_inode_t *inode);
void print_einode(Logger *p_log, struct EInode *p_einode);
void dump_einode(Logger *p_log, EInode *p_einode);

