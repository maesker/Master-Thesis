#include "gtest/gtest.h"
#include "coco/communication/UniqueIdGenerator.h"


namespace
{

// The fixture for testing class Foo.
class IdGeneratorTest: public ::testing::Test
{
protected:

    IdGeneratorTest()
    {
        // You can do set-up work for each test here.
    }

    virtual ~IdGeneratorTest()
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

    UniqueIdGenerator gen_1;
    UniqueIdGenerator gen_2;

};

TEST_F(IdGeneratorTest, generate_id)
{
    vector<long> myVec;
    long id_1 = gen_1.get_next_id();
    long id_2 = gen_2.get_next_id();
    long id_3 = gen_1.get_next_id();
    long id_4 = gen_2.get_next_id();

    EXPECT_TRUE(id_1 == id_2);
    EXPECT_TRUE(id_3 == id_4);
    EXPECT_TRUE(id_3 != id_1);
}
}
