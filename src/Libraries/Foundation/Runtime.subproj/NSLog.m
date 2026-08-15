/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

/* Apple's NSLog prefixes "yyyy-MM-dd HH:mm:ss.SSS progname[pid:tid] ". We keep
 * the shape but drop the thread id, which needs a lookup to be useful. libc
 * rather than CFDate: this is local time, and it works before CF is up. */
static void __NSLogWritePrefix(void) {
    struct timeval tv;
    struct tm tm;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);

    fprintf(stderr, "%04d-%02d-%02d %02d:%02d:%02d.%03d %s[%d] ",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000),
            getprogname(), (int)getpid());
}

void NSLogv(NSString *format, va_list args) {
    if (!format) {
        return;
    }

    CFStringRef message = CFStringCreateWithFormatAndArguments(
        kCFAllocatorDefault, NULL, (CFStringRef)format, args);
    if (!message) {
        return;
    }

    __NSLogWritePrefix();

    /* Fast path when the string is already 8-bit; otherwise convert into a
     * buffer sized for the worst-case UTF-8 expansion. */
    const char *direct = CFStringGetCStringPtr(message, kCFStringEncodingUTF8);
    if (direct) {
        fprintf(stderr, "%s\n", direct);
    } else {
        CFIndex length = CFStringGetLength(message);
        CFIndex size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
        char *buffer = (char *)malloc((size_t)size);
        if (buffer) {
            if (CFStringGetCString(message, buffer, size, kCFStringEncodingUTF8)) {
                fprintf(stderr, "%s\n", buffer);
            }
            free(buffer);
        }
    }

    fflush(stderr);
    CFRelease(message);
}

void NSLog(NSString *format, ...) {
    va_list args;
    va_start(args, format);
    NSLogv(format, args);
    va_end(args);
}
