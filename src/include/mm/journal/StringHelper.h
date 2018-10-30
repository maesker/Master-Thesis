/**
 * @file StringHelper.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.06.2011
 */

#ifndef STRINGHELPER_H_
#define STRINGHELPER_H_

#include <vector>
#include <sstream>

using namespace std;

class StringHelper {
public:


	static string intToString(int value);
	static string doubleToString(double value);
	static int stringToInt(string str);

	template <typename  T>
	static T stringToDecimal(string str){
		T value;
		stringstream ssm;
		ssm << str;
		ssm >> value;
		return value;
	};

	static double stringToDouble(string str);
	static void split(vector<string>& tokens, const string &input, char delimiter);
};

#endif /* STRINGHELPER_H_ */
