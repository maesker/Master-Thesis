/*
 * EmbeddedInodeLookUp.h
 *
 *  Created on: 29.04.2011
 *      Author: sergerit
 */

#ifndef EMBEDDEDINODELOOKUP_H_
#define EMBEDDEDINODELOOKUP_H_

class EmbeddedInodeLookUp;

#include <stdlib.h>
#include <algorithm>
#include <string>
#include <list>

#include "EmbeddedInode.h"
#include "mm/einodeio/ParentCache.h"
#include "ReadDirReturn.h"
#include "mm/storage/storage.h"
#include "global_types.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

using namespace std;

/**
 * Presents a uniform view to a set of in- and output methods for inode.
 */
class EmbeddedInodeLookUp
{
public:
    EmbeddedInodeLookUp (StorageAbstractionLayer *sal, InodeNumber root_inode_number);
    ~EmbeddedInodeLookUp();
    void write_inode(EInode *node, InodeNumber parent_inode_number);
    void write_inode(EInode *node, InodeNumber parent_inode_number, bool no_sync);
    void write_inode(EInode *node);
    void write_update_set(std::list<EInode*> *updates, InodeNumber parent);
    void delete_inode(InodeNumber inode_number);
    void delete_inode_set(InodeNumber parent, std::list<InodeNumber> *inodes);
    void get_inode(EInode *node, InodeNumber inode_number);
    void get_inode(EInode *node, InodeNumber parent_inode_number, char *identifier);
    void get_inode(EInode *node, InodeNumber parent_inode_number, InodeNumber inode);
    void move_inode(InodeNumber inode, InodeNumber old_parent, InodeNumber new_parent);
    void create_inode(InodeNumber parent, EInode *node);
    void create_inode_set(std::list<EInode*> *inodes, InodeNumber parent);
    InodeNumber get_parent(InodeNumber inode_number);
    ReadDirReturn read_dir(InodeNumber dir_id, off_t offset);
    std::string get_path(InodeNumber inode);
    void resolv_path(EInode *node, std::string path);
    int get_parent_hierarchy(InodeNumber inode_number, inode_number_list_t* parent_list);

private:
    int count_inodes(InodeNumber parent);
    void delete_inode(InodeNumber parent_inode_number, char *path_to_file, InodeNumber inode_number, bool no_sync);
    void create_inode(InodeNumber parent, EInode *node, bool no_sync);
    StorageAbstractionLayer *storage_abstraction_layer;
    ParentCache *parent_cache;
    int max_dir_entries;
    InodeNumber root_inode_number;
    Pc2fsProfiler *profiler;
};

#endif /* EMBEDDEDINODELOOKUP_H_ */
