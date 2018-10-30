#include "mm/storage/ChangeOwnershipDAOAdapter.h"

PartitionManager* ChangeOwnershipDAOAdapter::partition_manager = NULL;

ChangeOwnershipDAOAdapter::~ChangeOwnershipDAOAdapter()
{
}

ChangeOwnershipDAOAdapter::ChangeOwnershipDAOAdapter(PartitionManager *pm)
{
	partition_manager = pm;
	set_module(DAO_MetaData);
}

bool ChangeOwnershipDAOAdapter::is_coordinator(DAOperation* uncomplete_operation)
{
	ChangeOwnershipOperation *operation;
	operation = (ChangeOwnershipOperation *) uncomplete_operation->operation;

	if(strcmp(operation->target, partition_manager->get_host_identifier()) == 0)
		return true;
	else
		return false;
}

int ChangeOwnershipDAOAdapter::set_sending_addresses(DAOperation* uncomplete_operation)
{
	ChangeOwnershipOperation *operation = (ChangeOwnershipOperation *) uncomplete_operation->operation;

	Subtree initiator;
	initiator.server = operation->target;
	initiator.subtree_entry = 1;
	uncomplete_operation->participants.push_back(initiator);

	Subtree participant;
	participant.server = operation->source;
	participant.subtree_entry = 1;
	uncomplete_operation->participants.push_back(participant);

	return 0;
}

int ChangeOwnershipDAOAdapter::set_subtree_entry_point(DAOperation* uncomplete_operation)
{
  return 0;
}


Subtree ChangeOwnershipDAOAdapter::get_next_participant(void* operation, uint32_t operation_length)
{
  Subtree result;
  result.server = "";
  return result;
}

void ChangeOwnershipDAOAdapter::handle_operation_result(uint64_t id, bool success)
{
}

bool ChangeOwnershipDAOAdapter::handle_operation_request(uint64_t id, void* operation_description, uint32_t operation_length)
{
	ChangeOwnershipOperation *operation = (ChangeOwnershipOperation *) operation_description;
	if(strcmp(this->partition_manager->get_host_identifier(), operation->source))
	{
		try
		{
			Partition *partition = this->partition_manager->get_free_owned_partition();
			partition->set_owner(operation->target);
			strncpy(operation->partition_identifier, partition->get_identifier(), MAX_NAME_LEN);
		}
		catch(StorageException)
		{
			return false;
		}
	}
	return true;
}

bool ChangeOwnershipDAOAdapter::handle_operation_rerequest(uint64_t id, void* operation_description, uint32_t operation_length)
{
  return true;
}

bool ChangeOwnershipDAOAdapter::handle_operation_undo_request(uint64_t id, void* operation_description, uint32_t operation_length)
{
  return true;
}

bool ChangeOwnershipDAOAdapter::handle_operation_reundo_request(uint64_t id, void* operation_description, uint32_t operation_length)
{
  return true;
}
