/**
 * @file Operation.cpp
 * @class Operation
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @date 01.05.2011
 *
 * @brief The Operation class represents an operation inside a journal.
 *
 * An operation can be stand alone or distributed.
 * At the current state of implementation all operation to an inode are stand alone operations.
 * The distributed operations are all operations which involving multiple servers.
 *
 * An operation is described by the following data:
 * Operation Id: An unique id within the cluster, which represents an distributed operation.
 * Inode Id: An unique id within the cluster, which represents an operation to an inode.
 * Operation Type: Defines the type of an operation. @see OperationType
 * Operation status: Defines the status of an operation. @see OperationStatus
 * Module: Defines the module, which journaled this operation. @see Module
 * EInode: The einode data.
 * Data: Some space for particular information.
 * Bitfield: A bitfield, only used for einode operations.
 */

#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include "mm/journal/Operation.h"
#include "mm/journal/FailureCodes.h"

using namespace std;

/**
 * @brief Default constructor of Operation class.
 * Creates a dummy operation.
 */
Operation::Operation()
{
	ps_profiler = Pc2fsProfiler::get_instance();
}

/**
 * @brief Constructor of Operation class.
 * Creates an operation with the specified id.
 * @param operation_id The operation id.
 */
Operation::Operation(uint64_t operation_id)
{
	ps_profiler = Pc2fsProfiler::get_instance();
	set_operation_id( operation_id );
}

/**
 * @brief Constructor of Operation class.
 * Creates an operation without the operation number.
 * @param operation_id The operation id.
 * @param type The operation type.
 * @param status The operation status.
 * @param mode The operation mode.
 * @param data Pointer to the data.
 * @param data_size Size of the data array.
 */
Operation::Operation(uint64_t operation_id, OperationType type, OperationStatus status, OperationMode mode, char* data,
		int32_t data_size)
{
	ps_profiler = Pc2fsProfiler::get_instance();

	ps_profiler->function_start();

	set_operation_id( operation_id );
	set_mode( mode );
	set_status( status );
	set_data( data, data_size );
	set_operation_number( 0 );

	ps_profiler->function_end();
}

/**
 * @brief Constructor of Operation class.
 * Creates an operation with all necessary parameter.
 * @param operation_id The operation id.
 * @param operation_number The operation number.
 * @param type The operation type.
 * @param status The operation status.
 * @param mode The operation mode.
 * @param data Pointer to the data.
 * @param data_size Size of the data array.
 */
Operation::Operation(uint64_t operation_id, int32_t operation_number, OperationType type, OperationStatus status,
		OperationMode mode, char* data, int32_t data_size)
{
	ps_profiler = Pc2fsProfiler::get_instance();

	ps_profiler->function_start();
	set_operation_id( operation_id );
	set_operation_number( operation_number );
	set_mode( mode );
	set_status( status );
	set_data( data, data_size );

	ps_profiler->function_end();
}

/**
 * @brief Operation destructor.
 */
Operation::~Operation()
{
	// TODO clean up
}

/**
 *	@brief Sets the status of the operation.
 *	Status : completed or not completed.
 *	@param status The status value.
 */
void Operation::set_status(OperationStatus status)
{
	operation_data.status = status;
}

/**
 * @brief Get the data of the operation.
 * @return Pointer to the data.
 */
char* Operation::get_data()
{

	return operation_data.data;
}

/**
 * @brief Set the mode of the operation.
 * @param mode The mode value.
 */
void Operation::set_mode(OperationMode mode)
{
	operation_data.mode = mode;
}

/**
 * @brief Get the mode of the operation.
 * @return The mode value.
 */
OperationMode Operation::get_mode() const
{
	return operation_data.mode;
}

/**
 * @brief Get the operation id.
 * @return The operation id.
 */
uint64_t Operation::get_operation_id() const
{
	return operation_data.operation_id;
}

/**
 * @brief Sets the data array.
 * @param data Pointer to the data array.
 * @param data_size Size of the data array.
 * @return Returns 0 if set was successful; Returns SizeError if the size of the data is bigger as the expected size.
 */
int Operation::set_data(const char* data, int32_t data_size)
{
	ps_profiler->function_start();

	if ( data_size <= OPERATION_SIZE)
	{
		// first set all elements of the array to 0
		memset( operation_data.data, 0, sizeof(char) * OPERATION_SIZE);

		// copy data
		memcpy( operation_data.data, data, sizeof(char) * data_size );

		ps_profiler->function_end();
		return 0;
	}
	else
	{
		ps_profiler->function_end();
		return SizeError;
	}
}

/**
 * @brief Get the status of the operation.
 * @return status The operation status.
 */
OperationStatus Operation::get_status() const
{
	return operation_data.status;
}

/**
 * @brief Set the operation id.
 * @param operation_id The operation id.
 */
void Operation::set_operation_id(uint64_t operation_id)
{
	operation_data.operation_id = operation_id;
}

/**
 * @brief Sets the operation number.
 * @param number The operation number.
 */
void Operation::set_operation_number(int32_t number)
{
	operation_data.operation_number = number;
}

/**
 * @brief Gets the operation number.
 * @return The number of the operation.
 */
int32_t Operation::get_operation_number() const
{
	return operation_data.operation_number;
}

/**
 * @brief Sets the type of an operation.
 * @param type The type value.
 */
void Operation::set_type(OperationType type)
{
	operation_data.operation_type = type;
}

/**
 * @brief Gets the type of an operations.
 * @param The type of an operation.
 */
OperationType Operation::get_type() const
{
	return operation_data.operation_type;
}

/**
 * @brief Sets the module.
 * @param module The module.
 */
void Operation::set_module(Module module)
{
	operation_data.module = module;
}

/**
 * @brief Gets the module.
 * @return The module.
 */
Module Operation::get_module() const
{
	return operation_data.module;
}

/**
 * @brief Sets the embedded inode.
 * @param einode Reference to embedded inode.
 */
void Operation::set_einode(const EInode& einode)
{
	operation_data.einode = EInode( einode );
}

/**
 * @brief Gets the embedded inode.
 * @return Reference to the embedded inode.
 */
const EInode& Operation::get_einode() const
{
	return operation_data.einode;
}

/**
 * @brief Sets the name of an einode.
 * @param name Pointer to the name string.
 */
void Operation::set_einode_name(const FsObjectName* name)
{
	strcpy( (char*) this->operation_data.einode.name, (char*) name );
}

/**
 * @brief Gets the operation data.
 * @return Reference to the operation data.
 */
const OperationData& Operation::get_operation_data() const
{
	return operation_data;
}

/**
 * @brief Sets the inode id.
 * @param inode_id The inode id.
 */
void Operation::set_inode_id(InodeNumber inode_id)
{
	operation_data.einode.inode.inode_number = inode_id;
}

/**
 * @brief Sets the data structure of the operation.
 * @param operation_data Reference of the operation data.
 */
void Operation::set_operation_data(const OperationData& operation_data)
{
	this->operation_data = OperationData( operation_data );
}

/**
 * @brief Get the inode id.
 * @return The inode id.
 */
InodeNumber Operation::get_inode_id() const
{
	return operation_data.einode.inode.inode_number;
}

/**
 * @brief Gets the parent id.
 * @return The parent id.
 */
InodeNumber Operation::get_parent_id() const
{
	return operation_data.parent_id;
}

/**
 * @brief Sets the parent id.
 * @param paret_id The parent id.
 */
void Operation::set_parent_id(InodeNumber parent_id)
{
	operation_data.parent_id = parent_id;
}

/**
 * @brief Gets the bitfield.
 * @return The birfield.
 */
int32_t Operation::get_bitfield() const
{
	return operation_data.bitfield;
}

/**
 * @brief Sets the bitfield.
 * @param bitfield The bitfield.
 */
void Operation::set_bitfield(int32_t bitfield)
{
	operation_data.bitfield = bitfield;
}

