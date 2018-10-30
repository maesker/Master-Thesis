/**
 * @file JournalRecovery.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.07.2011
 */

#ifndef JOURNALRECOVERY_H_
#define JOURNALRECOVERY_H_

#include <set>
#include <vector>

#include "Journal.h"
#include "FailureCodes.h"
#include "Journal.h"


using namespace std;


class JournalRecovery
{
public:
	JournalRecovery(StorageAbstractionLayer* sal);
	virtual ~JournalRecovery();
	StorageAbstractionLayer& get_sal();

	int32_t start_recovery(map<InodeNumber, Journal*>& journals, const vector<InodeNumber>& partitions);
private:
	StorageAbstractionLayer* sal;

	int read_all_chunks(const vector<InodeNumber>& partitions);

	set<uint64_t> journal_set;
	multimap<uint64_t, int32_t> journal_to_chunks;



};

#endif /* JOURNALRECOVERY_H_ */
