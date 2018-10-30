/**
 * @file JournalException.cpp
 * @class JournalException
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @date 01.07.2011
 */

#include "mm/journal/JournalException.h"

JournalException::JournalException(const std::string& msg)
{
	this->msg = msg;
}

JournalException::~JournalException()
{
	// TODO Auto-generated destructor stub
}

const std::string& JournalException::get_msg() const
{
	return msg;
}
