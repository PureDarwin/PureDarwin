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

#include <IOKit/IOCircularDataQueue.h>

__BEGIN_DECLS

/*!
 *  @header IOCircularDataQueueMemory
 *
 *  This header contains the memory layout for a circular data queue.
 *
 *  A circular data queue supports a single producer and zero or more consumers.
 *
 *
 *  The producer does not wait for consumers to read the data.  If a
 *  consumer falls behind, it will miss data.
 *
 *  The queue can be configured to support either fixed or variable sized
 *  entries.
 *  Currently only fixed is supported.
 */

/*
 *  Fixed sized entry circular queue
 *
 +------------+
 |    Queue   |
 |   Header   |
 +------------+ <--- First Data Entry
 |Entry Header|
 +------------+
 |            |
 |    Entry   |
 |    Data    |
 |            |
 +------------+ <--- Second Data Entry
 |Entry Header|
 +------------+
 |            |
 |            |
 |     ...    |
 |     ...    |
 |            |
 |            |
 |            |
 +------------+ <--- Last Data Entry
 |Entry Header|
 +------------+
 |            |
 |    Entry   |
 |    Data    |
 |            |
 +------------+
 |
 */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 201112L
	#define _STATIC_ASSERT_OVERLOADED_MACRO(_1, _2, NAME, ...) NAME
	#define static_assert(...) _STATIC_ASSERT_OVERLOADED_MACRO(__VA_ARGS__, _static_assert_2_args, _static_assert_1_arg)(__VA_ARGS__)

	#define _static_assert_2_args(ex, str) _Static_assert((ex), str)
	#define _static_assert_1_arg(ex) _Static_assert((ex), #ex)
#endif

#define HEADER_16BYTE_ALIGNED 1 // do the entry and entry headers need to be 16 byte aligned for perf/correctness ?

/*!
 * @typedef IOCircularDataQueueEntryHeaderInfo
 * @abstract The state of an entry in the  circular data queue. The state is part of  each entry header in the queue.
 * @discussion The entry state has the sequence number, data size, generation and current state of the entry. The state
 * is read/updated atomically.
 * @field seqNum A unique sequence number for this entry. The sequence number is monotonically increased on each enqueue
 * to the queue. Each entry in the queue has a unique sequence number.
 * @field dataSize The size of the data at this entry.
 * @field generation The queue generation which is copied from the queue header memory into the entry state on an
 * enqueue.
 * @field `_reserved` Unused
 * @field wrStatus Represents if the queue entry is currently being written to or not.
 */

#define IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_SIZE 1
#define IOCIRCULARDATAQUEUE_ENTRY_STATE_GENERATION_SIZE 30
#define IOCIRCULARDATAQUEUE_ENTRY_STATE_DATATSIZE_SIZE 32
#define IOCIRCULARDATAQUEUE_ENTRY_STATE_SEQNUM_SIZE 64
// #define IOCIRCULARDATAQUEUE_ENTRY_STATE_RESERVED_SIZE   1
#define IOCIRCULARDATAQUEUE_ENTRY_STATE_RESERVED_SIZE                                                                  \
    ((8 * sizeof(__uint128_t)) - IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_SIZE                                            \
     - IOCIRCULARDATAQUEUE_ENTRY_STATE_GENERATION_SIZE - IOCIRCULARDATAQUEUE_ENTRY_STATE_DATATSIZE_SIZE                \
     - IOCIRCULARDATAQUEUE_ENTRY_STATE_SEQNUM_SIZE)

typedef union {
	__uint128_t val;
	struct {
		__uint128_t seqNum : IOCIRCULARDATAQUEUE_ENTRY_STATE_SEQNUM_SIZE; // Sequence Number
		__uint128_t dataSize : IOCIRCULARDATAQUEUE_ENTRY_STATE_DATATSIZE_SIZE; // datasize
		__uint128_t generation : IOCIRCULARDATAQUEUE_ENTRY_STATE_GENERATION_SIZE; // generation
		__uint128_t _reserved : IOCIRCULARDATAQUEUE_ENTRY_STATE_RESERVED_SIZE; // reserved, currently not used
		__uint128_t wrStatus : IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_SIZE; // queue writing status
	} fields;
} IOCircularDataQueueEntryHeaderInfo;

#define IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_INPROGRESS (1)
#define IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_COMPLETE (0)

static_assert(IOCIRCULARDATAQUEUE_ENTRY_STATE_RESERVED_SIZE > 0, "unexpected reserved field size");

/*!
 * @typedef IOCircularDataQueueEntryHeader
 * @abstract An entry in the  circular data queue. The entry header is written at the beginning of each entry in the
 * queue.
 * @discussion The entry has the current state, sentinel, followed by the data at the enty.
 * @field info The info of the queue entry. This  includes the size, sequence number, generation and write status of the
 * data at this entry.
 * @field sentinel unique value written to the queue entry. This is copied from the sentinel in the queue header memory
 * when an entry is written.
 * @field data Represents the beginning of the data region.  The address of the data field is a pointer to the start of
 * the data region.
 */
typedef struct {
	union {
		volatile _Atomic __uint128_t headerInfoVal;
		IOCircularDataQueueEntryHeaderInfo __headerInfo;         // for clarity, unused
	};
	volatile uint64_t sentinel;
	uint64_t _pad; // pad for 16 byte aligment of data that follows
#if HEADER_16BYTE_ALIGNED
	uint8_t data[16]; // Entry data begins.  Aligned to 16 bytes.
#else
	uint8_t data[8]; // Entry data begins.  Aligned to 8 bytes.
#endif
} IOCircularDataQueueEntryHeader;

#if HEADER_16BYTE_ALIGNED
#define CIRCULAR_DATA_QUEUE_ENTRY_HEADER_SIZE (sizeof(IOCircularDataQueueEntryHeader) - 16)
#else
#define CIRCULAR_DATA_QUEUE_ENTRY_HEADER_SIZE (sizeof(IOCircularDataQueueEntryHeader) - 8)
#endif

/*!
 * @typedef IOCircularDataQueueState
 * @abstract The current state of the circular data queue.
 * @discussion The queue state is part of the queue memory header. It has the current sequence number, next writing
 * index, generation and current reset and writing state off the queue. The queue state is read/updated atomically.
 * @field seqNum A monotonically increasing sequence number which is incremented for each enqueue.
 * @field wrIndex The next write position into the queue.
 * @field generation The generation of the queue. It is a monotonically increasing number, which is incremented on each
 * queue reset.
 * @field rstStatus The queue reset state. The bit is set if the queue is currently being reset.
 * @field wrStatus The queue writing state. The bit is set if an enqueue is in progress.
 */
// Fahad : I dont think we need a reset bit, since we are doing everything in one atomic op.

#define IOCIRCULARDATAQUEUE_STATE_WRITE_SIZE 1
#define IOCIRCULARDATAQUEUE_STATE_RESET_SIZE 1
#define IOCIRCULARDATAQUEUE_STATE_GENERATION_SIZE 30
#define IOCIRCULARDATAQUEUE_STATE_WRITEINDEX_SIZE 32
#define IOCIRCULARDATAQUEUE_STATE_SEQNUM_SIZE 64
//#define IOCIRCULARDATAQUEUE_STATE_RESERVED_SIZE                                                                  \
//    ((8 * sizeof(__uint128_t)) - IOCIRCULARDATAQUEUE_STATE_WRITE_SIZE                                            \
//     - IOCIRCULARDATAQUEUE_STATE_GENERATION_SIZE - IOCIRCULARDATAQUEUE_STATE_WRITEINDEX_SIZE                \
//     - IOCIRCULARDATAQUEUE_STATE_SEQNUM_SIZE)

typedef union {
	__uint128_t val;
	struct {
		__uint128_t seqNum : IOCIRCULARDATAQUEUE_STATE_SEQNUM_SIZE; // Sequence Number
		__uint128_t wrIndex : IOCIRCULARDATAQUEUE_STATE_WRITEINDEX_SIZE; // write index
		__uint128_t generation : IOCIRCULARDATAQUEUE_STATE_GENERATION_SIZE; // generation
		// Fahad: We may not need reset.
		__uint128_t rstStatus : IOCIRCULARDATAQUEUE_STATE_RESET_SIZE; // queue reset status
		//        __uint128_t _rsvd : IOCIRCULARDATAQUEUE_STATE_RESERVED_SIZE; // reserved
		__uint128_t wrStatus : IOCIRCULARDATAQUEUE_STATE_WRITE_SIZE; // queue writing status
	} fields;
} IOCircularDataQueueState;

#define IOCIRCULARDATAQUEUE_STATE_WRITE_INPROGRESS (1)
#define IOCIRCULARDATAQUEUE_STATE_WRITE_COMPLETE (0)
#define IOCIRCULARDATAQUEUE_STATE_RESET_INPROGRESS (1)
#define IOCIRCULARDATAQUEUE_STATE_RESET_COMPLETE (0)

// #define IOCircularDataQueueStateGeneration              (((uint32_t)1 << 30) - 1)
#define IOCIRCULARDATAQUEUE_STATE_GENERATION_MAX (((uint32_t)1 << 30))

// static_assert(IOCIRCULARDATAQUEUE_STATE_RESERVED_SIZE > 0,
//               "unexpected reserved field size");

static_assert(IOCIRCULARDATAQUEUE_STATE_GENERATION_SIZE == IOCIRCULARDATAQUEUE_ENTRY_STATE_GENERATION_SIZE,
    "mismatched generation sizes");
static_assert(IOCIRCULARDATAQUEUE_STATE_SEQNUM_SIZE == IOCIRCULARDATAQUEUE_ENTRY_STATE_SEQNUM_SIZE,
    "mismatched sequenece number sizes");

/*!
 * @typedef IOCircularDataQueueMemory
 * @abstract The queue memory header present at the start of  queue shared memory region.
 * @discussion The queue memory header contains the queue info and state and is followed by the data region of the
 * queue.
 * @field sentinel unique value when the queue was created.
 * @field allocMemSize the allocated memory size of the queue including the queue header and the entries
 * @field memorySize  the memory size of the queue excluding the queue header
 * @field entryDataSize size of each entry in the queue including the entry header. The size is a multiple of 8 bytes
 * @field dataSize size of each entry in the queue excluding the entry header.
 * @field numEntries the number of fixed entries in the queue
 * @field `_padding` memory padding for alingment.
 * @field state the current state of the queue.
 * @field entries Represents the beginning of the data region.  The address of the data field is a pointer to the start
 * of the queue data region.
 */

typedef struct IOCircularDataQueueMemory {
	uint64_t sentinel;
	uint64_t _padding; // since we want it to be 16 bytes aligned below this
	union {
		volatile _Atomic __uint128_t queueStateVal;           // needs to be 16 bytes aligned.
		IOCircularDataQueueState  __queueState;               // for clarity, unused
	};
	IOCircularDataQueueEntryHeader entries[1]; // Entries begin.  Aligned to 16 bytes.
} IOCircularDataQueueMemory;

#define CIRCULAR_DATA_QUEUE_MEMORY_HEADER_SIZE                                                                         \
    (sizeof(IOCircularDataQueueMemory) - sizeof(IOCircularDataQueueEntryHeader))

/*!
 * @typedef IOCircularDataQueueMemoryCursor
 * @abstract The circular data queue cursor struct.
 * @discussion This struct represents a readers reference to a position in the queue. Each client holds an instance of
 * this in its process indicating its current reading position in the queue. The cursor holds uniqely identifying
 * information for the queue entry.
 * @field generation  the generation for the entry data at the position in the queue. This generation is only changed
 * when the queue is reset.
 * @field position the position in the queue  the cursor is at
 * @field sequenceNum  The unique number for the data at the  cursor position. The sequence number is unique for each
 * entry in the queue.
 *
 */
typedef struct IOCircularDataQueueMemoryCursor {
	uint32_t generation; // uint32_t seems a little excessive right now, since we dont expect these many resets. but
	                     // lets leave it for now.
	uint32_t position;
	uint64_t sequenceNum;
} IOCircularDataQueueMemoryCursor;


/*!
 * @typedef IOCircularDataQueueDescription
 * @abstract The circular data queue header shadow struct.
 * @discussion This struct represents the queue header shadow. Each client has a copy of this struct in its process .
 * This is used to detect any memory corruption of the shared memory queue header. This struct needs to be shared from
 * the creator of the queue to the clients via an out of band mechanism.
 * @field sentinel unique value written to the queue header memory and each queue entry.
 * @field allocMemSize the allocated memory size of the queue including the queue header
 * @field entryDataSize size of each entry in the queue including the entry header. The size is a multiple of 8 bytes
 * @field memorySize  the memory size of the queue excluding the queue header
 * @field numEntries the number of fixed entries in the queue
 * IOCircularDataQueueDescription
 */
typedef struct IOCircularDataQueueDescription {
	uint64_t sentinel;
	uint32_t allocMemSize; // total allocated size of the queue including the queue header.
	uint32_t entryDataSize; // size of each queue entry including the per entry header.
	uint32_t memorySize; // memory size of the queue (excluding the queue header)
	uint32_t numEntries;
	uint32_t dataSize; // the client provided data size excluding the per entry header.
	uint32_t padding;
} IOCircularDataQueueDescription;

#define kIOCircularQueueDescriptionKey  "IOCircularQueueDescription"


#if !KERNEL
/*
 * IORound and IOTrunc convenience functions, in the spirit
 * of vm's round_page() and trunc_page().
 */
#define IORound(value, multiple) ((((value) + (multiple)-1) / (multiple)) * (multiple))

#define IONew(type, count) (type *)calloc(count, sizeof(type))
#define IODelete(p, type, count) free(p)

// libkern/os/base.h
#if __has_feature(ptrauth_calls)
#include <ptrauth.h>
#define OS_PTRAUTH_SIGNED_PTR(type) __ptrauth(ptrauth_key_process_independent_data, 1, ptrauth_string_discriminator(type))
#define OS_PTRAUTH_SIGNED_PTR_AUTH_NULL(type) __ptrauth(ptrauth_key_process_independent_data, 1, ptrauth_string_discriminator(type), "authenticates-null-values")
#define OS_PTRAUTH_DISCRIMINATOR(str) ptrauth_string_discriminator(str)
#define __ptrauth_only
#else //  __has_feature(ptrauth_calls)
#define OS_PTRAUTH_SIGNED_PTR(type)
#define OS_PTRAUTH_SIGNED_PTR_AUTH_NULL(type)
#define OS_PTRAUTH_DISCRIMINATOR(str) 0
#define __ptrauth_only __unused
#endif // __has_feature(ptrauth_calls)
#endif /* !KERNEL */

#pragma mark - Debugging

#define QUEUE_FORMAT "Queue(%" PRIu64 " gen:%" PRIu64 " pos:%" PRIu64 " next:%" PRIu64 ")"
#define QUEUE_ARGS(q) q->guard, q->generation, q->fixed.latestIndex, q->fixed.writingIndex

#define CURSOR_FORMAT "Cursor(%p gen:%" PRIu64 " pos:%" PRIu64 ")"
#define CURSOR_ARGS(c) c, c->generation, c->position

#define ENTRY_FORMAT "Entry(%" PRIu64 " gen:%" PRIu64 " pos:%" PRIu64 ")"
#define ENTRY_ARGS(e) e->guard, e->generation, e->position

#if 1
#define queue_debug_error(fmt, ...)
#define queue_debug_note(fmt, ...)
#define queue_debug_trace(fmt, ...)
#else
#define queue_debug_error(fmt, ...)                                                                                    \
    {                                                                                                                  \
	os_log_debug(LOG_QUEUE, "#ERROR %s:%d %s " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__);                  \
    }
#define queue_debug_note(fmt, ...)                                                                                     \
    {                                                                                                                  \
	os_log_debug(LOG_QUEUE, "#NOTE %s:%d %s " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__);                   \
    }
#define queue_debug_trace(fmt, ...)                                                                                    \
    {                                                                                                                  \
	os_log_debug(LOG_QUEUE, "#TRACE %s:%d %s " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__);                  \
    }
#endif

#if HEADER_16BYTE_ALIGNED
static_assert(offsetof(IOCircularDataQueueEntryHeader, data) % sizeof(__uint128_t) == 0,
    "IOCircularDataQueueEntryHeader.data is not 16-byte aligned!");
#else
static_assert(offsetof(IOCircularDataQueueEntryHeader, data) % sizeof(uint64_t) == 0,
    "IOCircularDataQueueEntryHeader.data is not 8-byte aligned!");
#endif

static_assert(sizeof(IOCircularDataQueueState) == sizeof(__uint128_t), "Unexpected padding");
static_assert(offsetof(IOCircularDataQueueMemory, queueStateVal) % sizeof(__uint128_t) == 0,
    "IOCircularDataQueueMemory.entries is not 16-byte aligned!");

#if HEADER_16BYTE_ALIGNED
static_assert(offsetof(IOCircularDataQueueMemory, entries) % sizeof(__uint128_t) == 0,
    "IOCircularDataQueueMemory.entries is not 16-byte aligned!");
#else
static_assert(offsetof(IOCircularDataQueueMemory, entries) % sizeof(uint64_t) == 0,
    "IOCircularDataQueueMemory.entries is not 8-byte aligned!");
#endif

/*!
 * @typedef IOCircularDataQueue
 * @abstract A fixed entry size circular queue that supports multiple concurrent readers and a single writer.
 * @discussion The queue currently supports fixed size entries. The queue memory size is configured at init when the
 * number of entries and size of each entry is specifiied and cannot be resized later. Since the queue is a circular
 * buffer, the writer can potentially overwrite an entry while a reader is still reading it. The queue provides facility
 * to check for data integrity after reading the entry is complete. There is no support for sending notifications to
 * readers when data is enqueued into an empty queue by the writer. The queue supports a "pull model" for reading data
 * from the queue. The queue can be used for passing data from user space to kernel and vice-versa.
 * @field queueHeaderShadow    The queue header shadow
 * @field queueCursor    The queue cursor
 * @field isQueueMemoryAllocated    Represents if the queue memory is allocated or if the queue uses a previously
 * created queue memory region.
 * @field queueMemory    Pointer to the queue shared memory region
 */
typedef struct IOCircularDataQueue {
	IOCircularDataQueueMemoryCursor queueCursor;
	IOCircularDataQueueMemory * OS_PTRAUTH_SIGNED_PTR("IOCircularDataQueue.queueMemory") queueMemory;
	IOCircularDataQueueDescription queueHeaderShadow;
#if KERNEL
	IOBufferMemoryDescriptor * OS_PTRAUTH_SIGNED_PTR("IOCircularDataQueue.iomd") iomd;
#else /* KERNEL */
	io_connect_t connect;
	uint32_t memoryType;
#endif /* !KERNEL */
} IOCircularDataQueue;


#if defined(__arm64__) && !KERNEL
#define ATTR_LSE2 __attribute__((target("lse2")))
#else
#define ATTR_LSE2
#endif /* defined(__arm64__) && !KERNEL */

#pragma mark - Queue

static bool ATTR_LSE2
_isQueueMemoryCorrupted(IOCircularDataQueue *queue)
{
	IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;

	const size_t queueSentinel = queueMemory->sentinel;
	if (os_unlikely(queueSentinel != queueHeaderShadow->sentinel)) {
		return true;
	}
	return false;
}

inline static bool ATTR_LSE2
_isCursorPositionInvalid(IOCircularDataQueue *queue)
{
//	IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;
	IOCircularDataQueueMemoryCursor const *cursor = &queue->queueCursor;

	if (os_unlikely(cursor->position >= queueHeaderShadow->numEntries)) {
		return true;
	}

	return false;
}

inline __unused static bool ATTR_LSE2
_isEntryOutOfBounds(IOCircularDataQueue *queue, IOCircularDataQueueEntryHeader *entry)
{
	IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;
//	IOCircularDataQueueMemoryCursor const *cursor = &queue->queueCursor;

	bool ret = false;
	IOCircularDataQueueEntryHeader *firstEntry = (IOCircularDataQueueEntryHeader *)(&queueMemory->entries[0]);
	IOCircularDataQueueEntryHeader *lastEntry
	        = (IOCircularDataQueueEntryHeader *)(uintptr_t)((uint8_t *)&queueMemory->entries[0]
	    + ((queueHeaderShadow->numEntries - 1) * queueHeaderShadow->entryDataSize));

	// SANITY CHECK - Final check to ensure the 'entry' pointer is
	// within the queueMemory allocation before we begin writing.
	if (os_unlikely(entry < firstEntry || entry > lastEntry)) {
		ret = true;
	}

	return ret;
}


#if !KERNEL
/*!
 * @function isQueueMemoryValid
 * Verify if the queue header shadow matches the queue header in shared memory.
 * @param queue Handle to the queue.
 * @return `true` if the queue header shadow matches the queue header in shared memory, else `false`.
 *
 */

static bool ATTR_LSE2
isQueueMemoryValid(IOCircularDataQueue *queue)
{
	return _isQueueMemoryCorrupted(queue) == false;
}
#endif /* KERNEL */

/*!
 * @function destroyQueueMem
 * @abstract Function that destroys a previously created IOCircularDataQueueMemory instance.
 * @param queue Handle to the queue.
 *  @return
 *  - `kIOReturnSuccess` if the queue was succesfully destroyed.
 *  - `kIOReturnBadArgument` if an invalid queue was provided.
 */

static IOReturn ATTR_LSE2
destroyQueueMem(IOCircularDataQueue *queue)
{
	IOReturn ret = kIOReturnBadArgument;
	if (queue != NULL) {
#if KERNEL
		OSSafeReleaseNULL(queue->iomd);
#else /* !KERNEL */
		IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
		IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;
		if (queueMemory) {
			ret = IOConnectUnmapMemory(queue->connect, queue->memoryType,
			    mach_task_self(), (mach_vm_address_t) queueMemory);
//			assert(KERN_SUCCESS == ret);
			queue->queueMemory = NULL;
		}
#endif
		ret = kIOReturnSuccess;
	}

	return ret;
}

static IOReturn ATTR_LSE2
_reset(IOCircularDataQueue *queue)
{
	IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;

	if (queueMemory == NULL || queueHeaderShadow == NULL) {
		return kIOReturnBadArgument;
	}

	const size_t queueEntryDataSize = queueHeaderShadow->entryDataSize;
	if (!queueEntryDataSize) {
		return kIOReturnUnsupported;
	}

	IOCircularDataQueueState currState;
	currState.val = atomic_load_explicit(&queueMemory->queueStateVal, memory_order_acquire);

	if (os_unlikely(currState.fields.wrStatus & IOCIRCULARDATAQUEUE_STATE_WRITE_INPROGRESS)) {
		// Another thread is modifying the queue
		return kIOReturnBusy;
	}

	uint32_t currGeneration = currState.fields.generation;
	uint32_t newGen = (currGeneration + 1) % IOCIRCULARDATAQUEUE_STATE_GENERATION_MAX;

	IOCircularDataQueueState newState;
	newState.fields.generation = newGen;
	newState.fields.wrIndex = 0;
	newState.fields.seqNum = UINT64_MAX; // since we first increment the seq num on an enqueue.

	if (!atomic_compare_exchange_strong(&queueMemory->queueStateVal, &currState.val, newState.val)) {
		return kIOReturnBusy;
	}

	if (os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	queue_debug_trace("Reset " QUEUE_FORMAT, QUEUE_ARGS(queueMemory));
	return kIOReturnSuccess;
}

/*!
 * @function _enqueueInternal
 * @abstract Internal function for enqueuing a new entry on the queue.
 * @discussion This method adds a new data entry of dataSize to the queue.  It sets the size parameter of the entry
 * pointed to by the tail value and copies the memory pointed to by the data parameter in place in the queue.  Once that
 * is done, it moves the tail to the next available location.  When attempting to add a new entry towards the end of the
 * queue and there isn't enough space at the end, it wraps back to the beginning.<br>
 * @param queue Handle to the queue.
 * @param data Pointer to the data to be added to the queue.
 * @param dataSize Size of the data pointed to by data.
 * @param earlyExitForTesting ealy exit flag used for testing only.
 *  @return
 *  - `kIOReturnSuccess` on success.
 *  - Other values indicate an error.
 */

static IOReturn ATTR_LSE2
_enqueueInternal(IOCircularDataQueue *queue,
    const void *data,
    size_t dataSize,
    int earlyExitForTesting)
{
	IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;
//	IOCircularDataQueueMemoryCursor const *cursor = &queue->queueCursor;

	if (queueMemory == NULL || data == NULL || dataSize == 0 || queueHeaderShadow == NULL) {
		return kIOReturnBadArgument;
	}

	if (os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	if (os_unlikely(dataSize > queueHeaderShadow->dataSize)) {
		return kIOReturnBadArgument;
	}

	const size_t queueEntryDataSize = queueHeaderShadow->entryDataSize;

	if (!queueEntryDataSize) {
		return kIOReturnUnsupported;
	}

	const size_t queueAllocMemSize = queueHeaderShadow->allocMemSize;
	const uint32_t queueNumEntries = queueHeaderShadow->numEntries;

	// Do not allow instruction re-ordering prior to the header check.
	os_compiler_barrier();

	IOCircularDataQueueState currState;
	currState.val = atomic_load_explicit(&queueMemory->queueStateVal, memory_order_acquire);

	if (os_unlikely(currState.fields.wrStatus & IOCIRCULARDATAQUEUE_STATE_WRITE_INPROGRESS)) {
		// Another thread is modifying the queue
		return kIOReturnBusy;
	}

	//            size_t queueEntriesBufferSize = queueMemory->allocMemSize - CIRCULAR_DATA_QUEUE_MEMORY_HEADER_SIZE;
	uint32_t writeIndex = currState.fields.wrIndex;
	uint64_t nextWriteIndex = (writeIndex + 1) % queueNumEntries;
	uint64_t nextSeqNum = currState.fields.seqNum + 1;
	if (os_unlikely(nextSeqNum == UINT64_MAX)) {
		// End of the world. How many enqueues are you trying to do !!!
//        abort();
		return kIOReturnOverrun;
	}

	__auto_type entry
	        = (IOCircularDataQueueEntryHeader *)(uintptr_t)((uint8_t *)&queueMemory->entries[0] + (writeIndex * queueEntryDataSize));
	//                printf("entry=%p\n", (void *)entry);

	// SANITY CHECK - Final check to ensure the 'entry' pointer is
	// within the queueMemory allocation before we begin writing.
	if (os_unlikely((uint8_t *)entry < (uint8_t *)(&queueMemory->entries[0])
	    || (uint8_t *)entry >= (uint8_t *)queueMemory + queueAllocMemSize)) {
		return kIOReturnBadArgument;
	}

	//            if (os_unlikely(_isEntryOutOfBounds(queueHeaderShadow, queueMemory, entry) )) {
	//                ret = kIOReturnBadArgument;
	//                break;
	//            }

	os_compiler_barrier();

	// All checks passed. Set the write bit.

	IOCircularDataQueueState newState = currState;
	newState.fields.wrStatus = IOCIRCULARDATAQUEUE_STATE_WRITE_INPROGRESS;
	// lets not change the writeIndex and seq num here.
	//            newState.fields.wrIndex = nextWriteIndex;
	//    newState.fields.seqNum = currState.fields.seqNum + 1; // its ok even if we ever rollover UINT64_MAX!!

	if (!atomic_compare_exchange_strong(&queueMemory->queueStateVal, &currState.val, newState.val)) {
		// someone else is modifying the queue
		return kIOReturnBusy;
	}

	// Update the entry header info
	IOCircularDataQueueEntryHeaderInfo enHeaderInfo;
	enHeaderInfo.val = 0;
	enHeaderInfo.fields.wrStatus = IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_INPROGRESS;
	enHeaderInfo.fields.generation = currState.fields.generation;
	//    enHeaderInfo.fields.seqNum = newState.fields.seqNum;
	enHeaderInfo.fields.seqNum = nextSeqNum;
	enHeaderInfo.fields.dataSize = dataSize;
	atomic_store_explicit(&entry->headerInfoVal, enHeaderInfo.val, memory_order_release);

	entry->sentinel = queueHeaderShadow->sentinel;
	memcpy(entry->data, data, dataSize);
	enHeaderInfo.fields.wrStatus = IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_COMPLETE;
	atomic_store_explicit(&entry->headerInfoVal, enHeaderInfo.val, memory_order_release);

	IOCircularDataQueueState finalState = newState;
	finalState.fields.wrStatus = IOCIRCULARDATAQUEUE_STATE_WRITE_COMPLETE;
	// Lets actually update the write index and seq num
	finalState.fields.wrIndex = nextWriteIndex;
	finalState.fields.seqNum = nextSeqNum;
	atomic_store_explicit(&queueMemory->queueStateVal, finalState.val, memory_order_release);

	if (os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	return kIOReturnSuccess;
}

/*!
 * @function enqueueQueueMem
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

static IOReturn ATTR_LSE2
enqueueQueueMem(IOCircularDataQueue *queue,
    const void *data,
    size_t dataSize)
{
	return _enqueueInternal(queue, data, dataSize, 0);
}

/*!
 * @function isDataEntryValidInQueueMem
 * Verify if the data at the cursor position is still valid. Call this function after having read the data from the
 * queue, since the buffer could potentially have been overwritten while being read. <br>
 * @param queue Handle to the queue.
 *  @return
 *  - `kIOReturnSuccess` if the data at the cursor position was valid.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer valid and is
 *     potentially overwritten. Call getLatestInQueueMem to get the latest data and cursor position.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnBadArgument` if an invalid param was passed.
 *  - `kIOReturnBadMedia` if the queueMemory is corrupted.
 *
 */

static IOReturn ATTR_LSE2
isDataEntryValidInQueueMem(IOCircularDataQueue *queue)
{
	IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;
	IOCircularDataQueueMemoryCursor const *cursor = &queue->queueCursor;

	if (os_unlikely(queueMemory == NULL || queueHeaderShadow == NULL)) {
		return kIOReturnBadArgument;
	}

	if (os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	if (os_unlikely(_isCursorPositionInvalid(queue))) {
		return kIOReturnBadArgument;
	}

	IOCircularDataQueueState currState;
	currState.val = atomic_load_explicit(&queueMemory->queueStateVal, memory_order_acquire);

	// Fahad: We may remove this filed since we don't actually use it. Instead just use generation check below.
	if (os_unlikely(currState.fields.rstStatus & IOCIRCULARDATAQUEUE_STATE_RESET_INPROGRESS)) {
		// Another thread is resetting the queue
		return kIOReturnBusy;
	}

	uint32_t queueGeneration = currState.fields.generation;
	if (queueGeneration != cursor->generation) {
		//        return kIOReturnOverrun;
		return kIOReturnAborted;
	}

	const size_t queueAllocMemSize = queueHeaderShadow->allocMemSize;
	const size_t queueEntryDataSize = queueHeaderShadow->entryDataSize;
	__auto_type entry = (IOCircularDataQueueEntryHeader *)(uintptr_t)((uint8_t *)&queueMemory->entries[0]
	    + (cursor->position * queueEntryDataSize));

	// SANITY CHECK - Final check to ensure the 'entry' pointer is
	// within the queueMemory entries buffer before we begin writing.
	if (os_unlikely((uint8_t *)entry < (uint8_t *)(&queueMemory->entries[0])
	    || (uint8_t *)entry >= (uint8_t *)queueMemory + queueAllocMemSize)) {
		queue_debug_error("Out of Bounds! " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT, QUEUE_ARGS(queueMemory),
		    CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnBadArgument;
	}

	os_compiler_barrier();

	if (os_unlikely(entry->sentinel != queueHeaderShadow->sentinel)) {
		queue_debug_error("entry->sentinel != queueMemory->sentinel " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT,
		    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnBadMedia;
	}

	IOCircularDataQueueEntryHeaderInfo enHeaderInfo;
	enHeaderInfo.val = atomic_load_explicit(&entry->headerInfoVal, memory_order_acquire);
	uint32_t entryGeneration = enHeaderInfo.fields.generation;
	if (os_unlikely(entryGeneration != queueGeneration)) {
		queue_debug_note("entryGeneration != queueGeneration " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT,
		    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnOverrun;
	}

	if (os_unlikely(enHeaderInfo.fields.wrStatus == IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_INPROGRESS
	    || enHeaderInfo.fields.seqNum != cursor->sequenceNum)) {
		return kIOReturnOverrun;
	}

	if (os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	return kIOReturnSuccess;
}

/*!
 * @function setCursorLatestInQueueMem
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

static IOReturn ATTR_LSE2
setCursorLatestInQueueMem(IOCircularDataQueue *queue)
{
	IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;
	IOCircularDataQueueMemoryCursor *cursor = &queue->queueCursor;

	if (queueMemory == NULL || queueHeaderShadow == NULL) {
		return kIOReturnBadArgument;
	}

	if (os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	const size_t queueAllocMemSize = queueHeaderShadow->allocMemSize;
	const size_t queueEntryDataSize = queueHeaderShadow->entryDataSize;

	IOCircularDataQueueState currState;
	currState.val = atomic_load_explicit(&queueMemory->queueStateVal, memory_order_acquire);

	if (os_unlikely(currState.fields.rstStatus & IOCIRCULARDATAQUEUE_STATE_RESET_INPROGRESS)) {
		// Another thread is resetting the queue
		return kIOReturnBusy;
	}

	if (os_unlikely(currState.fields.seqNum == UINT64_MAX)) {
		// Nothing has ever been written to the queue yet.
		return kIOReturnUnderrun;
	}

	uint32_t queueGeneration = currState.fields.generation;
	uint32_t readIndex
	        = (currState.fields.wrIndex > 0) ? (currState.fields.wrIndex - 1) : (queueHeaderShadow->numEntries - 1);

	__auto_type entry
	        = (IOCircularDataQueueEntryHeader *)(uintptr_t)((uint8_t *)&queueMemory->entries[0] + (readIndex * queueEntryDataSize));

	// SANITY CHECK - Final check to ensure the 'entry' pointer is
	// within the queueMemory entries buffer before we begin writing.
	if (os_unlikely((uint8_t *)entry < (uint8_t *)(&queueMemory->entries[0])
	    || (uint8_t *)entry >= (uint8_t *)queueMemory + queueAllocMemSize)) {
		queue_debug_error("Out of Bounds! " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT, QUEUE_ARGS(queueMemory),
		    CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnAborted;
	}

	os_compiler_barrier();

	if (os_unlikely(entry->sentinel != queueHeaderShadow->sentinel)) {
		queue_debug_error("entry->sentinel != queueMemory->sentinel " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT,
		    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnBadMedia;
	}

	IOCircularDataQueueEntryHeaderInfo enHeaderInfo;
	enHeaderInfo.val = atomic_load_explicit(&entry->headerInfoVal, memory_order_acquire);
	uint32_t entryGeneration = enHeaderInfo.fields.generation;
	if (os_unlikely(entryGeneration != queueGeneration)) {
		queue_debug_note("entryGeneration != queueGeneration " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT,
		    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnAborted;
	}

	cursor->position = readIndex;
	cursor->generation = entryGeneration;
	cursor->sequenceNum = enHeaderInfo.fields.seqNum;

	return kIOReturnSuccess;
}

static IOReturn ATTR_LSE2
_getLatestInQueueMemInternal(IOCircularDataQueue *queue,
    void **data,
    size_t *size,
    bool copyMem)
{
	IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;
	IOCircularDataQueueMemoryCursor *cursor = &queue->queueCursor;

	IOReturn ret = kIOReturnTimeout;
	if (queueMemory == NULL || data == NULL || size == NULL || queueHeaderShadow == NULL) {
		return kIOReturnBadArgument;
	}

	if (os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	const size_t kNumRetries = 5; // Number of retries if the latest index data gets overwritten by a writer.
	size_t retry = kNumRetries;
	const size_t queueAllocMemSize = queueHeaderShadow->allocMemSize;
	const size_t queueEntryDataSize = queueHeaderShadow->entryDataSize;
	size_t inSize;

	inSize = *size;
	do {
		*size = 0;
		retry--;
		IOCircularDataQueueState currState;
		currState.val = atomic_load_explicit(&queueMemory->queueStateVal, memory_order_consume);

		if (os_unlikely(currState.fields.rstStatus & IOCIRCULARDATAQUEUE_STATE_RESET_INPROGRESS)) {
			// Another thread is resetting the queue
			return kIOReturnBusy;
		}

		if (os_unlikely(currState.fields.seqNum == UINT64_MAX)) {
			// Nothing has ever been written to the queue yet.
			return kIOReturnUnderrun;
		}

		uint32_t queueGeneration = currState.fields.generation;
		uint32_t readIndex
		        = (currState.fields.wrIndex > 0) ? (currState.fields.wrIndex - 1) : (queueHeaderShadow->numEntries - 1);

		__auto_type entry = (IOCircularDataQueueEntryHeader *)(uintptr_t)((uint8_t *)&queueMemory->entries[0]
		    + (readIndex * queueEntryDataSize));

		// SANITY CHECK - Final check to ensure the 'entry' pointer is
		// within the queueMemory entries buffer before we begin writing.
		if (os_unlikely((uint8_t *)entry < (uint8_t *)(&queueMemory->entries[0])
		    || (uint8_t *)entry >= (uint8_t *)queueMemory + queueAllocMemSize)) {
			queue_debug_error("Out of Bounds! " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT,
			    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
			return kIOReturnBadArgument;
		}

		os_compiler_barrier();

		if (os_unlikely(entry->sentinel != queueHeaderShadow->sentinel)) {
			queue_debug_error("entry->sentinel != queueMemory->sentinel " QUEUE_FORMAT " " CURSOR_FORMAT
			    " " ENTRY_FORMAT,
			    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
			return kIOReturnBadMedia;
		}

		IOCircularDataQueueEntryHeaderInfo enHeaderInfo;
		enHeaderInfo.val = atomic_load_explicit(&entry->headerInfoVal, memory_order_acquire);
		uint32_t entryGeneration = enHeaderInfo.fields.generation;
		/* Since the time we read the queue header, was the queue
		 *   - reset
		 *   - the entry is being overwritten
		 *   - the entry was overwritten and hence the seq numbers don't match anymore.
		 *
		 *  Lets retry in such a case
		 */
		if (os_unlikely(entryGeneration != queueGeneration
		    || enHeaderInfo.fields.wrStatus == IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_INPROGRESS
		    || currState.fields.seqNum != enHeaderInfo.fields.seqNum)) {
			continue;
		}

		cursor->position = readIndex;
		cursor->generation = entryGeneration;
		cursor->sequenceNum = enHeaderInfo.fields.seqNum;

		if (os_unlikely(enHeaderInfo.fields.dataSize > queueHeaderShadow->entryDataSize)) {
			ret = kIOReturnOverrun;
			break;
		}
		*size = enHeaderInfo.fields.dataSize;

		if (!copyMem) {
			*data = entry->data;
			ret = kIOReturnSuccess;
			break; // break out, we're done
		} else {
			if (os_unlikely(enHeaderInfo.fields.dataSize > inSize)) {
				return kIOReturnOverrun;
			}
			memcpy(*data, entry->data, enHeaderInfo.fields.dataSize);
			// Lets re-verify after the memcpy if the buffer is/has been overwritten.

			IOCircularDataQueueEntryHeaderInfo enHeaderInfoAfter;
			enHeaderInfoAfter.val = atomic_load_explicit(&entry->headerInfoVal, memory_order_acquire);
			// Did something change ?
			if (enHeaderInfo.val == enHeaderInfoAfter.val) {
				ret = kIOReturnSuccess;
				break;
			} else {
				// we failed so we'll retry.
				*size = 0;
			}
		}
	} while (retry);

	if ((kIOReturnSuccess == ret) && os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	return ret;
}

/*!
 * @function getLatestInQueueMem
 * Access the latest entry data, also update the cursor position to the latest. No copy is made of the data. <br> Caller
 * is supposed to call isDataEntryValidInQueueMem() to check data integrity after reading the data is complete.
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
 *
 */

static IOReturn ATTR_LSE2
getLatestInQueueMem(IOCircularDataQueue *queue,
    void **data,
    size_t *size)
{
	return _getLatestInQueueMemInternal(queue, data, size, false);
}

/*!
 * @function copyLatestInQueueMem
 *  Access the latest entry data and copy into the provided buffer. Also update the cursor position to the latest.
 * Function gaurantees that the new data returned is always valid hence no need to call isDataEntryValidInQueueMem().
 * @param queue Handle to the queue.
 * @param data Pointer to memory into which the latest data from the queue is copied. Lifetime of this memory is
 * controlled by the caller.
 * @param size Size of the data buffer provided for copying. On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated.
 *  - `kIOReturnUnderrun` if nothing has ever been enqueued into the queue
 *  - `kIOReturnBadArgument` if the buffer provided to copy the data is NULL or  if an invalid queue was provided..
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - `kIOReturnTimeout` if the reader timed out when trying to copy the latest data. This is possible if the writer
 * overwrites the latest index a reader is about to copy. The function times out if the copy is unsuccessful after
 * multiple retries.
 *  - Other values indicate an error.
 *
 */

static IOReturn ATTR_LSE2
copyLatestInQueueMem(IOCircularDataQueue *queue,
    void *data,
    size_t *size)
{
	return _getLatestInQueueMemInternal(queue, &data, size, true);
}

static IOReturn ATTR_LSE2
_getNextInQueueMemInternal(IOCircularDataQueue *queue,
    void **data,
    size_t *size,
    bool copyMem)
{
	IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;
	IOCircularDataQueueMemoryCursor *cursor = &queue->queueCursor;

	IOReturn ret = kIOReturnError;
	size_t inSize;

	if (queueMemory == NULL || data == NULL || size == NULL || queueHeaderShadow == NULL) {
		return kIOReturnBadArgument;
	}

	inSize = *size;
	*size = 0;

	if (os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	if (os_unlikely(_isCursorPositionInvalid(queue))) {
		return kIOReturnAborted;
	}

	const size_t queueAllocMemSize = queueHeaderShadow->allocMemSize;
	const size_t queueEntryDataSize = queueHeaderShadow->entryDataSize;

	IOCircularDataQueueState currState;
	currState.val = atomic_load_explicit(&queueMemory->queueStateVal, memory_order_acquire);

	if (os_unlikely(currState.fields.rstStatus & IOCIRCULARDATAQUEUE_STATE_RESET_INPROGRESS)) {
		// Another thread is resetting the queue
		return kIOReturnBusy;
	}

	uint32_t queueGeneration = currState.fields.generation;

	// was the queue reset ?
	if (os_unlikely(cursor->generation != queueGeneration || cursor->sequenceNum > currState.fields.seqNum)) {
		return kIOReturnAborted;
	}

	if (os_unlikely(currState.fields.seqNum == UINT64_MAX)) {
		// Nothing has ever been written to the queue yet.
		return kIOReturnUnderrun;
	}

	// nothing new written or an active write is in progress for the next entry.
	if (os_unlikely(cursor->sequenceNum == currState.fields.seqNum
	    || ((cursor->sequenceNum + 1) == currState.fields.seqNum
	    && currState.fields.wrStatus == IOCIRCULARDATAQUEUE_STATE_WRITE_INPROGRESS))) {
		return kIOReturnUnderrun;
	}

	uint32_t nextIndex = (cursor->position + 1) % queueHeaderShadow->numEntries;
	__auto_type entry
	        = (IOCircularDataQueueEntryHeader *)(uintptr_t)((uint8_t *)&queueMemory->entries[0] + (nextIndex * queueEntryDataSize));

	// SANITY CHECK - Final check to ensure the 'entry' pointer is
	// within the queueMemory entries buffer before we begin writing.
	if (os_unlikely((uint8_t *)entry < (uint8_t *)(&queueMemory->entries[0])
	    || (uint8_t *)entry >= (uint8_t *)queueMemory + queueAllocMemSize)) {
		queue_debug_error("Out of Bounds! " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT, QUEUE_ARGS(queueMemory),
		    CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnBadArgument;
	}

	os_compiler_barrier();

	if (os_unlikely(entry->sentinel != queueHeaderShadow->sentinel)) {
		queue_debug_error("entry->sentinel != queueMemory->sentinel " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT,
		    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnBadMedia;
	}

	IOCircularDataQueueEntryHeaderInfo enHeaderInfo;
	enHeaderInfo.val = atomic_load_explicit(&entry->headerInfoVal, memory_order_acquire);
	uint32_t entryGeneration = enHeaderInfo.fields.generation;
	if (os_unlikely(entryGeneration != queueGeneration)) {
		queue_debug_note("entryGeneration != queueGeneration " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT,
		    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnAborted;
	}

	// is the entry currently being written to or has the cursor fallen too far behind and the cursor is no longer
	// valid.
	if (os_unlikely(enHeaderInfo.fields.wrStatus == IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_INPROGRESS
	    || enHeaderInfo.fields.seqNum != cursor->sequenceNum + 1)) {
		return kIOReturnOverrun;
	}

	cursor->position = nextIndex;
	cursor->generation = entryGeneration;
	cursor->sequenceNum = enHeaderInfo.fields.seqNum;

	if (os_unlikely(enHeaderInfo.fields.dataSize > queueHeaderShadow->entryDataSize)) {
		return kIOReturnOverrun;
	}
	*size = enHeaderInfo.fields.dataSize;

	if (!copyMem) {
		*data = entry->data;
		ret = kIOReturnSuccess;
	} else {
		if (os_unlikely(enHeaderInfo.fields.dataSize > inSize)) {
			return kIOReturnOverrun;
		}
		memcpy(*data, entry->data, enHeaderInfo.fields.dataSize);
		// Lets re-verify after the memcpy if the buffer is/has been overwritten.

		IOCircularDataQueueEntryHeaderInfo enHeaderInfoAfter;
		enHeaderInfoAfter.val = atomic_load_explicit(&entry->headerInfoVal, memory_order_acquire);
		// Did something change, while we were memcopying ?
		if (enHeaderInfo.val == enHeaderInfoAfter.val) {
			ret = kIOReturnSuccess;
		} else {
			// while we were memcopying, the writer wrapped around and is writing into our index. or the queue got reset
			*size = 0;
			ret = kIOReturnOverrun;
		}
	}

	if ((kIOReturnSuccess == ret) && os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	return ret;
}

/*!
 * @function getNextInQueueMem
 * Access the data at the next cursor position and updates the cursor position to the next. No copy is made of the data.
 * <br> Caller is supposed to call isDataEntryValidInQueueMem() to check data integrity after reading the data is
 * complete.
 * @param queue Handle to the queue.
 * @param data A pointer to the data memory region for the next entry data in the queue.
 * @param size A pointer to the size of the data parameter.  On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnUnderrun` if the cursor has reached the latest available data.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call getLatestInQueueMem to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */

static IOReturn ATTR_LSE2
getNextInQueueMem(IOCircularDataQueue *queue,
    void **data,
    size_t *size)
{
	return _getNextInQueueMemInternal(queue, data, size, false);
}

/*!
 * @function copyNextInQueueMem
 * Access the data at the next cursor position and copy into the provided buffer. Also update the cursor position to the
 * next. If successful, function gaurantees that the data returned is always valid hence no need to call
 * isDataEntryValidInQueueMem().
 * @param queue Handle to the queue.
 * @param data Pointer to memory into which the next data from the queue is copied. Lifetime of this memory is
 * controlled by the caller.
 * @param size Size of the data buffer provided for copying. On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnUnderrun` if the cursor has reached the latest available data.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call getLatestInQueueMem to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */

static IOReturn ATTR_LSE2
copyNextInQueueMem(IOCircularDataQueue *queue,
    void *data,
    size_t *size)
{
	return _getNextInQueueMemInternal(queue, &data, size, true);
}

/*!
 * @function getPrevInQueueMem
 * Access the data at the previous cursor position and updates the cursor position to the previous. No copy is made of
 * the data. <br> Caller is supposed to call isDataEntryValidInQueueMem() to check data integrity after reading the data
 * is complete.
 * @param queue Handle to the queue.
 * @param data A pointer to the data memory region for the previous entry data in the queue.
 * @param size A pointer to the size of the data parameter.  On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated to the previous.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call getLatestInQueueMem to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */

static IOReturn ATTR_LSE2
_getPrevInQueueMemInternal(IOCircularDataQueue *queue,
    void **data,
    size_t *size,
    bool copyMem)
{
	IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;
	IOCircularDataQueueMemoryCursor *cursor = &queue->queueCursor;
	size_t inSize;

	IOReturn ret = kIOReturnError;
	if (queueMemory == NULL || data == NULL || size == NULL || queueHeaderShadow == NULL) {
		return kIOReturnBadArgument;
	}

	inSize = *size;
	*size = 0;

	if (os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	if (os_unlikely(_isCursorPositionInvalid(queue))) {
		return kIOReturnAborted;
	}

	const size_t queueAllocMemSize = queueHeaderShadow->allocMemSize;
	const size_t queueEntryDataSize = queueHeaderShadow->entryDataSize;

	IOCircularDataQueueState currState;
	currState.val = atomic_load_explicit(&queueMemory->queueStateVal, memory_order_acquire);

	if (os_unlikely(currState.fields.rstStatus & IOCIRCULARDATAQUEUE_STATE_RESET_INPROGRESS)) {
		// Another thread is resetting the queue
		return kIOReturnBusy;
	}

	uint32_t queueGeneration = currState.fields.generation;

	// was the queue reset ?
	if (os_unlikely(cursor->generation != queueGeneration || cursor->sequenceNum > currState.fields.seqNum)) {
		return kIOReturnAborted;
	}

	if (os_unlikely(currState.fields.seqNum == UINT64_MAX)) {
		// Nothing has ever been written to the queue yet.
		return kIOReturnUnderrun;
	}

	uint32_t prevIndex = (cursor->position == 0) ? (queueHeaderShadow->numEntries - 1) : (cursor->position - 1);
	__auto_type entry
	        = (IOCircularDataQueueEntryHeader *)(uintptr_t)((uint8_t *)&queueMemory->entries[0] + (prevIndex * queueEntryDataSize));

	// SANITY CHECK - Final check to ensure the 'entry' pointer is
	// within the queueMemory entries buffer before we begin writing.
	if (os_unlikely((uint8_t *)entry < (uint8_t *)(&queueMemory->entries[0])
	    || (uint8_t *)entry >= (uint8_t *)queueMemory + queueAllocMemSize)) {
		queue_debug_error("Out of Bounds! " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT, QUEUE_ARGS(queueMemory),
		    CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnBadArgument;
	}

	os_compiler_barrier();

	IOCircularDataQueueEntryHeaderInfo enHeaderInfo;
	enHeaderInfo.val = atomic_load_explicit(&entry->headerInfoVal, memory_order_acquire);
	// is the entry currently being written to or this is the newest entry that was just written.
	if (os_unlikely(enHeaderInfo.fields.wrStatus == IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_INPROGRESS
	    || enHeaderInfo.fields.seqNum > cursor->sequenceNum)) {
		return kIOReturnOverrun;
	}

	uint32_t entryGeneration = enHeaderInfo.fields.generation;
	if (os_unlikely(entryGeneration != queueGeneration)) {
		queue_debug_note("entryGeneration != queueGeneration " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT,
		    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnOverrun;
	}

	// the sentinel has been corrupted.
	if (os_unlikely(entry->sentinel != queueHeaderShadow->sentinel)) {
		queue_debug_error("entry->sentinel != queueMemory->sentinel " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT,
		    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnBadMedia;
	}

	cursor->position = prevIndex;
	cursor->generation = entryGeneration;
	cursor->sequenceNum = enHeaderInfo.fields.seqNum;

	if (os_unlikely(enHeaderInfo.fields.dataSize > queueHeaderShadow->entryDataSize)) {
		return kIOReturnOverrun;
	}
	*size = enHeaderInfo.fields.dataSize;
	ret = kIOReturnSuccess;

	if (!copyMem) {
		*data = entry->data;
	} else {
		if (os_unlikely(enHeaderInfo.fields.dataSize > inSize)) {
			return kIOReturnOverrun;
		}
		memcpy(*data, entry->data, enHeaderInfo.fields.dataSize);
		// Lets re-verify after the memcpy if the buffer is/has been overwritten.

		IOCircularDataQueueEntryHeaderInfo enHeaderInfoAfter;
		enHeaderInfoAfter.val = atomic_load_explicit(&entry->headerInfoVal, memory_order_acquire);
		// Did something change, while we were memcopying ?
		if (enHeaderInfo.val != enHeaderInfoAfter.val) {
			// while we were memcopying, the writer wrapped around and is writing into our index. or the queue got reset
			*size = 0;
			ret = kIOReturnOverrun;
		}
	}

	if ((kIOReturnSuccess == ret) && os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	return ret;
}

static IOReturn ATTR_LSE2
getPrevInQueueMem(IOCircularDataQueue *queue,
    void **data,
    size_t *size)
{
	return _getPrevInQueueMemInternal(queue, data, size, false);
}

/*!
 * @function copyPrevInQueueMem
 * Access the data at the previous cursor position and copy into the provided buffer. Also update the cursor position to
 * the previous. If successful, function gaurantees that the data returned is always valid, hence no need to call
 * isDataEntryValidInQueueMem().
 * @param queue Handle to the queue.
 * @param data Pointer to memory into which the previous data is copied. Lifetime of this memory is controlled by the
 * caller.
 * @param size Size of the data buffer provided for copying. On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call getLatestInQueueMem to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */

static IOReturn ATTR_LSE2
copyPrevInQueueMem(IOCircularDataQueue *queue,
    void *data,
    size_t *size)
{
	return _getPrevInQueueMemInternal(queue, &data, size, true);
}

static IOReturn ATTR_LSE2
_getCurrentInQueueMemInternal(IOCircularDataQueue *queue,
    void **data,
    size_t *size,
    bool copyMem)
{
	IOCircularDataQueueMemory *queueMemory = queue->queueMemory;
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;
	IOCircularDataQueueMemoryCursor const *cursor = &queue->queueCursor;

	size_t inSize;

	if (queueMemory == NULL || data == NULL || size == NULL || queueHeaderShadow == NULL) {
		return kIOReturnBadArgument;
	}

	inSize = *size;
	*size = 0;

	if (os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	if (os_unlikely(_isCursorPositionInvalid(queue))) {
		return kIOReturnAborted;
	}

	const size_t queueAllocMemSize = queueHeaderShadow->allocMemSize;
	const size_t queueEntryDataSize = queueHeaderShadow->entryDataSize;

	IOCircularDataQueueState currState;
	currState.val = atomic_load_explicit(&queueMemory->queueStateVal, memory_order_acquire);

	if (os_unlikely(currState.fields.rstStatus & IOCIRCULARDATAQUEUE_STATE_RESET_INPROGRESS)) {
		// Another thread is resetting the queue
		return kIOReturnBusy;
	}

	uint32_t queueGeneration = currState.fields.generation;

	// was the queue reset ?
	if (os_unlikely(cursor->generation != queueGeneration || cursor->sequenceNum > currState.fields.seqNum)) {
		return kIOReturnAborted;
	}

	if (os_unlikely(currState.fields.seqNum == UINT64_MAX)) {
		// Nothing has ever been written to the queue yet.
		return kIOReturnUnderrun;
	}

	__auto_type entry = (IOCircularDataQueueEntryHeader *)(uintptr_t)((uint8_t *)&queueMemory->entries[0]
	    + (cursor->position * queueEntryDataSize));

	// SANITY CHECK - Final check to ensure the 'entry' pointer is
	// within the queueMemory entries buffer before we begin writing.
	if (os_unlikely((uint8_t *)entry < (uint8_t *)(&queueMemory->entries[0])
	    || (uint8_t *)entry >= (uint8_t *)queueMemory + queueAllocMemSize)) {
		queue_debug_error("Out of Bounds! " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT, QUEUE_ARGS(queueMemory),
		    CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnBadArgument;
	}

	os_compiler_barrier();

	if (os_unlikely(entry->sentinel != queueHeaderShadow->sentinel)) {
		queue_debug_error("entry->sentinel != queueMemory->sentinel " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT,
		    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnBadMedia;
	}

	IOCircularDataQueueEntryHeaderInfo enHeaderInfo;
	enHeaderInfo.val = atomic_load_explicit(&entry->headerInfoVal, memory_order_acquire);
	uint32_t entryGeneration = enHeaderInfo.fields.generation;
	if (os_unlikely(entryGeneration != queueGeneration)) {
		queue_debug_note("entryGeneration != queueGeneration " QUEUE_FORMAT " " CURSOR_FORMAT " " ENTRY_FORMAT,
		    QUEUE_ARGS(queueMemory), CURSOR_ARGS(cursor), ENTRY_ARGS(entry));
		return kIOReturnAborted;
	}

	// is the entry currently being written to or has the cursor fallen too far behind and the cursor is no longer
	// valid.
	if (os_unlikely(enHeaderInfo.fields.wrStatus == IOCIRCULARDATAQUEUE_ENTRY_STATE_WRITE_INPROGRESS
	    || enHeaderInfo.fields.seqNum != cursor->sequenceNum)) {
		return kIOReturnOverrun;
	}

	if (os_unlikely(enHeaderInfo.fields.dataSize > queueHeaderShadow->entryDataSize)) {
		return kIOReturnOverrun;
	}
	*size = enHeaderInfo.fields.dataSize;

	if (!copyMem) {
		*data = entry->data;
	} else {
		if (os_unlikely(enHeaderInfo.fields.dataSize > inSize)) {
			return kIOReturnOverrun;
		}
		memcpy(*data, entry->data, enHeaderInfo.fields.dataSize);
		// Lets re-verify after the memcpy if the buffer is/has been overwritten.

		IOCircularDataQueueEntryHeaderInfo enHeaderInfoAfter;
		enHeaderInfoAfter.val = atomic_load_explicit(&entry->headerInfoVal, memory_order_acquire);
		// Did something change, while we were memcopying ?
		if (enHeaderInfo.val != enHeaderInfoAfter.val) {
			// while we were memcopying, the writer wrapped around and is writing into our index. or the queue got reset
			*size = 0;
			return kIOReturnBusy;
		}
	}

	if (os_unlikely(_isQueueMemoryCorrupted(queue))) {
		return kIOReturnBadMedia;
	}

	return kIOReturnSuccess;
}

/*!
 * @function getCurrentInQueueMem
 * Access the data at the current cursor position. The cursor position is unchanged. No copy is made of the data. <br>
 * Caller is supposed to call isDataEntryValidInQueueMem() to check data integrity after reading the data is complete.
 * @param queue Handle to the queue.
 * @param data A pointer to the data memory region for the previous entry data in the queue.
 * @param size A pointer to the size of the data parameter.  On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated to the previous.
 *  - `kIOReturnAborted` if the cursor has become invalid, possibly due to a reset of the queue.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call getLatestInQueueMem to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */

static IOReturn ATTR_LSE2
getCurrentInQueueMem(IOCircularDataQueue *queue,
    void **data,
    size_t *size)
{
	return _getCurrentInQueueMemInternal(queue, data, size, false);
}

/*!
 * @function copyCurrentInQueueMem
 * Access the data at the current cursor position and copy into the provided buffer. The cursor position is unchanged.
 * If successful, function gaurantees that the data returned is always valid, hence no need to call
 * isDataEntryValidInQueueMem().
 * @param queue Handle to the queue.
 * @param data Pointer to memory into which the previous data is copied. Lifetime of this memory is controlled by the
 * caller.
 * @param size Size of the data buffer provided for copying. On return, this contains the actual size of the data
 * pointed to by data param.
 *  @return
 *  - `kIOReturnSuccess` if the cursor position was updated.
 *  - `kIOReturnAborted` if the cursor has become invalid.
 *  - `kIOReturnOverrun` if the entry at the cursor position is no longer in
 *     the queue's buffer. Call getLatestInQueueMem to get the latest data and cursor position.
 *  - `kIOReturnBadArgument` if an invalid argument is passsed.
 *  - `kIOReturnBadMedia` if the queue shared memory has been compromised.
 *  - Other values indicate an error.
 *
 */

static IOReturn ATTR_LSE2
copyCurrentInQueueMem(IOCircularDataQueue *queue,
    void *data,
    size_t *size)
{
	return _getCurrentInQueueMemInternal(queue, &data, size, true);
}


/* API */

static void ATTR_LSE2
_initCursor(IOCircularDataQueue *queue)
{
	// Invalidate the cursor
	IOCircularDataQueueMemoryCursor *cursor = &queue->queueCursor;
	cursor->generation = UINT32_MAX;
	cursor->position = UINT32_MAX;
	cursor->sequenceNum = UINT64_MAX;
}

#if KERNEL

IOReturn ATTR_LSE2
IOCircularDataQueueCreateWithEntries(IOCircularDataQueueCreateOptions options, uint32_t numEntries, uint32_t entrySize, IOCircularDataQueue **pQueue)
{
	IOCircularDataQueueMemory *queueMemory;
	IOReturn ret;

	if (!pQueue) {
		return kIOReturnBadArgument;
	}
	*pQueue = NULL;
	if (!numEntries || !entrySize) {
		return kIOReturnBadArgument;
	}

	uint64_t sentinel = 0xA5A5A5A5A5A5A5A5;

#if HEADER_16BYTE_ALIGNED
	size_t entryRoundedDataSize = IORound(entrySize, sizeof(__uint128_t));
#else
	size_t entryRoundedDataSize = IORound(entrySize, sizeof(UInt64));
#endif
	size_t entryDataSize = entryRoundedDataSize + CIRCULAR_DATA_QUEUE_ENTRY_HEADER_SIZE;
	size_t entriesSize = numEntries * (entryDataSize);
	size_t totalSize = entriesSize + CIRCULAR_DATA_QUEUE_MEMORY_HEADER_SIZE;

	if (os_unlikely(numEntries > UINT32_MAX - 1
	    || entryRoundedDataSize > (UINT32_MAX - sizeof(IOCircularDataQueueEntryHeader))
	    || entryDataSize > UINT32_MAX || totalSize > UINT32_MAX)) {
		return kIOReturnBadArgument;
	}

	IOCircularDataQueue *queue = IONew(IOCircularDataQueue, 1);
	if (!queue) {
		return kIOReturnNoMemory;
	}
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;

	OSData * desc;
	queue->iomd = IOBufferMemoryDescriptor::inTaskWithOptions(
		kernel_task, kIOMemoryDirectionOutIn | kIOMemoryKernelUserShared, totalSize, page_size);
	if (os_unlikely(queue->iomd == NULL)) {
		ret = kIOReturnNoMemory;
		goto error;
	}
	queueMemory = (IOCircularDataQueueMemory *)queue->iomd->getBytesNoCopy();
	queue->queueMemory = queueMemory;
	queueMemory->sentinel = queueHeaderShadow->sentinel = sentinel;

	queueHeaderShadow->allocMemSize = (uint32_t)totalSize;
	queueHeaderShadow->entryDataSize
	        = (uint32_t)entryDataSize; // totalSize check above gaurantess this will not overflow UINT32_MAX.
	queueHeaderShadow->numEntries = numEntries;
	queueHeaderShadow->dataSize = entrySize; // the client requested fixed entry size.
	queueHeaderShadow->memorySize = (uint32_t)entriesSize;

	desc = OSData::withBytes(queueHeaderShadow, sizeof(*queueHeaderShadow));
	queue->iomd->setSharingContext(kIOCircularQueueDescriptionKey, desc);

	IOCircularDataQueueState newState;
	newState.val = 0;
	newState.fields.seqNum = UINT64_MAX;
	atomic_store_explicit(&queueMemory->queueStateVal, newState.val, memory_order_release);

	ret = _reset(queue);
	if (ret != kIOReturnSuccess) {
		goto error;
	}

	_initCursor(queue);
	*pQueue = queue;
	return kIOReturnSuccess;


error:
	IOCircularDataQueueDestroy(&queue);
	return ret;
}

IOMemoryDescriptor * ATTR_LSE2
IOCircularDataQueueCopyMemoryDescriptor(IOCircularDataQueue  *queue)
{
	IOMemoryDescriptor * md;
	md = queue->iomd;
	if (md) {
		md->retain();
	}
	return md;
}

#else /* KERNEL */

#if defined(__arm64__) && defined(__LP64__)
#include <System/arm/cpu_capabilities.h>
#endif /* defined(__arm64__) */

IOReturn ATTR_LSE2
IOCircularDataQueueCreateWithConnection(IOCircularDataQueueCreateOptions options, io_connect_t connect, uint32_t memoryType, IOCircularDataQueue **pQueue)
{
	if (!pQueue) {
		return kIOReturnBadArgument;
	}
	*pQueue = NULL;

#if defined(__arm64__) && defined(__LP64__)
	if (0 == (kHasFeatLSE2 & _get_cpu_capabilities())) {
		return kIOReturnUnsupported;
	}
#else
	return kIOReturnUnsupported;
#endif /* defined(__arm64__) */

	uint64_t sentinel = 0xA5A5A5A5A5A5A5A5;

	IOCircularDataQueue *queue = IONew(IOCircularDataQueue, 1);
	if (!queue) {
		return kIOReturnNoMemory;
	}
	IOCircularDataQueueDescription *queueHeaderShadow = &queue->queueHeaderShadow;

	queue->connect = connect;
	queue->memoryType = memoryType;

	io_struct_inband_t inband_output;
	mach_msg_type_number_t inband_outputCnt;
	mach_vm_address_t map_address;
	mach_vm_size_t map_size;
	IOReturn ret;

	inband_outputCnt = sizeof(inband_output);

	ret = io_connect_map_shared_memory(connect, memoryType, mach_task_self(),
	    &map_address, &map_size,
	    /* flags */ 0,
	    (char *) kIOCircularQueueDescriptionKey,
	    inband_output,
	    &inband_outputCnt);

	printf("%x, %lx, 0x%llx, 0x%llx\n", inband_outputCnt, sizeof(IOCircularDataQueueDescription), map_address, map_size);

	assert(sizeof(IOCircularDataQueueDescription) == inband_outputCnt);
	memcpy(queueHeaderShadow, inband_output, sizeof(IOCircularDataQueueDescription));
	printf("sentinel %qx\n", queueHeaderShadow->sentinel);
	assert(queueHeaderShadow->allocMemSize == map_size);
	queue->queueMemory = (IOCircularDataQueueMemory *) map_address;

	if (!isQueueMemoryValid(queue)) {
		IOCircularDataQueueDestroy(&queue);
		return kIOReturnBadArgument;
	}

	_initCursor(queue);
	*pQueue = queue;

	return ret;
}

#endif /* !KERNEL */

IOReturn ATTR_LSE2
IOCircularDataQueueDestroy(IOCircularDataQueue **pQueue)
{
	IOCircularDataQueue * queue;
	IOReturn ret = kIOReturnSuccess;

	if (!pQueue) {
		return kIOReturnBadArgument;
	}
	queue = *pQueue;
	if (queue) {
		ret = destroyQueueMem(queue);
		IODelete(queue, IOCircularDataQueue, 1);
		*pQueue = NULL;
	}
	return ret;
}

IOReturn ATTR_LSE2
IOCircularDataQueueEnqueue(IOCircularDataQueue *queue, const void *data, size_t dataSize)
{
	if (!queue) {
		return kIOReturnBadArgument;
	}

	return enqueueQueueMem(queue, data, dataSize);
}

IOReturn ATTR_LSE2
IOCircularDataQueueGetLatest(IOCircularDataQueue *queue, void **data, size_t *size)
{
	if (!queue) {
		return kIOReturnBadArgument;
	}

	return getLatestInQueueMem(queue, data, size);
}

IOReturn ATTR_LSE2
IOCircularDataQueueCopyLatest(IOCircularDataQueue *queue, void *data, size_t *size)
{
	if (!queue) {
		return kIOReturnBadArgument;
	}

	return copyLatestInQueueMem(queue, data, size);
}

IOReturn ATTR_LSE2
IOCircularDataQueueGetNext(IOCircularDataQueue *queue, void **data, size_t *size)
{
	if (!queue) {
		return kIOReturnBadArgument;
	}

	return getNextInQueueMem(queue, data, size);
}

IOReturn ATTR_LSE2
IOCircularDataQueueCopyNext(IOCircularDataQueue *queue, void *data, size_t *size)
{
	if (!queue) {
		return kIOReturnBadArgument;
	}

	return copyNextInQueueMem(queue, data, size);
}

IOReturn ATTR_LSE2
IOCircularDataQueueGetPrevious(IOCircularDataQueue *queue, void **data, size_t *size)
{
	if (!queue) {
		return kIOReturnBadArgument;
	}

	return getPrevInQueueMem(queue, data, size);
}

IOReturn ATTR_LSE2
IOCircularDataQueueCopyPrevious(IOCircularDataQueue *queue, void *data, size_t *size)
{
	if (!queue) {
		return kIOReturnBadArgument;
	}

	return copyPrevInQueueMem(queue, data, size);
}

// IOReturn
//IOCircularDataQueueGetLatestWithBlock(IOCircularDataQueue *queue, void (^handler)(void * data, size_t size))
//{
//     if (!queue) {
//         return kIOReturnBadArgument;
//     }
//
////    return getPrevInQueueMem(queue->queueMemory, (IOCircularDataQueueDescription *)
///&queue->queueHeaderShadow, (IOCircularDataQueueMemoryCursor *) &queue->queueCursor, data, size);
//}
//

IOReturn ATTR_LSE2
IOCircularDataQueueIsCurrentDataValid(IOCircularDataQueue *queue)
{
	if (!queue) {
		return kIOReturnBadArgument;
	}

	return isDataEntryValidInQueueMem(queue);
}

IOReturn ATTR_LSE2
IOCircularDataQueueSetCursorLatest(IOCircularDataQueue *queue)
{
	if (!queue) {
		return kIOReturnBadArgument;
	}

	return setCursorLatestInQueueMem(queue);
}

IOReturn ATTR_LSE2
IOCircularDataQueueGetCurrent(IOCircularDataQueue *queue, void **data, size_t *size)
{
	if (!queue) {
		return kIOReturnBadArgument;
	}

	return getCurrentInQueueMem(queue, data, size);
}

IOReturn ATTR_LSE2
IOCircularDataQueueCopyCurrent(IOCircularDataQueue *queue, void *data, size_t *size)
{
	if (!queue) {
		return kIOReturnBadArgument;
	}

	return copyCurrentInQueueMem(queue, data, size);
}

__END_DECLS
