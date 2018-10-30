/**
 * @file mlt.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.05.2011
 */


#ifndef MLT_H_
#define MLT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "global_types.h"

#define INIT_LIST_SIZE 16

/**
 * Defines the mlt structure.
 */
struct mlt
{
	int number_entries; /**< The number of entries inside the mlt. */
	struct mlt_entry* root; /**< Pointer to the root entry. */
	struct server_list* server_list; /**< List of servers. */
	int list_size;
	int last_added;	
	struct mlt_entry** entry_list;	/* List of entries. */
};

/**
 * Defines the mlt entry structure.
 */
struct mlt_entry
{
	struct asyn_tcp_server* mds; 	/**< Pointer to the server which is responsible for this entry. */
	int export_id; 			/**< The export id. */
	InodeNumber root_inode;	/**< The root inode number of the subtree. */

	int entry_id;

	int version;			/**< The version of this entry.  */
	int* replicas;			/**< List of replicas. */
	char* path;				/**< The absolute path to the entry. */

	int tokens;
	char** tokens_arr;
	struct mlt_replica* replica;

	struct mlt_entry* ancestor;
	struct mlt_entry* sibling;
	struct mlt_entry* child;
	struct mlt_entry* parent;
};

struct asyn_tcp_server{
	char* address;
	unsigned short port;
};

struct mlt_replica
{
	int n;
	struct asyn_tcp_server** replica_list;
};

struct server_list{
	struct asyn_tcp_server** servers;
	int n;
};


// Structure creation/destroying.
//

struct mlt* mlt_create_empty_mlt();

struct mlt* mlt_create_new_mlt(struct asyn_tcp_server* mds, int export_id, InodeNumber root_inode);

void mlt_destroy_mlt(struct mlt* list);

struct mlt_entry* mlt_create_new_entry(struct asyn_tcp_server* mds, int export_id, InodeNumber root_inode,
		const char* path_string, struct mlt_entry* parent,
		struct mlt_entry* ancestor);

struct mlt_entry* mlt_add_new_entry(struct mlt* list, struct asyn_tcp_server* mds, int export_id, InodeNumber root_inode, const char* path_string);
void mlt_destroy_entry(struct mlt_entry* e);

struct mlt_replica* mlt_create_new_replica(int n);
void mlt_destroy_replica(struct mlt_replica* r);

char* mlt_create_new_path(const char* path_string);

struct mlt_entry* mlt_find_subtree(struct mlt*, const char* path);

void mlt_remove_entry(struct mlt* list, struct mlt_entry* e);

struct asyn_tcp_server* mlt_create_server(const char* address, unsigned short port);
void mlt_destroy_server(struct asyn_tcp_server* s);

struct asyn_tcp_server* mlt_find_server(const struct mlt* list, const char* address, unsigned short port);

struct server_list* mlt_create_server_list(int list_size);

int mlt_resize_server_list(struct mlt* list);

/** untested, seem to work */
struct mlt_entry *get_successor(struct mlt_entry *p_entry);
struct mlt_entry *get_upwards_successor(struct mlt_entry *p_entry);
/** untested, seem to work */

//
//Internal stuff
//
char** mlt_split_path(int* n, const char* path);
int mlt_count_delimiters(const char * path, char c);
struct mlt_entry* mlt_traverse_path(struct mlt_entry* e, char** tokens_arr, int tokens);
int mlt_compare_tokens(const char** t_1, int l_1, const char** t_2, int l_2);

int mlt_resize_entry_list(struct mlt* list);

#ifdef __cplusplus
}
#endif

#endif /* MLT_H_ */
