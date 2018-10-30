#include "gtest/gtest.h"
#include "mm/storage/LockManager.h"

#define OBJECT "my_object"

class LockManagerTest : public ::testing::Test
{
protected:
    LockManagerTest()
    {
    }

    ~LockManagerTest()
    {
    }

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

TEST(LockManagerTest, TestLock)
{
    LockManager *lm = new LockManager();

    ASSERT_NO_THROW( lm->lock_object(OBJECT) );
    ASSERT_TRUE( lm->is_locked(OBJECT) );
    ASSERT_NO_THROW( lm->unlock_object(OBJECT) );
    ASSERT_FALSE( lm->is_locked(OBJECT) );

    delete lm;
}