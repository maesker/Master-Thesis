#include "custom_protocols/cluster/CCCdata.h"

void print_CCCHead(struct CCCHead *p_head, std::stringstream& ss)
{
    //printf("printsphead:inum=%llu.\n",p_head->ophead.inum);
    print_custom_protocol_reqhead((struct custom_protocol_reqhead_t*)p_head, ss);
    ss << "Receiver:" << p_head->receiver << "\n";
    //print_operation((operation*)&p_head->ophead, ss);
}

void print_CCC_Message(CCC_message *p_msg, std::stringstream& ss)
{
    ss << "\n--- Printing CCC_Message ---\n";
    struct CCCHead *p_head = (struct CCCHead*) p_msg;
    print_CCCHead(p_head,ss);
}

