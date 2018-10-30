/*
 * @author Viktor Gottfried
 */


#include <gtest/gtest.h>
#include <string.h>

#include "mm/journal/InodeCache.h"

namespace
{

class InodeCacheTest: public ::testing::Test
{
protected:
	virtual void SetUp()
	{
		name_1 = "einode_1";
		name_2 = "einode_2";
		name_3 = "einode_3";

		inode_1 = 1;
		inode_2 = 2;
		inode_3 = 3;

		type_1 = OperationType::CreateINode;
		type_2 = OperationType::SetAttribute;
		type_3 = OperationType::DeleteINode;

		chunk_1 = 1;
		chunk_2 = 2;
		chunk_3 = 3;

		einode_1.inode.inode_number = inode_1;
		strcpy(einode_1.name, (char *)name_1.c_str());

		einode_2.inode.inode_number = inode_2;
		strcpy(einode_2.name, (char *)name_2.c_str());

		einode_3.inode.inode_number = inode_3;
		strcpy(einode_3.name, (char *)name_3.c_str());
	}

	virtual void TearDown()
	{
	}

	string name_1;
	string name_2;
	string name_3;

	EInode einode_1;
	EInode einode_2;
	EInode einode_3;

	InodeNumber inode_1;
	InodeNumber inode_2;
	InodeNumber inode_3;

	OperationType type_1;
	OperationType type_2;
	OperationType type_3;

	int32_t chunk_1;
	int32_t chunk_2;
	int32_t chunk_3;



};



TEST_F(InodeCacheTest, entry_test)
{
	EInode einode;
	einode.inode.inode_number = inode_1;

	Operation op;

	InodeNumber parent_id = 100;
	InodeCache cache_1;

	EXPECT_TRUE(cache_1.add_to_cache(inode_1, parent_id, einode_1) == 0);
	EXPECT_FALSE(cache_1.add_to_cache(inode_1, parent_id, einode) == 0);

	EXPECT_TRUE(cache_1.get_einode(inode_1, einode) == 0);

	EXPECT_TRUE(strcmp(einode.name, einode_1.name) == 0);

	EXPECT_FALSE(cache_1.get_einode(inode_2, einode) == 0);

	op.set_parent_id(INVALID_INODE_ID);
	op.set_inode_id(inode_1);
	op.set_einode(einode_1);
	EXPECT_NO_THROW(cache_1.update_inode_cache(&op, chunk_1));

	op.set_inode_id(inode_2);
	op.set_einode(einode_2);
	EXPECT_ANY_THROW(cache_1.update_inode_cache(&op, chunk_1));

	op.set_parent_id(parent_id);
	EXPECT_NO_THROW(cache_1.update_inode_cache(&op, chunk_1));
	EXPECT_TRUE(cache_1.get_einode(inode_1, einode) == 0);
	EXPECT_TRUE(cache_1.get_einode(inode_2, einode) == 0);

	EXPECT_TRUE(cache_1.lookup_by_object_name((const FsObjectName*) name_1.c_str(), parent_id) == inode_1);
	EXPECT_TRUE(cache_1.lookup_by_object_name((const FsObjectName*) name_2.c_str(), parent_id) == inode_2);
}


} /* namespace */

