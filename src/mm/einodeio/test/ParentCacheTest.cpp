#include "gtest/gtest.h"
#include "mm/einodeio/ParentCache.h"
#include "mm/einodeio/EInodeIOException.h"
#include "global_types.h"

namespace
{

class ParentCacheTest : public ::testing::Test
{
protected:
    ParentCacheTest()
    {
    }

    ~ParentCacheTest()
    {
    }

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

TEST(ParentCacheTest, TestCache)
{
    InodeNumber inode1 = 1;
    InodeNumber inode2 = 2;
    InodeNumber inode3 = 3;
    ParentCache *cache = new ParentCache(10);

    cache->set_parent(inode1, inode2, 0);
    EXPECT_TRUE(cache->get_parent(inode1).parent == inode2);
    EXPECT_THROW(
    {
        InodeNumber inode = cache->get_parent(inode3).parent;
    }, ParentCacheException);

    cache->set_parent(inode1, inode3, 0);
    EXPECT_TRUE(cache->get_parent(inode1).parent == inode3);
    delete cache;
}

} // namespace
