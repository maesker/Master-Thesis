#include "custom_protocols/global_protocol_data.h"

void print_custom_protocol_reqhead(struct custom_protocol_reqhead_t *p, std::stringstream& ss)
{
    ss << "Protocolid:" << p->protocol_id << "\n";
    ss << "Msgtype:" << p->msg_type << "\n";
    ss << "Sequencenum:" << p->sequence_number << "\n";
}

void print_custom_protocol_resphead(struct custom_protocol_resphead_t *p, std::stringstream& ss)
{
    ss << "-------------\nProtocolid:" << p->protocol_id << "\nMsgtype:" << p->msg_type << 
            ".\nSequencenum:" << p->sequence_number << ".\nError.major:" << p->error.major_id <<
            ".\nError.minor:" << p->error.minor_id << ".\n-------------\n";
}

void free_custom_protocol_reqhead_t(custom_protocol_reqhead_t *p)
{
    if (p->ip!=NULL)
    {
        free(p->ip);
    }
    if (p->datablock!=NULL)
    {
        free(p->datablock);
    }
    free(p);
}