#include "gtest/gtest.h"
#include "mm/journal/Operation.h"

#include <sstream>
#include <fstream>

namespace
{

#define FILE_NAME "operation.file"
#define DATA "testdata"
#define DATA_SIZE 9

class OperationTest: public ::testing::Test
{
protected:

	OperationTest()
    {
        id = 1;
        type = 2;
        status = 0;
    }

    virtual ~OperationTest()
    {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    void writeOperation(Operation& op){

    }

    int id;
    int type;
    int status;
};


TEST_F(OperationTest, write_read_operation)
{
//	/*
//	 * Writing an operation
//	 */
//	Operation op(id, type, status, DATA, DATA_SIZE-1);
//
//	ofstream ofs;
//	ofs.open(FILE_NAME);
//
//	// test if file is open
//	EXPECT_TRUE(ofs);
//
//	op.write_operation(ofs);
//
//	ofs.close();
//
//
//	/*
//	 * Read file
//	 */
//	ifstream ifs(FILE_NAME);
//
//	// test if file is open
//	EXPECT_TRUE(ifs);
//
//	Operation op_2;
//
//	op_2.read_operation(ifs);
//	ifs.close();
//
//	EXPECT_TRUE(op_2.get_operation_id() == id);
//	EXPECT_TRUE(op_2.get_status() == status);
//	EXPECT_TRUE(op_2.get_type() == type);
//	EXPECT_TRUE(strcmp(op_2.get_data(), op.get_data()) == 0);
}
} // namespace
