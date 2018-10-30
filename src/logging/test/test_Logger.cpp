/*
* Test_Layout.cpp
*
* Author: Sebastian Moors <mauser@smoors.de>
*/
#include <string>
#include <sstream>

#include "gtest/gtest.h"
#include "Logger.h"

#define OBJECT_NAME "LoggerTest"

class LoggerTest : public ::testing::Test
{
	protected:
	    LoggerTest()
	    {
	    }

	    ~LoggerTest()
	    {
	    }

	    virtual void SetUp()
	    {
	    }

	    virtual void TearDown()
	    {
	    }
};


void removeFile(const char* fname)
{
	ifstream File;
	File.open(fname);
	if(File.is_open())
	{
		File.close();
		if(remove( fname ) == -1)
		{
			//can't remove the existing file -> can't be sure that environment is ok
			FAIL();		
		}
	}
}

TEST (LoggerTest,DefaultLocationFileWriteTest)
{
	const char* fname ="/tmp/default.log"; 
	removeFile(fname);
	
	ifstream File;

	Logger* l = new Logger();
	l->set_console_output(false);
	time_t t = time(NULL);
    	l->warning_log("message");
	delete l;

	File.open(fname);
	string line;
	getline(File,line);

	stringstream log_stream;
	string log_message;
	log_stream << "["  << t << "] " << __PRETTY_FUNCTION__  << ": " << "message";
	log_message = log_stream.str();
	
	ASSERT_TRUE(line==log_message);
}

TEST (LoggerTest,CustomLocationFileWriteTest)
{
	const char* fname ="/tmp/default.log"; 
	removeFile(fname);
	
	ifstream File;

	Logger* l = new Logger();
	l->set_log_location( fname );
	l->set_console_output(false);
	time_t t = time(NULL);
    	l->warning_log("message");
	delete l;

	File.open( fname );
	string line;
	getline(File,line);
	
	stringstream log_stream;
	string log_message;
	log_stream << "["  << t << "] " << __PRETTY_FUNCTION__  << ": " << "message";
	log_message = log_stream.str();
	
	ASSERT_TRUE(line==log_message);
}

TEST (LoggerTest,LogLevelTest)
{
	const char* fname ="/tmp/default.log"; 
	removeFile(fname);
	
	ifstream File;

	Logger* l = new Logger();
	l->set_log_location( fname );
	l->set_console_output(false);
	l->set_log_level(0);
	time_t t = time(NULL);
    	l->warning_log("warning");
    	l->error_log("error");
    	l->debug_log("debug");
	delete l;

	File.open( fname );
	string line;
	getline(File,line);
	
	stringstream log_stream;
	string log_message;
	log_stream << "["  << t << "] " << __PRETTY_FUNCTION__  << ": " << "warning";
	log_message = log_stream.str();
	
	ASSERT_TRUE(line==log_message);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
