#ifndef MLTHANDLEREXCEPTION_H_
#define MLTHANDLEREXCEPTION_H_

#include <string>

class MltHandlerException
{
public:
    MltHandlerException(const std::string& message) { this->message = message;};
    virtual ~MltHandlerException() {};
    std::string& get_message() { return this->message; };
private:
    std::string message;
};

#endif /* MLTHANDLEREXCEPTION_H_ */
