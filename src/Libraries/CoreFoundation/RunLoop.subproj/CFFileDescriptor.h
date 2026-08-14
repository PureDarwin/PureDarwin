/*
 * CFFileDescriptor.h
 *
 * A CFFileDescriptor watches a native file descriptor for readability or
 * writability and reports it through a run loop source. It is the counterpart
 * of CFSocket for descriptors that are not sockets - character devices such as
 * BPF, pipes, kqueues - where the socket calls do not apply.
 */

#if !defined(__COREFOUNDATION_CFFILEDESCRIPTOR__)
#define __COREFOUNDATION_CFFILEDESCRIPTOR__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFRunLoop.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_EXTERN_C_BEGIN

typedef int CFFileDescriptorNativeDescriptor;

typedef struct CF_BRIDGED_MUTABLE_TYPE(id) __CFFileDescriptor * CFFileDescriptorRef;

/* Callback types */
typedef CF_OPTIONS(CFOptionFlags, CFFileDescriptorCallBackType) {
    kCFFileDescriptorReadCallBack   = 1UL << 0,
    kCFFileDescriptorWriteCallBack  = 1UL << 1
};

typedef void (*CFFileDescriptorCallBack)(CFFileDescriptorRef f,
                                         CFOptionFlags callBackTypes,
                                         void *info);

typedef struct {
    CFIndex     version;
    void *      info;
    void *      (*retain)(void *info);
    void        (*release)(void *info);
    CFStringRef (*copyDescription)(void *info);
} CFFileDescriptorContext;

CF_EXPORT CFTypeID CFFileDescriptorGetTypeID(void);

CF_EXPORT CFFileDescriptorRef CFFileDescriptorCreate(CFAllocatorRef allocator,
        CFFileDescriptorNativeDescriptor fd, Boolean closeOnInvalidate,
        CFFileDescriptorCallBack callout, const CFFileDescriptorContext *context);

CF_EXPORT CFFileDescriptorNativeDescriptor CFFileDescriptorGetNativeDescriptor(CFFileDescriptorRef f);

CF_EXPORT void CFFileDescriptorGetContext(CFFileDescriptorRef f, CFFileDescriptorContext *context);

CF_EXPORT void CFFileDescriptorEnableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes);
CF_EXPORT void CFFileDescriptorDisableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes);

CF_EXPORT void CFFileDescriptorInvalidate(CFFileDescriptorRef f);
CF_EXPORT Boolean CFFileDescriptorIsValid(CFFileDescriptorRef f);

CF_EXPORT CFRunLoopSourceRef CFFileDescriptorCreateRunLoopSource(CFAllocatorRef allocator,
        CFFileDescriptorRef f, CFIndex order);

CF_EXTERN_C_END
CF_IMPLICIT_BRIDGING_DISABLED

#endif /* ! __COREFOUNDATION_CFFILEDESCRIPTOR__ */
