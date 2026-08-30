/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef IOGCCircularDataQueue_h
#define IOGCCircularDataQueue_h

#if KERNEL
#include <IOKit/IOLib.h>
#else
#include <IOKit/IOKitLib.h>
#endif

__BEGIN_DECLS

struct IOCircularDataQueue;
typedef struct IOCircularDataQueue IOCircularDataQueue;

// IOCircularDataQueueCreate* options
typedef enum __OS_ENUM_ATTR IOCircularDataQueueCreateOptions {
	kIOCircularDataQueueCreateConsumer = 0x00000001,
	kIOCircularDataQueueCreateProducer = 0x00000002
} IOCircularDataQueueCreateOptions;

#if KERNEL

/*!
 * @function IOCircularDataQueueCreateWithEntries
 * @abstract Function that creates a new IOCircularDataQueue instance with the specified number of entries of the given
 * size.
 * @discussion This method will create a new IOCircularDataQueue instance with enough capacity for numEntries of
 * entrySize each.  It does account for the IOCircularDataQueueEntryHeader overhead for each entry.  Note that the
 * numEntries and entrySize are simply used to determine the data region size.  They do not actually restrict the number
 * of enqueues to the queue since its a circular buffer and will eventually overwrite old data. At any time, the
 * allocated data region can hold a maximum of numEntries queue entries. <br>  This method allocates a new
 * IOCircularDataQueue instance with the given numEntries and entrySize parameters.
 * @param options IOCircularDataQueueCreateOptions.
 * @param numEntries Number of entries to allocate space for.
 * @param entrySize Size of each entry.
 * @param pQueue Pointer to a queue handle. On return, this holds a handle to the newly allocated queue.
 * @return
 *  - `kIOReturnSuccess` if the queue was succesfully intialized.
 *  - `kIOReturnBadArgument` if the parameters passed were invalid.
 *  - `kIOReturnNoMemory` if there was a memory allocation failure.
 */
IOReturn IOCircularDataQueueCreateWithEntries(IOCircularDataQueueCreateOptions options, uint32_t numEntries, uint32_t entrySize, IOCircularDataQueue **pQueue);

/*!
 * @function IOCircularDataQueueCopyMemoryDescriptor
 * @abstract Returns a reference to the IOMemoryDescriptor for the queue's memory.
 * @discussion Returns a reference to the IOMemoryDescriptor for the queue's memory.
 * @param queue Queue handle. On return, this holds a handle to the newly allocated queue.
 * @return IOMemoryDescriptor reference
 */
IOMemoryDescriptor * IOCircularDataQueueCopyMemoryDescriptor(IOCircularDataQueue *queue);

#else /* KERNEL */

/*!
 * @function IOCircularDataQueueCreateWithConnection
 * @abstract Function that creates a new IOCircularDataQueue instance with an open IOUserClient connection.
 * @discussion This method will create a new IOCircularDataQueue instance with a queue owned by the IOUserClient
 * instance passed in. The memory and queue attributes are created from the IOMemoryDescriptor returned by the IOUC
 * returned by clientMemoryForType() with the memoryType parameter passed to this function. This memory descriptor must
 * be one returned by the kernel api IOCircularDataQueueCopyMemoryDescriptor().
 * @param options IOCircularDataQueueCreateOptions.
 * @param connect An open IOUserClient connection created by the caller with IOServiceOpen(). The connection must be
 *        valid while the queue is in use.
 * @param memoryType memoryType argument that will passed to the IOUC clientMemoryForType() function to obtain the queue memory.
 * @param pQueue Pointer to a queue handle. On return, this holds a handle to the newly allocated queue.
 * @return
 *  - `kIOReturnSuccess` if the queue was succesfully intialized.
 *  - `kIOReturnBadArgument` if the parameters passed were invalid.
 *  - `kIOReturnNoMemory` if there was a memory allocation failure.
 */
IOReturn IOCircularDataQueueCreateWithConnection(IOCircularDataQueueCreateOptions options, io_connect_t connect, uint32_t memoryType, IOCircularDataQueue **pQueue);

#endif /* !KERNEL */

/*!
 * @function IOCircularDataQueueDestroy
 * @abstract Function that destroys a previously created IOCircularDataQueue instance (created with
 * IOCircularDataQueueCreateWithEntries).
 * @param pQueue Pointer to the queue handle.
 *  @return
 *  - `kIOReturnSuccess` if the queue was succesfully destroyed.
 *  - `kIOReturnBadArgument` if an invalid queue was provided.
 */
IOReturn IOCircularDataQueueDestroy(IOCircularDataQueue **pQueue);


/*!
 * @function IOCircularDataQueueEnqueue
 * @abstract Enqueues a new entry on the queue.
 * @discussion This method adds a new data entry of dataSize to the queue.  It sets the size parameter of the entry
 * pointed to by the write index  and copies the memory pointed to by the data parameter in place in the queue.  Once
 * that is done, it moves the write index to the next index.
 * @param queue Handle to the queue.
 * @param data Pointer to the data to be added to the queue.
 * @param dataSize Size of the data pointed to by data.
 *  @return
 *  - `kIOReturnSuccess` on success.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - `kIOReturnBadArgument` if an invalid queue was provided.
 *  - `kIOReturnBusy` if another thread is enqueing concurrently
 *  - `kIOReturnUnsupported` if the queue has not been configured to support fixed size entries. Variable size is
 * currently not supported
 *  - Other values indicate an error.
 */
IOReturn IOCircularDataQueueEnqueue(IOCircularDataQueue *queue, const void *data, size_t dataSize);

/*!
 * @function IOCircularDataQueueGetLatest
 * Access the latest entry data, also update the cursor position to the latest. No copy is made of the data. <br> Caller
 * is supposed to call IOCircularDataQueueIsCurrentDataValid() to check data integrity after reading the data is
 * complete.
 * @param queue Handle to the queue.
 * @param data A pointer to the data memory region for the latest entry data in the queue.
 * @param size A pointer to the size of the data parameter.  On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated.
 *  - `kIOReturnUnderrun` if nothing has ever been enqueued into the queue
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - `kIOReturnBadArgument` if an invalid queue was provided.
 *  - `kIOReturnTimeout` if the reader timed out when trying to read. This is possible if the writer overwrites the
 * latest index a reader is about to read. The function times out if the read is unsuccessful after multiple retries.
 *  - Other values indicate an error.
 */
IOReturn IOCircularDataQueueGetLatest(IOCircularDataQueue *queue, void **data, size_t *size);

/*!
 * @function IOCircularDataQueueCopyLatest
 *  Access the latest entry data and copy into the provided buffer. Also update the cursor position to the latest. On a
 * successful return, the function gaurantees that the latest data was successfully copied. In this case there is no
 * need to call IOCircularDataQueueIsCurrentDataValid() after reading the data is complete, since the function returns a
 * copy which cannot be overwritten by the writer.
 * @param queue Handle to the queue.
 * @param data Pointer to memory into which the latest data from the queue is copied. Lifetime of this memory is
 * controlled by the caller.
 * @param size Size of the data buffer provided for copying. On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated to latest and the data was successfully copied.
 *  - `kIOReturnUnderrun` if nothing has ever been enqueued into the queue
 *  - `kIOReturnBadArgument` if the buffer provided to copy the data is NULL or  if an invalid queue was provided..
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - `kIOReturnTimeout` if the reader timed out when trying to copy the latest data. This is possible if the writer
 * overwrites the latest index a reader is about to copy. The function times out if the copy is unsuccessful after
 * multiple retries.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueCopyLatest(IOCircularDataQueue *queue, void *data, size_t *size);

/*!
 * @function IOCircularDataQueueGetNext
 * Access the data at the next cursor position and updates the cursor position to the next. No copy is made of the data.
 * <br> Caller is supposed to call IOCircularDataQueueIsCurrentDataValid() to check data integrity after reading the
 * data is complete.
 * @param queue Handle to the queue.
 * @param data A pointer to the data memory region for the next entry data in the queue.
 * @param size A pointer to the size of the data parameter.  On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated to the latest.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnUnderrun` if the cursor has reached the latest available data.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call IOCircularDataQueueGetLatest to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueGetNext(IOCircularDataQueue *queue, void **data, size_t *size);

/*!
 * @function IOCircularDataQueueCopyNext
 * Access the data at the next cursor position and copy into the provided buffer. Also update the cursor position to the
 * next. On a successful return, the function gaurantees that the next entry data was successfully copied. In this case
 * there is no need to call IOCircularDataQueueIsCurrentDataValid() after reading the data is complete, since the
 * function returns a copy which cannot be overwritten by the writer.
 * @param queue Handle to the queue.
 * @param data Pointer to memory into which the next data from the queue is copied. Lifetime of this memory is
 * controlled by the caller.
 * @param size Size of the data buffer provided for copying. On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated to next and the data was successfully copied.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnUnderrun` if the cursor has reached the latest available data.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call IOCircularDataQueueCopyLatest to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueCopyNext(IOCircularDataQueue *queue, void *data, size_t *size);

/*!
 * @function IOCircularDataQueueGetPrevious
 * Access the data at the previous cursor position and updates the cursor position to the previous. No copy is made of
 * the data. <br> Caller is supposed to call IOCircularDataQueueIsCurrentDataValid() to check data integrity after
 * reading the data is complete.
 * @param queue Handle to the queue.
 * @param data A pointer to the data memory region for the previous entry data in the queue.
 * @param size A pointer to the size of the data parameter.  On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated to the previous.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call IOCircularDataQueueGetLatest to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueGetPrevious(IOCircularDataQueue *queue, void **data, size_t *size);

/*!
 * @function IOCircularDataQueueCopyPrevious
 * Access the data at the previous cursor position and copy into the provided buffer. Also update the cursor position to
 * the previous. On a successful return, the function gaurantees that the previous entry data was successfully copied.
 * In this case there is no need to call IOCircularDataQueueIsCurrentDataValid() after reading the data is complete,
 * since the function returns a copy which cannot be overwritten by the writer.
 * @param queue Handle to the queue.
 * @param data Pointer to memory into which the previous data is copied. Lifetime of this memory is controlled by the
 * caller.
 * @param size Size of the data buffer provided for copying. On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated to the previous and the data was successfully copied.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call IOCircularDataQueueCopyLatest to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueCopyPrevious(IOCircularDataQueue *queue, void *data, size_t *size);

/*!
 * @function IOCircularDataQueueIsCurrentDataValid
 * Verify if the data at the current cursor position is the same as the data when the cursor was first updated to this
 * position. Call this function after having read the data at the current cursor position from the queue, since the
 * queue entry could potentially have been overwritten by the writer while being read by the caller. <br>
 * @param queue Handle to the queue.
 *  @return
 *  - `kIOReturnSuccess` if the data at the cursor position is unchanged.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer the same  and is
 *     potentially overwritten. Call IOCircularDataQueueGetLatest to get the latest data and cursor position.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnBadArgument` if an invalid param was passed.
 *  - `kIOReturnBadMedia` if the queueMemory is corrupted.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueIsCurrentDataValid(IOCircularDataQueue *queue);

/*!
 * @function IOCircularDataQueueSetCursorLatest
 * Set the current cursor position to the latest entry in the queue. This only updates the cursor and does not read the
 * data from the queue. If nothing has been enqueued into the queue yet, this returns an error.
 * @param queue Handle to the queue.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated to the latest.
 *  - `kIOReturnUnderrun` if nothing has ever been enqueued into the queue since there is no latest entry.
 *  - `kIOReturnAborted` if the queue is in an irrecoverable state.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueSetCursorLatest(IOCircularDataQueue *queue);

/*!
 * @function IOCircularDataQueueGetCurrent
 * Access the data at the current cursor position. The cursor position is unchanged. No copy is made of the data. <br>
 * Caller is supposed to call IOCircularDataQueueIsCurrentDataValid() to check data integrity after reading the data is
 * complete.
 * @param queue Handle to the queue.
 * @param data A pointer to the data memory region for the next entry data in the queue.
 * @param size A pointer to the size of the data parameter.  On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnUnderrun` if nothing has ever been enqueued into the queue hence there is no entry at the current
 * position..
 *  - `kIOReturnOverrun` if the entry at the current cursor position is no longer in
 *     the queue's buffer. Call IOCircularDataQueueGetLatest to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueGetCurrent(IOCircularDataQueue *queue, void **data, size_t *size);

/*!
 * @function IOCircularDataQueueCopyCurrent
 * Access the data at the current cursor position and copy into the provided buffer. The cursor position is unchanged.
 * If successful, function gaurantees that the data returned is always valid, hence no need to call
 * IOCircularDataQueueIsCurrentDataValid().
 * @param queue Handle to the queue.
 * @param data Pointer to memory into which the previous data is copied. Lifetime of this memory is controlled by the
 * caller.
 * @param size Size of the data buffer provided for copying. On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnUnderrun` if nothing has ever been enqueued into the queue hence there is no entry at the current
 * position..
 *  - `kIOReturnOverrun` if the entry at the current cursor position is no longer in
 *     the queue's buffer. Call IOCircularDataQueueCopyLatest to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueCopyCurrent(IOCircularDataQueue *queue, void *data, size_t *size);

/*!
 * @function IOCircularDataQueueGetLatestWithBlock
 * Access the latest entry data, also update the cursor position to the latest. Calls the provided block with the data
 * at the cursor position. No copy is made of the data. <br> Optionally the caller can call
 * IOCircularDataQueueIsCurrentDataValid() to check data integrity after reading the data is complete in the block.
 * Additionally the function also returns an error if the data has been overwritten after the block completion
 * @param queue Handle to the queue.
 * @param handler Block to call
 * -param data Pointer to the latest data in the queue that the block is called with.
 * -param size Size of the data pointed to by data that the block is called with.
 * @return
 *  - `kIOReturnSuccess` if the cursor position was updated to the latest.
 *  - `kIOReturnUnderrun` if nothing has ever been enqueued into the queue
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - `kIOReturnBadArgument` if an invalid queue was provided.
 *  - `kIOReturnAborted` if the queue was reset.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueGetLatestWithBlock(IOCircularDataQueue * queue, void (^handler)(const void *data, size_t size));

/*!
 * @function IOCircularDataQueueGetNextWithBlock
 * Access the data at the next cursor position and updates the cursor position to the next. Calls the provided block
 * with the data at the cursor position. No copy is made of the data. <br> Optionally the caller can call
 * IOCircularDataQueueIsCurrentDataValid() to check data integrity after reading the data is complete in the block.
 * Additionally the function also returns an error if the data has been overwritten after the block completion.
 * @param queue Handle to the queue.
 * @param handler Block to call
 * -param data A pointer to the data memory region for the next entry data in the queue that the block is called with.
 * -param size Size of the data pointed to by data that the block is called with.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated to next.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnUnderrun` if the cursor has reached the latest available data.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call IOCircularDataQueueGetLatest to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueGetNextWithBlock(IOCircularDataQueue * queue, void (^handler)(const void *data, size_t size));

/*!
 * @function IOCircularDataQueueGetPreviousWithBlock
 * Access the data at the previous cursor position and updates the cursor position to the previous. Calls the provided
 * block with the data at the cursor position. No copy is made of the data. <br> Optionally the caller can call
 * IOCircularDataQueueIsCurrentDataValid() to check data integrity after reading the data is complete in the block.
 * Additionally the function also returns an error if the data has been overwritten after the block completion.
 * @param queue Handle to the queue.
 * @param handler Block to call
 * -param data A pointer to the data memory region for the previous entry data in the queue that the block is called
 * with.
 * -param size Size of the data pointed to by data that the block is called with.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated to previous.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnUnderrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call IOCircularDataQueueGetLatest to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */
IOReturn IOCircularDataQueueGetPreviousWithBlock(IOCircularDataQueue * queue, void (^handler)(const void *data, size_t size));

__END_DECLS

#endif /* IOGCCircularDataQueue_h */
