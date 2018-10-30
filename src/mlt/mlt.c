/**
 * @file mlt.c
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.05.2011
 *
 * @brief Implements the metadata lookup table.
 */


#include "mlt/mlt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief Create a new empty mlt.
 * @return Pointer to the mlt.
 */
struct mlt* mlt_create_empty_mlt()
{
	struct mlt* l = (struct mlt*) malloc( sizeof(struct mlt) );

	if(l == NULL)
		return NULL;

	l->number_entries = 0;
	return l;
}

/**
 * @brief Finds an entry for the given path.
 * @param list Pointer to the mlt.
 * @param path Pointer to the string path.
 * @return The corresponding entry.
 */
struct mlt_entry* mlt_find_subtree(struct mlt* list, const char* path)
{
	if(list == NULL || path == NULL)
		return NULL;
	if(list->root->child == NULL)
		return list->root;

	int tokens;
	struct mlt_entry* e;
	char** tokens_arr = mlt_split_path( &tokens, path );

	if(tokens_arr == NULL)
		return NULL;

	e = mlt_traverse_path( list->root->child, tokens_arr, tokens );

	int i;
	for ( i = 0; i < tokens; i++ )
	{
		free( tokens_arr[i] );
	}
	free( tokens_arr );

	return e;
}

/**
 * @brief Creates a new mlt.
 * @param mds Pointer to the server address.
 * @param export id
 * @return Pointer to the mlt.
 */
struct mlt *mlt_create_new_mlt(struct asyn_tcp_server* mds, int export_id, InodeNumber root_inode)
{
	struct mlt* l = mlt_create_empty_mlt();

	if ( l == NULL )
		return NULL;

	l->list_size = INIT_LIST_SIZE;
	l->entry_list = (struct mlt_entry**) malloc( sizeof(struct mlt_entry*) * l->list_size );
	if(l->entry_list == NULL)
	{
		free(l);
		return NULL;
	}

	l->last_added = -1;

	int i;
	for ( i = 0; i < l->list_size; i++ )
	{
		l->entry_list[i] = NULL;
	}

	struct mlt_entry* root = (struct mlt_entry*) malloc( sizeof(struct mlt_entry) );
	if(root == NULL)
	{
		free(l->entry_list);
		free(l);
		return NULL;
	}

	root->mds = mds;
	root->root_inode = root_inode;
	root->export_id = export_id;
	root->version = 0;
	root->ancestor = NULL;
	root->parent = NULL;
	root->sibling = NULL;
	root->child = NULL;
	root->path = mlt_create_new_path( "/" );
	root->tokens = 0;
	root->replica = NULL;
	root->tokens_arr = NULL;
	root->entry_id = 0;

	l->root = root;
	l->number_entries = 1;

	l->entry_list[0] = l->root;
	l->last_added = 0;

	return l;
}

/**
 * @brief Creates a new mlt entry.
 * @param mds Pointer to the server address.
 * @param export_id
 * @param path_string Pointer to the string path.
 * @param parent Pointer to the parent entry.
 * @param ancestor Pointer to the ancestor entry.
 * @return The created entry.
 */
struct mlt_entry *mlt_create_new_entry(struct asyn_tcp_server* mds, int export_id, InodeNumber root_inode,
		const char* path_string, struct mlt_entry* parent, struct mlt_entry* ancestor)
{
	struct mlt_entry* e = (struct mlt_entry*) malloc( sizeof(struct mlt_entry) );

	if(e == NULL)
		return NULL;

	e->mds = mds;
	e->export_id = export_id;
	e->root_inode = root_inode;
	e->version = 0;
	e->ancestor = ancestor;
	e->parent = parent;
	e->sibling = NULL;
	e->child = NULL;
	e->path = mlt_create_new_path( path_string );
	e->tokens_arr = mlt_split_path( &(e->tokens), e->path );
	e->replica = NULL;

	return e;
}

/**
 * @brief Creates and add a new entry to the mlt.
 * The entry position is calculating using the given path.
 * @param list Pointer to the @see mlt.
 * @param mds Pointer to the server address.
 * @param export_id
 * @param path_string Pointer to the string path
 * @return Pointer to the entry.
 */
struct mlt_entry* mlt_add_new_entry(struct mlt* list, struct asyn_tcp_server* mds, int export_id, InodeNumber root_inode,
		const char* path_string)
{

	struct mlt_entry* ancestor = NULL;
	struct mlt_entry* temp = NULL;
	struct mlt_entry* parent = NULL;
	struct mlt_entry* new_entry = NULL;

	// First we need the parent of the new entry

	if ( list->root->child == NULL ) // root has no children, so root is our parent
		parent = list->root;
	else
		parent = mlt_find_subtree( list, path_string ); // determine parent

	if(parent == NULL)
		return NULL;

	new_entry = mlt_create_new_entry( mds, export_id, root_inode, path_string, parent, NULL );

	if(new_entry == NULL)
		return NULL;

	// Parent has no child?
	if ( parent->child == NULL )
		parent->child = new_entry;

	// Parent has already a child.
	else
	{
		temp = parent->child; // store the child here


		// Check whether the new entry is an new parent of an entry
		/*
		 * We know the parent of our new entry and we know, that the new entry is not a direct child of it.
		 * Now we need to determine, where we must put the new entry.
		 * There are two cases:
		 * 1: Our new entry is just a sibling without children.
		 * 2: Our new entry has children.
		 *
		 * We iterate over the siblings of the direct child from our parent, starting with the direct child and check
		 * whether the new entry is a new parent of it.
		 * If there is no possible child, we put the new entry as last sibling.
		 */
		do
		{
			if ( mlt_compare_tokens( (const char**) temp->tokens_arr, temp->tokens,
					(const char**) new_entry->tokens_arr, new_entry->tokens ) == 0 )
			{
				// New entry is new direct child?
				if ( temp == parent->child )
					parent->child = new_entry;

				// set parent and child pointers
				new_entry->child = temp;
				temp->parent = new_entry;

				// Set ancestor pointer
				if ( temp->ancestor != NULL )
				{
					new_entry->ancestor = temp->ancestor;
					new_entry->ancestor->sibling = new_entry;
					temp->ancestor = NULL;
				}

				// Set sibling pointer
				if ( temp->sibling != NULL )
				{
					new_entry->sibling = temp->sibling;
					new_entry->sibling->ancestor = new_entry;
					temp->sibling = NULL;
				}

				list->number_entries++;
				return new_entry;
			}
			ancestor = temp;
		} while ( (temp = temp->sibling) != NULL );

		//Set new entry as sibling
		ancestor->sibling = new_entry;
		new_entry->ancestor = ancestor;
	}

	new_entry->entry_id = ++list->last_added;

	// check if the list array provides enough space for an addition entry, if not resize it
	if ( list->last_added == list->list_size )
		mlt_resize_entry_list( list );

	list->entry_list[list->last_added] = new_entry;

	list->number_entries++;
	return new_entry;
}

/**
 * @brief Remove the given entry.
 * @param e Pointer to the entry to remove.
 */
void mlt_remove_entry(struct mlt* list, struct mlt_entry* e)
{

	struct mlt_entry* temp;
	struct mlt_entry* ancestor;

	if ( list->root == e || e == NULL)
		return;

	// Check if entry is a direct child.
	if ( e->parent->child == e )
	{
		// Check if entry has a child
		if ( e->child != NULL )
		{
			// Set parent/child pointers
			e->parent->child = e->child;
			e->child->parent = e->parent;

			temp = e->child->sibling;
			ancestor = e->child;
			// Set sibling/ancestor pointers
			while ( temp != NULL )
			{
				temp->parent = e->parent;
				ancestor = temp;
				temp = temp->sibling;
			}
			if ( e->sibling != NULL )
			{
				e->sibling->ancestor = ancestor;
				ancestor->sibling = e->sibling;
			}
		}

		// If entry has no children
		else
		{
			if ( e->sibling != NULL )
			{
				e->parent->child = e->sibling;
				e->sibling->ancestor = NULL;
			}
			else
			{
				e->parent->child = NULL;
			}
		}
	}

	// If entry is not direct child
	else
	{
		// If entry has children
		if ( e->child != NULL )
		{
			e->child->parent = e->parent;
			e->child->ancestor = e->ancestor;
			e->ancestor->sibling = e->child;
			if ( e->sibling != NULL )
			{
				e->sibling->ancestor = e->child;
				e->child->sibling = e->sibling;
			}
		}
		else
		{
			if ( e->sibling != NULL )
			{
				e->ancestor->sibling = e->sibling;
				e->sibling->ancestor = e->ancestor;
			}
			else
			{
				e->ancestor->sibling = NULL;
			}
		}
	}
	list->entry_list[e->entry_id] = NULL;
	mlt_destroy_entry( e );
	list->number_entries--;
}

/**
 * @brief Creates a copy of the path string.
 * @param path_string Pointer to the string.
 * @return Pointer to the copy of the string.
 */
char* mlt_create_new_path(const char* path_string)
{
	if(path_string == NULL)
		return NULL;

	char* c = (char*) malloc( sizeof(char) * (strlen( path_string ) + 1) );

	if(c == NULL)
		return NULL;

	// Create a copy of string
	memcpy( c, path_string, sizeof(char) * strlen( path_string ) );
	c[strlen( path_string )] = '\0';
	return c;
}

/**
 * @brief Creates a new replica struct.
 * @param n Number of replicas.
 * @return Pointer to the replica struct.
 */
struct mlt_replica* mlt_create_new_replica(int n)
{
	struct mlt_replica* r = (struct mlt_replica*) malloc( sizeof(struct mlt_replica) );
	if(r == NULL)
		return NULL;

	r->replica_list = (struct asyn_tcp_server**) malloc( sizeof(struct asyn_tcp_server*) * n );

	r->n = n;
	return r;
}

/**
 * @brief Destroy replica.
 * @param r Pointer to the replica.
 */
void mlt_destroy_replica(struct mlt_replica* r)
{
	free( r->replica_list );
	free( r );
}

/**
 * @brief Destroy entry.
 * @param e Pointer to the entry.
 */
void mlt_destroy_entry(struct mlt_entry* e)
{
	// Destroy replica
	if ( e->replica != NULL )
		mlt_destroy_replica( e->replica );

	int i;
	for ( i = 0; i < e->tokens; i++ )
	{
		free( e->tokens_arr[i] );
	}

	free( e->tokens_arr );
	free( e->path );
	free( e );
}

/**
 * @brief Split path string into tokens.
 * @param n Pointer to the number of tokens.
 * @param path Pointer to the string path.
 * @return Pointer to the destination string array where the tokens are stored.
 */
char** mlt_split_path(int* n, const char* path)
{
	if ( strlen( path ) < 2 )
	{
		*n = 0;
		return NULL;
	}
	int counter;
	char** tokens_arr;
	int tokens;
	char* temp;
	// allocate some memory for the copy of the string path
	char* str = (char*) malloc( sizeof(char) * (strlen( path ) + 1) );
	if( str == NULL)
		return NULL;

	// Create a copy of string path
	memcpy( str, path, sizeof(char) * strlen( path ) );
	str[strlen( path )] = '\0';

	// Get the number of delemiters
	tokens = mlt_count_delimiters( path, '/' );
	*n = tokens;

	// allocate some memory for the tokens (array of strings)
	tokens_arr = (char**) malloc( sizeof(char*) * tokens );

	if(tokens_arr == NULL)
	{
		free(str);
		return NULL;
	}
	// Split the string.
	temp = strtok( str, "/" );

	// Allocate memory and copy the token into the array
	tokens_arr[0] = (char*) malloc( sizeof(char) * (strlen( temp ) + 1) );
	memcpy( tokens_arr[0], temp, sizeof(char) * strlen( temp ) );
	tokens_arr[0][strlen( temp )] = '\0';

	counter = 1;
	while ( counter < tokens )
	{
		temp = strtok( NULL, "/" );
		tokens_arr[counter] = (char*) malloc( sizeof(char) * (strlen( temp ) + 1) );
		memcpy( tokens_arr[counter], temp, sizeof(char) * strlen( temp ) );
		tokens_arr[counter][strlen( temp )] = '\0';

		counter++;
	}
	free( str );
	return tokens_arr;
}

/**
 * @brief Count the number of delimiters in a path.
 * @param path Pointer to the string path.
 * @param c Delimiter.
 * @return Number of delimeters.
 */
int mlt_count_delimiters(const char * path, char c)
{
	char* pch;
	int counter = 0;
	pch = strchr( (char*) path, c );
	while ( pch != NULL )
	{
		counter++;
		pch = strchr( pch + 1, c );
	}
	return counter;
}

/**
 *	@brief Traverses the path recursively and find the corresponding entry to the tokens array.
 *	@param e Pointer to the the entry to traverse.
 *	@param tokens_arr Pointer to the token array.
 *	@param token Number of tokens in the token array.
 *	@return The corresponding entry to the tokens array.
 */
struct mlt_entry* mlt_traverse_path(struct mlt_entry* e, char** tokens_arr, int tokens)
{

	if(e == NULL || tokens_arr == NULL)
		return NULL;
	// If number of tokens are smaller than the number of tokens of e
	if ( tokens < e->tokens )
	{
		//	traverse sibling, otherwise
		//parent of e is also parent of the searched path.
		if ( e->sibling != NULL )
			return mlt_traverse_path( e->sibling, tokens_arr, tokens );
		else
			return e->parent;
	}
	// If tokens of tokens_arr and entry e are equal, e maybe parent.
	else if ( mlt_compare_tokens( (const char**) e->tokens_arr, e->tokens, (const char**) tokens_arr, tokens ) == 0 )
	{
		// If e has no child than e is the parent.
		if ( e->child == NULL )
			return e;
		// Otherwise traverse layer deeper.
		else
			return mlt_traverse_path( e->child, tokens_arr, tokens );
	}
	// Tokens are not equal.
	else
	// If e has no sibling, so e is ancestor, so parent of e is also the corrosponding parent.
	if ( e->sibling == NULL )
		return e->parent;
	// Otherwise traverse the next sibling.
	else
		return mlt_traverse_path( e->sibling, tokens_arr, tokens );
}

/**
 * @brief Compare two tokens arrays from 0 to l_1 or l_2.
 * @param t_1 Pointer to the first token array.
 * @param l_1 Length of the t_1 token array.
 * @param t_2 Pointer to the second token array.
 * @param l_2 Length of the t_2 token array.
 * @return 0 if tokens equal; otherwise -1
 */
int mlt_compare_tokens(const char** t_1, int l_1, const char** t_2, int l_2)
{
	int i = 0;

	if ( l_1 < l_2 )
	{
		while ( i < l_1 )
		{
			if ( strcmp( t_1[i], t_2[i] ) != 0 )
				return -1;
			i++;
		}
	}
	else
		while ( i < l_2 )
		{
			if ( strcmp( t_1[i], t_2[i] ) != 0 )
				return -1;
			i++;
		}

	return 0;
}

/**
 * @brief Destroys the mlt.
 * @param list Pointer to the mlt.
 */
void mlt_destroy_mlt(struct mlt* list)
{
	int i;

	while ( list->root->child != NULL )
	{
		mlt_remove_entry( list, list->root->child );
	}
	mlt_destroy_entry( list->root );

	for ( i = 0; i < list->server_list->n; i++ )
	{
		if ( list->server_list->servers[i] != NULL )
			mlt_destroy_server( list->server_list->servers[i] );
	}
	free( list->server_list->servers );
	free( list->server_list );
	free( list->entry_list );
	free( list );
}

/**
 * @brief Creates a new server.
 * @param address Server address.
 * @param port Port.
 * @return Pointer to the created server structure.
 */
struct asyn_tcp_server* mlt_create_server(const char* address, unsigned short port)
{
	struct asyn_tcp_server* s = (struct asyn_tcp_server*) malloc( sizeof(struct asyn_tcp_server) );

	if(s == NULL)
		return NULL;

	s->address = (char*) malloc( sizeof(char) * strlen( address ) + 1 );
	s->port = port;

	// Create a copy of the address string
	memcpy( s->address, address, sizeof(char) * strlen( address ) );
	s->address[strlen( address )] = '\0';
	return s;
}

/**
 * @brief Destroys the server structure.
 * @param s Pointer to the server.
 */
void mlt_destroy_server(struct asyn_tcp_server* s)
{
	free( s->address );
	free( s );
}

/**
 * @brief Find the corresponding server to the address and port in the server list.
 * @param list Pointer to the mlt.
 * @param address Server address.
 * @param port Port number.
 * @return The corresponding; NULL if the server does not exists in the server list
 */
struct asyn_tcp_server* mlt_find_server(const struct mlt* list, const char* address, unsigned short port)
{
	int i;
	for ( i = 0; i < list->server_list->n; i++ )
	{
		if ( list->server_list->servers[i] != NULL && strcmp( list->server_list->servers[i]->address, address ) == 0 )
			if ( list->server_list->servers[i]->port == port )
				return list->server_list->servers[i];
	}

	return NULL;
}

/**
 * @brief Resize the mlt entry list by allocating more memory.
 * @param list Pointer to the mlt.
 * @return 0 if resizing was successful; otherwise -1.
 */
int mlt_resize_entry_list(struct mlt* list)
{
	struct mlt_entry** temp_list;
	int next_pos = list->list_size;
	list->list_size = list->list_size * 2;
	temp_list = (struct mlt_entry**) realloc( list->entry_list, sizeof(struct mlt_entry*) * list->list_size );

	if ( temp_list != NULL )
		list->entry_list = temp_list;
	else
		return -1;

	int i;
	for ( i = next_pos; i < list->list_size; i++ )
	{
		list->entry_list[i] = NULL;
	}

	return 0;
}

/**
 * @brief Creates a new server list.
 * @param list_size The size of the list.
 * @return Pointer to the server list.
 */
struct server_list* mlt_create_server_list(int list_size)
{

	struct server_list* s_list = (struct server_list*) malloc( sizeof(struct server_list) );

	if( s_list == NULL)
		return NULL;

	struct asyn_tcp_server** servers = (struct asyn_tcp_server**) malloc( sizeof(struct asyn_tcp_server*) * list_size );

	if( servers == NULL)
	{
		free(s_list);
		return NULL;
	}

	s_list->servers = servers;
	s_list->n = list_size;

	int i;
	for ( i = 0; i < list_size; i++ )
		s_list->servers[i] = NULL;

	return s_list;
}

/**
 * @brief Resize the mlt server list by duplicating the memory space.
 * @param list Pointer to the mlt.
 * @return 0 if resizing was successful; otherwise -1.
 */
int mlt_resize_server_list(struct mlt* list)
{
	struct asyn_tcp_server** temp_list;
	int next_pos = list->server_list->n;
	list->server_list->n = list->server_list->n * 2;

	temp_list = (struct asyn_tcp_server**) realloc( list->server_list->servers, sizeof(struct asyn_tcp_server*) * list->server_list->n );

	if ( temp_list != NULL )
		list->server_list->servers = temp_list;
	else
		return -1;

	int i;
	for ( i = next_pos; i < list->server_list->n; i++ )
		list->server_list->servers[i] = NULL;
	return 0;
}

/** 
 * @brief travers the mlt tree and return the next entry
 * @param[in] p_entry mlt entry to start the lookup
 * @return mlt entry pointer representing the next mlt entry
 * */
struct mlt_entry *get_upwards_successor(struct mlt_entry *p_entry)
{
	if ( p_entry == NULL )
	{
		return NULL;
	}
	else
	{
		if ( p_entry->sibling != NULL )
		{
			return p_entry->sibling;
		}
		else if ( p_entry->parent != NULL )
		{
			return get_upwards_successor( p_entry->parent );
		}
	}
	return NULL;
}

/** 
 * @brief travers the mlt tree and return the next entry
 * @param[in] p_entry mlt entry to start the lookup
 * @return mlt entry pointer representing the next mlt entry
 * */
struct mlt_entry *get_successor(struct mlt_entry *p_entry)
{
	if ( p_entry == NULL )
	{
		return NULL;
	}
	if ( p_entry->child != NULL )
	{
		return p_entry->child;
	}
	else if ( p_entry->sibling != NULL )
	{
		return p_entry->sibling;
	}
	else if ( p_entry->parent != NULL )
	{
		return get_upwards_successor( p_entry->parent );
	}
	else
	{
		return NULL;
	}
}

