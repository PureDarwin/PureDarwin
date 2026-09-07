/*
 * CFFileDescriptor.c
 */

#include <CoreFoundation/CFFileDescriptor.h>
#include "CFInternal.h"
#include <dispatch/dispatch.h>
#include <unistd.h>

struct __CFFileDescriptor {
    CFRuntimeBase               _base;
    CFLock_t                    _lock;
    CFFileDescriptorNativeDescriptor _fd;
    Boolean                     _closeOnInvalidate;
    Boolean                     _valid;
    /* Types the caller has armed, and types that have become ready and are
     * waiting for the run loop to deliver them. */
    CFOptionFlags               _enabled;
    CFOptionFlags               _ready;
    CFFileDescriptorCallBack    _callout;
    CFFileDescriptorContext     _context;
    CFRunLoopSourceRef          _source;
    /* The run loops the source is scheduled in, so a dispatch source that
     * fires on the queue can wake the right thread. */
    CFMutableArrayRef           _runLoops;
    dispatch_source_t           _rdsrc;
    dispatch_source_t           _wrsrc;
    Boolean                     _rdsuspended;
    Boolean                     _wrsuspended;
};

static CFTypeID __kCFFileDescriptorTypeID = _kCFRuntimeNotATypeID;

static dispatch_queue_t
__CFFileDescriptorQueue(void)
{
    static dispatch_queue_t queue;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        queue = dispatch_queue_create("com.apple.CFFileDescriptor", DISPATCH_QUEUE_SERIAL);
    });
    return queue;
}

static void
__CFFileDescriptorCancelSources(CFFileDescriptorRef f)
{
    /* A suspended source cannot be cancelled, so resume first. The cancel
     * handler is not used to close the descriptor: invalidate does that, and
     * doing it here would race with a caller that dup'd it. */
    if (f->_rdsrc != NULL) {
        if (f->_rdsuspended) {
            dispatch_resume(f->_rdsrc);
            f->_rdsuspended = false;
        }
        dispatch_source_cancel(f->_rdsrc);
        dispatch_release(f->_rdsrc);
        f->_rdsrc = NULL;
    }
    if (f->_wrsrc != NULL) {
        if (f->_wrsuspended) {
            dispatch_resume(f->_wrsrc);
            f->_wrsuspended = false;
        }
        dispatch_source_cancel(f->_wrsrc);
        dispatch_release(f->_wrsrc);
        f->_wrsrc = NULL;
    }
}

static void
__CFFileDescriptorDeallocate(CFTypeRef cf)
{
    CFFileDescriptorRef f = (CFFileDescriptorRef)cf;

    __CFFileDescriptorCancelSources(f);
    if (f->_source != NULL) {
        CFRunLoopSourceInvalidate(f->_source);
        CFRelease(f->_source);
        f->_source = NULL;
    }
    if (f->_runLoops != NULL) {
        CFRelease(f->_runLoops);
        f->_runLoops = NULL;
    }
    if (f->_context.info != NULL && f->_context.release != NULL) {
        f->_context.release(f->_context.info);
        f->_context.info = NULL;
    }
    if (f->_closeOnInvalidate && f->_fd >= 0) {
        close(f->_fd);
    }
    f->_fd = -1;
}

static CFStringRef
__CFFileDescriptorCopyDescription(CFTypeRef cf)
{
    CFFileDescriptorRef f = (CFFileDescriptorRef)cf;

    return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL,
                                    CFSTR("<CFFileDescriptor %p [fd = %d, valid = %s, enabled = %lu]>"),
                                    cf, f->_fd, f->_valid ? "yes" : "no",
                                    (unsigned long)f->_enabled);
}

static const CFRuntimeClass __CFFileDescriptorClass = {
    0,
    "CFFileDescriptor",
    NULL,      // init
    NULL,      // copy
    __CFFileDescriptorDeallocate,
    NULL,      // equal
    NULL,      // hash
    NULL,      //
    __CFFileDescriptorCopyDescription
};

CFTypeID
CFFileDescriptorGetTypeID(void)
{
    static dispatch_once_t initOnce;
    dispatch_once(&initOnce, ^{
        __kCFFileDescriptorTypeID = _CFRuntimeRegisterClass(&__CFFileDescriptorClass);
    });
    return __kCFFileDescriptorTypeID;
}

/* Runs on the dispatch queue: record the ready type, stop watching for it, and
 * hand the delivery over to the run loop. */
static void
__CFFileDescriptorBecameReady(CFFileDescriptorRef f, CFOptionFlags type)
{
    CFArrayRef  loops = NULL;
    Boolean     signalled = false;

    __CFLock(&f->_lock);
    if (f->_valid) {
        f->_ready |= type;
        if (type == kCFFileDescriptorReadCallBack) {
            if (f->_rdsrc != NULL && !f->_rdsuspended) {
                dispatch_suspend(f->_rdsrc);
                f->_rdsuspended = true;
            }
        } else {
            if (f->_wrsrc != NULL && !f->_wrsuspended) {
                dispatch_suspend(f->_wrsrc);
                f->_wrsuspended = true;
            }
        }
        if (f->_source != NULL) {
            CFRunLoopSourceSignal(f->_source);
            signalled = true;
        }
        if (f->_runLoops != NULL && CFArrayGetCount(f->_runLoops) > 0) {
            loops = CFArrayCreateCopy(kCFAllocatorSystemDefault, f->_runLoops);
        }
    }
    __CFUnlock(&f->_lock);

    /* Wake outside the lock: CFRunLoopWakeUp takes its own. */
    if (signalled && loops != NULL) {
        CFIndex n = CFArrayGetCount(loops);
        for (CFIndex i = 0; i < n; i++) {
            CFRunLoopWakeUp((CFRunLoopRef)CFArrayGetValueAtIndex(loops, i));
        }
    }
    if (loops != NULL) {
        CFRelease(loops);
    }
}

CFFileDescriptorRef
CFFileDescriptorCreate(CFAllocatorRef allocator, CFFileDescriptorNativeDescriptor fd,
                       Boolean closeOnInvalidate, CFFileDescriptorCallBack callout,
                       const CFFileDescriptorContext *context)
{
    CFFileDescriptorRef f;
    CFIndex             size;

    if (fd < 0 || callout == NULL) {
        return NULL;
    }

    size = sizeof(struct __CFFileDescriptor) - sizeof(CFRuntimeBase);
    f = (CFFileDescriptorRef)_CFRuntimeCreateInstance(allocator,
                                                      CFFileDescriptorGetTypeID(),
                                                      size, NULL);
    if (f == NULL) {
        return NULL;
    }

    f->_lock = CFLockInit;
    f->_fd = fd;
    f->_closeOnInvalidate = closeOnInvalidate;
    f->_valid = true;
    f->_enabled = 0;
    f->_ready = 0;
    f->_callout = callout;
    f->_source = NULL;
    f->_rdsrc = NULL;
    f->_wrsrc = NULL;
    f->_rdsuspended = false;
    f->_wrsuspended = false;
    f->_runLoops = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, NULL);

    memset(&f->_context, 0, sizeof(f->_context));
    if (context != NULL) {
        f->_context = *context;
        if (f->_context.info != NULL && f->_context.retain != NULL) {
            f->_context.info = f->_context.retain(f->_context.info);
        }
    }
    return f;
}

CFFileDescriptorNativeDescriptor
CFFileDescriptorGetNativeDescriptor(CFFileDescriptorRef f)
{
    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());
    return f->_fd;
}

void
CFFileDescriptorGetContext(CFFileDescriptorRef f, CFFileDescriptorContext *context)
{
    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());
    if (context != NULL && context->version == 0) {
        *context = f->_context;
    }
}

Boolean
CFFileDescriptorIsValid(CFFileDescriptorRef f)
{
    Boolean valid;

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());
    __CFLock(&f->_lock);
    valid = f->_valid;
    __CFUnlock(&f->_lock);
    return valid;
}

void
CFFileDescriptorEnableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes)
{
    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    __CFLock(&f->_lock);
    if (!f->_valid) {
        __CFUnlock(&f->_lock);
        return;
    }
    f->_enabled |= callBackTypes;

    if ((callBackTypes & kCFFileDescriptorReadCallBack) != 0) {
        if (f->_rdsrc == NULL) {
            f->_rdsrc = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
                                               (uintptr_t)f->_fd, 0,
                                               __CFFileDescriptorQueue());
            if (f->_rdsrc != NULL) {
                /* Not retained by the block: the source is cancelled and
                 * released in deallocate, so it cannot outlive the object. */
                dispatch_source_set_event_handler(f->_rdsrc, ^{
                    __CFFileDescriptorBecameReady(f, kCFFileDescriptorReadCallBack);
                });
                /* Sources start suspended. */
                f->_rdsuspended = true;
            }
        }
        if (f->_rdsrc != NULL && f->_rdsuspended) {
            f->_rdsuspended = false;
            dispatch_resume(f->_rdsrc);
        }
    }
    if ((callBackTypes & kCFFileDescriptorWriteCallBack) != 0) {
        if (f->_wrsrc == NULL) {
            f->_wrsrc = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE,
                                               (uintptr_t)f->_fd, 0,
                                               __CFFileDescriptorQueue());
            if (f->_wrsrc != NULL) {
                dispatch_source_set_event_handler(f->_wrsrc, ^{
                    __CFFileDescriptorBecameReady(f, kCFFileDescriptorWriteCallBack);
                });
                f->_wrsuspended = true;
            }
        }
        if (f->_wrsrc != NULL && f->_wrsuspended) {
            f->_wrsuspended = false;
            dispatch_resume(f->_wrsrc);
        }
    }
    __CFUnlock(&f->_lock);
}

void
CFFileDescriptorDisableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes)
{
    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    __CFLock(&f->_lock);
    f->_enabled &= ~callBackTypes;
    f->_ready &= ~callBackTypes;
    if ((callBackTypes & kCFFileDescriptorReadCallBack) != 0 &&
        f->_rdsrc != NULL && !f->_rdsuspended) {
        dispatch_suspend(f->_rdsrc);
        f->_rdsuspended = true;
    }
    if ((callBackTypes & kCFFileDescriptorWriteCallBack) != 0 &&
        f->_wrsrc != NULL && !f->_wrsuspended) {
        dispatch_suspend(f->_wrsrc);
        f->_wrsuspended = true;
    }
    __CFUnlock(&f->_lock);
}

void
CFFileDescriptorInvalidate(CFFileDescriptorRef f)
{
    CFRunLoopSourceRef source;

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    CFRetain(f);
    __CFLock(&f->_lock);
    if (!f->_valid) {
        __CFUnlock(&f->_lock);
        CFRelease(f);
        return;
    }
    f->_valid = false;
    f->_enabled = 0;
    f->_ready = 0;
    __CFFileDescriptorCancelSources(f);
    source = f->_source;
    f->_source = NULL;
    if (f->_runLoops != NULL) {
        CFArrayRemoveAllValues(f->_runLoops);
    }
    __CFUnlock(&f->_lock);

    if (source != NULL) {
        CFRunLoopSourceInvalidate(source);
        CFRelease(source);
    }
    if (f->_closeOnInvalidate && f->_fd >= 0) {
        close(f->_fd);
        f->_fd = -1;
    }
    if (f->_context.info != NULL && f->_context.release != NULL) {
        f->_context.release(f->_context.info);
        f->_context.info = NULL;
    }
    CFRelease(f);
}

static void
__CFFileDescriptorSourceSchedule(void *info, CFRunLoopRef rl, CFStringRef mode)
{
    CFFileDescriptorRef f = (CFFileDescriptorRef)info;

    __CFLock(&f->_lock);
    if (f->_runLoops != NULL) {
        CFArrayAppendValue(f->_runLoops, rl);
    }
    __CFUnlock(&f->_lock);
}

static void
__CFFileDescriptorSourceCancel(void *info, CFRunLoopRef rl, CFStringRef mode)
{
    CFFileDescriptorRef f = (CFFileDescriptorRef)info;

    __CFLock(&f->_lock);
    if (f->_runLoops != NULL) {
        CFIndex idx = CFArrayGetFirstIndexOfValue(f->_runLoops,
                            CFRangeMake(0, CFArrayGetCount(f->_runLoops)), rl);
        if (idx != kCFNotFound) {
            CFArrayRemoveValueAtIndex(f->_runLoops, idx);
        }
    }
    __CFUnlock(&f->_lock);
}

/* Runs on the run loop thread. */
static void
__CFFileDescriptorSourcePerform(void *info)
{
    CFFileDescriptorRef         f = (CFFileDescriptorRef)info;
    CFOptionFlags               types;
    CFFileDescriptorCallBack    callout;
    void *                      cbinfo;

    __CFLock(&f->_lock);
    types = f->_ready & f->_enabled;
    f->_ready &= ~types;
    /* One-shot: the caller re-arms with CFFileDescriptorEnableCallBacks. The
     * dispatch source was already suspended when it fired. */
    f->_enabled &= ~types;
    callout = f->_callout;
    cbinfo = f->_context.info;
    if (!f->_valid) {
        types = 0;
    }
    __CFUnlock(&f->_lock);

    if (types != 0 && callout != NULL) {
        callout(f, types, cbinfo);
    }
}

CFRunLoopSourceRef
CFFileDescriptorCreateRunLoopSource(CFAllocatorRef allocator, CFFileDescriptorRef f,
                                    CFIndex order)
{
    CFRunLoopSourceRef result = NULL;

    __CFGenericValidateType(f, CFFileDescriptorGetTypeID());

    __CFLock(&f->_lock);
    if (!f->_valid) {
        __CFUnlock(&f->_lock);
        return NULL;
    }
    if (f->_source == NULL) {
        CFRunLoopSourceContext context;

        memset(&context, 0, sizeof(context));
        context.version = 0;
        context.info = (void *)f;
        context.retain = (const void *(*)(const void *))CFRetain;
        context.release = (void (*)(const void *))CFRelease;
        context.copyDescription = (CFStringRef (*)(const void *))__CFFileDescriptorCopyDescription;
        context.equal = NULL;
        context.hash = NULL;
        context.schedule = __CFFileDescriptorSourceSchedule;
        context.cancel = __CFFileDescriptorSourceCancel;
        context.perform = __CFFileDescriptorSourcePerform;
        f->_source = CFRunLoopSourceCreate(allocator, order, &context);
    }
    if (f->_source != NULL) {
        result = (CFRunLoopSourceRef)CFRetain(f->_source);
    }
    __CFUnlock(&f->_lock);
    return result;
}
