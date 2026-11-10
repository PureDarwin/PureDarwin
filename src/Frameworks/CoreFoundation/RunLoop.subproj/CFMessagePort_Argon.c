// CFMessagePort_Argon.c - CFMessagePort for Argon on Linux, over Unix domain sockets
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// a port listens on the abstract socket "argon.cfmessageport.<name>", one connection per request and reply
// ports serve on their dispatch queue, run loop sources on the main queue, and run loop modes are not kept apart

#include <CoreFoundation/CFMessagePort.h>
#include <CoreFoundation/CFRunLoop.h>
#include "CFInternal.h"
#include "CFRuntime_Internal.h"

#if ARGON_LIBOBJC2
#include <dispatch/dispatch.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct __CFMessagePort {
    CFRuntimeBase _base;
    CFStringRef _name;
    Boolean _local;
    Boolean _valid;
    int _fd;
    CFMessagePortCallBack _callout;
    CFMessagePortContext _context;
    CFMessagePortInvalidationCallBack _invalidation;
    dispatch_queue_t _queue;
    dispatch_source_t _listener;
    pthread_mutex_t _lock;
};

struct argon_message { int32_t code; uint32_t length; };

static CFTypeID __kCFMessagePortTypeID = _kCFRuntimeNotATypeID;

static void __CFMessagePortDeallocate(CFTypeRef cf) {
    CFMessagePortRef ms = (CFMessagePortRef)cf;

    CFMessagePortInvalidate(ms);
    if (ms->_name) CFRelease(ms->_name);
    if (ms->_context.release && ms->_context.info) ms->_context.release(ms->_context.info);
    pthread_mutex_destroy(&ms->_lock);
}

static const CFRuntimeClass __CFMessagePortClass = {
    0, "CFMessagePort", NULL, NULL, __CFMessagePortDeallocate, NULL, NULL, NULL, NULL
};

CFTypeID CFMessagePortGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ __kCFMessagePortTypeID = _CFRuntimeRegisterClass(&__CFMessagePortClass); });
    return __kCFMessagePortTypeID;
}

// the abstract socket address for a port name, its length in *size
static Boolean __CFMessagePortAddress(CFStringRef name, struct sockaddr_un *address, socklen_t *size) {
    static const char prefix[] = "argon.cfmessageport.";
    char utf8[sizeof(address->sun_path) - sizeof(prefix) - 1];

    if (name == NULL || !CFStringGetCString(name, utf8, sizeof(utf8), kCFStringEncodingUTF8)) return false;
    memset(address, 0, sizeof(*address));
    address->sun_family = AF_UNIX;
    // sun_path[0] stays NUL, which makes the name abstract
    memcpy(address->sun_path + 1, prefix, sizeof(prefix) - 1);
    memcpy(address->sun_path + sizeof(prefix), utf8, strlen(utf8));
    *size = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + sizeof(prefix) + strlen(utf8));
    return true;
}

static Boolean __CFReadAll(int fd, void *buffer, size_t length, int timeoutMs) {
    char *at = (char *)buffer;

    while (length > 0) {
        struct pollfd p = { fd, POLLIN, 0 };
        if (timeoutMs >= 0 && poll(&p, 1, timeoutMs) <= 0) return false;
        ssize_t n = read(fd, at, length);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return false;
        }
        at += n;
        length -= (size_t)n;
    }
    return true;
}

static Boolean __CFWriteAll(int fd, const void *buffer, size_t length) {
    const char *at = (const char *)buffer;

    while (length > 0) {
        ssize_t n = write(fd, at, length);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return false;
        }
        at += n;
        length -= (size_t)n;
    }
    return true;
}

static CFMessagePortRef __CFMessagePortCreate(CFAllocatorRef allocator, CFStringRef name, Boolean local) {
    CFIndex size = sizeof(struct __CFMessagePort) - sizeof(CFRuntimeBase);
    struct __CFMessagePort *ms = (struct __CFMessagePort *)_CFRuntimeCreateInstance(allocator, CFMessagePortGetTypeID(), size, NULL);

    if (ms == NULL) return NULL;
    ms->_name = name ? CFStringCreateCopy(allocator, name) : NULL;
    ms->_local = local;
    ms->_valid = true;
    ms->_fd = -1;
    pthread_mutex_init(&ms->_lock, NULL);
    return ms;
}

CFMessagePortRef CFMessagePortCreateLocal(CFAllocatorRef allocator, CFStringRef name, CFMessagePortCallBack callout, CFMessagePortContext *context, Boolean *shouldFreeInfo) {
    struct sockaddr_un address;
    socklen_t size;
    int fd = -1;

    if (shouldFreeInfo) *shouldFreeInfo = true;
    if (name != NULL) {
        if (!__CFMessagePortAddress(name, &address, &size)) return NULL;
        fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) return NULL;
        // a name already taken is a port another process owns
        if (bind(fd, (struct sockaddr *)&address, size) != 0 || listen(fd, 16) != 0) {
            close(fd);
            return NULL;
        }
    }
    struct __CFMessagePort *ms = (struct __CFMessagePort *)__CFMessagePortCreate(allocator, name, true);
    if (ms == NULL) {
        if (fd >= 0) close(fd);
        return NULL;
    }
    ms->_fd = fd;
    ms->_callout = callout;
    if (context) {
        ms->_context = *context;
        if (context->retain && context->info) ms->_context.info = (void *)context->retain(context->info);
    }
    if (shouldFreeInfo) *shouldFreeInfo = false;
    return ms;
}

CFMessagePortRef CFMessagePortCreateRemote(CFAllocatorRef allocator, CFStringRef name) {
    struct sockaddr_un address;
    socklen_t size;

    if (!__CFMessagePortAddress(name, &address, &size)) return NULL;
    // as on Darwin, a name nobody has checked in gives no port
    int probe = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (probe < 0) return NULL;
    Boolean listening = connect(probe, (struct sockaddr *)&address, size) == 0;
    close(probe);
    return listening ? __CFMessagePortCreate(allocator, name, false) : NULL;
}

Boolean CFMessagePortIsRemote(CFMessagePortRef ms) { return !ms->_local; }
CFStringRef CFMessagePortGetName(CFMessagePortRef ms) { return ms->_name; }
Boolean CFMessagePortSetName(CFMessagePortRef ms, CFStringRef newName) { (void)ms; (void)newName; return false; }
void CFMessagePortGetContext(CFMessagePortRef ms, CFMessagePortContext *context) { if (context) *context = ms->_context; }
Boolean CFMessagePortIsValid(CFMessagePortRef ms) { return ms != NULL && ms->_valid; }
CFMessagePortInvalidationCallBack CFMessagePortGetInvalidationCallBack(CFMessagePortRef ms) { return ms->_invalidation; }
void CFMessagePortSetInvalidationCallBack(CFMessagePortRef ms, CFMessagePortInvalidationCallBack callout) { ((struct __CFMessagePort *)ms)->_invalidation = callout; }

void CFMessagePortInvalidate(CFMessagePortRef port) {
    struct __CFMessagePort *ms = (struct __CFMessagePort *)port;
    CFMessagePortInvalidationCallBack callout = NULL;

    pthread_mutex_lock(&ms->_lock);
    if (ms->_valid) {
        ms->_valid = false;
        callout = ms->_invalidation;
        if (ms->_listener) {
            dispatch_source_cancel(ms->_listener);
            ms->_listener = NULL;
        } else if (ms->_fd >= 0) {
            close(ms->_fd);
        }
        ms->_fd = -1;
    }
    pthread_mutex_unlock(&ms->_lock);
    if (callout) callout(port, ms->_context.info);
}

// one request on an accepted connection: read it, run the callout, answer
static void __CFMessagePortServe(CFMessagePortRef ms, int client) {
    struct argon_message header;
    UInt8 *bytes = NULL;

    if (__CFReadAll(client, &header, sizeof(header), 5000) && header.length < (64u << 20)) {
        bytes = header.length ? malloc(header.length) : NULL;
        if (header.length == 0 || (bytes && __CFReadAll(client, bytes, header.length, 5000))) {
            CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorSystemDefault, bytes ? bytes : (const UInt8 *)"", header.length, kCFAllocatorNull);
            CFDataRef reply = ms->_callout ? ms->_callout(ms, header.code, data, ms->_context.info) : NULL;
            struct argon_message answer = { kCFMessagePortSuccess, reply ? (uint32_t)CFDataGetLength(reply) : 0 };

            if (__CFWriteAll(client, &answer, sizeof(answer)) && answer.length)
                __CFWriteAll(client, CFDataGetBytePtr(reply), answer.length);
            if (reply) CFRelease(reply);
            CFRelease(data);
        }
    }
    free(bytes);
    close(client);
}

void CFMessagePortSetDispatchQueue(CFMessagePortRef port, dispatch_queue_t queue) {
    struct __CFMessagePort *ms = (struct __CFMessagePort *)port;

    pthread_mutex_lock(&ms->_lock);
    if (ms->_listener) {
        dispatch_source_cancel(ms->_listener);
        ms->_listener = NULL;
    }
    if (queue && ms->_valid && ms->_local && ms->_fd >= 0) {
        int fd = ms->_fd;
        dispatch_source_t listener = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)fd, 0, queue);

        CFRetain(port);
        dispatch_source_set_event_handler(listener, ^{
            int client = accept4(fd, NULL, NULL, SOCK_CLOEXEC);
            if (client >= 0) __CFMessagePortServe(port, client);
        });
        dispatch_source_set_cancel_handler(listener, ^{
            close(fd);
            CFRelease(port);
        });
        ms->_listener = listener;
        ms->_queue = queue;
        dispatch_resume(listener);
    }
    pthread_mutex_unlock(&ms->_lock);
}

static void __CFMessagePortSourceNoop(void *info) { (void)info; }

CFRunLoopSourceRef CFMessagePortCreateRunLoopSource(CFAllocatorRef allocator, CFMessagePortRef local, CFIndex order) {
    CFRunLoopSourceContext context = { 0 };

    if (local == NULL || !local->_local) return NULL;
    // the requests are served on the main queue, the source only stands for the port
    if (local->_queue == NULL) CFMessagePortSetDispatchQueue(local, dispatch_get_main_queue());
    context.info = (void *)local;
    context.perform = __CFMessagePortSourceNoop;
    return CFRunLoopSourceCreate(allocator, order, &context);
}

SInt32 CFMessagePortSendRequest(CFMessagePortRef remote, SInt32 msgid, CFDataRef data, CFTimeInterval sendTimeout, CFTimeInterval rcvTimeout, CFStringRef replyMode, CFDataRef *returnData) {
    struct sockaddr_un address;
    socklen_t size;
    struct argon_message header = { msgid, data ? (uint32_t)CFDataGetLength(data) : 0 };
    (void)sendTimeout;

    if (returnData) *returnData = NULL;
    if (!CFMessagePortIsValid(remote) || !__CFMessagePortAddress(remote->_name, &address, &size)) return kCFMessagePortIsInvalid;
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return kCFMessagePortTransportError;
    if (connect(fd, (struct sockaddr *)&address, size) != 0) {
        close(fd);
        CFMessagePortInvalidate(remote);
        return kCFMessagePortIsInvalid;
    }
    if (!__CFWriteAll(fd, &header, sizeof(header)) || (header.length && !__CFWriteAll(fd, CFDataGetBytePtr(data), header.length))) {
        close(fd);
        return kCFMessagePortSendTimeout;
    }
    // a one-way message has no reply mode
    if (replyMode == NULL) {
        close(fd);
        return kCFMessagePortSuccess;
    }
    int timeoutMs = rcvTimeout > 0 ? (int)(rcvTimeout * 1000.0) : -1;
    struct argon_message answer;
    SInt32 result = kCFMessagePortReceiveTimeout;
    if (__CFReadAll(fd, &answer, sizeof(answer), timeoutMs)) {
        UInt8 *bytes = answer.length ? malloc(answer.length) : NULL;
        if (answer.length == 0 || (bytes && __CFReadAll(fd, bytes, answer.length, timeoutMs))) {
            if (returnData) *returnData = CFDataCreate(kCFAllocatorSystemDefault, bytes, answer.length);
            result = answer.code;
        }
        free(bytes);
    }
    close(fd);
    return result;
}
#endif
