/**
 * log.c
 * Author: Zoe Knox      Created: 2026-02-15
 *
 * Copyright (C) 2026 Zoe Knox. All rights reserved.
 * SPDX: BSD-2-Clause
 *
 * This is an original implementation of Apple's libsystem_trace.dylib based on
 * open-source code, including ASL, XNU, Swift, and other sources, and the API
 * specs on developer.apple.com. It is not based on decompilation or diassembly
 * of any closed-source object files.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* We really want to use the structs internally */
#undef OS_OBJECT_USE_OBJC
#define OS_OBJECT_USE_OBJC 0

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <dlfcn.h>
#include <errno.h>
#include <mach/mach_time.h>
/* <objc/runtime.h> intentionally not included: nothing here uses the ObjC
 * runtime, and objc4 links against libSystem, so it cannot be a build
 * dependency of libSystem. */
/* CoreFoundation types come from init.h - see the note there on why the real
 * CoreFoundation headers are not included. */
#include "log_internal.h"
#include <os/log_private.h>
/*
 * <os/log_encode_types.h> and <os/log_encode.h> are not included: the copies in
 * Kernel/xnu/libkern are the kernel-side encoder and pull in <os/log_mem.h>,
 * whose logmem_s embeds lck_spin_t. The only declaration wanted from them here
 * is the argument-chunk header walked by os_log_pack_size(), defined below.
 */

/*
 * Layout of one entry in the packed argument chunk: a descriptor byte, a
 * payload size byte, then payload_size bytes of data.
 */
typedef struct {
    uint8_t descriptor;
    uint8_t payload_size;
} _os_log_arg_header;
#include "init.h"


struct os_log_s _os_log_default;
struct os_log_s _os_log_disabled;
struct os_log_s _os_log_null;

__BEGIN_DECLS

os_log_t
os_log_create(const char* subsystem, const char* category)
{
    struct os_log_s* log = calloc(1, sizeof(struct os_log_s));
    if (!log)
        return (os_log_t)NULL;

    // FIXME: these probably leak
    if (_CFStringCreateWithCString) {
        log->subsystem = (void*)_CFStringCreateWithCString(NULL,
            subsystem, kCFStringEncodingUTF8);
        log->category = (void*)_CFStringCreateWithCString(NULL,
            category, kCFStringEncodingUTF8);
    }

    log->sink_type = OS_LOG_SINK_TYPE_FD;
    log->sink_dest = STDOUT_FILENO;

    /*
     * No os_retain() here: it lowers to objc_retain(), but this object is
     * plain calloc'd memory with a zeroed CFRuntimeBase - objc_retain would
     * mask a NULL isa and fault reading the class. The log objects are cached
     * in statics by their callers and never released, so the caller owns the
     * single reference.
     */
    return log;
}

bool
os_log_shim_enabled(os_log_t log, os_log_type_t type)
{
    switch (type) {
        case OS_LOG_TYPE_DEFAULT:
        case OS_LOG_TYPE_DEBUG:
        case OS_LOG_TYPE_INFO:
            return true;
        default:
            return false;
    }
    return true;
}

bool os_log_type_enabled(os_log_t log, os_log_type_t type)
{
    static _Atomic(int) resolved;
    static os_log_type_t lowest_enabled;

    (void)log;

    if (!atomic_load_explicit(&resolved, memory_order_acquire)) {
        const char *mode = getenv("OS_ACTIVITY_MODE");
        os_log_type_t lowest = OS_LOG_TYPE_DEFAULT;

        if (mode != NULL) {
            if (strcmp(mode, "debug") == 0) {
                lowest = OS_LOG_TYPE_DEBUG;
            } else if (strcmp(mode, "info") == 0) {
                lowest = OS_LOG_TYPE_INFO;
            }
        }
        lowest_enabled = lowest;
        atomic_store_explicit(&resolved, 1, memory_order_release);
    }

    /* The type values are not ordered by severity: debug is 0x02 and info is
     * 0x01, both below default (0x00) in importance but above it numerically. */
    switch (type) {
        case OS_LOG_TYPE_DEBUG:
            return lowest_enabled == OS_LOG_TYPE_DEBUG;
        case OS_LOG_TYPE_INFO:
            return lowest_enabled == OS_LOG_TYPE_DEBUG
                || lowest_enabled == OS_LOG_TYPE_INFO;
        default:
            /* default, error and fault are always emitted. */
            return true;
    }
}

bool
os_log_info_enabled(os_log_t log)
{
    return os_log_shim_enabled(log, OS_LOG_TYPE_INFO)
        && os_log_type_enabled(log, OS_LOG_TYPE_INFO);
}

bool
os_log_debug_enabled(os_log_t log)
{
    return os_log_shim_enabled(log, OS_LOG_TYPE_DEBUG)
        && os_log_type_enabled(log, OS_LOG_TYPE_DEBUG);
}

static const char* _typeToStr(os_log_type_t type)
{
    switch (type) {
        case OS_LOG_TYPE_DEFAULT: return "Normal";
        case OS_LOG_TYPE_INFO: return "Info";
        case OS_LOG_TYPE_DEBUG: return "Debug";
        case OS_LOG_TYPE_ERROR: return "ERROR";
        case OS_LOG_TYPE_FAULT: return "FAULT";
    }
    return "unknown";
}

__OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0)
OS_EXPORT OS_NOTHROW
void
os_log_with_args(os_log_t oslog, os_log_type_t type, const char* format, va_list args, void* ret_addr)
{
}

__WATCHOS_AVAILABLE(3.0) __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0)
OS_EXPORT OS_NOTHROW
void
_os_log_internal(void* dso, os_log_t log, os_log_type_t type, const char* message, ...)
{
}

__WATCHOS_AVAILABLE(3.0) __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0)
OS_EXPORT OS_NOTHROW
int
_os_log_internal_driverKit(void* dso, os_log_t log, os_log_type_t type, const char* message, ...)
{
    return 0; // FIXME: should return EPERM, ENOBUFS, EINVAL on error
}

/**
 * @function os_log_encode
 * @abstract Encodes the provided variable arguments using the provided
 *           format string into the memory pointed to by buffer. It will
 *           write at most buffer_size bytes. The encoded buffer can then
 *           be passed to os_log_pack_size() and os_log_pack_fill().
 * @param buffer       Pointer to destination buffer
 * @param buffer_size  Size of the destination buffer
 * @param format       printf-style format string
 * @param args         Varargs list to fill in format string
 * @param flags        Currently not used
 * @return The number of bytes written to the buffer. Zero is returned
 *         if buffer is NULL, buffer_size is too small, or an error
 *         occurred in serialization.
 */
size_t os_log_encode(void* buffer,
    size_t buffer_size,
    const char* format,
    va_list args,
    uint32_t flags)
{
    /*
     * Serialising into the binary os_log tracepoint format needs
     * _os_log_encode() from libtrace's private encoder, which PureDarwin does
     * not have: the only encoder in tree is XNU's kernel-side
     * os_log_context_encode(), which requires a logmem_t arena and a
     * preinitialised os_log_context_s that only the kernel can supply.
     *
     * Returning 0 is the documented "nothing was encoded" answer and is what
     * every caller here already handles; the human-readable paths below do not
     * route through this function.
     */
    (void)buffer; (void)buffer_size; (void)format; (void)args; (void)flags;
    return 0;
}

/* Wrapper used by os/assumes.h */
size_t __os_log_encode(void* buffer,
    size_t buffer_size,
    const char* format,
    ...)
{
    va_list args;
    va_start(args, format);
    size_t size = os_log_encode(buffer, buffer_size, format, args, 0);
    va_end(args);
    return size;
}

/*
 * %@ is CoreFoundation's specifier, not printf's, so rendering it means asking
 * CF for the object's description. libsystem_trace sits below CoreFoundation
 * and cannot link it, so look the three entry points up at runtime instead:
 * processes without CF (and there are several here) simply get the pointer.
 */
static void pd_append_cf_object(char** outp, size_t* remp, const void* cf)
{
    static void*    (*copy_description)(const void*);
    static int      (*get_cstring)(const void*, char*, long, uint32_t);
    static void     (*cf_release)(const void*);
    static int      looked_up;
    char            desc[512];
    void*           str;
    int             n;

    if (!looked_up) {
        copy_description = dlsym(RTLD_DEFAULT, "CFCopyDescription");
        get_cstring      = dlsym(RTLD_DEFAULT, "CFStringGetCString");
        cf_release       = dlsym(RTLD_DEFAULT, "CFRelease");
        looked_up = 1;
    }

    if (cf == NULL) {
        n = snprintf(*outp, *remp, "(null)");
        goto advance;
    }

    if (copy_description == NULL || get_cstring == NULL || cf_release == NULL) {
        n = snprintf(*outp, *remp, "<%p>", cf);
        goto advance;
    }

    str = copy_description(cf);
    if (str == NULL) {
        n = snprintf(*outp, *remp, "<%p>", cf);
        goto advance;
    }

    /* 0x08000100 is kCFStringEncodingUTF8. */
    if (get_cstring(str, desc, (long)sizeof(desc), 0x08000100)) {
        n = snprintf(*outp, *remp, "%s", desc);
    } else {
        n = snprintf(*outp, *remp, "<%p>", cf);
    }
    cf_release(str);

advance:
    if (n < 0) {
        return;
    }
    if ((size_t)n >= *remp) {
        *outp += *remp - 1;
        *remp = 1;
        return;
    }
    *outp += n;
    *remp -= (size_t)n;
}

/*
 * Render a format string and its arguments to text.
 *
 * vsnprintf cannot be used on the whole string because os_log formats may carry
 * %@, and once one specifier has to be handled here the rest must be too - a
 * va_list cannot be re-entered partway through. So walk the format, hand each
 * ordinary specifier to snprintf on its own, and expand %@ via CoreFoundation.
 */
static void pd_os_log_render(char* out, size_t out_size, const char* format,
    va_list ap)
{
    const char* p = format;
    char*       o = out;
    size_t      rem = out_size;

    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (format == NULL) {
        return;
    }

    while (*p != '\0' && rem > 1) {
        char    spec[64];
        size_t  slen = 0;
        int     star_width = 0, star_prec = 0;
        int     width = 0, prec = 0;
        int     n = 0;
        char    conv;
        /* 0 = int, 1 = long, 2 = long long, 3 = size_t/ptrdiff, 4 = short/char */
        int     lenmod = 0;

        if (*p != '%') {
            *o++ = *p++;
            rem--;
            continue;
        }

        spec[slen++] = *p++;            /* '%' */
        if (*p == '%') {
            *o++ = '%';
            rem--;
            p++;
            continue;
        }

        /*
         * os_log annotations - %{public}@, %{private}s, %{bool}d and friends.
         * They carry privacy and type hints for the logging system, not the
         * formatting, so drop them rather than passing them to snprintf.
         */
        if (*p == '{') {
            while (*p != '\0' && *p != '}') {
                p++;
            }
            if (*p == '}') {
                p++;
            }
        }

        /* flags */
        while (*p && strchr("-+ #0'", *p) && slen < sizeof(spec) - 8) {
            spec[slen++] = *p++;
        }
        /* width */
        if (*p == '*') {
            star_width = 1;
            width = va_arg(ap, int);
            p++;
        } else {
            while (*p >= '0' && *p <= '9' && slen < sizeof(spec) - 8) {
                spec[slen++] = *p++;
            }
        }
        /* precision */
        if (*p == '.') {
            spec[slen++] = *p++;
            if (*p == '*') {
                star_prec = 1;
                prec = va_arg(ap, int);
                p++;
            } else {
                while (*p >= '0' && *p <= '9' && slen < sizeof(spec) - 8) {
                    spec[slen++] = *p++;
                }
            }
        }
        /* length modifier */
        while (*p && strchr("hljztLq", *p) && slen < sizeof(spec) - 4) {
            if (*p == 'l') {
                lenmod = (lenmod == 1) ? 2 : 1;
            } else if (*p == 'h') {
                lenmod = 4;
            } else if (*p == 'j' || *p == 'z' || *p == 't' || *p == 'q') {
                lenmod = 3;
            }
            spec[slen++] = *p++;
        }

        conv = *p;
        if (conv == '\0') {
            break;
        }
        spec[slen++] = *p++;
        spec[slen] = '\0';

        /*
         * Rebuild the width/precision that were consumed as arguments, so the
         * single-specifier snprintf below sees a self-contained format.
         */
        if (star_width || star_prec) {
            char rebuilt[80];

            if (star_width && star_prec) {
                snprintf(rebuilt, sizeof(rebuilt), "%%%d.%d%c", width, prec, conv);
            } else if (star_width) {
                snprintf(rebuilt, sizeof(rebuilt), "%%%d%c", width, conv);
            } else {
                snprintf(rebuilt, sizeof(rebuilt), "%%.%d%c", prec, conv);
            }
            strlcpy(spec, rebuilt, sizeof(spec));
        }

        switch (conv) {
        case '@':
            pd_append_cf_object(&o, &rem, va_arg(ap, const void*));
            continue;
        case 'd': case 'i':
            if (lenmod == 2 || lenmod == 3) {
                n = snprintf(o, rem, spec, va_arg(ap, long long));
            } else if (lenmod == 1) {
                n = snprintf(o, rem, spec, va_arg(ap, long));
            } else {
                n = snprintf(o, rem, spec, va_arg(ap, int));
            }
            break;
        case 'o': case 'u': case 'x': case 'X':
            if (lenmod == 2 || lenmod == 3) {
                n = snprintf(o, rem, spec, va_arg(ap, unsigned long long));
            } else if (lenmod == 1) {
                n = snprintf(o, rem, spec, va_arg(ap, unsigned long));
            } else {
                n = snprintf(o, rem, spec, va_arg(ap, unsigned int));
            }
            break;
        case 'c':
            n = snprintf(o, rem, spec, va_arg(ap, int));
            break;
        case 's': {
            const char* sv = va_arg(ap, const char*);
            n = snprintf(o, rem, spec, sv ? sv : "(null)");
            break;
        }
        case 'p':
            n = snprintf(o, rem, spec, va_arg(ap, void*));
            break;
        case 'f': case 'F': case 'e': case 'E':
        case 'g': case 'G': case 'a': case 'A':
            n = snprintf(o, rem, spec, va_arg(ap, double));
            break;
        case 'n':
            /* Never write through a caller pointer from a log format. */
            (void)va_arg(ap, void*);
            n = 0;
            break;
        default:
            n = snprintf(o, rem, "%s", spec);
            break;
        }

        if (n < 0) {
            break;
        }
        if ((size_t)n >= rem) {
            o += rem - 1;
            rem = 1;
            break;
        }
        o += n;
        rem -= (size_t)n;
    }

    *o = '\0';
}

/*
 * The pack payload holds the rendered message rather than the format string
 * plus encoded arguments: without libtrace's binary tracepoint encoder (see
 * os_log_encode() above) the arguments cannot be serialised, and discarding
 * them left every %@/%s to reach the console literally. Reserve a fixed payload
 * so the rendering has somewhere to go.
 */
#define PD_OS_LOG_RENDER_MAX 1024

size_t os_log_pack_size(const char* format, ...)
{
    if (!format) {
        return 0;
    }
    return sizeof(struct os_log_pack_s) + PD_OS_LOG_RENDER_MAX;
}

uint8_t* os_log_pack_fill(void* pack,
    size_t pack_size,
    int saved_errno,
    const char* format, ...)
{
    const void* dso = __builtin_return_address(0);
    Dl_info info;
    struct os_log_pack_s *s;
    uint8_t* payload;
    size_t avail;
    va_list ap;

    (void)saved_errno;

    if (!pack || !format || pack_size <= sizeof(struct os_log_pack_s)) {
        return NULL;
    }

    s = (struct os_log_pack_s *)pack;
    payload = (uint8_t*)pack + sizeof(struct os_log_pack_s);
    avail = pack_size - sizeof(struct os_log_pack_s);

    s->olp_continuous_time = mach_continuous_time();
    clock_gettime(CLOCK_REALTIME, &s->olp_wall_time);

    /* Find the mach_header */
    if (dladdr(dso, &info)) {
        s->olp_mh = info.dli_fbase;
    }
    s->olp_pc = dso;

    va_start(ap, format);
    pd_os_log_render((char*)payload, avail, format, ap);
    va_end(ap);

    s->olp_format = (const char *)payload;

    return payload + strlen((const char*)payload) + 1;
}

/*
 * Render a pack to text. The argument payload cannot be expanded without the
 * binary tracepoint encoder (see os_log_encode() above), so the format string
 * is emitted as-is - which is what a caller printing the result wants to see.
 */
char* os_log_pack_compose(os_log_pack_t pack,
    os_log_t log,
    os_log_type_t type,
    char* buffer,
    size_t buffer_size)
{
    (void)log; (void)type;

    if (buffer == NULL || buffer_size == 0)
        return NULL;

    if (pack == NULL || pack->olp_format == NULL) {
        buffer[0] = '\0';
        return buffer;
    }

    strlcpy(buffer, pack->olp_format, buffer_size);
    return buffer;
}

char* os_log_pack_send_and_compose(os_log_pack_t pack,
    os_log_t log,
    os_log_type_t type,
    char* buffer,
    size_t buffer_size)
{
    os_log_pack_send(pack, log, type);
    return os_log_pack_compose(pack, log, type, buffer, buffer_size);
}

void os_log_pack_send(os_log_pack_t pack,
    os_log_t log,
    os_log_type_t type)
{
    /* FIXME: Send completed pack to ring buffer for logging */
}

void __os_log_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    if (!format)
        return;

    /* Every emission path funnels through here, so this is where the level
     * filter belongs; the os_log_debug()/os_log_info() macros do check
     * os_log_*_enabled() first, but only when the caller was compiled against
     * an SDK whose macros expand that way. */
    if (!os_log_type_enabled(log, type))
        return;

    /* This _should_ be identical to the NSObject layout */
    struct os_log_s* s = (struct os_log_s*)log;
    fprintf(stderr, "[%s] %s.%s(%p): ", _typeToStr(type), s->subsystem, s->category, dso);
    fputs(format, stderr);
    //vfprintf(stderr, format, args); // FIXME
    fputc('\n', stderr);

}

/*
 * The symbols <os/log.h>'s os_log_debug()/os_log_error() macros actually emit
 * carry a single leading underscore; the double-underscore variants below are
 * the ones this file already provided. Both spellings appear in the wild
 * depending on which SDK a caller was compiled against, so provide each.
 */
void _os_log_debug_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, type, format, buffer, buffer_size);
}

void _os_log_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, type, format, buffer, buffer_size);
}

void _os_log_fault_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, type, format, buffer, buffer_size);
}

void _os_log_error_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, type, format, buffer, buffer_size);
}

void __os_log_error_impl(void* dso,
    os_log_t log,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, OS_LOG_TYPE_ERROR, format, buffer, buffer_size);
}

void __os_log_fault_impl(void* dso,
    os_log_t log,
    const char* format,
    uint8_t* buffer,
    uint32_t buffer_size)
{
    __os_log_impl(dso, log, OS_LOG_TYPE_FAULT, format, buffer, buffer_size);
}

size_t _os_log_pack_init(void* pack,
    size_t pack_size,
    int saved_errno,
    const void* dso,
    const char* format,
    va_list args)
{

}

size_t _os_trace_encode(void* buffer,
    size_t buffer_size,
    const char* format,
    va_list args,
    uint32_t flags)
{
    return os_log_encode(buffer, buffer_size, format, args, flags);
}

void _os_trace_encode_and_send(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* format,
    va_list args,
    uint32_t flags)
{
    size_t buffer_size = 4096;
    char buffer[buffer_size];
    os_log_pack_t pack;

    size_t len = os_log_encode(buffer, buffer_size, format, args, flags);
    if (!len)
        return;

    //os_log_pack_size()
}

void _os_trace_debug(const char* message)
{
    _os_trace_encode_and_send(__builtin_return_address(0),
                              OS_LOG_DEFAULT,
                              OS_LOG_TYPE_DEBUG,
                              message,
                              NULL,
                              0);
}

void _os_trace_error(const char* message)
{
    _os_trace_encode_and_send(__builtin_return_address(0),
                              OS_LOG_DEFAULT,
                              OS_LOG_TYPE_ERROR,
                              message,
                              NULL,
                              0);
}

void _os_log_simple(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* message)
{
}

void _os_log_simple_impl(void* dso,
    os_log_t log,
    os_log_type_t type,
    const char* message)
{
}

void _os_log_error(void* dso, os_log_t log, const char* format, ...)
{
    char buffer[4096] = {0};
    size_t buffer_size = sizeof(buffer);
    va_list args;

    va_start(args, format);
    size_t len = os_log_encode(buffer, buffer_size, format, args, 0);
    va_end(args);
    __os_log_impl(dso, log, OS_LOG_TYPE_ERROR, format, buffer, buffer_size);
}

void _os_log_fault(void* dso, os_log_t log, const char* format, ...)
{
    char buffer[4096] = {0};
    size_t buffer_size = sizeof(buffer);
    va_list args;

    va_start(args, format);
    size_t len = os_log_encode(buffer, buffer_size, format, args, 0);
    va_end(args);
    __os_log_impl(dso, log, OS_LOG_TYPE_FAULT, format, buffer, buffer_size);
}

void _os_log_info(void* dso, os_log_t log, const char* format, ...)
{
    char buffer[4096] = {0};
    size_t buffer_size = sizeof(buffer);
    va_list args;

    va_start(args, format);
    size_t len = os_log_encode(buffer, buffer_size, format, args, 0);
    va_end(args);
    __os_log_impl(dso, log, OS_LOG_TYPE_INFO, format, buffer, buffer_size);
}

void _os_log_debug(void* dso, os_log_t log, const char* format, ...)
{
    char buffer[4096] = {0};
    size_t buffer_size = sizeof(buffer);
    va_list args;

    va_start(args, format);
    size_t len = os_log_encode(buffer, buffer_size, format, args, 0);
    va_end(args);
    __os_log_impl(dso, log, OS_LOG_TYPE_DEBUG, format, buffer, buffer_size);
}

void _os_log_set_mode(uint32_t mode)
{
}

uint32_t _os_log_get_mode(void)
{
    return 0; // FIXME: what should this return?
}

void _os_log_preferences_refresh(void)
{
}

// Heuristic: treat small or obviously sensitive data as private
static bool
isLikelyPrivateData(const UInt8 *bytes, CFIndex len)
{
    if (len == 0 || len > 4096) return false;

    // Simple heuristic: check for ASCII ranges typical of passwords/tokens
    int printable = 0;
    for (CFIndex i = 0; i < len; i++) {
        if (bytes[i] >= 32 && bytes[i] <= 126)
            printable++;
    }

    // If mostly printable and short -> likely a secret
    if (len <= 64 && printable > (len * 0.8))
        return true;

    return false;
}

bool
_NSCF2data(const void *obj, char *string_value, size_t string_sz, bool *is_private)
{
    if (!string_value || string_sz == 0)
        return false;

    string_value[0] = '\0';
    if (is_private) *is_private = false;

    // Check if this is toll-free bridged CFData/NSData
    /* CoreFoundation is resolved lazily (see init.h); if it never loaded, or
     * this is not a CFData, there is nothing to decode. */
    if (!obj || !_CFGetTypeID || !_CFDataGetTypeID || !_CFDataGetLength ||
        !_CFDataGetBytePtr ||
        _CFGetTypeID(obj) != _CFDataGetTypeID()) {
        snprintf(string_value, string_sz, "<not CFData>");
        return false;
    }

    CFDataRef data = (CFDataRef)obj;
    CFIndex len = _CFDataGetLength(data);
    const UInt8 *bytes = _CFDataGetBytePtr(data);

    if (!bytes) {
        snprintf(string_value, string_sz, "<null bytes>");
        return false;
    }

    // Determine privacy
    bool privateFlag = isLikelyPrivateData(bytes, len);
    if (is_private) *is_private = privateFlag;

    if (privateFlag) {
        // Redacted output similar to Apple logging
        snprintf(string_value, string_sz,
                 "<NSData %ld bytes: private>", (long)len);
        return true;
    }

    // Non-private: produce a hex dump (truncated to fit buffer)
    size_t maxBytes = (string_sz - 1) / 2; // 2 chars per byte
    if (maxBytes > (size_t)len) maxBytes = len;

    char *p = string_value;
    for (size_t i = 0; i < maxBytes; i++) {
        snprintf(p, 3, "%02X", bytes[i]);
        p += 2;
    }

    // If truncated, append ellipsis
    if (maxBytes < (size_t)len && (p - string_value + 4) < string_sz) {
        strcpy(p, "...");
    }

    return true;
}

bool _os_log_string_is_public(const char* str)
{
    return true; // FIXME
}
