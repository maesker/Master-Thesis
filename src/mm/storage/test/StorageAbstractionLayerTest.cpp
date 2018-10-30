/*
 * TestStorageAbstractionLayer.cpp
 *
 *  Created on: 17.04.2011
 *      Author: chrkl
 */
#include <string.h>
#include <string>
#include <list>

#include "gtest/gtest.h"
#include "mm/storage/storage.h"
#include "EmbeddedInode.h"

#define OBJECT_NAME "storageAbstractionLayerTest"

namespace
{



class StorageAbstractionLayerTest : public ::testing::Test
{
protected:
    StorageAbstractionLayerTest()
    {
    }

    ~StorageAbstractionLayerTest()
    {
    }

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

TEST(StorageAbstractionLayerTest, DeviceReadWrite)
{
    char input[11] = "1243567890";
    char output[11];

    AbstractStorageDevice *device;
    char *device_identifier = strdup("/tmp");
    device = new FileStorageDevice(device_identifier);

    if(!device)
        FAIL ();

    device->write_object(OBJECT_NAME, 0, sizeof(input), (void *) input);
    device->read_object(OBJECT_NAME, 0 , sizeof(input), (void *) output);
    EXPECT_TRUE(memcmp(input, output, sizeof(input)) == 0);

    bool object_found = false;
    std::list<std::string> *objects = device->list_objects();
    for(std::list<std::string>::iterator it = objects->begin(); it != objects->end(); it++)
    {
        if(it->compare(OBJECT_NAME))
            object_found = true;
    }
    delete objects;
    EXPECT_TRUE(object_found);

    EXPECT_TRUE(device->has_object(OBJECT_NAME));

    EXPECT_NO_THROW(
    {
        device->remove_object(OBJECT_NAME);
    });

    EXPECT_THROW(
    {
        device->remove_object(OBJECT_NAME);
    }, StorageException);

    EXPECT_FALSE(device->has_object(OBJECT_NAME));

    delete device;
    free (device_identifier);
}

TEST(StorageAbstractionLayerTest, ReadWrite)
{
    char input[11] = "1243567890";
    char output[11];

    StorageAbstractionLayer *storage_abstraction_layer;
    char *device_identifier = strdup("/tmp");
    storage_abstraction_layer = new StorageAbstractionLayer(device_identifier);
    storage_abstraction_layer->write_object(0, OBJECT_NAME, 0, sizeof(input), (void *) input);
    storage_abstraction_layer->read_object(0, OBJECT_NAME, 0 , sizeof(input), (void *) output);
    EXPECT_TRUE(memcmp(input, output, sizeof(input)) == 0);
    EXPECT_TRUE(sizeof(input) == storage_abstraction_layer->get_object_size(0, OBJECT_NAME));

    bool object_found = false;
    std::list<std::string> *objects = storage_abstraction_layer->list_objects(0);
    for(std::list<std::string>::iterator it = objects->begin(); it != objects->end(); it++)
    {
        if(it->compare(OBJECT_NAME))
            object_found = true;
    }
    delete objects;
    EXPECT_TRUE(object_found);

    EXPECT_NO_THROW(
    {
        storage_abstraction_layer->remove_object(0, OBJECT_NAME);
    });

    EXPECT_THROW(
    {
        storage_abstraction_layer->remove_object(0, OBJECT_NAME);
    }, StorageException);


    EInode einode1, einode2;
    memset(&einode1, 0, sizeof(EInode));
    strncpy(einode1.name, "test1", MAX_NAME_LEN);
    einode1.inode.inode_number = 2;

    storage_abstraction_layer->write_object(0, OBJECT_NAME, 0, sizeof(einode1), (void *) &einode1);
    storage_abstraction_layer->read_object(0, OBJECT_NAME, 0, sizeof(EInode), (void *) &einode2);
    EXPECT_TRUE(memcmp(&einode1, &einode2, sizeof(EInode)) == 0);
    EXPECT_NO_THROW(
    {
        storage_abstraction_layer->remove_object(0, OBJECT_NAME);
    });

    delete storage_abstraction_layer;
    free(device_identifier);
}
}  // namespace
