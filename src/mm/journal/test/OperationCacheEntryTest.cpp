/*
 * @author Viktor Gottfried
 */

#include <gtest/gtest.h>
#include "mm/journal/OperationCacheEntry.h"

namespace
{

class OperationCacheEntryTest: public ::testing::Test
{
protected:
	virtual void SetUp()
	{
		chunk_1 = 1;
		chunk_2 = 2;
		chunk_3 = 3;
	}

	virtual void TearDown()
	{
	}

	int32_t chunk_1;
	int32_t chunk_2;
	int32_t chunk_3;
};

TEST_F(OperationCacheEntryTest, entry_test)
{
	uint64_t operation_id = 1;

	OperationCacheEntry entry_1(operation_id, chunk_1, OperationStatus::NotCommitted);
	EXPECT_TRUE(entry_1.get_next_number() == 2);
	EXPECT_TRUE(entry_1.get_operation_id() == operation_id);

	EXPECT_TRUE(entry_1.get_last_chunk() == chunk_1);

	entry_1.add(chunk_2, OperationStatus::Committed);

	EXPECT_TRUE(entry_1.get_last_chunk() == chunk_2);

	entry_1.add(chunk_3, OperationStatus::NotCommitted);
	EXPECT_TRUE(entry_1.get_last_chunk() == chunk_3);
}



} /* namespace */

