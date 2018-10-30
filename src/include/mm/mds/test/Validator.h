
#ifndef MDS_VALIDATOR_H_
#define MDS_VALIDATOR_H_


/** 
 * @author {Markus Mäsker, maesker@gmx.net}
 * @date 15.08.2011
 * 
 * @brief A couple of helper functions to check einode structures
 * */

#include "gtest/gtest.h"
#include "global_types.h"
#include "EmbeddedInode.h"

void validate_einode_mode_equal(EInode *a, EInode *b, int bitfield);

#endif
