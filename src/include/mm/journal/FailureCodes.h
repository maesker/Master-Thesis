/**
 * @file FailureCoudes.h
 * @author Viktor Gottfried, viktorgt1985@gmail.com
 * @data 01.08.2011
 * @brief Defines some common journal failure codes.
 */

#ifndef FAILURECODES_H_
#define FAILURECODES_H_

enum JournalFailureCodes {SizeError = 1, FileOpenFailed = 2, JournalEmpty = 3,
	NotUniqueId = 4, ChunkToLarge = 5,  WrongOperationId = 6, StorageAccessError = 7,
	NoFileFound = 8, CannotReadChunk = 9, CannotWriteInode = 10, InodeExists = 11, InodeNotExists = 12,
	NotInCache = 13, InvalidOperation = 14};

#endif /* FAILURECODES_H_ */
