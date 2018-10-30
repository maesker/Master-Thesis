#include "gtest/gtest.h"
#include "mm/einodeio/InodeNumberDistributor.h"
#include "mm/storage/storage.h"
#include "global_types.h"
#include <math.h>

namespace
{

class InodeNumberDistributorTest : public ::testing::Test
{
protected:
    InodeNumberDistributorTest()
    {
    }

    ~InodeNumberDistributorTest()
    {
    }

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

TEST(InodeNumberDistributorTest, TestCreateInodeNumber)
{
    int i;

    int max_rank = pow(2, PARTITION_OFFSET_BYTES * 8);

    InodeNumberDistributor *generator0 = new InodeNumberDistributor(0, NULL, 0);
    InodeNumberDistributor *generator1 = new InodeNumberDistributor(1, NULL, 0);
    for(i=0; i < 1000; i++)
    {
        InodeNumber x, y;
        ASSERT_NO_THROW( x = generator0->get_free_inode_number() );
        ASSERT_NO_THROW( y= generator1->get_free_inode_number() );
        ASSERT_TRUE( x != y );
        ASSERT_FALSE( x == FS_ROOT_INODE_NUMBER );
        ASSERT_FALSE( y == FS_ROOT_INODE_NUMBER );
        ASSERT_FALSE( x == 0 );
        ASSERT_FALSE( y == 0 );
    }


    ASSERT_THROW( new InodeNumberDistributor(max_rank + 1, NULL, 0), EInodeIOException);
    delete generator0;
    delete generator1;
}

TEST(InodeNumberDistributorTest, TestRecovery)
{
    int rank = 0;
    StorageAbstractionLayer *storage_abstraction_layer;
    char *device_identifier;
    device_identifier = strdup("/tmp/");
    storage_abstraction_layer = new StorageAbstractionLayer(device_identifier);

    InodeNumberConfig config;
    config.allocated_numbers = 123;
    config.rank = rank;

    char object_name[MAX_NAME_LEN];
    snprintf(object_name, MAX_NAME_LEN, "%s%d", CONFIG_FILE_PREFIX, rank);
    storage_abstraction_layer->write_object(1, object_name, 0, sizeof(InodeNumberConfig), &config);

    InodeNumberDistributor *generator0 = new InodeNumberDistributor(rank, storage_abstraction_layer, 1);

    ASSERT_TRUE( generator0->get_free_inode_number() > config.allocated_numbers );

    storage_abstraction_layer->remove_object(1, object_name);
    delete generator0;
    delete storage_abstraction_layer;
    free(device_identifier);
}

} // namespace
