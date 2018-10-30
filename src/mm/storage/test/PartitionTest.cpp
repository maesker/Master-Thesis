#include "gtest/gtest.h"
#include "mm/storage/PartitionManager.h"
#include "mm/storage/Partition.h"
#include "global_types.h"
#include "mm/storage/storage.h"
#include "mm/storage/DummyMountHelper.h"
#include "mm/einodeio/EmbeddedInodeLookUp.h"

#include <list>
#include <algorithm>

#define TEST_DATA   "1234567890"
#define TEST_OBJECT "my_object"
#define MY_HOSTNAME "myhostname"

class PartitionTest : public testing::Test
{
protected:
    Partition *p1, *p2;
    PartitionManager *pm;
    AbstractMountHelper *mh;

    PartitionTest()
    {
    }

    ~PartitionTest()
    {
    }

    virtual void SetUp()
    {
        char mount_dir[MAX_PATH_LEN] = "/tmp";
        std::list<std::string> devices = std::list<std::string>();

        mh = new DummyMountHelper();
        std::string device1 = "device1";
        std::string device2 = "device2";
        devices.push_back((char *)device1.data());
        devices.push_back((char *)device2.data());

        pm = new PartitionManager(&devices, mount_dir, mh, MY_HOSTNAME, 0, 1);
        p1 = pm->get_partition_by_identifier((char *) "device1");
        p2 = pm->get_partition_by_identifier((char *) "device2");
        p1->set_owner(MY_HOSTNAME);
        p2->set_owner(MY_HOSTNAME);
    }

    virtual void TearDown()
    {
        p1->reset();
        p2->reset();
        delete pm;
        delete mh;
    }
};

TEST_F(PartitionTest, TestMounting)
{
    char *data = TEST_DATA;
    p1->set_owner((char *) "xyz");
    ASSERT_THROW( p1->write_object(TEST_OBJECT, 0, sizeof(TEST_DATA), data), StorageException );
    ASSERT_THROW( p1->mount_rw(), StorageException );

    p1->set_owner((char *) "myhostname");
    ASSERT_NO_THROW( p1->mount_rw() );
    ASSERT_NO_THROW( p1->write_object(TEST_OBJECT, 0, sizeof(TEST_DATA), data) );
    ASSERT_NO_THROW( p1->remove_object(TEST_OBJECT) );
    ASSERT_NO_THROW( p1->reset() );
}

TEST_F(PartitionTest, TestListSubtreeObjects)
{
    EInode inode1;
    EInode inode2;
    char obj0[MAX_NAME_LEN];
    char obj1[MAX_NAME_LEN];
    std::list<std::string> subtreeObjects;

    memset(&inode1, 0, sizeof(EInode));
    memset(&inode2, 0, sizeof(EInode));
    strncpy(inode1.name, "dir1", MAX_NAME_LEN);
    inode1.inode.inode_number = 1;
    strncpy(inode2.name, "file1", MAX_NAME_LEN);
    inode2.inode.inode_number = 2;

    ASSERT_NO_THROW( p1->mount_rw() );

    snprintf(obj0, MAX_NAME_LEN, "%ld", 0);
    snprintf(obj1, MAX_NAME_LEN, "%ld", 1);
    ASSERT_NO_THROW( p1->write_object(obj0, 0, sizeof(EInode), &inode1) );
    ASSERT_NO_THROW( p1->write_object(obj1, 0, sizeof(EInode), &inode2) );

    ASSERT_NO_THROW( p1->list_subtree_objects(0, 0, &subtreeObjects) );
    ASSERT_TRUE( std::find(subtreeObjects.begin(),  subtreeObjects.end(), std::string(obj1)) != subtreeObjects.end() );

    ASSERT_NO_THROW( p1->remove_object(obj0) );
    ASSERT_NO_THROW( p1->remove_object(obj1) );
}

TEST_F(PartitionTest, TestMigrating)
{
    EInode inode1;
    EInode inode2;
    char obj0[MAX_NAME_LEN];
    char obj1[MAX_NAME_LEN];
    std::list<std::string> subtreeObjects;

    memset(&inode1, 0, sizeof(EInode));
    memset(&inode2, 0, sizeof(EInode));
    strncpy(inode1.name, "dir1", MAX_NAME_LEN);
    inode1.inode.inode_number = 1;
    strncpy(inode2.name, "file1", MAX_NAME_LEN);
    inode2.inode.inode_number = 2;

    ASSERT_NO_THROW( p1->mount_rw() );

    snprintf(obj0, MAX_NAME_LEN, "%ld", 0);
    snprintf(obj1, MAX_NAME_LEN, "%ld", 1);
    ASSERT_NO_THROW( p1->write_object(obj0, 0, sizeof(EInode), &inode1) );
    ASSERT_NO_THROW( p1->write_object(obj1, 0, sizeof(EInode), &inode2) );

    ASSERT_NO_THROW( p2->mount_rw() );
    ASSERT_NO_THROW( p2->start_migration(p1, 0) );

    ASSERT_NO_THROW( p2->list_subtree_objects(0, 0, &subtreeObjects) );
    ASSERT_TRUE( std::find(subtreeObjects.begin(),  subtreeObjects.end(), std::string(obj1)) != subtreeObjects.end() );

    ASSERT_NO_THROW( p2->remove_object(obj0) );
    ASSERT_NO_THROW( p2->remove_object(obj1) );
}

TEST_F(PartitionTest, TestTruncateSubtree)
{
	EInode inode1;
	EInode inode2;
	char obj0[MAX_NAME_LEN];
	char obj1[MAX_NAME_LEN];
	std::list<std::string> subtreeObjects;

	memset(&inode1, 0, sizeof(EInode));
	memset(&inode2, 0, sizeof(EInode));
	strncpy(inode1.name, "dir1", MAX_NAME_LEN);
	inode1.inode.inode_number = 1;
	strncpy(inode2.name, "file1", MAX_NAME_LEN);
	inode2.inode.inode_number = 2;

	ASSERT_NO_THROW( p1->mount_rw() );

	snprintf(obj0, MAX_NAME_LEN, "%ld", 0);
	snprintf(obj1, MAX_NAME_LEN, "%ld", 1);
	ASSERT_NO_THROW( p1->write_object(obj0, 0, sizeof(EInode), &inode1));
	ASSERT_NO_THROW( p1->write_object(obj1, 0, sizeof(EInode), &inode2));

	ASSERT_NO_THROW( p1->truncate_subtree(inode1.inode.inode_number) );
	ASSERT_FALSE( p1->has_object(obj0) );
	ASSERT_TRUE( p1->has_object(obj1) );
}

TEST_F(PartitionTest, TestEnlargeSubtree)
{
	EInode inode1;
	EInode inode2;
	char obj0[MAX_NAME_LEN];
	char obj1[MAX_NAME_LEN];
	std::list<std::string> subtreeObjects;

	memset(&inode1, 0, sizeof(EInode));
	memset(&inode2, 0, sizeof(EInode));
	strncpy(inode1.name, "dir1", MAX_NAME_LEN);
	inode1.inode.inode_number = 1;
	strncpy(inode2.name, "file1", MAX_NAME_LEN);
	inode2.inode.inode_number = 2;

	ASSERT_NO_THROW( p1->mount_rw() );
	ASSERT_NO_THROW( p2->mount_rw() );

	snprintf(obj0, MAX_NAME_LEN, "%ld", 0);
	snprintf(obj1, MAX_NAME_LEN, "%ld", 1);

	ASSERT_NO_THROW( p1->write_object(obj0, 0, sizeof(EInode), &inode1));
	ASSERT_NO_THROW( p1->set_root_inode(0) );
	ASSERT_NO_THROW( p2->write_object(obj1, 0, sizeof(EInode), &inode2));
	ASSERT_NO_THROW( p2->set_root_inode(2) );

	ASSERT_NO_THROW( p2->enlarge_subtree(p1, 0) );

	ASSERT_TRUE( p2->has_object(obj0) );
	ASSERT_TRUE( p2->has_object(obj1) );
	ASSERT_FALSE( p1->has_object(obj0) );
	ASSERT_FALSE( p1->has_object(obj0) );
}
