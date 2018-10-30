/**
 * @file JournalException.h
 * @class JournalException
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.06.2011
 */

#ifndef JOURNALEXCEPTION_H_
#define JOURNALEXCEPTION_H_

#include <string>

class JournalException
{
public:
	JournalException(const std::string& msg);
	virtual ~JournalException();

	const std::string& get_msg() const;

private:
	std::string msg;
};

#endif /* JOURNALEXCEPTION_H_ */
