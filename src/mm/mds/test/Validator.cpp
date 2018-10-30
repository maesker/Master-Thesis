
#ifndef VALIDATOR_CPP_
#define VALIDATOR_CPP_


/** 
 * @author {Markus Mäsker, maesker@gmx.net}
 * @date 15.08.2011
 * 
 * @brief A couple of helper functions to check einode structures
 * */

#include "mm/mds/test/Validator.h"

/**
 * @brief check
 * */
void validate_einode_mode_equal(EInode *a, EInode *b, int bitfield)
{
    if (IS_MODE_SET(bitfield))
    {   
        // attribute was changed, the values must differ
        ASSERT_NE(a->inode.mode, b->inode.mode);
    }
    else
    {
        // attribute unchanged, the values must be equal
        ASSERT_EQ(a->inode.mode, b->inode.mode);
    }
    if (IS_CTIME_SET(bitfield))
    {
        // attribute was changed, the values must differ
        ASSERT_NE(a->inode.ctime, b->inode.ctime);
    }
    else
    {
        // attribute unchanged, the values must be equal
        ASSERT_EQ(a->inode.ctime, b->inode.ctime);
    }
    if (IS_MTIME_SET(bitfield))
    {
        // attribute was changed, the values must differ
        ASSERT_NE(a->inode.mtime, b->inode.mtime);
    }
    else
    {
        // attribute unchanged, the values must be equal
        ASSERT_EQ(a->inode.mtime, b->inode.mtime);
    }
    if (IS_ATIME_SET(bitfield))
    {
        // attribute was changed, the values must differ
        ASSERT_NE(a->inode.atime, b->inode.atime);
    }
    else
    {
        // attribute unchanged, the values must be equal
        ASSERT_EQ(a->inode.atime, b->inode.atime);
    }
    if (IS_SIZE_SET(bitfield))
    {
        // attribute was changed, the values must differ
        ASSERT_NE(a->inode.size, b->inode.size);
    }
    else
    {
        // attribute unchanged, the values must be equal
        ASSERT_EQ(a->inode.size, b->inode.size);
    }
    if (IS_NLINK_SET(bitfield))
    {
        // attribute was changed, the values must differ
        ASSERT_NE(a->inode.link_count, b->inode.link_count);
    }
    else
    {
        // attribute unchanged, the values must be equal
        ASSERT_EQ(a->inode.link_count, b->inode.link_count);
    }
    if (IS_HAS_ACL_SET(bitfield))
    {
        // attribute was changed, the values must differ
        ASSERT_NE(a->inode.has_acl, b->inode.has_acl);
    }
    else
    {
        // attribute unchanged, the values must be equal
        ASSERT_EQ(a->inode.has_acl, b->inode.has_acl);
    }
    if (IS_UID_SET(bitfield))
    {
        // attribute was changed, the values must differ
        ASSERT_NE(a->inode.uid, b->inode.uid);
    }
    else
    {
        // attribute unchanged, the values must be equal
        ASSERT_EQ(a->inode.uid, b->inode.uid);
    }
    if (IS_GID_SET(bitfield))
    {
        // attribute was changed, the values must differ
        ASSERT_NE(a->inode.gid, b->inode.gid);
    }
    else
    {
        // attribute unchanged, the values must be equal
        ASSERT_EQ(a->inode.gid, b->inode.gid);
    }
}


#endif
