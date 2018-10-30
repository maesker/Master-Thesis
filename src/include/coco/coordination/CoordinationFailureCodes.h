/**
 * @file CoordinationFailureCodes.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 * @brief Defines some common failure codes.
 */


#ifndef COORDINATIONFAILURECODES_H_
#define COORDINATIONFAILURECODES_H_


enum CoordinationFailureCodes {UnableToReadMlt = 1, UnableToWriteMlt = 2,
	MltExists = 3, MltNotExists = 4, ServerAlreadyExists = 5, ReallocationError = 6,
	ServerNotExists = 7, AllocationError = 8};


#endif /* COORDINATIONFAILURECODES_H_ */
