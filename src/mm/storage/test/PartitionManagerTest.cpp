#include "gtest/gtest.h"
#include "mm/storage/PartitionManager.h"
#include "mm/storage/Partition.h"
#include "global_types.h"
#include "mm/storage/storage.h"
#include "mm/storage/DummyMountHelper.h"

#include <list>
#include <sys/stat.h>
#include <sys/types.h>


#define MY_HOSTNAME "myhostname"

class PartitionManagerTest : public ::testing::Test
{
protected:
    PartitionManagerTest()
    {
    }

    ~PartitionManagerTest()
    {
    }

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

TEST(PartitionManagerTest, TestGetPartition)
{
    char mount_dir[MAX_PATH_LEN] = "/tmp";
    std::list<std::string> devices = std::list<std::string>();
    
    AbstractMountHelper mh = DummyMountHelper();
    std::string device1 = "device1";
    std::string device2 = "device2";
    std::string device3 = "device3";
    std::string device4 = "device4";

    devices.push_back(device1);
    devices.push_back(device2);
    devices.push_back(device3);
    devices.push_back(device4);

    PartitionManager pm = PartitionManager(&devices, mount_dir, &mh, MY_HOSTNAME, 0, 1);
    Partition *p, *p1, *p2, *p3, *p4;

    ASSERT_TRUE(pm.get_partition_by_identifier((char*) device1.data()));

    ASSERT_TRUE(p = pm.get_free_partition());
    p->mount_rw();
    p->set_root_inode(1);
    ASSERT_NO_THROW(p1 = pm.get_partition(1));
    ASSERT_TRUE(p == p1);

    ASSERT_TRUE(p = pm.get_free_partition());
    p->mount_rw();
    p->set_root_inode(2);
    ASSERT_NO_THROW(p2 = pm.get_partition(2));
    ASSERT_TRUE(p == p2);

    ASSERT_TRUE(p = pm.get_free_partition());
    p->mount_rw();
    p->set_root_inode(3);
    ASSERT_NO_THROW(p3 = pm.get_partition(3));
    ASSERT_TRUE(p == p3);

    ASSERT_TRUE(p = pm.get_free_partition());
    p->mount_rw();
    p->set_root_inode(4);
    ASSERT_NO_THROW(p4 = pm.get_partition(4));
    ASSERT_TRUE(p == p4);

    ASSERT_THROW(pm.get_free_partition(), StorageException);

    p1->reset();
    p2->reset();
    p3->reset();
    p4->reset();
}

TEST(PartitionManagerTest, TestOwnership)
{
    char mount_dir1[MAX_PATH_LEN] = "/tmp/1";
	char mount_dir2[MAX_PATH_LEN] = "/tmp/2";
	mkdir(mount_dir1, S_IRWXU);
	mkdir(mount_dir2, S_IRWXU);

    std::list<std::string> devices = std::list<std::string>();

	AbstractMountHelper mh = DummyMountHelper();
    std::string device1 = "device1";
    std::string device2 = "device2";
    std::string device3 = "device3";
    std::string device4 = "device4";

	devices.push_back(device1);
	devices.push_back(device2);
	devices.push_back(device3);
	devices.push_back(device4);

	PartitionManager *pm1 = new PartitionManager(&devices, mount_dir1, &mh, "PM1",
			0, 2);
	PartitionManager *pm2 = new PartitionManager(&devices, mount_dir2, &mh, "PM2",
			1, 2);

	// Check if no PM gets all partitions and each PM gets at least one partition
	ASSERT_FALSE(
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device1.data())->get_owner()) == 0 &&
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device2.data())->get_owner()) == 0 &&
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device3.data())->get_owner()) == 0 &&
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device4.data())->get_owner()) == 0);

	ASSERT_TRUE(
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device1.data())->get_owner()) == 0 ||
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device2.data())->get_owner()) == 0 ||
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device3.data())->get_owner()) == 0 ||
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device4.data())->get_owner()) == 0);

	ASSERT_FALSE(
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device1.data())->get_owner()) == 0 &&
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device2.data())->get_owner()) == 0 &&
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device3.data())->get_owner()) == 0 &&
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device4.data())->get_owner()) == 0);

	ASSERT_TRUE(
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device1.data())->get_owner()) == 0 ||
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device2.data())->get_owner()) == 0 ||
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device3.data())->get_owner()) == 0 ||
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device4.data())->get_owner()) == 0);

	// Check if no partition is owned by both PMs
	ASSERT_FALSE(
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device1.data())->get_owner()) == 0 &&
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device1.data())->get_owner()) == 0);

	ASSERT_FALSE(
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device2.data())->get_owner()) == 0 &&
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device2.data())->get_owner()) == 0);

	ASSERT_FALSE(
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device3.data())->get_owner()) == 0 &&
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device3.data())->get_owner()) == 0);

	ASSERT_FALSE(
			strcmp("PM1", pm1->get_partition_by_identifier((char *)device4.data())->get_owner()) == 0 &&
			strcmp("PM2", pm2->get_partition_by_identifier((char *)device4.data())->get_owner()) == 0);

	delete pm1;
	delete pm2;

	rmdir(mount_dir1);
	rmdir(mount_dir2);
}
