/*
 * @author Viktor Gottfried
 */


#include <gtest/gtest.h>
#include "mm/journal/OperationCache.h"

namespace
{

class OperationCacheTest: public ::testing::Test
{
protected:
	virtual void SetUp()
	{
		chunk_1 = 1;
		chunk_2 = 2;
		chunk_3 = 3;

		operation_1 = 1;
		operation_2 = 2;
		operation_3 = 3;
	}

	virtual void TearDown()
	{
	}

	int32_t chunk_1;
	int32_t chunk_2;
	int32_t chunk_3;

	uint64_t operation_1;
	uint64_t operation_2;
	uint64_t operation_3;
};



TEST_F(OperationCacheTest, add_remove_test)
{
	OperationCache cache_1;

	EXPECT_TRUE(cache_1.count(operation_1) == 0);

	cache_1.add(operation_1, chunk_1, OperationStatus::NotCommitted);
	EXPECT_TRUE(cache_1.count(operation_1) == 1);
	EXPECT_TRUE(cache_1.get_last_chunk(operation_1) == chunk_1);
	EXPECT_TRUE(cache_1.get_next_number(operation_1) == 2);


	cache_1.add(operation_1, chunk_2, OperationStatus::NotCommitted);
	EXPECT_TRUE(cache_1.count(operation_1) == 2);
	EXPECT_TRUE(cache_1.get_last_chunk(operation_1) == chunk_2);
	EXPECT_TRUE(cache_1.get_next_number(operation_1) == 3);

	cache_1.add(operation_1, chunk_3, OperationStatus::NotCommitted);
	EXPECT_TRUE(cache_1.count(operation_1) == 3);
	EXPECT_TRUE(cache_1.get_last_chunk(operation_1) == chunk_3);
	EXPECT_TRUE(cache_1.get_next_number(operation_1) == 4);

	cache_1.add(operation_2, chunk_1, OperationStatus::NotCommitted);
	EXPECT_TRUE(cache_1.count(operation_2) == 1);
	EXPECT_TRUE(cache_1.get_last_chunk(operation_2) == chunk_1);
	EXPECT_TRUE(cache_1.get_next_number(operation_2) == 2);

	EXPECT_TRUE(cache_1.remove(operation_1) == 0);
	EXPECT_TRUE(cache_1.remove(operation_1) == -1);
}

} /* namespace */

