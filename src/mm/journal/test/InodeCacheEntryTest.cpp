/*
 * @author Viktor Gottfried
 */


#include <gtest/gtest.h>
#include "mm/journal/InodeCacheEntry.h"

namespace
{

class InodeCacheEntryTest: public ::testing::Test
{
protected:
	virtual void SetUp()
	{
	}

	virtual void TearDown()
	{
	}
};

TEST_F(InodeCacheEntryTest, entry_test)
{
	InodeNumber inode_1 = 1;

	OperationType type_1 = OperationType::CreateINode;
	OperationType type_2 = OperationType::SetAttribute;
	OperationType type_3 = OperationType::DeleteINode;
	OperationType test_type;
	int32_t chunk_1 = 1;
	int32_t chunk_2 = 2;
	int32_t chunk_3 = 3;

	InodeCacheEntry entry_1 = InodeCacheEntry(inode_1, type_1, chunk_1);

	EXPECT_TRUE(entry_1.get_indode_number() == inode_1);
	EXPECT_TRUE(entry_1.get_last_chunk_number() == chunk_1);
	EXPECT_TRUE(entry_1.get_last_type(test_type) == 0);
	EXPECT_TRUE(test_type == type_1);

	// add new data to entry
	entry_1.add(type_2, chunk_2);

	EXPECT_TRUE(entry_1.get_indode_number() == inode_1);
	EXPECT_TRUE(entry_1.get_last_chunk_number() == chunk_2);
	EXPECT_TRUE(entry_1.get_last_type(test_type) == 0);
	EXPECT_TRUE(test_type == type_2);

	// add new data to entry
	entry_1.add(type_3, chunk_3);

	EXPECT_TRUE(entry_1.get_indode_number() == inode_1);
	EXPECT_TRUE(entry_1.get_last_chunk_number() == chunk_3);
	EXPECT_TRUE(entry_1.get_last_type(test_type) == 0);
	EXPECT_TRUE(test_type == type_3);

	EXPECT_TRUE(entry_1.get_first_chunk_number() == chunk_1);
}



} /* namespace */

