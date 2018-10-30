/**
 * @file Operation.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.05.2011
 */

#ifndef OPERATIONS_H_
#define OPERATIONS_H_
#include <iostream>
#include <string>
#include <list>
#include "CommonJournalTypes.h"
#include "pc2fsProfiler/Pc2fsProfiler.h"

using namespace std;

class Operation
{
private:
	OperationData operation_data; /* < Contains all information about an operation */
	Pc2fsProfiler* ps_profiler;
public:

	Operation();
	Operation(uint64_t operation_id);
	Operation(uint64_t operation_id, OperationType type, OperationStatus status, OperationMode mode, char* data,
			int32_t data_size);
	Operation(uint64_t operation_id, int32_t operation_number, OperationType type, OperationStatus status,
			OperationMode mode, char* data, int32_t data_size);
	~Operation();

	uint64_t get_operation_id() const;
	void set_operation_id(uint64_t operation_id);

	OperationMode get_mode() const;
	void set_mode(OperationMode mode);

	OperationStatus get_status() const;

	char* get_data();
	int32_t set_data(const char* data, int32_t data_size);

	void set_status(OperationStatus status);

	void set_operation_number(int32_t number);
	int32_t get_operation_number() const;

	void set_type(OperationType type);
	OperationType get_type() const;

	void set_module(Module module);
	Module get_module() const;

	void set_einode(const EInode& einode);
	const EInode& get_einode() const;

	void set_einode_name(const FsObjectName* name);

	const OperationData& get_operation_data() const;
	void set_operation_data(const OperationData& operation_data);

	void set_inode_id(InodeNumber inode_id);
	InodeNumber get_inode_id() const;

	InodeNumber get_parent_id() const;
	void set_parent_id(InodeNumber parent_id);

	int32_t get_bitfield() const;
	void set_bitfield(int32_t bitfield);

	/**
	 * Defines the compare function for the Operation class.
	 * @see http://www.cplusplus.com/reference/algorithm/sort/
	 */
	struct compareOperationNumberFunctor: public binary_function<Operation, Operation, bool>
	{
		bool operator()(Operation lhs, Operation rhs)
		{
			return (lhs.operation_data.operation_number < rhs.operation_data.operation_number);
		}
	};
};

#endif /* OPERATIONS_H_ */
