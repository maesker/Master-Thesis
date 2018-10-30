/**
 * @file MltHandler.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 */

#ifndef MLTHANDLER_H_
#define MLTHANDLER_H_

#define DEFAULT_SERVER_NUMBER 4

#include <mlt/mlt.h>
#include <mlt/mlt_file_provider.h>
#include <string>
#include <vector>

#include "CommonCoodinationTypes.h"
#include "MltHandlerException.h"

using namespace std;

class MltHandler
{
public:

	virtual ~MltHandler();

	bool path_to_root(string& path, InodeNumber& root_number, string& relative_path);

	int init_new_mlt(const string& server_address, unsigned short server_port, int export_id, InodeNumber root_inode);

	int read_from_file(const string& mlt_file);

	int write_to_file(const string& mlt_file);

	void destroy_mlt();

	int reload_mlt(const string& mlt_path);

	int add_server(const string& server_address, unsigned short server_port);

	int remove_server(const string& server_address, unsigned short server_port);

	int get_server_list(vector<Server>& server_list);
	int get_other_servers(vector<Server>& server_list);

	int set_my_address(const Server& address);
	const Server get_my_address() const;
	int get_my_rank();

	bool is_partition_root(const InodeNumber inode_id) const;
	
	int get_my_partitions(vector<InodeNumber>& partitions) const;

	InodeNumber get_parent(InodeNumber inode_id) const;
	int get_children(InodeNumber parent_id, vector<InodeNumber>& children);
	
    /* @brief Returns an instance of "MltHandler"
     *
     * This method is needed to get a reference to the singleton instance.
     */
    static MltHandler& get_instance()
    {
    	if(instance == NULL)
    	{
    		instance = new MltHandler();
    	}
        return *instance;
    }

  int handle_add_new_entry(const Server& mds, ExportId_t export_id, InodeNumber root_inode, const char* path_string);
  
  int handle_remove_entry(InodeNumber root_id);

  ExportId_t get_export_id(InodeNumber root_id);
  int get_version(InodeNumber root_id);
  string get_path(InodeNumber root_id);
  Server get_mds(InodeNumber root_id);

  int update_entry(InodeNumber root_id, const Server& mds); 
  mlt* get_mlt();


private:

	MltHandler();

	mlt* m_lookup_table;

    static MltHandler* instance;	    /* < Singleton instance of MltHandler. */

    Server my_address;

    mutable pthread_mutex_t mutex; /* < mutex */

    /**
     * @brief Copy constructor of MltHandler
     *
     * @param server - MltHandler whose values are copied into the new object
     *
     * Private copy constructor to ensure that no other class can use it. It is not used inside the class to ensure that only one single
     * instance of the class is available.
     */
	MltHandler(const MltHandler& mlt_handler);
  
  struct asyn_tcp_server mlt_server_converter(const Server *serv);
  mlt_entry* get_entry(InodeNumber root_id);
};

#endif /* MLTHANDLER_H_ */
