/* iig-lite generated from IODispatchQueue.iig - kernel-side subset; msgids are NOT Apple-ABI */

/*
 * Copyright (c) 2019-2019 Apple Inc. All rights reserved.
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

#ifndef _IOKIT_UIODISPATCHQUEUE_H
#define _IOKIT_UIODISPATCHQUEUE_H

#include <DriverKit/OSObject.h>  /* .iig include */
#include <DriverKit/OSAction.h>  /* .iig include */
#include <DriverKit/IODispatchSource.h>  /* .iig include */

typedef int (*IODispatchLogFunction)(const char *format, ...);
typedef void (^IODispatchBlock)(void);
typedef void (*IODispatchFunction)(void * context);
typedef kern_return_t (^IODispatchAction)(void);

typedef void (^IODispatchQueueCancelHandler)(void);


// options for Create()
enum {
	kIODispatchQueueReentrant              = 0x00000001,
	kIODispatchQueueMethodsNotSynchronized = 0x00000002,
};

// options for WakeupWithOptions()
enum {
	kIODispatchQueueWakeupAll              = 0x00000001,
};

// options for SleepWithDeadline()
enum {
	// Sleep on an event which other threads are still waiting on, but has already been signaled
	kIODispatchQueueSleepReuseEvent        = 0x00000100,
};

/*!
 * @class IODispatchQueue
 *
 * @abstract
 * IODispatchQueue provides a queue for ordered execution of blocks.
 *
 * @discussion
 * All blocks submitted to dispatch queues are dequeued in FIFO order.
 * By default the queue is serial and will execute one block at a time.
 */

/* source class IODispatchQueue IODispatchQueue.iig:72-239 */

#if __DOCUMENTATION__
#define KERNEL IIG_KERNEL

class NATIVE KERNEL IODispatchQueue : public OSObject
{
public:
    /*!
     * @brief       Creates a new dispatch queue object.
     * @discussion  Creates a new dispatch queue object. All queues are currently serial, executing one block at time
     *              FIFO order. The new object has retain count 1 and should be released by the caller.
     * @param       options
                    kIODispatchQueueReentrant Creates a queue that allows release with the Sleep
                    method, so that other threads and callers may acquire the queue.
     * @param       priority No priorities are currently defined, pass zero.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	static kern_return_t
	Create(
		const IODispatchQueueName name,
		uint64_t                  options,
		uint64_t                  priority,
		IODispatchQueue        ** queue) LOCAL;

	virtual bool
	init() override;

	virtual void
	free() override;

    /*!
     * @brief       Determines if the current thread is running on the queue.
     * @discussion  Determines if the current thread is running on the queue, including if the queue invoked a
     *              second queue (ie. OnQueue can return true for more than one queue in a given context.)
     * @return      bool true if current thread is running on this queue.
     */
	bool
	OnQueue() LOCALONLY;

    /*!
     * @brief       Return the name the queue was created with.
     * @discussion  Returns a pointer to the queues name. Only valid while the queue is retained.
     * @return      C-string pointer in the queues internal storage.
     */
	const char *
	GetName() LOCALONLY;

    /*!
     * @brief       Stop the queue from executing futher work.
     * @discussion  Stops the queue from dequeuing work, and on completion of any block currently being executed,
     *              invokes a callback block. Canceling is asynchronous.
     * @param       handler Block that will executed when the queue has completed any inflight work 
     *              and will not execute further work.
     * @return      C-string pointer in the queues internal storage.
     */
	kern_return_t
	Cancel(IODispatchQueueCancelHandler handler) LOCALONLY;

    /*!
     * @brief       Schedule a block to be executed on the queue asynchronously.
     * @discussion  Schedules work to be done on the queue without waiting for it to complete. The queue will be
     *              retained until the block completes.
     * @param       block Block that will executed on the queue, not in the context of the caller.
     */
	void
	DispatchAsync(IODispatchBlock block) LOCALONLY;

    /*!
     * @brief       C-function callback version of DispatchAsync.
	 */
	void
	DispatchAsync_f(void * context, IODispatchFunction function) LOCALONLY;

    /*!
     * @brief       Schedule a block to be executed concurrently & asynchronously.
     * @discussion  Schedules work to be done on the queue without waiting for it to complete, and
     *              concurrently with other blocks executed with DispatchConcurrent. The queue will be
     *              retained until the block completes. May only be used with a queue created with
     *              the kIODispatchQueueReentrant option.
     * @param       block Block that will executed on the queue, not in the context of the caller.
     */
	void
	DispatchConcurrent(IODispatchBlock block) LOCALONLY;

    /*!
     * @brief       C-function callback version of DispatchConcurrent.
	 */
	void
	DispatchConcurrent_f(void * context, IODispatchFunction function) LOCALONLY;

    /*!
     * @brief       Execute a block on the queue synchronously.
     * @discussion  Execute a block on the queue synchronously.
     * @param       block Block that will executed on the queue.
     */
	void
	DispatchSync(IODispatchBlock block) LOCALONLY;

    /*!
     * @brief       C-function callback version of DispatchSync.
	 */
	void
	DispatchSync_f(void * context, IODispatchFunction function) LOCALONLY;

    /*!
     * @brief       Log the current execution context with respect to any queues the current thread holds.
     * @param       output printf like output function. The address of IOLog is suitable to be used.
	 */
	static void
	Log(const char * message, IODispatchLogFunction output) LOCALONLY;

    /*!
     * @brief       Version of DispatchSync that returns a kern_return_t status.
	 */
	kern_return_t
	RunAction(IODispatchAction action) LOCALONLY;

    /*!
     * @brief       Put a thread that is currently running the queue to sleep, releasing the queue.
     * @discussion  Put a thread to sleep waiting for an event but release the queue first.
     *              In all cases (signal, timeout or error), the caller resumes running on the queue.
	 *              The caller must be currently running on the queue to call Sleep().
     * @param       event A unique token matching one later passed to Wakeup().
     * @param       options Pass one of the kIOTimerClock* options to specify the timebase for the
     *              deadline, or zero for no timeout.
     * @param       deadline Clock deadline to timeout the sleep.
     * @return 		kIOReturnSuccess  or kIOReturnTimeout
     */
	kern_return_t
	SleepWithDeadline(void * event, uint64_t options, uint64_t deadline) LOCALONLY;

    /*!
     * @brief       Put a thread that is currently running the queue to sleep, releasing the queue.
     * @discussion  Put a thread to sleep waiting for an event but release the queue first.
     *              In all cases (signal, timeout or error), the caller resumes running on the queue.
	 *              The caller must be currently running on the queue to call Sleep().
     * @param       event A unique token matching one later passed to Wakeup().
     * @param       timeout Clock delay to timeout the sleep.
     * @return 		kIOReturnSuccess  or kIOReturnTimeout
     */
	kern_return_t
	SleepWithTimeout(void * event, uint64_t timeout) LOCALONLY;

    /*!
     * @brief       Synonym for SleepWithTimeout()
     */
	kern_return_t
	Sleep(void * event, uint64_t timeout) LOCALONLY;

    /*!
     * @brief       Wakes a thread that is blocked in Sleep().
     * @discussion  Signals a thread that gave up the queue with Sleep() to continue running.
	 *              The caller must be currently running on the queue.
     * @param       event A unique token matching one passed to Sleep().
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	kern_return_t
	Wakeup(void * event) LOCALONLY;

    /*!
     * @brief       Wakes a thread that is blocked in Sleep().
     * @discussion  Signals a thread that gave up the queue with Sleep() to continue running.
	 *              The caller must be currently running on the queue.
     * @param       event A unique token matching one passed to Sleep().
     * @param       options
					kIODispatchQueueWakeupAll wake all threads waiting in Sleep().
					The default is to wake only one of any waiting threads.
     * @return      kIOReturnSuccess on success. See IOReturn.h for error codes.
     */
	kern_return_t
	WakeupWithOptions(void * event, uint64_t options) LOCALONLY;
};

#undef KERNEL
#else /* __DOCUMENTATION__ */

/* generated class IODispatchQueue IODispatchQueue.iig:72-239 */

#define IODispatchQueue_SetPort_ID            0x3903c97d61e69ec8ULL
#define IODispatchQueue_Create_ID            0x22a0fc42ad35124bULL

#define IODispatchQueue_SetPort_Args \
        mach_port_t port

#define IODispatchQueue_Create_Args \
        const IODispatchQueueName name, \
        uint64_t options, \
        uint64_t priority, \
        IODispatchQueue ** queue

#define IODispatchQueue_Methods \
\
public:\
\
    virtual kern_return_t\
    Dispatch(const IORPC rpc) APPLE_KEXT_OVERRIDE;\
\
    static kern_return_t\
    _Dispatch(IODispatchQueue * self, const IORPC rpc);\
\
    kern_return_t\
    SetPort(\
        mach_port_t port,\
        OSDispatchMethod supermethod = NULL);\
\
    static kern_return_t\
    Create(\
        const char * name,\
        uint64_t options,\
        uint64_t priority,\
        IODispatchQueue ** queue);\
\
    bool\
    OnQueue(\
);\
\
    const char *\
    GetName(\
);\
\
    kern_return_t\
    Cancel(\
        IODispatchQueueCancelHandler handler);\
\
    void\
    DispatchAsync(\
        IODispatchBlock block);\
\
    void\
    DispatchAsync_f(\
        void * context,\
        IODispatchFunction function);\
\
    void\
    DispatchConcurrent(\
        IODispatchBlock block);\
\
    void\
    DispatchConcurrent_f(\
        void * context,\
        IODispatchFunction function);\
\
    void\
    DispatchSync(\
        IODispatchBlock block);\
\
    void\
    DispatchSync_f(\
        void * context,\
        IODispatchFunction function);\
\
    static void\
    Log(\
        const char * message,\
        IODispatchLogFunction output);\
\
    kern_return_t\
    RunAction(\
        IODispatchAction action);\
\
    kern_return_t\
    SleepWithDeadline(\
        void * event,\
        uint64_t options,\
        uint64_t deadline);\
\
    kern_return_t\
    SleepWithTimeout(\
        void * event,\
        uint64_t timeout);\
\
    kern_return_t\
    Sleep(\
        void * event,\
        uint64_t timeout);\
\
    kern_return_t\
    Wakeup(\
        void * event);\
\
    kern_return_t\
    WakeupWithOptions(\
        void * event,\
        uint64_t options);\
\
\
protected:\
    /* _Impl methods */\
\
    static kern_return_t\
    Create_Impl(IODispatchQueue_Create_Args);\
\
\
public:\
    /* _Invoke methods */\
\
    typedef kern_return_t (*SetPort_Handler)(OSMetaClassBase * target, IODispatchQueue_SetPort_Args);\
    static kern_return_t\
    SetPort_Invoke(const IORPC rpc,\
        OSMetaClassBase * target,\
        SetPort_Handler func);\
\
    typedef kern_return_t (*Create_Handler)(IODispatchQueue_Create_Args);\
    static kern_return_t\
    Create_Invoke(const IORPC rpc,\
        Create_Handler func);\
\


#define IODispatchQueue_KernelMethods \
\
protected:\
    /* _Impl methods */\
\
    kern_return_t\
    SetPort_Impl(IODispatchQueue_SetPort_Args);\
\


#define IODispatchQueue_VirtualMethods \
\
public:\
\
    virtual bool\
    init(\
) APPLE_KEXT_OVERRIDE;\
\
    virtual void\
    free(\
) APPLE_KEXT_OVERRIDE;\
\


#if !KERNEL

extern OSMetaClass          * gIODispatchQueueMetaClass;
extern const OSClassLoadInformation IODispatchQueue_Class;

class IODispatchQueueMetaClass : public OSMetaClass
{
public:
    virtual kern_return_t
    New(OSObject * instance) override;
    virtual kern_return_t
    Dispatch(const IORPC rpc) override;
};

#endif /* !KERNEL */

class IODispatchQueueInterface : public OSInterface
{
public:
};

struct IODispatchQueue_IVars;
struct IODispatchQueue_LocalIVars;

class IODispatchQueue : public OSObject, public IODispatchQueueInterface
{
#if KERNEL
    OSDeclareDefaultStructorsWithDispatch(IODispatchQueue);
#endif /* KERNEL */

#if !KERNEL
    friend class IODispatchQueueMetaClass;
#endif /* !KERNEL */

public:
#ifdef IODispatchQueue_DECLARE_IVARS
IODispatchQueue_DECLARE_IVARS
#else /* IODispatchQueue_DECLARE_IVARS */
    union
    {
        IODispatchQueue_IVars * ivars;
        IODispatchQueue_LocalIVars * lvars;
    };
#endif /* IODispatchQueue_DECLARE_IVARS */
#if !KERNEL
    static OSMetaClass *
    sGetMetaClass() { return gIODispatchQueueMetaClass; };
    virtual const OSMetaClass *
    getMetaClass() const APPLE_KEXT_OVERRIDE { return gIODispatchQueueMetaClass; };
#endif /* KERNEL */

    using super = OSObject;

#if !KERNEL
    IODispatchQueue_Methods
#endif /* !KERNEL */

    IODispatchQueue_VirtualMethods
};


#endif /* !__DOCUMENTATION__ */



#if DRIVERKIT_PRIVATE
/* source class IODispatchQueuePrivate IODispatchQueue.iig:242-247 */

#if __DOCUMENTATION__
#define KERNEL IIG_KERNEL

class EXTENDS (IODispatchQueue) IODispatchQueuePrivate
{
	virtual kern_return_t
	SetPort(
		mach_port_t port PORTMAKESEND);
};

#undef KERNEL
#else /* __DOCUMENTATION__ */

#endif /* !__DOCUMENTATION__ */


#endif

#endif /* ! _IOKIT_UIODISPATCH_H */
