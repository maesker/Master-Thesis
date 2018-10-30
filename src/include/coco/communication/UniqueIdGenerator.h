

#ifndef UNIQUEIDGENERATOR_H_
#define UNIQUEIDGENERATOR_H_

#include <string>

using namespace std;


/** The UniqueIdGenerator provides unique ids.
 *
 * An id consists of the MAC-address and an decimal value.
 * @author Viktor Gottfried
 */
class UniqueIdGenerator
{
public:
    UniqueIdGenerator();
    virtual ~UniqueIdGenerator();

    uint64_t get_next_id();

private:
    string determine_uid();

    uint64_t last_id;	/* < Last generated id */
    string uid;			/* < The unique system id, defined by the MAC address. */
    string sep;			/* < Seperator between decimal value and the mac adress string. */
};

#endif /* UNIQUEIDGENERATOR_H_ */
