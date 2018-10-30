/*
 * StringHelper.cpp
 *
 *  Created on: 27.05.2011
 *      Author: viktor
 */

#include "mm/journal/StringHelper.h"

string StringHelper::intToString(int value)
{
	string str;
	stringstream ssm;
	ssm << value;
	ssm >> str;
	return str;
}

string StringHelper::doubleToString(double value)
{
	string str;
	stringstream ssm;
	ssm << value;
	ssm >> str;
	return str;
}


int StringHelper::stringToInt(string str)
{
	int value;
	stringstream ssm;
	ssm << str;
	ssm >> value;
	return value;
}

double StringHelper::stringToDouble(string str)
{
	double value;
	stringstream ssm;
	ssm << str;
	ssm >> value;
	return value;
}

void StringHelper::split(vector<string>& tokens, const string &input, char delimiter)
{
	string token;
	istringstream tokenizer(input);

	while (getline(tokenizer, token, delimiter))
	{
		tokens.push_back(token);
	}
}
