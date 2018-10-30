/**
 * \file CommonDAOTypes.h
 *
 * \author Benjamin Raddatz	Marie-Christine Jakobs
 *
 * \brief Contains data structures and definitions which are used for Distributed Atomic Operations
 *
 * Defines which operation are allowed as distributed atomic operations, the possible protocols, message types sent by the respective protocols,
 * the events occuring in the respective protocols, log entries differing from begin log, commit or abort, status information and different
 * data types used in the implementation of distributed atomic operations.
 */
#ifndef COMMONDOATYPES_H_
#define COMMONDOATYPES_H_

#include <string>
#include <vector>
#include <iostream>
#include <queue>

#include "global_types.h"
#include <boost/date_time/posix_time/posix_time.hpp>

/**
 *\brief Allowed atomic operations.
 *
 * Identification of all allowed distributed atomic operations. OrderedOperationTest and OOE_LBTest are only needed to test the ordered
 * operation execution protocol since it is not used by any distributed atomic operation. Notice that the OrderedOperationTest and
 * OOE_LBTestmust be the last defined  identification.
 */
enum OperationTypes {Rename, MoveSubtree, ChangePartitionOwnership, SetAttr, OrderedOperationTest, OOE_LBTest};
/**
 *\brief Implemented protocols
 *
 * Allows the easy identification of the protocol used for a currently executed distributed atomic operation.
 */
enum Protocol {TwoPhaseCommit, ModifiedTwoPhaseCommit, OrderedOperationExecution};
/**
 *\brief Events occuring during the execution of the respective protocols
 *
 * Identification of the different events which occur during the execution of one of the protocols defined in the previous enumeration
 * Protocol. Events must be ordered by protocol.If new events are inserted for a protocol, they must be inserted between the first and the
 * last event of the respective protocol.
 */
enum Events {OOEOperationFinishedSuccess, OOEOperationFinishedFailure, OOELastOperationFinishedSuccess, OOEAckRecieved,OOEAbortRecieved,
             OOEOperationUndone,
             TPCPOperationFinSuccess, TPCPOperationFinFailure, TPCPStartVote, TPCPCommit, TPCPAbort, TPCIOperationFinSuccess,
             TPCIOperationFinFailure, TPCIVotesFin, TPCIAckRecv, TPCOperationUndone,
             MTPCPOperationFinSuccess, MTPCPOperationFinFailure, MTPCPAckRec, MTPCIOperationFinSuccess, MTPCIOperationFinFailure,
             MTPCICommit, MTPCIAbort, MTPCOperationUndone
            };
/**
 *\brief Defines content of log message
 *
 * Allows to identify the different log message of the protocols. No log messages are needed for commit or abort. The begin log entry
 * looks different and does not need an extra LogMessages identification.
 */
enum LogMessages {TPCPVoteYes,TPCPVoteNo, TPCIVoteStart, TPCIAborting, TPCICommiting,MTPCPCommit,MTPCPAbort, MTPCIStartP,
                  OOEStartNext, OOEUndo
                 };
/**
 *\brief Types of messages which are exchanged between
 *
 * Defines the types of messages which are exchanged by the different protocols. All these message are sent and processed by other
 * DistributedAtomicOperations modules on different servers. The messages are split into normal protocol messages, which are further
 * subdivided into messages of the respective protocols, and failure messages. Notice that new protocol messages are only allowed to be
 * placed between the first and last message of the respective protocol.
 */
enum ExchangeMessages {MTPCOpReq, MTPCCommit, MTPCRStatusReq, MTPCRAbort, MTPCRCommit, MTPCAbort, MTPCAck,
                       OOEOpReq, OOEAck, OOERStatusReq, OOERAborted,OOEAborted,
                       TPCOpReq, TPCVoteReq, TPCPRAbort, TPCRVoteN, TPCRVoteY, TPCRVoteReq, TPCRCommit, TPCVoteY, TPCVoteN, TPCCommit, TPCAbort, TPCAck, // up to here normal protocol messages
                       OOEStatusReq, MTPCStatusReq, NotResponsible, EventReRequest, ContentRequest, ContentResponse, StatusRequest, StatusResponse,
                       ReqOpReq
                      };	// failure messages
/**
 *\brief Status of a distributed atomic operation
 *
 * Describes the different stati a distributed atomic operation may be in. The status is saved in the followingly defined temporary
 * datastructure DAOperation.
 * Notice: Not all of these states are actually saved in a log entry since e.g. messages may occur before they are expected and can be treated.
 */
enum Status {TPCCoordinatorComp, TPCCoordinatorVReqSend, TPCCoordinatorVResultSend, TPCWaitUndoAck, TPCPartComp, TPCPartWaitVReqYes,
             TPCPartWaitVReqNo, TPCPartVReqRec, TPCPartWaitVResultExpectYes, TPCPartWaitVResultExpectNo, TPCAborting, TPCWaitUndoToFinish,
             MTPCCoordinatorComp,MTPCPartComp, MTPCCoordinatorReqSend, MTPCIWaitResultUndone, MTPCPartVoteSendYes, MTPCPartVoteSendNo,
             OOEComp, OOEWaitResult, OOEWaitResultUndone
            };
/**
 * \brief Modules which may participate in a distributed atomic operation
 *
 * The identifier for a module is used to offer the execution request and the client response to the correct module.
 */
enum DistributedAtomicOperationModule {DAO_LoadBalancing, DAO_MetaData};

/**
 *\brief Description of subtree participating in a distributed atomic operation
 *
 * This datastructure is used to describe a particpant or a coordinator of a distributed atomic operation. The server is needed to send
 * a message to the respective distributed atomic operation module and the subtree_entry is needed to access the respective journal.
 */
struct Subtree
{
    std::string server;/**< server address of the server which is currently responsible for the subtree referenced by this datastructure*/
    InodeNumber subtree_entry;/**< root of the subtree which is referenced by this datastructure*/
};

/**
 * \brief Temporary datastructure for a distributed atomic operation
 *
 * Contains all necessary information which is needed to execute a distributed atomic operation correctly. The datastructure for a distributed
 * atomic operation is created at the start of the operation at the server and deleted after commit or abort.
 */
struct DAOperation
{
    uint64_t ID;/**< unique identification of the distributed atomic operation */
    OperationTypes type;/**< Defines the kind of operation which is executed*/
    void* operation;/**< Reference to operation description for this distributed atomic operation*/
    uint32_t operation_length;/**< Length of the operation description in number of bytes*/
    Status status;/**< Current status of the protocol for the respective distributed atomic operation*/
    Protocol used_protocol;/**< Protocol used for the respective distributed atomic operation*/
    InodeNumber subtree_entry;/**< root of the subtree which is affected by this part of the distributed atomic operation, needed to access the journal*/
    std::vector<Subtree> participants;/**< In case of coordinator, the list of participants, in ordered operation execution only the next participant which is not set from the beginning. In case of a participant, the coordinator (or previous executor) and additionally in ordered operation execution the next participant if existing*/
    int recieved_votes;/**< number of received votes or acknowledges, only used by the two phase commit protocol to ensure that all responses are received and the next event can be triggered.*/
    boost::posix_time::ptime overall_timeout; /**< overall timeout of the operation*/
};

/**
 * \brief Provides all necessary information to execute the subtree related part of a distributed atomic operation or informs the client about the results
 *
 * Datastructure used to provide information to the executing module. The information is exchanged using a <code>ConcurrentQueue</code>. If the
 * operation_length is 0 it is the final result of the distributed atomic operation. In any other case it is a request to execute the respective
 * part of the execution.
 */
struct OutQueue
{
    uint64_t ID;/**< unique identification of the distributed atomic operation*/
    void* operation;/**< Reference to operation description for this distributed atomic operation (Operation request type, operation code) or the client result (0 for failure, 1 for success). Notice that the pointer must be freed after it is not used anymore.*/
    uint32_t operation_length;/**<Length of the operation description in number of bytes, 0 if it is a response to client*/
    Protocol protocol;/**< Protocol used for the respective distributed atomic operation*/
};

/**
 * \brief Contains the result of the execution of a subtree related part of a distributed atomic operation
 *
 * Datastructure used to retrieve information from the executing module about the success of an operation. The information is exchanged using
 * a <code>ConcurrentQueue</code>. The distributed atomic operation class offers a method to put the information into the queue.
 */
struct InQueue
{
    uint64_t ID;/**< unique identification of the distributed atomic operation*/
    int success;/**< result of subtree related part execution, 0 for success of an execution, 1 for being unsuccessful, 2 for an successful undo and 3 for an unsuccessful undo, if execution was requested as redo or reundo the behavior is the same as in the normal case*/
    Subtree next_participant;/**< only needed for ordered operation execution protocol, description of the following executor, if server string has length 0 no following executor exist, last execution in order*/
};

/**
 *\brief Defines internal failures noticed by the distributed atomic operations module
 *
 * Defines a common identification of internal failures which occur in the distributed atomic operations module and which can be detected. The
 * identifications are used to react to internal failures at one place.
 */
enum DAOFailure {DAOMutexFailure = 1, DAOJournalFailure = 2, DAOInsufficientMemory = 3, DAOLoadBalancingNotAnswering = 4,
                 DAOMetaDataNotAnswering = 5, DAOMltNotExisting = 6, DAOSalNotSet = 7, DAOOperationCorrupted = 8
                };

/**
 * \brief Defines error codes for the distributed atomic operation module
 *
 * The error codes defined here are used by methods of the distributed atomic operations module. Never define an error code of 0 which
 * represents a successful execution in most of the cases.
 */
enum DAOFailureCodes {DAOWrongParameter = 1, DAOOperationNotInJournal = 2, DAOSubtreeUnequal = 3, DAODifferentServerAddresses = 4,
                      DAOOperationExisting = 5, DAOSendingFailed = 6, DAOLoggingFailed = 7, DAOSubtreeNotExisting = 8,
                      DAOSelfWrongServer = 9, DAONoMlt = 10, DAONoSal = 11, DAONoBeginLog = 12, DAOInternal = 13, DAOUnknownAddress = 14,
                      DAOWrongEvent = 15, DAOOperationFinished = 16, DAODifferentStatus = 17, DAOUnknownLog = 18,
                      DAONotAllOperationsRecoverable = 19, DAONoFailureTreatmentPossible = 20, DAOSettingAddressesFailed = 21
                     };

/**
 *\brief Requests which can be sent to a module executing a suboperation of a distributed atomic operation
 *
 * Defines which requests can be sent to a module executing a suboperation of a distributed atomic operation. The requests are specified as
 * char at the beginning of the content of operation in <code>OutQueue</code>.
 */
enum OperationRequestType {DAORequest = 1, DAORedoRequest = 2, DAOUndoRequest = 3, DAOReundoRequest = 4};

/**
 *\brief Structure used to store the timeouts
 *
 * This structure is used to store the timeouts. If an operation changes its status one of those objects is added to the priority queue.
 */
struct DAOTimeoutStruct
{
    uint64_t ID;
    boost::posix_time::ptime enter_time;
    double rel_timeout;
    Status last_status;
};


/**
 *\brief Comparision class used in a priority queue
 *
 * The comparision class defines how the items in the priority queue are ordered. This class orderes after relative timeouts starting with the next one and ending with the one furthest away in time.
 */
class DAOComparison
{
public:
    bool operator() (const DAOTimeoutStruct &lhs, const DAOTimeoutStruct &rhs) const
    {
        return ((lhs.enter_time + boost::posix_time::milliseconds(lhs.rel_timeout)) < (rhs.enter_time + boost::posix_time::milliseconds(rhs.rel_timeout)));
    }
};


/**
 * Timeout in ms for one step of the TPC protocoll
 */
#define TPC_REL_TIMEOUT 3000

/**
 * Timeout in ms for one step of the MTPC protocoll
 */
#define MTPC_REL_TIMEOUT 3000

/**
 * Timeout in ms for one step of the OOE protocoll
 */
#define OOE_REL_TIMEOUT 3000

/**
 * Timeout in ms for a whole DAO
 */
#define OVERALL_TIMEOUT 60000

/**
 * Time in ms that is spend sleeping when no timeout is found
 */
#define DAO_MIN_SLEEP_TIME 1000

/**
 *\brief Defines the results of an operation request
 *
 * Defines the results of an execution of an <code>OperationRequestType</code>. The result is specified in the success variable of <code>
 * InQueue<code>.
 */
enum OperationRequestResults {ExecutionSuccessful = 0, ExecutionUnsuccessful = 1, UndoSuccessful =2, UndoUnsuccessful = 3};
#endif //#ifndef COMMONDOATYPES_H_
