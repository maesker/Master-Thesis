/** \brief Brief description.
 *         Brief description continued.
 *
 *  Detailed description starts here.
 *
 * @author Viktor Gottfried
 */

#include "gtest/gtest.h"
#include "mlt/mlt.h"
#include "mlt/mlt_file_provider.h"
#include "global_types.h"

#include <malloc.h>
#include <sstream>

#define MLT_FILE "data.mlt"

namespace
{

class MltTester: public ::testing::Test
{
protected:

	MltTester()
	{
		// You can do set-up work for each test here.
	}

	virtual ~MltTester()
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
};

TEST_F(MltTester, create_mlt)
{
	struct mallinfo mi_init = mallinfo();
	std::cout << "Setup allocation status : " << mi_init.uordblks << std::endl;
	/**
	 * Init some server
	 */

	int server_number = 3;
	struct server* server_1 = mlt_create_server("127.0.0.1", 1337);
	struct server* server_2 = mlt_create_server("168.162.0.1", 1337);
	struct server* server_3 = mlt_create_server("178.162.0.7", 1337);

	struct server_list* s_list = mlt_create_server_list(server_number);

	s_list->servers[0] = server_1;
	s_list->servers[1] = server_2;
	s_list->servers[2] = server_3;

	// create new mlt
	struct mlt* list = mlt_create_new_mlt(server_1, 1, FS_ROOT_INODE_NUMBER);

	list->server_list = s_list;

	EXPECT_TRUE(mlt_find_server(list, "127.0.0.1", 1337) !=NULL);

	struct mlt_entry* e_1 = mlt_add_new_entry(list, server_2, 3, 2, "/usr/local");

	struct mlt_entry* e_2 = mlt_add_new_entry(list, server_2, 3, 2, "/usr/local/share");

	struct mlt_entry* e_3 = mlt_add_new_entry(list, server_3, 4, 4, "/home/user");

	struct mlt_entry* e_4 = mlt_add_new_entry(list, server_3, 4, 4, "/home/user/Downloads");

	// child of root must be e_1: /usr/local
	EXPECT_TRUE(list->root->child == e_1);
	EXPECT_TRUE(strcmp(e_1->path, list->root->child->path) == 0);

	// child of e_1 must be e_2
	EXPECT_TRUE(e_1->child == e_2);
	EXPECT_TRUE(e_1 == e_2->parent);

	// sibling of e_1 must be e_3
	EXPECT_TRUE(e_1->sibling == e_3);

	// find subtree of /usr/local/temp
	struct mlt_entry* unknown_1 = mlt_find_subtree(list, "/usr/local/temp");

	// must be /usr/local (e_1)
	EXPECT_TRUE(e_1 == unknown_1);

	// find subtree of /usr/local/share/include/src
	struct mlt_entry* unknown_2 = mlt_find_subtree(list, "/usr/local/share/include/src");

	//must be /usr/local/share (e_2)
	EXPECT_TRUE(e_2 == unknown_2);

	// write mlt to file
	write_mlt(list, MLT_FILE);

	// read mlt from file
	struct mlt* list_2 = read_mlt(MLT_FILE);

	// check number of entries
	EXPECT_TRUE(list->number_entries == list_2->number_entries);

	// check number of servers
	EXPECT_TRUE(list->server_list->n == list_2->server_list->n);

	// check the list size
	EXPECT_TRUE(list->list_size == list_2->list_size);

	EXPECT_TRUE(list->root->child->entry_id == list_2->root->child->entry_id);
	EXPECT_TRUE(list->root->child->sibling->entry_id == list_2->root->child->sibling->entry_id);

	// find e_2 in list_2
	struct mlt_entry* unknown_3 = mlt_find_subtree(list_2, e_2->path);

	EXPECT_TRUE(strcmp(e_2->path, unknown_3->path) == 0);

	EXPECT_TRUE(e_2->root_inode == unknown_3->root_inode);
	EXPECT_TRUE(e_2->export_id == unknown_3->export_id);

	EXPECT_TRUE(strcmp(e_2->mds->address, unknown_3->mds->address) == 0);
	EXPECT_TRUE(e_2->mds->port == unknown_3->mds->port);

	for(int i = 0; i<list->list_size; i++)
	{
		if(list->entry_list[i] != NULL)
		EXPECT_TRUE(strcmp(list->entry_list[i]->path, list_2->entry_list[i]->path) == 0);
	}

	// clean up
	mlt_destroy_mlt(list);
	mlt_destroy_mlt(list_2);

	struct mallinfo mi_finished = mallinfo();
	std::cout << "Finished allocation status: " << mi_finished.uordblks << std::endl;

	EXPECT_TRUE(mi_init.uordblks == mi_finished.uordblks);
}

}
