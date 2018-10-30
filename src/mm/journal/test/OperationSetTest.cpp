#include <gtest/gtest.h>
#include "mm/journal/Operation.h"

namespace
{

class OperationSetTest: public ::testing::Test
{
protected:
	virtual void SetUp()
	{
		uint64_t operation_id = 1;
		OperationType type = SetAttribute;
		OperationStatus status = Committed;
		OperationMode mode = Atomic;
		char d = 's';
		char* data = &d;
		int32_t data_size = 10;
		Operation op(operation_id, type, status, mode, data, data_size);

		//EXPECT_TRUE(op.get_status() == status);
	}

	virtual void TearDown()
	{
	}

	Operation op;

};

TEST_F(OperationSetTest, get_set_id_test)
{
	uint64_t id1 = 1,id2 = 2;
	op.set_operation_id(id1);

	//test for get method
	EXPECT_TRUE(op.get_operation_id() == id1);

	op.set_operation_id(id2);

	//test for set method
	EXPECT_TRUE(op.get_operation_id() == id2);
}

TEST_F(OperationSetTest, get_set_mode_test)
{
//	Operation op(operation_id, type, status, mode, data, data_size);

	OperationMode mode1 = Atomic;
	op.set_mode(mode1);
	EXPECT_TRUE(op.get_mode() == mode1);

	//setting new mode
	OperationMode mode2 = Disributed;
	op.set_mode(mode2);

	EXPECT_TRUE(op.get_mode() == mode2);

}

TEST_F(OperationSetTest, get_set_status_test)
{
	//set new status
	OperationStatus stat = NotCommitted;
	op.set_status(stat);

	EXPECT_TRUE(op.get_status() == stat);
}

TEST_F(OperationSetTest, get_set_data_test)
{
	char data = 'B';
	EXPECT_TRUE(op.set_data(&data, sizeof(data)) == 0);

	EXPECT_TRUE(*op.get_data() == data);
}

TEST_F(OperationSetTest, get_set_operation_number)
{
	int32_t op_number = 10;
	op.set_operation_number(op_number);

	EXPECT_TRUE(op.get_operation_number() == op_number);
}

TEST_F(OperationSetTest, get_set_type)
{
	OperationType type = SubtreeMigration;
	op.set_type(type);

	EXPECT_TRUE(op.get_type() == type);
}

TEST_F(OperationSetTest, get_set_module)
{
	Module module = DistributedAtomicOp;
	op.set_module(module);

	EXPECT_TRUE(op.get_module() == module);
}

TEST_F(OperationSetTest, get_set_op_data)
{
	OperationData op_data;
	op.set_operation_data(op_data);

	OperationData op_d = op.get_operation_data();
	EXPECT_TRUE(op_d.operation_number == op_data.operation_number);
}

TEST_F(OperationSetTest, get_set_inode_number)
{
	int32_t inumber = 2;
	op.set_inode_number(inumber);

	EXPECT_TRUE(op.get_inode_number() == inumber);
}
//TEST_F(OperationSetTest, add_test)
//{
//	OperationSet operation_set;
//	uint64_t id = 1;
//	int32_t chunk_number = 2;
//	multimap<uint64_t, OperationMapping>::iterator it;
//	multimap<uint64_t, OperationMapping> operations_map;
//
//	//utilizing the method 1
//	operation_set.add_operation_info(id, chunk_number);
//
//	//cross check
//	operations_map = operation_set.get_map();
//	it = operations_map.begin();
//	EXPECT_TRUE( (*it).first == id);
//	EXPECT_FALSE((*it).first != id);
//}
//
//TEST_F(OperationSetTest, add_withOperationNumber_test)
//{
//	OperationSet operation_set;
//	uint64_t id = 1;
//	int32_t chunk_number = 2;
//	int32_t operation_number = 3;
//
//	//utilizing the method
//	operation_set.add_operation_info(id, operation_number, chunk_number);
//
//	//cross check
//	multimap<uint64_t, OperationMapping>::iterator it;
//	multimap<uint64_t, OperationMapping> operations_map = operation_set.get_map();
//	it = operations_map.begin();
//	OperationMapping object;
//	object = (*it).second;
//	EXPECT_TRUE( object.chunk_number == chunk_number);
//}
//
//TEST_F(OperationSetTest, add_vector_test)
//{
//	OperationSet operation_set;
//	uint64_t id = 1;
//	int32_t chunk_number = 2;
//	vector<OperationMapping> list;
//
//	operation_set.add_operation_info(id, chunk_number);
//
//	EXPECT_TRUE(operation_set.get_operation_info(id, list) == 1);
//}
//
//TEST_F(OperationSetTest, remove_operation_test)
//{
//	OperationSet operation_set;
//	uint64_t id = 1;
//	int32_t chunk_number = 2;
//	multimap<uint64_t, OperationMapping>::iterator it;
//	multimap<uint64_t, OperationMapping> operations_map;
//
//	operation_set.add_operation_info(id, chunk_number);
//
//	operations_map = operation_set.get_map();
//
////	cout << operation_set.remove_operation_info(id);
//	operation_set.remove_operation_info(id);
//
//	vector<OperationMapping> vec;
//	operation_set.get_operation_info(id, vec);
//	EXPECT_TRUE(operation_set.get_operation_info(id, vec) == 0);
//}
//
//TEST_F(OperationSetTest, check_set_test)
//{
//	OperationSet os, os_other;
//	int size;
//	uint64_t id1 = 1;
//	uint64_t id2 = 2;
//	uint64_t id3 = 3;
//	uint64_t id4 = 4;
//	int32_t chunk_number = 1;
//	multimap<uint64_t, OperationMapping>::iterator it;
//	multimap<uint64_t, OperationMapping> operation_map;
//
//	os.add_operation_info(id1, chunk_number);
//	os.add_operation_info(id2, chunk_number);
//
//	os_other.add_operation_info(id1, chunk_number);
//	os_other.add_operation_info(id3, chunk_number);
//	os_other.add_operation_info(id4, chunk_number);
//
//	operation_map = os.get_map();
//
//	operation_map = os_other.get_map();
//	size=operation_map.size();
//	EXPECT_TRUE(size == 3);
//
//	os.check_set(&os_other);
//
//	operation_map = os_other.get_map();
//	size=operation_map.size();
//	EXPECT_TRUE(size == 2);
//}
//
//
//TEST_F(OperationSetTest, count_test)
//{
//	OperationSet os;
//	int size;
//	uint64_t id = 1;
//	int32_t chunk_number = 1;
//	os.add_operation_info(id, chunk_number);
//	EXPECT_TRUE(os.count(id) == 1);
//}

} /* namespace */
