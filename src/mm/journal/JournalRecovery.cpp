/**
 * @file JournalRecovery.cpp
 * @class  JournalRecovery
 * @author Viktor Gottfried
 * @data 01.07.2011
 *
 * @brief Represents the journal recovery.
 *
 * The journal recovery rebuilds the last known state of the journals by reading the journal data (journal chunks).
 * Because of the journal fashion, the chunks are numbered in ascending order
 * and the entries inside follow also this fashion, the whole journal can be seen as a fifo queue.
 * If the recovery is performed, it reads all chunks, starting with the oldest one and reconstruct
 * the last state by adding the entries one by one.
 * As soon as the reconstruction is complete, the recovery runs the write back process once
 * to write back the data and clean up the unnecessary journal data.
 * Finally the state of the journals is set to recovered and all modules can now access the journal
 * logs to perform their own recovery
 * process, if necessary.
 */

#include <list>

#include "mm/journal/JournalRecovery.h"
#include "mm/journal/FailureCodes.h"
#include "mm/journal/CommonJournalTypes.h"
#include "mm/journal/StringHelper.h"

/**
 * @brief Constructor of JournalRecovery.
 * @parma sal Pointer to the @see StorageAbstractionLayer object.
 */
JournalRecovery::JournalRecovery(StorageAbstractionLayer* sal)
{
	this->sal = sal;
}

/**
 * @brief Destructor of JournalRecovery.
 */
JournalRecovery::~JournalRecovery()
{
}

/**
 * @brief Gets the @see StorageAbstractionLayer.
 * @Return Pointer to the @see StorageAbstractionLayer object.
 */
StorageAbstractionLayer& JournalRecovery::get_sal()
{
	return *sal;
}

/*
 * @brief Starts the recovery of the journal.
 * Tries to recover all journals on the system.
 * @param[out] journals A map that will contain the recovered journals.
 * @param partitions A vector of all partitions handling by the system.
 * @return The number of recoverd journals.
 */
int32_t JournalRecovery::start_recovery(map<InodeNumber, Journal*>& journals, const vector<InodeNumber>& partitions)
{
	read_all_chunks(partitions);

	set<uint64_t>::iterator sit;
	multimap<uint64_t, int32_t>::iterator mmit;
	pair<multimap<uint64_t, int32_t>::iterator, multimap<uint64_t, int32_t>::iterator> p;

	// first loop: for the journals
	for (sit = journal_set.begin(); sit != journal_set.end(); ++sit)
	{
		Journal* journal = new Journal(*sit);
		journal->set_journal_id(*sit);
		EmbeddedInodeLookUp *embedded_inode_lookup = new EmbeddedInodeLookUp(sal, *sit);
		journal->set_inode_io(embedded_inode_lookup);
		journal->set_sal(sal);
		journal->set_recovery(true);

		JournalCache* journal_cache = new JournalCache();

		p = journal_to_chunks.equal_range(*sit);

		// copy the chunks of the journal from the multimap to a set, because the multimap do not provide the desired order.
		set<int32_t> chunk_set;
		for (mmit = p.first; mmit != p.second; ++mmit)
		{
			chunk_set.insert(mmit->second);
		}

		/*
		 * To restore the journal we rebuild the journal cache by reading all chunks one by one.
		 * Because of the consecutive numeration of the chunks and the order inside the set,
		 * we reconstruct an equivalent state of the old journal.
		 * To restore the inode and operation cache, we do the same with each operation inside the cache.
		 * Inside the chunk, a vector contains all operations. Cause of the insertion order,
		 * we just need to iterate over the vector and the fill the caches.
		 * Finally the write back process can be started, and the recovery is completed.
		 */

		// second loop: for all chunks of a journal
		for (set<int32_t>::iterator cs_it = chunk_set.begin(); cs_it != chunk_set.end(); ++cs_it)
		{
			int32_t chunk_id = *cs_it;
			JournalChunk* jc = new JournalChunk(chunk_id, journal->get_prefix(), *sit);

			// read the chunk
			jc->read_chunk(*sal);
			jc->set_closed(true);
			// put it into the journal cache
			journal_cache->add(jc);

			vector<Operation*>* operations = &(jc->get_operations());
			vector<Operation*>::iterator ov_it;
			for (ov_it = operations->begin(); ov_it != operations->end(); ov_it++)
			{
				MoveData* data;
				Operation* op = *ov_it;
				EInode einode = op->get_einode();
				InodeNumber inode_number = op->get_inode_id();
				InodeNumber parent_id = op->get_parent_id();
				int32_t bitfield = op->get_bitfield();

				switch (op->get_type()) {
					case OperationType::CreateINode:
						journal->handle_mds_create_einode_request( &parent_id, &einode);
						break;
					case OperationType::DeleteINode:
						journal->handle_mds_delete_einode_request(&inode_number);
						break;
					case OperationType::SetAttribute:
						inode_update_attributes_t update_attr;
						update_attr.atime = op->get_einode().inode.atime;
						update_attr.gid = op->get_einode().inode.gid;
						update_attr.has_acl = op->get_einode().inode.has_acl;
						update_attr.mode = op->get_einode().inode.mode;
						update_attr.mtime = op->get_einode().inode.mtime;
						update_attr.size = op->get_einode().inode.size;
						update_attr.st_nlink = op->get_einode().inode.link_count;
						update_attr.uid = op->get_einode().inode.uid;
						journal->handle_mds_update_attributes_request(&inode_number,
								&update_attr, &bitfield);
						break;
					case OperationType::RenameObject:
						data = (MoveData*) op->get_data();
						journal->handle_mds_move_einode_request(&parent_id, &(einode.name), &(data->old_parent), &(data->old_name) );
						break;
					case OperationType::MoveInode:
						data = (MoveData*) op->get_data();
						journal->handle_mds_move_einode_request(&parent_id, &(einode.name), &(data->old_parent), &(data->old_name) );
						break;
					case OperationType::DistributedOp:
						journal->add_distributed(op->get_operation_id(), op->get_module(), op->get_type(), op->get_status(), op->get_data(), OPERATION_SIZE);
					default:
						break;
				}
			}

		}

		journal->set_journal_cache(journal_cache);
		journal->run_write_back();
		journal->set_recovery(false);
		journals.insert(pair<InodeNumber, Journal*> (*sit, journal));
	}

	return journals.size();
}

/**
 * qbrief Read all chunk files from the file storage and reconstruct journals and the cache.
 * A chunk file name consists of a prefix, that is determined by the journal id and an infix
 * that is determined by the chunk id and ends with the ".chunk" extension.
 * e.g.: journal id = 10, chunk id = 20 => chunk file name = 10_00...020.chunk
 * @param partitions A vector of all partitions handling by the system.
 */
int JournalRecovery::read_all_chunks(const vector<InodeNumber>& partitions)
{
	int delimiter_pos = -1;
	int extension_pos = -1;

	string prefix;
	string chunk;

	int chunk_id;
	unsigned long journal_id;

	list<string>* chunk_files;
	list<string>::iterator lit;

	vector<InodeNumber>::const_iterator cvit;

	for (cvit = partitions.begin(); cvit != partitions.end(); cvit++)
	{
		// get all chunks file names
		chunk_files = sal->list_objects(*cvit);

		if (chunk_files->empty())
			return NoFileFound;

		//for every chunk, separate the journal id and the chunk id
		for (lit = chunk_files->begin(); lit != chunk_files->end(); ++lit)
		{
			// check whether the file is a chunk file
			int pos = lit->find(CHUNK_EXTENSION);
			if (pos > 0)
			{
				string extension = lit->substr(pos);
				if (extension.compare(CHUNK_EXTENSION) == 0)
				{
					delimiter_pos = lit->find("_");
					extension_pos = lit->find(".");

					if (delimiter_pos > 0 && delimiter_pos < extension_pos)
					{
						// split the file name into chunk name and journal prefix
						prefix = lit->substr(0, delimiter_pos);
						chunk = lit->substr(delimiter_pos + 1, extension_pos);

						// string to decimal values
						journal_id = StringHelper::stringToDecimal<uint64_t>(prefix);
						chunk_id = StringHelper::stringToDecimal<int32_t>(chunk);

						journal_to_chunks.insert(pair<uint64_t, int32_t> (journal_id, chunk_id));
						journal_set.insert(journal_id);
					}
				}
			}
		}
	}

	// the list is not longer necessary
	chunk_files->clear();
	delete chunk_files;

	return 0;
}
