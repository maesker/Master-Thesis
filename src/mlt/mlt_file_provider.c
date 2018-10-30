/**
 * @file mlt_file_provider.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.05.2011
 *
 * @brief Implements the interface to read and write and mlt from file.
 */

#include "mlt/mlt_file_provider.h"
#include <stdlib.h>
#include <string.h>

/**
 * @breif Writes an mlt to the file system.
 * @param list Pointer to the mlt.
 * @param file_path Pointer to the string of the file path.
 */
int write_mlt(struct mlt* list, const char* file_path)
{

	FILE *fp;
	fp = fopen(file_path, "wb");

	if (fp == NULL)
	{
		printf("write_mlt -- Error during open the file: %s\n", file_path);
		return -1;
	}

	// write number of entries
	write_int(fp, list->number_entries);

	//write the entry list size
	write_int(fp, list->list_size);

	//write the last added number
	write_int(fp, list->last_added);

	// write size of server list
	write_int(fp, list->server_list->n);

	// determine server number
	int server_number = 0;
	int i;
	for(i = 0; i<list->server_list->n; i++)
	{
		if(list->server_list->servers[i] != NULL)
			server_number ++;
	}

	// write number of server
	write_int(fp, server_number);

	// write server
	for(i = 0; i<list->server_list->n; i++)
	{
		if(list->server_list->servers[i] != NULL)
			write_server(fp, list->server_list->servers[i]);
	}

	//write root node
	write_entry(list->root, fp);

	// write all entries
	if (list->root->child != NULL)
	{
		traverse_pre_order(list->root->child, fp);
	}
	fclose(fp);

	return 0;
}

/**
 * @brief Read mlt from file.
 * @param File path.
 * @return The pointer to the read mlt; NULL if an error occurred.
 */
struct mlt* read_mlt(const char* file_path)
{
	FILE *fp;
	int entries_number;
	int entry_id;
	int server_number;
	int server_list_s;
	int readed_entries;
	int export_id;
	InodeNumber root_inode;
	int version;
	int path_length;
	int last_added;
	int list_size;
	char* path;

	int** associations_list;

	struct asyn_tcp_server* mds;
	struct mlt* list = NULL;
	struct mlt_entry* temp = NULL;

	list = mlt_create_empty_mlt();

	fp = fopen(file_path, "rb");


	if (fp == NULL)
	{
		printf("read_mlt -- Error during open the file: %s\n", file_path);
		return NULL;
	}

	// read number of entries
	fread(&entries_number, sizeof(int), 1, fp);
	list->number_entries = entries_number;

	// read list size
	fread(&list_size, sizeof(int), 1, fp);
	list->list_size = list_size;

	// Allocate memory for the entry list in the mlt
	list->entry_list = (struct mlt_entry**) calloc(list_size, sizeof(struct mlt_entry*));

	// Allocate memory for the associations_list
	associations_list = (int**) calloc(list_size, sizeof(int*));
	int k;
	for (k = 0; k < list_size; k++)
	{
		associations_list[k] = (int*) malloc(sizeof(int) * 4);
	}

	// read last added id
	fread(&last_added, sizeof(int), 1, fp);
	list->last_added = last_added;

	// read size of server list
	fread(&server_list_s, sizeof(int), 1, fp);
	list->server_list
			= (struct server_list*) malloc(sizeof(struct server_list));

	// allocate and init pointers to NULL
	list->server_list->servers = (struct asyn_tcp_server**) calloc(server_list_s,
			sizeof(struct asyn_tcp_server*));
	list->server_list->n = server_list_s;

	// write number of server
	fread(&server_number, sizeof(int), 1, fp);
	int i;
	for(i = 0; i<server_number; i++)
		read_server(fp, list);


	/*
	 * read the root node
	 */
	mds = read_server(fp, list);

	fread(&entry_id, sizeof(int), 1, fp);

	// read the associations
	//parent
	fread(&(associations_list[entry_id][0]), sizeof(int), 1, fp);
	//child
	fread(&(associations_list[entry_id][1]), sizeof(int), 1, fp);
	//ancestor
	fread(&(associations_list[entry_id][2]), sizeof(int), 1, fp);
	//sibling
	fread(&(associations_list[entry_id][3]), sizeof(int), 1, fp);

	fread(&export_id, sizeof(int), 1, fp);
	fread(&root_inode, sizeof(InodeNumber), 1, fp);
	fread(&version, sizeof(int), 1, fp);
	fread(&path_length, sizeof(int), 1, fp);

	path = malloc(sizeof(char) * path_length + 1);
	fread(path, sizeof(char), path_length, fp);
	path[path_length] = '\0';

	temp = mlt_create_new_entry(mds, export_id, root_inode, path, NULL,
			NULL);
	temp->version = version;
	temp->entry_id = entry_id;

	list->root = temp;

	list->entry_list[0] = list->root;
	/* finished read the root node */

	free(path);

	readed_entries = 1;

	// read the entries
	while (!feof(fp) && readed_entries < entries_number)
	{
		mds = read_server(fp, list);

		fread(&entry_id, sizeof(int), 1, fp);

		// read the associations
		//parent
		fread(&(associations_list[entry_id][0]), sizeof(int), 1, fp);
		//child
		fread(&(associations_list[entry_id][1]), sizeof(int), 1, fp);
		//ancestor
		fread(&(associations_list[entry_id][2]), sizeof(int), 1, fp);
		//sibling
		fread(&(associations_list[entry_id][3]), sizeof(int), 1, fp);

		fread(&export_id, sizeof(int), 1, fp);
		fread(&root_inode, sizeof(InodeNumber), 1, fp);
		fread(&version, sizeof(int), 1, fp);
		fread(&path_length, sizeof(int), 1, fp);

		path = malloc(sizeof(char) * path_length + 1);
		fread(path, sizeof(char), path_length, fp);
		path[path_length] = '\0';

		temp = mlt_create_new_entry(mds, export_id, root_inode, path, NULL,
				NULL);
		temp->version = version;
		temp->entry_id = entry_id;
		temp->replica = NULL;
		list->entry_list[entry_id] = temp;

		free(path);
		readed_entries++;
	}
	fclose(fp);

	// reconstruct the mlt
	for (i = 0; i < list_size; i++)
	{
		if (list->entry_list[i] != NULL)
		{
			if (associations_list[i][0] != -1)
				list->entry_list[i]->parent = list->entry_list[associations_list[i][0]];
			if (associations_list[i][1] != -1)
				list->entry_list[i]->child = list->entry_list[associations_list[i][1]];
			if (associations_list[i][2] != -1)
				list->entry_list[i]->ancestor = list->entry_list[associations_list[i][2]];
			if (associations_list[i][3] != -1)
							list->entry_list[i]->sibling = list->entry_list[associations_list[i][3]];
		}
	}

	// destroy the associations_list
	for(i = 0; i<list_size; i++)
	{
		free(associations_list[i]);
	}
	free(associations_list);

	return list;
}

/**
 * @brief Writes an entry.
 * @param e Pointer to the entry.
 * @param fp Pointer to the file.
 */
int write_entry(struct mlt_entry* e, FILE* fp)
{
	write_server(fp, e->mds);
	write_int(fp, e->entry_id);

	// write the associations

	// write parent
	if (e->parent == NULL)
		write_int(fp, -1);
	else
		write_int(fp, e->parent->entry_id);

	// writ child
	if (e->child == NULL)
		write_int(fp, -1);
	else
		write_int(fp, e->child->entry_id);

	// write ancestor
	if (e->ancestor == NULL)
		write_int(fp, -1);
	else
		write_int(fp, e->ancestor->entry_id);

	// write sibling
	if (e->sibling == NULL)
		write_int(fp, -1);
	else
		write_int(fp, e->sibling->entry_id);

	write_int(fp, e->export_id);

	fwrite(&(e->root_inode), sizeof(InodeNumber), 1, fp);

	write_int(fp, e->version);
	write_int(fp, strlen(e->path));
	fwrite(e->path, sizeof(char), strlen(e->path), fp);
	return 0;
}

/**
 * @brief Writes an integer value to file.
 * @param fp File pointer.
 * @param i Integer value.
 */
int write_int(FILE* fp, int i)
{
	if (fp == NULL)
	{
		fprintf(stderr, "write_int: Error during writing to file.\n");
		return -1;
	}

	if(fwrite(&i, sizeof(int), 1, fp) != 1)
		return -1;

	return 0;
}

/**
 * @brief Traverses the structure in pre order and writes it to file.
 * @param e Pointer to entry.
 * @param fp Pointer to the file.
 */
struct mlt_entry* traverse_pre_order(struct mlt_entry* e, FILE* fp)
{

	write_entry(e, fp);
	if (e->child != NULL)
	{
		traverse_pre_order(e->child, fp);
	}

	if (e->sibling != NULL)
	{
		traverse_pre_order(e->sibling, fp);
	}

	return NULL;
}

/**
 * @brief Write server address and port.
 * @param fp File pointer.
 * @param s Pointer to the server structure.
 */
int write_server(FILE* fp, struct asyn_tcp_server* s)
{
	// write the size of the char arrray
	if(write_int(fp, strlen(s->address)) != 0)
		return -1;

	 // write the char array
	if(fwrite(s->address, sizeof(char), strlen(s->address), fp) != strlen(s->address))
		return -1;

	 // write the port number
	if(fwrite(&s->port, sizeof(unsigned short), 1, fp) != 1)
		return -1;

	return 0;
}

/**
 * @brief Read server address and port.
 * @param fp File pointer.
 * @return Read server structure.
 */
struct asyn_tcp_server* read_server(FILE* fp, struct mlt* list)
{
	int size;

	char* address;
	unsigned short port;
	struct asyn_tcp_server* s;

	fread(&size, sizeof(int), 1, fp); // read the size of the following char array
	address = (char*) malloc(sizeof(char) * size + 1);

	fread(address, sizeof(char), size, fp); // read char array
	address[size] = '\0';

	fread(&port, sizeof(unsigned short), 1, fp); // read the port number

	s = mlt_find_server(list, address, port);

	// server is not in the server list
	if (s == NULL)
	{
		s = mlt_create_server(address, port);
		;

		int i = 0;
		while (list->server_list->servers[i] != NULL)
			i++;
		list->server_list->servers[i] = s;
	}
	free(address);
	return s;
}
