/**
 * @file CommonJournalFunctions.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 * @brief Defines some common journal utility functions.
 */


#ifndef COMMONJOURNALFUNCTIONS_H_
#define COMMONJOURNALFUNCTIONS_H_

#include "CommonJournalTypes.h"


/**
 * @brief Defines the compare function for the AccessData structure, compares the access time.
 * @see http://www.cplusplus.com/reference/algorithm/sort/
 */
struct compareAccessTime: public binary_function<AccessData, AccessData, bool>
{
	bool operator()(AccessData lhs, AccessData rhs)
	{
		return (lhs.last_access.tv_sec < lhs.last_access.tv_sec);
	}
};

#endif /* COMMONJOURNALFUNCTIONS_H_ */
