/**
 * @file InodeCacheException.h
 * @class InodeCacheException
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 */

#ifndef INODECACHEEXCEPTION_H_
#define INODECACHEEXCEPTION_H_

#include <string>

class InodeCacheException {
public:
	InodeCacheException(const std::string& msg) {this->msg = msg;};

	virtual ~InodeCacheException() {};

	const std::string& get_msg() const { return msg; };

private:
	std::string msg;
};


#endif /* INODECACHEEXCEPTION_H_ */
