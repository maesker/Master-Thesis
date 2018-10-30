#include "DAOTestEmulator.h"

JournalManager JournalManager::manager = JournalManager();
ConcurrentQueue<JournalRequest> Journal::journal_queue = ConcurrentQueue<JournalRequest>();
ConcurrentQueue<SendRequest> CommunicationHandler::queue = ConcurrentQueue<SendRequest>();
bool CommunicationHandler::success = true;
bool CommunicationHandler::first = true;
bool CommunicationHandler::op_undo = true;
const OperationData Operation::my_data = Operation::init_data();
bool DAOAdapterTestRealization::coordinator = true;