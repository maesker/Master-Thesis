#include "gtest/gtest.h"
#include "mm/einodeio/EmbeddedInodeLookUp.h"
#include "mm/einodeio/EInodeIOException.h"
#include "mm/storage/storage.h"
#include "global_types.h"

#include <string.h>
#include <time.h>

namespace
{

#define ROOT_INODE 1
#define MAX_DIR_SIZE 100

class EmbeddedInodeLookUpTest : public ::testing::Test
{
protected:
    EmbeddedInodeLookUpTest()
    {
    }

    ~EmbeddedInodeLookUpTest()
    {
    }

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

TEST(EmbeddedInodeLookUpTest, TestRW)
{
    StorageAbstractionLayer *storage_abstraction_layer;
    char *device_identifier;
    device_identifier = strdup("/tmp/");
    storage_abstraction_layer = new StorageAbstractionLayer(device_identifier);
    EmbeddedInodeLookUp *inode_lookup = new EmbeddedInodeLookUp(storage_abstraction_layer, ROOT_INODE);

    EInode einode1, einode2, einode3, einode4;
    ReadDirReturn ret;
    time_t ctime;

    memset(&einode1, 0, sizeof(EInode));
    memset(&einode2, 0, sizeof(EInode));
    memset(&einode3, 0, sizeof(EInode));
    memset(&einode4, 0, sizeof(EInode));

    strncpy(einode1.name, "test1", MAX_NAME_LEN);
    strncpy(einode3.name, "test1", MAX_NAME_LEN);

    einode1.inode.inode_number = 2;
    einode1.inode.uid = 123;
    time(&(einode1.inode.ctime));
    einode3.inode.inode_number = 999;

    ASSERT_NO_THROW( ret = inode_lookup->read_dir(ROOT_INODE, 0) );
    ASSERT_TRUE( ret.dir_size == 0 );
    ASSERT_NO_THROW( inode_lookup->write_inode(&einode1, ROOT_INODE) );
    ASSERT_NO_THROW( ret = inode_lookup->read_dir(ROOT_INODE, 0) );
    ASSERT_TRUE( ret.dir_size == 1 );
    ASSERT_NO_THROW( inode_lookup->get_inode(&einode2, 2) );
    ASSERT_NO_THROW( inode_lookup->get_inode(&einode4, ROOT_INODE, "test1") );
    ASSERT_TRUE( memcmp(&einode1, &einode4, sizeof(EInode)) == 0 );
    ASSERT_TRUE( memcmp(&einode1, &einode2, sizeof(EInode)) == 0 );
    ASSERT_TRUE( inode_lookup->get_parent(einode1.inode.inode_number) == ROOT_INODE );
    ASSERT_THROW( inode_lookup->write_inode(&einode3, ROOT_INODE), EInodeIOException );
    ASSERT_NO_THROW( inode_lookup->delete_inode(2) );

    delete inode_lookup;
    delete storage_abstraction_layer;
    free(device_identifier);
}

TEST(EmbeddedInodeLookUpTest, TestCreate)
{
    StorageAbstractionLayer *storage_abstraction_layer;
    char *device_identifier;
    device_identifier = strdup("/tmp/");
    storage_abstraction_layer = new StorageAbstractionLayer(device_identifier);
    EmbeddedInodeLookUp *inode_lookup = new EmbeddedInodeLookUp(storage_abstraction_layer, ROOT_INODE);

    EInode einode1, einode2, einode3, einode4;
    ReadDirReturn ret;
    time_t ctime;

    memset(&einode1, 0, sizeof(EInode));
    memset(&einode2, 0, sizeof(EInode));
    memset(&einode3, 0, sizeof(EInode));
    memset(&einode4, 0, sizeof(EInode));

    strncpy(einode1.name, "test1", MAX_NAME_LEN);
    strncpy(einode3.name, "test1", MAX_NAME_LEN);

    einode1.inode.inode_number = 2;
    einode1.inode.uid = 123;
    time(&(einode1.inode.ctime));
    einode3.inode.inode_number = 999;

    ASSERT_NO_THROW( ret = inode_lookup->read_dir(ROOT_INODE, 0) );
    ASSERT_TRUE( ret.dir_size == 0 );
    ASSERT_NO_THROW( inode_lookup->create_inode(ROOT_INODE, &einode1) );
    ASSERT_NO_THROW( ret = inode_lookup->read_dir(ROOT_INODE, 0) );
    ASSERT_TRUE( ret.dir_size == 1 );
    ASSERT_NO_THROW( inode_lookup->get_inode(&einode2, 2) );
    ASSERT_NO_THROW( inode_lookup->get_inode(&einode4, ROOT_INODE, "test1") );
    ASSERT_TRUE( memcmp(&einode1, &einode4, sizeof(EInode)) == 0 );
    ASSERT_TRUE( memcmp(&einode1, &einode2, sizeof(EInode)) == 0 );
    ASSERT_NO_THROW( inode_lookup->delete_inode(2) );

    delete inode_lookup;
    delete storage_abstraction_layer;
    free(device_identifier);
}

TEST(EmbeddedInodeLookUpTest, TestGetPath)
{
    StorageAbstractionLayer *storage_abstraction_layer;
    char *device_identifier;
    device_identifier = strdup("/tmp/");
    storage_abstraction_layer = new StorageAbstractionLayer(device_identifier);
    EmbeddedInodeLookUp *inode_lookup = new EmbeddedInodeLookUp(storage_abstraction_layer, FS_ROOT_INODE_NUMBER);

    EInode einode1, einode2, einode3, result;

    memset(&einode1, 0, sizeof(EInode));
    memset(&einode2, 0, sizeof(EInode));
    memset(&einode3, 0, sizeof(EInode));

    strncpy(einode1.name, "a", MAX_NAME_LEN);
    einode1.inode.inode_number = 2;
    strncpy(einode2.name, "b", MAX_NAME_LEN);
    einode2.inode.inode_number = 3;
    strncpy(einode3.name, "c", MAX_NAME_LEN);
    einode3.inode.inode_number = 4;

    ASSERT_NO_THROW( inode_lookup->write_inode(&einode1, FS_ROOT_INODE_NUMBER) );
    ASSERT_NO_THROW( inode_lookup->write_inode(&einode2, einode1.inode.inode_number) );
    ASSERT_NO_THROW( inode_lookup->write_inode(&einode3, einode2.inode.inode_number) );

    std::string path;
    ASSERT_NO_THROW( path = inode_lookup->get_path(4) );
    ASSERT_TRUE( path == std::string("/a/b/c") );
    ASSERT_THROW( path = inode_lookup->get_path(5), ParentCacheException );

    ASSERT_NO_THROW( inode_lookup->resolv_path(&result, std::string("/a/b/c")) );
    ASSERT_TRUE( memcmp(&result, &einode3, sizeof(EInode)) == 0 );

    ASSERT_NO_THROW( inode_lookup->delete_inode(2) );
    ASSERT_NO_THROW( inode_lookup->delete_inode(3) );
    ASSERT_NO_THROW( inode_lookup->delete_inode(4) );

    delete inode_lookup;
    delete storage_abstraction_layer;
    free(device_identifier);
}

TEST(EmbeddedInodeLookUpTest, TestMoveInode)
{
    StorageAbstractionLayer *storage_abstraction_layer;
    char *device_identifier;
    device_identifier = strdup("/tmp/");
    storage_abstraction_layer = new StorageAbstractionLayer(device_identifier);
    EmbeddedInodeLookUp *inode_lookup = new EmbeddedInodeLookUp(storage_abstraction_layer, FS_ROOT_INODE_NUMBER);

    EInode einode1, einode2, einode3, result;

    memset(&einode1, 0, sizeof(EInode));
    memset(&einode2, 0, sizeof(EInode));
    memset(&einode3, 0, sizeof(EInode));
    memset(&result, 0, sizeof(EInode));

    strncpy(einode1.name, "a", MAX_NAME_LEN);
    einode1.inode.inode_number = 2;
    strncpy(einode2.name, "b", MAX_NAME_LEN);
    einode2.inode.inode_number = 3;
    strncpy(einode3.name, "c", MAX_NAME_LEN);
    einode3.inode.inode_number = 4;

    ASSERT_NO_THROW( inode_lookup->write_inode(&einode1, FS_ROOT_INODE_NUMBER) );
    ASSERT_NO_THROW( inode_lookup->write_inode(&einode2, FS_ROOT_INODE_NUMBER) );
    ASSERT_NO_THROW( inode_lookup->write_inode(&einode3, einode1.inode.inode_number) );

    ASSERT_NO_THROW( inode_lookup->get_inode(&result, einode1.inode.inode_number, einode3.name) );
    ASSERT_NO_THROW( inode_lookup->move_inode(einode3.inode.inode_number,einode1.inode.inode_number, einode2.inode.inode_number) );
    ASSERT_NO_THROW( inode_lookup->get_inode(&result, einode2.inode.inode_number, einode3.name) );
    ASSERT_TRUE( memcmp(&result, &einode3, sizeof(EInode)) == 0 );
    ASSERT_THROW( inode_lookup->get_inode(&result, einode1.inode.inode_number, einode3.name), EInodeIOException );
    ASSERT_TRUE( inode_lookup->get_parent(einode3.inode.inode_number) == einode2.inode.inode_number );

    ASSERT_NO_THROW( inode_lookup->delete_inode(2) );
    ASSERT_NO_THROW( inode_lookup->delete_inode(3) );
    ASSERT_NO_THROW( inode_lookup->delete_inode(4) );

    delete inode_lookup;
    delete storage_abstraction_layer;
    free(device_identifier);
}

TEST(EmbeddedInodeLookUpTest, TestReadDir)
{
    StorageAbstractionLayer *storage_abstraction_layer;
    char *device_identifier;
    device_identifier = strdup("/tmp/");
    storage_abstraction_layer = new StorageAbstractionLayer(device_identifier);
    EmbeddedInodeLookUp *inode_lookup = new EmbeddedInodeLookUp(storage_abstraction_layer, FS_ROOT_INODE_NUMBER);
	int i;

	// Create inodes, read them, and check dir size
	for(i = 0; i < MAX_DIR_SIZE; i++)
	{
		EInode einode1, einode2;
		ReadDirReturn ret;
		int j;
		bool found = false;

		memset(&einode1, 0, sizeof(EInode));
		memset(&einode2, 0, sizeof(EInode));
		snprintf(einode1.name, MAX_NAME_LEN, "%ld", i + 2);
		einode1.inode.inode_number = i + 2;
		ASSERT_NO_THROW( inode_lookup->write_inode(&einode1, ROOT_INODE) );
		ASSERT_NO_THROW( ret = inode_lookup->read_dir(ROOT_INODE, 0));
		ASSERT_NO_THROW( inode_lookup->get_inode(&einode2, ROOT_INODE, i + 2) );
		ASSERT_TRUE( memcmp(&einode1, &einode2, sizeof(EInode)) == 0 );
		ASSERT_TRUE( ret.dir_size == i + 1 );

		// Try to find inode in read_dir return
		for(j = 0; j < ret.dir_size; j += FSAL_READDIR_EINODES_PER_MSG)
		{
			int k;
			ReadDirReturn my_ret;

			ASSERT_NO_THROW( my_ret = inode_lookup->read_dir(ROOT_INODE, j) );

			for(k = 0; k < my_ret.nodes_len; k++)
			{
				if(memcmp(&einode2, &(my_ret.nodes[k]), sizeof(EInode)) == 0)
				{
					found = true;
					break;
				}
			}
			if(found)
				break;
		}
		ASSERT_TRUE( found );
	}

	// Delete created inodes
	for(i = 0; i < MAX_DIR_SIZE; i++)
	{
		ASSERT_NO_THROW( inode_lookup->delete_inode(i + 2) );
	}

	delete inode_lookup;
	delete storage_abstraction_layer;
	free(device_identifier);
}

TEST(EmbeddedInodeLookUpTest, TestDelete)
{
    StorageAbstractionLayer *storage_abstraction_layer;
    char *device_identifier;
    device_identifier = strdup("/tmp/");
    storage_abstraction_layer = new StorageAbstractionLayer(device_identifier);
    EmbeddedInodeLookUp *inode_lookup = new EmbeddedInodeLookUp(storage_abstraction_layer, FS_ROOT_INODE_NUMBER);

	int i, j;

	for(j = 0; j < MAX_DIR_SIZE; j++)
	{
		// Create inodes
		for(i = 0; i < j; i++)
		{
			EInode einode1, einode2;
			ReadDirReturn ret;

			memset(&einode1, 0, sizeof(EInode));
			memset(&einode2, 0, sizeof(EInode));
			snprintf(einode1.name, MAX_NAME_LEN, "%ld", i + 2);
			einode1.inode.inode_number = i + 2;
			ASSERT_NO_THROW( inode_lookup->write_inode(&einode1, ROOT_INODE) );
		}

		// Delete created inodes and check dir size
		for(i = 0; i < j; i++)
		{
			ReadDirReturn my_ret;

			ASSERT_NO_THROW( inode_lookup->delete_inode(i + 2) );
			ASSERT_NO_THROW( my_ret = inode_lookup->read_dir(ROOT_INODE, 0) );
			ASSERT_TRUE( my_ret.dir_size == j - i - 1 );
		}
	}

	delete inode_lookup;
	delete storage_abstraction_layer;
	free(device_identifier);
}

TEST(EmbeddedInodeLookUpTest, TestDelete_2)
{
    StorageAbstractionLayer *storage_abstraction_layer;
    char *device_identifier;
    device_identifier = strdup("/tmp/");
    storage_abstraction_layer = new StorageAbstractionLayer(device_identifier);
    EmbeddedInodeLookUp *inode_lookup = new EmbeddedInodeLookUp(storage_abstraction_layer, FS_ROOT_INODE_NUMBER);

	int i, j;

	for(j = 0; j < MAX_DIR_SIZE; j++)
	{
		// Create inodes
		for(i = 0; i < j; i++)
		{
			EInode einode1, einode2;
			ReadDirReturn ret;

			memset(&einode1, 0, sizeof(EInode));
			memset(&einode2, 0, sizeof(EInode));
			snprintf(einode1.name, MAX_NAME_LEN, "%ld", i + 2);
			einode1.inode.inode_number = i + 2;
			ASSERT_NO_THROW( inode_lookup->write_inode(&einode1, ROOT_INODE) );
		}

		// Read created inodes and delete them
		for(i = 0; i < j; i++)
		{
			ReadDirReturn my_ret;
			EInode einode;
			EXPECT_NO_THROW( inode_lookup->get_inode(&einode, i + 2) );
			ASSERT_NO_THROW( inode_lookup->delete_inode(i + 2) );
		}
	}

	delete inode_lookup;
	delete storage_abstraction_layer;
	free(device_identifier);
}

} // namespace
