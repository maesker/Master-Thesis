/**
 * @file CommonCoodinationTypes.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 * @brief Defines some common coodination data structures.
 */


#ifndef COMMONCOODINATIONTYPES_H_
#define COMMONCOODINATIONTYPES_H_


/**
 * Defines the structure of a server.
 */
struct Server
{
	std::string address; /**< IP address or other identifier of the server*/
	uint16_t port; /**< Port number of the server. */
};

#endif /* COMMONCOODINATIONTYPES_H_ */
