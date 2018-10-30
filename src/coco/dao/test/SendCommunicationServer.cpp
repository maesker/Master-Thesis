/**
 * \file SendCommunicationServer.cpp
 *
 * \author Marie-Christine Jakobs
 *
 * Emulates the SendCommunicationServer for test purposes.
 */

#include "coco/communication/SendCommunicationServer.h"

SendCommunicationServer SendCommunicationServer::instance = SendCommunicationServer();

int SendCommunicationServer::asynch_send(void* message, int message_length, CommunicationModule sending_module, CommunicationModule receiving_module, vector<string>& server_list)
{
    return 0;
}

RequestResult SendCommunicationServer::ganesha_send(void* message, int message_length)
{
    RequestResult result;
    return result;
}


int SendCommunicationServer::add_server_send_socket(string ip_address)
{
    // TODO change for testing?
    return 0;
}

int SendCommunicationServer::delete_server_send_socket(string ip_address)
{
    return 0;
}

int SendCommunicationServer::set_up_external_sockets()
{
}


SendCommunicationServer::SendCommunicationServer()
{
    //TODO change for testing?
}

SendCommunicationServer::~SendCommunicationServer()
{
    //TODO change for testing?
}

int SendCommunicationServer::connect()
{
    return 0;
}

int deconnect()
{
    return 0;
}

int initialize_ganesha_communication()
{
    return 0;
}

int delete_ganesha_communication()
{
    return 0;
}