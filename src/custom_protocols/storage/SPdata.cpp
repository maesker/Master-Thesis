#include "custom_protocols/storage/SPdata.h"

void print_SPHead(struct SPHead *p_head, stringstream& ss)
{
    printf("printsphead:inum=%llu.\n",p_head->ophead.inum);
    print_custom_protocol_reqhead((struct custom_protocol_reqhead_t*)p_head, ss);
    print_operation((operation*)&p_head->ophead, ss);
}

void print_SPN_Message(SPN_message *p_msg, stringstream& ss)
{
    ss << "\n--- Printing SPN_Message ---\n";
    struct SPHead *p_head = (struct SPHead*) p_msg;
    print_SPHead(p_head,ss);
}

void* spn_ioservicerun(void *ios)
{
    boost::asio::io_service *io_service = (boost::asio::io_service *) ios;
    io_service->run();
}
