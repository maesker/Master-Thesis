#include "gtest/gtest.h"

#include <sstream>
#include <fstream>
#include <string>

#include "mm/storage/storage.h"
#include "mm/journal/JournalChunk.h"
#include "mm/journal/FailureCodes.h"

namespace
{

#define FILE_NAME "operation.file"
#define DATA "testdata"
#define DATA_SIZE 9

class JournalChunkTest: public ::testing::Test
{
protected:

	JournalChunkTest()
    {
		o1 = new Operation(1, OperationType::CreateINode, OperationStatus::NotCommitted, OperationMode::Atomic, DATA, DATA_SIZE);
		o2 = new Operation(2, OperationType::CreateINode, OperationStatus::NotCommitted, OperationMode::Atomic, DATA, DATA_SIZE);
		o3 = new Operation(3, OperationType::CreateINode, OperationStatus::NotCommitted, OperationMode::Atomic, DATA, DATA_SIZE);

	    char *device_identifier;
	    device_identifier = strdup("/tmp");

	    storage_abstraction_layer = new StorageAbstractionLayer(device_identifier);
	    parent_id = 1;
    }

    virtual ~JournalChunkTest()
    {
    	delete storage_abstraction_layer;
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


    int id;
    int type;
    int status;
    Operation* o1;
    Operation* o2;
    Operation* o3;

    InodeNumber parent_id;

    StorageAbstractionLayer *storage_abstraction_layer;
    AbstractDataDistributionFunction *ddf;
    DataDistributionManager *ddm;
    AbstractStorageDevice *device;
};


TEST_F(JournalChunkTest, chunkID_test)
{
	int32_t chunk_id = 1;
	std::string prefix = "chunk";
	JournalChunk chunk(chunk_id, prefix, parent_id);

	EXPECT_TRUE(chunk.get_chunk_id() == chunk_id);

}

TEST_F(JournalChunkTest, add_chunk_test)
{
	int32_t chunk_id = 1;
	std::string prefix = "chunk";
	JournalChunk chunk(chunk_id, prefix, parent_id);

	chunk.add_operation(o1);

	EXPECT_TRUE(chunk.size() == 1);

}



TEST_F(JournalChunkTest, operations_number_method_test)
{
	int32_t chunk_id = 1;
	std::string prefix = "chunk";

	JournalChunk chunk(chunk_id, prefix, parent_id);

	chunk.add_operation(o1);
	chunk.add_operation(o2);

	EXPECT_TRUE(chunk.size() == 2);
}


TEST_F(JournalChunkTest, write_read_chunk)
{
	JournalChunk chunk(1, "1", parent_id);

	EXPECT_TRUE(chunk.add_operation(o1) == 0);
	EXPECT_TRUE(chunk.add_operation(o2) == 0);
	EXPECT_TRUE(chunk.add_operation(o3) == 0);
	EXPECT_TRUE(chunk.size() == 3);

    EXPECT_TRUE(chunk.write_chunk(*storage_abstraction_layer) == 0);

   	JournalChunk* chunk_2 = new  JournalChunk(1, "1", parent_id);
   	chunk_2->read_chunk(*storage_abstraction_layer);
   	EXPECT_TRUE(chunk_2->size() == chunk.size());


   	EXPECT_TRUE(chunk_2->get_operations().size() == chunk.get_operations().size());


   	vector<Operation*> list_1 = chunk.get_operations();
   	vector<Operation*> list_2 = chunk_2->get_operations();

   	for(int i = 0; i<list_1.size(); i++)
   	{
   		Operation* first = list_1.at(i);
   		Operation* copy = list_2.at(i);
   		EXPECT_TRUE(first->get_operation_id() == copy->get_operation_id());
   		EXPECT_TRUE(first->get_mode() == copy->get_mode());
   		EXPECT_TRUE(first->get_module() == copy->get_module());
   		EXPECT_TRUE(first->get_operation_number() == copy->get_operation_number());
   		EXPECT_TRUE(first->get_status() == copy->get_status());
   		EXPECT_TRUE(first->get_einode().inode.inode_number == copy->get_einode().inode.inode_number);
   	}
	delete chunk_2;

	EXPECT_TRUE(chunk.delete_chunk(*storage_abstraction_layer) == 0);
	EXPECT_TRUE(chunk.delete_chunk(*storage_abstraction_layer) == JournalFailureCodes::StorageAccessError);
	EXPECT_TRUE(chunk.read_chunk(*storage_abstraction_layer) == JournalFailureCodes::StorageAccessError);
}

TEST_F(JournalChunkTest, set_get_identifier)
{
	int32_t chunk_id = 1;
	std::string prefix = "chunk";
	std::string identifier = "id";

	JournalChunk chunk(chunk_id, prefix, parent_id);

	EXPECT_FALSE(chunk.get_identifier() == identifier);

	chunk.set_identifier(identifier);

	EXPECT_TRUE(chunk.get_identifier() == identifier);
}

TEST_F(JournalChunkTest, get_operations_set_test)
{
	int32_t chunk_id = 1;
	std::string prefix = "chunk";
	std::string identifier = "id";
	multimap<uint64_t, Operation*> map;
	multimap<uint64_t, Operation*>::iterator iterator;
	JournalChunk chunk(chunk_id, prefix, parent_id);

	//add few operations
	EXPECT_TRUE(chunk.add_operation(o1) == 0);
	EXPECT_TRUE(chunk.add_operation(o2) == 0);
	EXPECT_TRUE(chunk.add_operation(o3) == 0);

}


} // namespace
