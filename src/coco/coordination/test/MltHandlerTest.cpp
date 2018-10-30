/**
 * @author Viktor Gottfried
 */

#include "gtest/gtest.h"
#include <pthread.h>

#include <malloc.h>
#include "coco/coordination/MltHandler.h"

#define MLT_FILE "mlt.dat"

namespace
{

class MltHandlerTest: public ::testing::Test
{
protected:

	MltHandlerTest()
	{
		// You can do set-up work for each test here.
	}

	virtual ~MltHandlerTest()
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
		MltHandler *instance = &(MltHandler::get_instance());
		delete instance;
		// Code here will be called immediately after each test (right
		// before the destructor).
	}


};

TEST_F(MltHandlerTest, test_path_to_root)
{
	string relative_path;
	string root_path = "/";
	string some_path = "/home/user/name";
	string some_path_2 =  "/home/user/name/d/f";
	InodeNumber result_inode = 0;
	Server s;
	s.address = "127.0.0.1";
	s.port = 1337;
	MltHandler* mlt_handler = &(MltHandler::get_instance());
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);
	EXPECT_FALSE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);
	mlt_handler->set_my_address(s);
	EXPECT_TRUE(mlt_handler->path_to_root(root_path, result_inode, relative_path));
	EXPECT_TRUE(result_inode == 1);
	EXPECT_TRUE(root_path.compare(relative_path) == 0);

	result_inode = 0;

	EXPECT_FALSE(mlt_handler->path_to_root(some_path, result_inode, relative_path));
	EXPECT_TRUE(some_path.compare(relative_path) == 0);
	EXPECT_TRUE(result_inode == 1);


	EXPECT_TRUE( mlt_handler->handle_add_new_entry(mlt_handler->get_my_address(), 2, 2, some_path.c_str()) == 0);
	EXPECT_TRUE(mlt_handler->path_to_root(some_path, result_inode, relative_path));
	EXPECT_TRUE(some_path.compare(relative_path) == 0);
	EXPECT_TRUE(result_inode == 2);

	string expected_relative_path  = "/d/f";
	EXPECT_FALSE(mlt_handler->path_to_root(some_path_2, result_inode, relative_path));
	EXPECT_TRUE(expected_relative_path.compare(relative_path) == 0);
	EXPECT_TRUE(result_inode == 2);
}


TEST_F(MltHandlerTest, test_init)
{

	MltHandler* mlt_handler = &(MltHandler::get_instance());
	struct mallinfo mi_init = mallinfo();
	struct mallinfo mi_finish;
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);
	EXPECT_FALSE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);

	mlt_handler->destroy_mlt();
	mi_finish = mallinfo();
	EXPECT_TRUE( mi_init.uordblks == mi_finish.uordblks);
}

TEST_F(MltHandlerTest, test_write_read)
{

	MltHandler* mlt_handler = &(MltHandler::get_instance());
	struct mallinfo mi_init = mallinfo();
	struct mallinfo mi_finish;
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);
	EXPECT_TRUE(mlt_handler->add_server("192.168.0.1", 1337) == 0);
	EXPECT_TRUE(mlt_handler->add_server("192.168.0.3", 1337) == 0);

	EXPECT_TRUE(mlt_handler->write_to_file(MLT_FILE) == 0);

	mlt_handler->destroy_mlt();

	EXPECT_TRUE(mlt_handler->read_from_file(MLT_FILE) == 0);

	{
		vector<Server> server_list;
		mlt_handler->get_server_list(server_list);
		EXPECT_TRUE(server_list.size() == 3);
	}

	mlt_handler->destroy_mlt(),

	mi_finish = mallinfo();
	EXPECT_TRUE( mi_init.uordblks == mi_finish.uordblks);
}

TEST_F(MltHandlerTest, test_get_server_list)
{

	MltHandler* mlt_handler = &(MltHandler::get_instance());
	struct mallinfo mi_init = mallinfo();
	struct mallinfo mi_finish;
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);
	EXPECT_FALSE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);
	EXPECT_TRUE(mlt_handler->add_server("192.168.0.1", 1337) == 0);
	EXPECT_TRUE(mlt_handler->add_server("192.168.0.3", 1337) == 0);
	{

		vector<Server> server_list;
		EXPECT_TRUE(server_list.empty());

		mlt_handler->get_server_list(server_list);
		EXPECT_TRUE(server_list.size() == 3);

		EXPECT_TRUE(server_list.front().port == 1337);
		EXPECT_TRUE(server_list.front().address == "127.0.0.1");

		mlt_handler->destroy_mlt();
		server_list.clear();
	}// scope to deallocate strings before memory check

	mi_finish = mallinfo();

	EXPECT_TRUE( mi_init.uordblks == mi_finish.uordblks);
}

TEST_F(MltHandlerTest, test_get_other_server_list)
{

	MltHandler* mlt_handler = &(MltHandler::get_instance());
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);
	Server s;
	s.address = "127.0.0.1";
	s.port = 1337;
	mlt_handler->set_my_address(s);

	EXPECT_FALSE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);

	EXPECT_TRUE(mlt_handler->add_server("192.168.0.1", 1337) == 0);
	EXPECT_TRUE(mlt_handler->add_server("192.168.0.2", 1337) == 0);
	{

		vector<Server> server_list;
		EXPECT_TRUE(server_list.empty());

		mlt_handler->get_other_servers(server_list);
		EXPECT_TRUE(server_list.size() == 2);
		EXPECT_TRUE(server_list.front().port == 1337);
		EXPECT_TRUE(server_list.front().address == "192.168.0.1");
		mlt_handler->destroy_mlt();
		server_list.clear();
	}// scope to deallocate strings before memory check
}

TEST_F(MltHandlerTest, test_add_server)
{

	MltHandler* mlt_handler = &(MltHandler::get_instance());
	struct mallinfo mi_init = mallinfo();
	struct mallinfo mi_finish;
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);
	EXPECT_FALSE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);

	EXPECT_FALSE(mlt_handler->add_server("127.0.0.1", 1337) == 0);

	int server_number = 20;
	for(int i = 0; i < server_number; i++)
	{
		EXPECT_TRUE(mlt_handler->add_server("192.168.0.1", i) == 0);
		EXPECT_FALSE(mlt_handler->add_server("192.168.0.1", i) == 0);
	}

	{

		vector<Server> server_list;
		EXPECT_TRUE(server_list.empty());

		mlt_handler->get_server_list(server_list);
		EXPECT_TRUE(server_list.size() == server_number +1);

		server_list.clear();
	}// scope to deallocate strings before memory check


	mlt_handler->destroy_mlt();
	mi_finish = mallinfo();

	EXPECT_TRUE( mi_init.uordblks == mi_finish.uordblks);
}

TEST_F(MltHandlerTest, test_remove_server)
{

	MltHandler* mlt_handler = &(MltHandler::get_instance());
	struct mallinfo mi_init = mallinfo();
	struct mallinfo mi_finish;
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);
	EXPECT_FALSE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);


	EXPECT_TRUE(mlt_handler->add_server("192.168.0.1", 1337) == 0);
	EXPECT_FALSE(mlt_handler->add_server("192.168.0.1", 1337) == 0);


	EXPECT_FALSE(mlt_handler->remove_server("255.255.255.0", 1337) == 0);

	EXPECT_TRUE(mlt_handler->remove_server("192.168.0.1", 1337) == 0);

	{

		vector<Server> server_list;
		EXPECT_TRUE(server_list.empty());

		mlt_handler->get_server_list(server_list);
		EXPECT_TRUE(server_list.size() == 1);

		server_list.clear();
	}// scope to deallocate strings before memory check

	mlt_handler->destroy_mlt();
	mi_finish = mallinfo();
	EXPECT_TRUE( mi_init.uordblks == mi_finish.uordblks);
}

TEST_F(MltHandlerTest, test_is_partition_root)
{
	MltHandler* mlt_handler = &(MltHandler::get_instance());
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);

	EXPECT_TRUE(mlt_handler->is_partition_root(1));
	EXPECT_FALSE(mlt_handler->is_partition_root(2));

	mlt_handler->destroy_mlt();
}

TEST_F(MltHandlerTest, test_get_my_partitions)
{
	MltHandler* mlt_handler = &(MltHandler::get_instance());
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);

	Server address;
	address.address = "127.0.0.1";
	address.port = 1337;
	mlt_handler->set_my_address(address);

	vector<InodeNumber> partitions;


	EXPECT_TRUE( mlt_handler->get_my_partitions(partitions) == 0);

	EXPECT_TRUE( partitions.front() == 1);

	mlt_handler->destroy_mlt();
}



TEST_F(MltHandlerTest, handle_add_new_entry)
{
	Server server;
	server.address = "127.0.0.1";
	server.port = 1337;
	MltHandler* mlt_handler = &(MltHandler::get_instance());
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);

	mlt_handler->set_my_address(server);
	EXPECT_TRUE(mlt_handler->handle_add_new_entry(mlt_handler->get_my_address(), 2, 2, "/home/user") == 0);
	EXPECT_TRUE(mlt_handler->get_path(2).compare("/home/user") == 0);
	EXPECT_TRUE(mlt_handler->get_version(2) == 0);

	Server s = mlt_handler->get_mds(2);
	EXPECT_TRUE(s.address.compare("127.0.0.1") == 0);
	EXPECT_TRUE(s.port == 1337);

	s.address = "127.0.0.1";
	s.port = 1338;
	EXPECT_TRUE(mlt_handler->add_server("127.0.0.1", 1338) == 0);

	EXPECT_TRUE(mlt_handler->handle_add_new_entry(s, 2, 2, "/home/user") == 0);
	Server s_2 = mlt_handler->get_mds(2);

	EXPECT_TRUE(s_2.address.compare(s.address) == 0);
	EXPECT_TRUE(s_2.port == s.port);

	mlt_handler->destroy_mlt();
}


TEST_F(MltHandlerTest, get_children_parent_test)
{
	Server server;
	server.address = "127.0.0.1";
	server.port = 1337;
	MltHandler* mlt_handler = &(MltHandler::get_instance());
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);
	mlt_handler->set_my_address(server);

	EXPECT_TRUE(mlt_handler->handle_add_new_entry(mlt_handler->get_my_address(), 2, 2, "/home/user") == 0);
	EXPECT_TRUE(mlt_handler->handle_add_new_entry(mlt_handler->get_my_address(), 3, 3, "/boot") == 0);
	EXPECT_TRUE(mlt_handler->handle_add_new_entry(mlt_handler->get_my_address(), 4, 4, "/src") == 0);
	EXPECT_TRUE(mlt_handler->handle_add_new_entry(mlt_handler->get_my_address(), 5, 5, "/share") == 0);
	EXPECT_TRUE(mlt_handler->handle_add_new_entry(mlt_handler->get_my_address(), 6, 6, "/home/user/src") == 0);

	InodeNumber inode_id;
	inode_id = mlt_handler->get_parent(2);
	EXPECT_TRUE(inode_id == 1);

	inode_id = mlt_handler->get_parent(3);
	EXPECT_TRUE(inode_id == 1);

	inode_id = mlt_handler->get_parent(4);
	EXPECT_TRUE(inode_id == 1);

	inode_id = mlt_handler->get_parent(5);
	EXPECT_TRUE(inode_id == 1);

	inode_id = mlt_handler->get_parent(6);
	EXPECT_TRUE(inode_id == 2);


	// test get_children
	vector<InodeNumber> children;

	int ret = mlt_handler->get_children(1, children);

	EXPECT_TRUE(ret);
	EXPECT_TRUE(children.size() == 4);

	mlt_handler->destroy_mlt();
}



TEST_F(MltHandlerTest, destroy_mlt)
{
	Server server;
	server.address = "127.0.0.1";
	server.port = 1337;
	MltHandler* mlt_handler = &(MltHandler::get_instance());
	EXPECT_TRUE(mlt_handler->init_new_mlt("127.0.0.1", 1337, 1, 1) == 0);
    mlt_handler->destroy_mlt();
    SUCCEED();
}

}
