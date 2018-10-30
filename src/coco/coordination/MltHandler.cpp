/**
 * @file MltHandler.cpp
 * @class MltHandler
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 *
 * @brief Frontend to access the mlt functions
 **/

#include <iostream>
#include <string.h>

#include "coco/coordination/CoordinationFailureCodes.h"
#include "coco/coordination/MltHandler.h"


/**
 * Declaration and definition of class variable instance which represents the singleton instance.
 */
MltHandler *MltHandler::instance = NULL;

/**
 * @brief Default constructor of MltHandler class.
 */
MltHandler::MltHandler()
{
	m_lookup_table = NULL;
	mutex = PTHREAD_MUTEX_INITIALIZER;
}

/**
 * @brief Destructor of MltHandler class.
 */
MltHandler::~MltHandler()
{
	if( m_lookup_table != NULL )
	{
		mlt_destroy_mlt( m_lookup_table );
		m_lookup_table = NULL;
	}
	instance = NULL;
	pthread_mutex_destroy( &mutex );
}

/**
 * @brief Gets the root inode number to a path.
 * @param[in] path The path to an inode.
 * @param[out] root_number The root inode number of the path, the root_inode number will
 * be set to INVALID_INODE_ID if the path cannot be resolved.
 * @param relative_path The relative path from the root inode number.
 * @return true if the path shows to the root inode number, otherwise false.
 * @throws MltHandlerException if the root inode number of the path is unknown.
 */
bool MltHandler::path_to_root(string& path, InodeNumber& root_number, string& relative_path)
{
	bool rtrn = false;
	mlt_entry* entry = NULL;
	root_number = INVALID_INODE_ID;

	pthread_mutex_lock( &mutex );

	if ( m_lookup_table != NULL )
	{
		entry = mlt_find_subtree(m_lookup_table, path.c_str());

		if(entry != NULL)
		{
			root_number = entry->root_inode;

			if(path.compare(entry->path) == 0)
			{
				relative_path = path;
				rtrn = true;
			}
			else
			{
				if ( strlen( entry->path ) == 1 )
				{
					relative_path = path;
				}
				else
				{
					relative_path = path.substr( strlen( entry->path ), path.length() );
				}
			}


		}
		else
		{
			throw MltHandlerException("Unable to determine the root einode for a given path.");
		}
	}

	pthread_mutex_unlock( &mutex );

	return rtrn;
}

/**
 * @brief Gets all servers expect of the own server address.
 * @param[out] server_list Vector that will contain the servers.
 * @return 0 if the operation was successful. Otherwise an error code is returned.
 * MltNotExists: The mlt handler does not provides any mlt.
 */
int MltHandler::get_server_list(vector<Server>& server_list)
{
	int rtrn = 0;
	pthread_mutex_lock( &mutex );

	if ( m_lookup_table == NULL )
		rtrn = MltNotExists;

	for ( int i = 0; i < m_lookup_table->server_list->n; i++ )
		if ( m_lookup_table->server_list->servers[i] != NULL )
		{
			Server s;
			s.address = string( m_lookup_table->server_list->servers[i]->address );
			s.port = m_lookup_table->server_list->servers[i]->port;
			server_list.push_back( s );
		}

	pthread_mutex_unlock( &mutex );

	return rtrn;
}


/**
 * @brief Gets all servers .
 * @param[out] server_list Vector that will contain the servers.
 * @return 0 if the operation was successful. Otherwise an error code is returned.
 * MltNotExists: The mlt handler does not provides any mlt.
 */
int MltHandler::get_other_servers(vector<Server>& server_list)
{
	cout << "MltHandler::get_other_servers()" << endl;
	cout << "MyAddress is: " << my_address.address << endl;
	int rtrn = 0;
	pthread_mutex_lock( &mutex );

	if ( m_lookup_table == NULL )
		rtrn = MltNotExists;

	for ( int i = 0; i < m_lookup_table->server_list->n; i++ )


		if ( m_lookup_table->server_list->servers[i] != NULL )
		{
			Server s;
			s.address = string( m_lookup_table->server_list->servers[i]->address );
			s.port = m_lookup_table->server_list->servers[i]->port;

			if(s.address.compare(my_address.address) != 0)
			{
				cout << "Other server: " << s.address << endl;
				server_list.push_back( s );
			}
		}

	pthread_mutex_unlock( &mutex );

	return rtrn;
}




/**
 * @brief Adds a new server to the mlt.
 * @param server_address The address of the root server.
 * @param server_port The port of the root server.
 * @return 0 if adding was successful, otherwise an error code is returned.
 * MltNotExists: The mlt handler does not provides any mlt.
 * ServerAlreadyExists: The given server already exists.
 * ReallocationError: An error occurred during resizing the server list,
 * the server was not added to the list, no changes on the mlt are processed.
 */
int MltHandler::add_server(const string& server_address, unsigned short server_port)
{
	int rtrn = 0;
	int i = 0;
	bool found = false;
	asyn_tcp_server* mds = NULL;

	pthread_mutex_lock( &mutex );

	if ( m_lookup_table == NULL )
		rtrn = MltNotExists;
	// check if the server already exists
	else if ( mlt_find_server( m_lookup_table, server_address.c_str(), server_port ) != NULL )
	{
		rtrn = ServerAlreadyExists;
	}
	else
	{
		// create a new server
		mds = mlt_create_server( server_address.c_str(), server_port );
		if ( mds == NULL )
			rtrn = AllocationError;
		else
		{

			// find the next position in the server list
			while ( i < m_lookup_table->server_list->n && !found )
			{
				if ( m_lookup_table->server_list->servers[i] == NULL )
				{
					m_lookup_table->server_list->servers[i] = mds;
					found = true;
				}
				i++;
			}

			if ( !found )
			{
				// no free slot in the list => resize it
				int pos = m_lookup_table->server_list->n;
				if ( mlt_resize_server_list( m_lookup_table ) != 0 )
				{
					mlt_destroy_server( mds );
					rtrn = ReallocationError;
				}
				else
					m_lookup_table->server_list->servers[pos] = mds;
			}
		}
	}

	pthread_mutex_unlock( &mutex );
	return rtrn;
}

/**
 * @brief Removes server from the mlt.
 * @param server_address The address of the root server.
 * @param server_port The port of the root server.
 * @return 0 if adding was successful, otherwise an error code is returned.
 * MltNotExists: The mlt handler does not provides any mlt.
 * ServerAlreadyExists: The given server does not exists.
 */
int MltHandler::remove_server(const string& server_address, unsigned short server_port)
{
	int rtrn = ServerNotExists;
	pthread_mutex_lock( &mutex );

	if ( m_lookup_table == NULL )
		rtrn = MltNotExists;
	else
	{
		for ( int i = 0; i < m_lookup_table->server_list->n; i++ )
		{
			if ( m_lookup_table->server_list->servers[i] != NULL
					&& server_address.compare( m_lookup_table->server_list->servers[i]->address ) == 0
					&& m_lookup_table->server_list->servers[i]->port == server_port )
			{
				mlt_destroy_server( m_lookup_table->server_list->servers[i] );
				m_lookup_table->server_list->servers[i] = NULL;
				rtrn = 0;
				break;
			}
		}
	}
	pthread_mutex_unlock( &mutex );
	return rtrn;
}
/**
 * @brief Initialize a new metadata lookup table.
 * @param server_address The address of the root server.
 * @param server_port The port of the root server.
 * @param export_id Export id.
 * @return 0 if reading was successful, otherwise an error code is returned.
 * MltExists: An existing mlt is currently provided by the mlt handler.
 */
int MltHandler::init_new_mlt(const string& server_address, unsigned short server_port, int export_id,
		InodeNumber root_inode)
{
	int rtrn = 0;
	asyn_tcp_server* mds = NULL;

	pthread_mutex_lock( &mutex );

	// Check if an mlt already exists.
	if ( m_lookup_table != NULL )
		rtrn = MltExists;
	else
	{
		// create root md server
		mds = mlt_create_server( server_address.c_str(), server_port );

		if ( mds == NULL )
			rtrn = AllocationError;
		else
		{
			// create new mlt
			m_lookup_table = mlt_create_new_mlt( mds, export_id, root_inode );

			// create server list
			m_lookup_table->server_list = mlt_create_server_list( DEFAULT_SERVER_NUMBER );

			// set server
			m_lookup_table->server_list->servers[0] = mds;
		}
	}
	pthread_mutex_unlock( &mutex );

	return rtrn;
}

/**
 * @brief Read an existing mlt form the file storage.
 * @param mlt_file File path to the mlt.
 * @return 0 if reading was successful, otherwise an error code is returned.
 * MltExists: An existing mlt is currently provided by the mlt handler.
 * UnableToReadMlt: The read process failed.
 */
int MltHandler::read_from_file(const string& mlt_file)
{
	int rtrn = 0;

	pthread_mutex_lock( &mutex );

	// Check if an mlt already exists.
	if ( m_lookup_table != NULL )
		rtrn = MltExists;
	else
	{
		m_lookup_table = read_mlt( mlt_file.c_str() );
		if ( m_lookup_table == NULL )
			rtrn = UnableToReadMlt;
	}

	pthread_mutex_unlock( &mutex );

	return rtrn;
}

/**
 * @brief Writes the mlt to the file storage.
 * @param mlt_file File path to the mlt.
 * @return 0 if writing was successful, otherwise an error code is returned.
 * MltNotExists: The mlt handler does not provides any mlt.
 */
int MltHandler::write_to_file(const string& mlt_file)
{
	int rtrn = 0;

	pthread_mutex_lock( &mutex );

	// Check if an mlt exists.
	if ( m_lookup_table == NULL )
		rtrn = MltNotExists;
	else
	{
		if ( write_mlt( m_lookup_table, mlt_file.c_str() ) != 0 )
			rtrn = UnableToWriteMlt;
	}
	pthread_mutex_unlock( &mutex );

	return rtrn;
}

/**
 * @brief Destroy the mlt.
 */
void MltHandler::destroy_mlt()
{
	pthread_mutex_lock( &mutex );

	if ( m_lookup_table != NULL )
	{
		mlt_destroy_mlt( m_lookup_table );
		m_lookup_table = NULL;
	}

	pthread_mutex_unlock( &mutex );
}

/**
 * Reloads the mlt.
 * @param mlt_path The path to the mlt file.
 * @return 0 if the reloading of the mlt was successful, otherwise an error code is returned.
 * UnableToReadMlt Unable to read the mlt from file.
 */
int MltHandler::reload_mlt(const string& mlt_path)
{

	int rtrn = 0;

	pthread_mutex_lock( &mutex );

	if ( m_lookup_table != NULL )
	{
		mlt_destroy_mlt( m_lookup_table );
		m_lookup_table = NULL;
	}
	m_lookup_table = read_mlt( mlt_path.c_str() );
	if ( m_lookup_table == NULL )
		rtrn = UnableToReadMlt;

	pthread_mutex_unlock( &mutex );

	return rtrn;
}


/**
 * @brief Sets the address of the server.
 * @param address The server address.
 * @return 0 if the operation was successful, otherwise an error code is returned.
 * MltNotExists: The mlt handler does not provides any mlt.
 */
int MltHandler::set_my_address(const Server& address)
{
	int rtrn = 0;
	pthread_mutex_lock( &mutex );
	if ( m_lookup_table == NULL )
		rtrn = MltNotExists;
	else
		my_address = address;
	pthread_mutex_unlock( &mutex );

	return rtrn;
}

/**
 * @brief Get server rank
 * @return rank
 */
int MltHandler::get_my_rank()
{
	int rank = 0;
	Server s;
	vector<Server> server_list;

	this->get_server_list(server_list);
	s = this->get_my_address();

	for (vector<Server>::iterator it = server_list.begin();
			it != server_list.end(); it++)
			{
		if (it->address != s.address)
			rank++;
		else
			break;
	}

	return rank;
}

/**
 * @brief Gets all inode ids of the partitions that are managed by this server.
 * @param[out] partitions A vector where the inode ids will be written.
 * @return 0 if the operation was successful, otherwise an error code is returned.
 * MltNotExists: The mlt handler does not provides any mlt.
 */
int MltHandler::get_my_partitions(vector<InodeNumber>& partitions) const
{
	int rtrn = 0;

	pthread_mutex_lock( &mutex );

	if ( m_lookup_table == NULL )
	{
		rtrn = MltNotExists;
	}
	else
	{
		for ( int i = 0; i < m_lookup_table->list_size; i++ )
		{
			if ( m_lookup_table->entry_list[i] != NULL )
			{
				// checks whether the entry server address is equal to our server address
				if ( my_address.address.compare( m_lookup_table->entry_list[i]->mds->address ) == 0
						&& my_address.port == m_lookup_table->entry_list[i]->mds->port )
					partitions.push_back( m_lookup_table->entry_list[i]->root_inode );
			}
		}
	}
	pthread_mutex_unlock( &mutex );
	return rtrn;
}

/**
 * @brief Checks whether the inode with the given inode id is a root of a partition.
 * @param inode_id The id of the inode to check.
 * @param true if the inode with the given inode id is a root of a partition, otherwise false.
 * @throws MltHandlerException: The mlt handler does not provides any mlt.
 */
bool MltHandler::is_partition_root(const InodeNumber inode_id) const
{
	bool rtrn = false;

	pthread_mutex_lock( &mutex );

	if ( m_lookup_table == NULL )
	{
		pthread_mutex_unlock( &mutex );
		throw MltHandlerException( "The mlt handler does not provide any mlt!" );
	}

	for ( int i = 0; i < m_lookup_table->list_size; i++ )
	{
		if ( m_lookup_table->entry_list[i] != NULL && m_lookup_table->entry_list[i]->root_inode == inode_id )
		{
			rtrn = true;
			break;
		}
	}

	pthread_mutex_unlock( &mutex );

	return rtrn;
}

struct asyn_tcp_server MltHandler::mlt_server_converter(const Server *serv)
{
	struct asyn_tcp_server s;
	s.address = (char *) serv->address.c_str();
	s.port = serv->port;
	return s;
}

/**
 * @brief Gets the address of the server.
 * @return The own server address.
 * @throws MltHandlerException: The mlt handler does not provides any mlt.
 */
const Server MltHandler::get_my_address() const
{
	pthread_mutex_lock( &mutex );
	if ( m_lookup_table == NULL )
	{
		pthread_mutex_unlock( &mutex );
		throw MltHandlerException( "The mlt handler does not provide any mlt!" );
	}
	Server s = my_address;
	pthread_mutex_unlock( &mutex );
	return s;
}

/**
 *	@brief Gets the parent inode number of an inode.
 *	@param inode_id The inode id of.
 *	@return The parent id if the inode is in the mlt, 0 if the inode is not in the mlt.
 *	@throws MltHandlerException if the inode with is the root entry of the file system.
 */
InodeNumber MltHandler::get_parent(InodeNumber inode_id) const
{
	InodeNumber parent = 0;
	pthread_mutex_lock( &mutex );

	for ( int i = 0; i < m_lookup_table->list_size; i++ )
	{
		mlt_entry* e = m_lookup_table->entry_list[i];
		if ( e != NULL && e->root_inode == inode_id )
		{
			if ( e->parent == NULL)
			{
				pthread_mutex_unlock( &mutex );
				throw MltHandlerException( "The inode seems to be the root of the FS!" );
			}
			parent = e->parent->root_inode;
			break;
		}
	}

	pthread_mutex_unlock( &mutex );

	return parent;
}

/**
 * @brief Gets all children of an inode entry.
 * @parem parent_id The id of the parent.
 * @param children[out] The buffer to write all inode numbers of the children.
 * @return 1 if the parent has some children, 0 if the parent do not have any children, -1
 * if the mlt do not provides any entry with the given parent id.
 */
int MltHandler::get_children(InodeNumber parent_id, vector<InodeNumber>& children)
{
	int rtrn = -1;
	pthread_mutex_lock( &mutex );

	for ( int i = 0; i < m_lookup_table->list_size; i++ )
	{
		mlt_entry* e = m_lookup_table->entry_list[i];

		if ( e != NULL && e->root_inode == parent_id )
		{
			// check if the inode has children
			if ( e->child != NULL)
			{
				rtrn = 1;
				children.push_back( e->child->root_inode );
				e = e->child->sibling;
				while ( e != NULL )
				{
					children.push_back( e->root_inode );
					e = e->sibling;
				}
			}

			// no children, leave loop
			else
			{
				rtrn = 0; // no children
				break;
			}
		}
	}

	pthread_mutex_unlock( &mutex );
	return rtrn;
}

/**
 * @brief Creates a new entry in the mlt.
 * @param mds The server that is responsible for the partition.
 * @param export_id TODO
 * @param root_inode The root inode number of the partition.
 * @param path_string The path of the partition.
 * @return 0 if the operation is successful, otherwise an error code is returned.
 * MltNotExists: The mlt handler does not provides any mlt.
 * ServerNotExists: The server is unkown.
 */
int MltHandler::handle_add_new_entry(const Server& mds, ExportId_t export_id, InodeNumber root_inode,
		const char* path_string)
{
	int rtrn = 0;
	mlt_entry *e;
	pthread_mutex_lock( &mutex );
	if ( m_lookup_table == NULL )
	{
		rtrn = MltNotExists;
	}
	else
	{
		e = mlt_find_subtree( m_lookup_table, path_string );

		if ( e == NULL)
		{
			pthread_mutex_unlock( &mutex );
			throw MltHandlerException( "Unexpected error occurred, the path was not related to any subtree!" );
		}

		asyn_tcp_server *s = mlt_find_server( m_lookup_table, mds.address.c_str(), mds.port );
		if ( s == NULL )
			rtrn = CoordinationFailureCodes::ServerNotExists;

		// if the given inode is already a root inode of a subtree, just update it
		if ( e->root_inode == root_inode )
		{
			e->mds = s;
			e->version++;
		}

		// otherwise create a new entry for the new subtree
		else
		{
			if ( mlt_add_new_entry( m_lookup_table, s, export_id, root_inode, path_string ) == NULL )
				rtrn = CoordinationFailureCodes::AllocationError;
		}
	}

	pthread_mutex_unlock( &mutex );
	return rtrn;
}

/**
 * @brief Gets the entry with the given root id from the list.
 * @param root_id The root id of the partition.
 * @return The pointer to the entry, if an entry with the given id is holding by the mlt, otherwise NULL.
 */
mlt_entry* MltHandler::get_entry(InodeNumber root_id)
{
	for ( int i = 0; i < m_lookup_table->list_size; i++ )
	{
		if ( m_lookup_table->entry_list[i] != NULL && m_lookup_table->entry_list[i]->root_inode == root_id )
		{
			return m_lookup_table->entry_list[i];
		}
	}
	return NULL;
}

/**
 * @brief Gets the export id of the partition with the given root id.
 * @param root_id The root id of the partition.
 * @return The export id.
 * @throws MltHandlerException: The mlt handler does not provides any mlt or if the entry with the root id doest not exists.
 */
ExportId_t MltHandler::get_export_id(InodeNumber root_id)
{
	ExportId_t id;
	pthread_mutex_lock( &mutex );

	if ( m_lookup_table == NULL )
	{
		pthread_mutex_unlock( &mutex );
		throw MltHandlerException( "The mlt handler does not provide any mlt!" );
	}

	mlt_entry* e = get_entry( root_id );
	if ( e == NULL)
	{
		pthread_mutex_unlock( &mutex );
		throw MltHandlerException( "Any entry with the given root does not exists." );
	}

	id = e->export_id;
	pthread_mutex_unlock( &mutex );
	return id;
}

/**
 * @brief Gets the version of the partition with the given root id.
 * @param root_id The root id of the partition.
 * @return The version of the partition.
 * @throws MltHandlerException: The mlt handler does not provides any mlt or if the entry with the root id doest not exists.
 */
int MltHandler::get_version(InodeNumber root_id)
{
	int version;
	pthread_mutex_lock( &mutex );

	if ( m_lookup_table == NULL )
	{
		pthread_mutex_unlock( &mutex );
		throw MltHandlerException( "The mlt handler does not provide any mlt!" );
	}

	mlt_entry* e = get_entry( root_id );
	if ( e == NULL)
	{
		pthread_mutex_unlock( &mutex );
		throw MltHandlerException( "Any entry with the given root does not exists." );
	}
	version = e->version;
	pthread_mutex_unlock( &mutex );
	return version;
}

/**
 * @brief Gets the path of the partition with the given root id.
 * @param root_id The root id of the partition.
 * @return The string path of the partition.
 * @throws MltHandlerException: The mlt handler does not provides any mlt or if the entry with the root id doest not exists.
 */
string MltHandler::get_path(InodeNumber root_id)
{
	string path;
	pthread_mutex_lock( &mutex );

	if ( m_lookup_table == NULL )
	{
		pthread_mutex_unlock( &mutex );
		throw MltHandlerException( "The mlt handler does not provide any mlt!" );
	}

	mlt_entry* e = get_entry( root_id );
	if ( e == NULL)
	{
		pthread_mutex_unlock( &mutex );
		throw MltHandlerException( "Any entry with the given root does not exists." );
	}

	path = string( e->path );
	pthread_mutex_unlock( &mutex );
	return path;
}

/**
 * @brief Gets the server address of the partition with the given root id.
 * @param root_id The root id of the partition.
 * @return The address of the server that is responsible for the the partition.
 * @throws MltHandlerException: The mlt handler does not provides any mlt.
 */
Server MltHandler::get_mds(InodeNumber root_id)
{
	Server s;
	mlt_entry* e = NULL;

	pthread_mutex_lock( &mutex );

	if ( m_lookup_table == NULL )
	{
		pthread_mutex_unlock( &mutex );
		throw MltHandlerException( "The mlt handler does not provide any mlt!" );
	}

	e = get_entry( root_id );
	if ( e == NULL)
	{
		pthread_mutex_unlock( &mutex );
		throw MltHandlerException( "Any entry with the given root does not exists." );
	}
	s.address = string( e->mds->address );
	s.port = e->mds->port;
	pthread_mutex_unlock( &mutex );

	return s;
}

/**
 * @brief Updates the responsible server of the partition with the given id.
 * @param root_id The root id of the partition.
 * @param mds The address of the server.
 * @return 0 if the operation was successful. -1 if the mlt does not contain any entry with the given root id.
 * MltNotExists: The mlt handler does not provides any mlt.
 * @throws MltHandlerException if the given server is unknown by the mlt.
 */
int MltHandler::update_entry(InodeNumber root_id, const Server& mds)
{
	int rtrn = 0;
	mlt_entry* e = NULL;
	asyn_tcp_server* mlt_mds;
	pthread_mutex_lock( &mutex );

	if ( m_lookup_table == NULL )
		rtrn = MltNotExists;
	else
	{
		e = get_entry( root_id );
		if ( e == NULL )
			rtrn = -1;
		else
		{
			mlt_mds = mlt_find_server( m_lookup_table, mds.address.c_str(), mds.port );
			if ( mlt_mds == NULL)
			{
				pthread_mutex_unlock( &mutex );
				throw MltHandlerException( "Unable to update the entry, because the given mds is unknown by the mlt!" );
			}
			e->mds = mlt_mds;
			e->version += 1;
		}
	}
	pthread_mutex_unlock( &mutex );

	return rtrn;
}

/**
 * @brief Removes the partition from the mlt with the given id.
 * @param root_id The root id of the partition.
 * @return 0 if the operation was successful. -1 if the mlt does not contain any entry with the given root id.
 * MltNotExists: The mlt handler does not provides any mlt.
 */
int MltHandler::handle_remove_entry(InodeNumber root_id)
{
	int rtrn = 0;
	mlt_entry* e;
	pthread_mutex_lock( &mutex );
	if ( m_lookup_table == NULL )
		rtrn = MltNotExists;
	else
	{
		e = get_entry( root_id );
		if ( e == NULL )
			rtrn = -1;
		else
			mlt_remove_entry( m_lookup_table, e );
	}
	pthread_mutex_unlock( &mutex );

	return rtrn;
}

/**
 * Please, never use it!
 */
mlt* MltHandler::get_mlt()
{
    return m_lookup_table;
}
