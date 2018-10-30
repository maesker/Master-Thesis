/**
 * \file MltHandler.cpp
 *
 * \author Marie-Christine Jakobs	Benjamin Raddatz
 *
 * Emulates the MltHandler for test purposes.
 */

#include "coco/coordination/MltHandler.h"

MltHandler* MltHandler::instance =NULL;

MltHandler::MltHandler()
{
    my_address.address = "127.0.0.1";
    my_address.port = 49152;
}

int MltHandler::init_new_mlt(const string& server_address, unsigned short server_port, int export_id, InodeNumber root_inode)
{
    return 0;
}

int MltHandler::read_from_file(const string& mlt_file)
{
    return 0;
}


int MltHandler::write_to_file(const string& mlt_file)
{
    return 0;
}

void MltHandler::destroy_mlt()
{
}

int MltHandler::add_server(const string& server_address, unsigned short server_port)
{
    return 0;
}

int MltHandler::remove_server(const string& server_address, unsigned short server_port)
{
    return 0;
}

int MltHandler::get_server_list(vector<Server>& server_list)
{
    Server x;
    x.address = "127.0.0.1";
    x.port = 49152;
    server_list.push_back(x);
    return 0;
}

int MltHandler::set_my_address(const Server& address)
{
    return 0;
}

const Server MltHandler::get_my_address() const
{
    return my_address;
}

bool MltHandler::is_partition_root(const InodeNumber inode_id) const
{
    return true;
}

int MltHandler::get_my_partitions(vector<InodeNumber>& partitions) const
{
    return 0;
}

int MltHandler::handle_add_new_entry(const Server& mds, ExportId_t export_id, InodeNumber root_inode, const char* path_string)
{
    return 0;
}

int MltHandler::handle_remove_entry(InodeNumber root_id)
{
    return 0;
}

ExportId_t MltHandler::get_export_id(InodeNumber root_id)
{
    ExportId_t id;
    return id;
}

int MltHandler::get_version(InodeNumber root_id)
{
    return 0;
}

string MltHandler::get_path(InodeNumber root_id)
{
    return string();
}

Server MltHandler::get_mds(InodeNumber root_id)
{
    Server s;
    return s;
}

int MltHandler::update_entry(InodeNumber root_id, const Server& mds)
{
    return 0;
}

struct server MltHandler::mlt_server_converter(const Server *serv)
{
    struct server s;
    return s;
}

mlt_entry* MltHandler::get_entry(InodeNumber root_id)
{
    return NULL;
}

MltHandler::~MltHandler()
{
}

