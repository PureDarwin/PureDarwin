/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * Distributed notifications without a distnoted daemon.
 *
 * notifyd carries the wakeup and a shared append-only log carries the payload.
 * A broker service could hold the payload instead, but it could not push to
 * clients: bootstrap_check_in only hands out names a job declares in its
 * MachServices, so an arbitrary process cannot register a port for callbacks.
 * Every process can, however, read a file and receive a notify token.
 *
 * A posting process appends a record under an exclusive flock and posts the
 * notify name; observers wake, read whatever is new since their own offset,
 * and hand each record to the inherited NSNotificationCenter for matching.
 */

#import <Foundation/NSDistributedNotificationCenter.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSData.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSKeyedUnarchiver.h>
#include <notify.h>
#include <dispatch/dispatch.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

NSNotificationCenterType const NSLocalNotificationCenterType =
    @"NSLocalNotificationCenterType";

static const char *kDistributedNotifyName = "org.puredarwin.distributed_notification";
static const uint32_t kRecordMagic = 0x504e4f54; /* 'PNOT' */

/* Bounded so a long-running session cannot grow the log without limit; a
 * writer that trips the cap truncates, and readers notice their offset is
 * past the end and start over. */
static const long long kMaximumLogBytes = 1 << 20;

static NSString * const kNameKey = @"name";
static NSString * const kObjectKey = @"object";
static NSString * const kUserInfoKey = @"userInfo";

static const char *_logPath(void) {
    static char path[256];
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        const char *dir = getenv("PUREDARWIN_DISTNOTE_DIR");

        if (dir == NULL) {
            dir = "/var/run";
            if (access(dir, W_OK) != 0) {
                dir = "/tmp";
            }
        }
        snprintf(path, sizeof(path), "%s/puredarwin-distnote.log", dir);
    });
    return path;
}

@implementation NSDistributedNotificationCenter

+ (NSDistributedNotificationCenter *)defaultCenter {
    static NSDistributedNotificationCenter *shared = nil;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        shared = [[self alloc] init];
    });
    return shared;
}

+ (NSDistributedNotificationCenter *)notificationCenterForType:(NSNotificationCenterType)type {
    return [self defaultCenter];
}

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        _notifyToken = -1;
        _queue = (id)dispatch_queue_create("org.puredarwin.distnote", NULL);

        /* Start at the end: a new observer does not receive history. */
        struct stat info;

        _offset = (stat(_logPath(), &info) == 0) ? (long long)info.st_size : 0;
        [self _startListening];
    }
    return self;
}

- (void)dealloc {
    if (_notifyToken != -1) {
        notify_cancel(_notifyToken);
    }
    if (_queue != nil) {
        dispatch_release((dispatch_queue_t)_queue);
    }
    [super dealloc];
}

- (void)_startListening {
    if (_notifyToken != -1) {
        return;
    }

    __block NSDistributedNotificationCenter *center = self;
    uint32_t status = notify_register_dispatch(kDistributedNotifyName, &_notifyToken,
                                               (dispatch_queue_t)_queue, ^(int token) {
        [center _drainLog];
    });

    if (status != NOTIFY_STATUS_OK) {
        _notifyToken = -1;
    }
}

/* Reads every record written since this process last looked. */
- (void)_drainLog {
    int fd = open(_logPath(), O_RDONLY);

    if (fd < 0) {
        return;
    }
    flock(fd, LOCK_SH);

    struct stat info;

    if (fstat(fd, &info) != 0) {
        flock(fd, LOCK_UN);
        close(fd);
        return;
    }
    if (info.st_size < _offset) {
        /* The log was truncated by a writer that hit the size cap. */
        _offset = 0;
    }

    if (lseek(fd, (off_t)_offset, SEEK_SET) == (off_t)-1) {
        flock(fd, LOCK_UN);
        close(fd);
        return;
    }

    NSMutableArray *pending = [NSMutableArray array];

    for (;;) {
        uint32_t header[2];
        ssize_t got = read(fd, header, sizeof(header));

        if (got != (ssize_t)sizeof(header) || header[0] != kRecordMagic) {
            break;
        }

        uint32_t length = header[1];
        void *bytes = malloc(length);

        if (bytes == NULL) {
            break;
        }
        if (read(fd, bytes, length) != (ssize_t)length) {
            free(bytes);
            break;
        }

        NSData *data = [NSData dataWithBytes:bytes length:length];

        free(bytes);
        _offset += (long long)(sizeof(header) + length);

        id record = [NSKeyedUnarchiver unarchiveObjectWithData:data];

        if ([record isKindOfClass:[NSDictionary class]]) {
            [pending addObject:record];
        }
    }
    flock(fd, LOCK_UN);
    close(fd);

    if (_suspended) {
        return;
    }

    /* Deliver through the inherited centre so observer matching, including
     * the name and object filters, behaves exactly as it does locally. */
    for (NSDictionary *record in pending) {
        id object = [record objectForKey:kObjectKey];
        id userInfo = [record objectForKey:kUserInfoKey];

        [super postNotificationName:[record objectForKey:kNameKey]
                             object:([object isKindOfClass:[NSNull class]] ? nil : object)
                           userInfo:([userInfo isKindOfClass:[NSNull class]] ? nil : userInfo)];
    }
}

- (void)addObserver:(id)observer
           selector:(SEL)selector
               name:(NSNotificationName)name
             object:(NSString *)object
 suspensionBehavior:(NSNotificationSuspensionBehavior)suspensionBehavior {
    [self addObserver:observer selector:selector name:name object:object];
}

- (void)postNotificationName:(NSNotificationName)name object:(NSString *)object {
    [self postNotificationName:name object:object userInfo:nil];
}

- (void)postNotificationName:(NSNotificationName)name
                      object:(NSString *)object
                    userInfo:(NSDictionary *)userInfo {
    if (name == nil) {
        return;
    }

    NSDictionary *record = [NSDictionary dictionaryWithObjectsAndKeys:
        name, kNameKey,
        (object != nil) ? (id)object : (id)[NSNull null], kObjectKey,
        (userInfo != nil) ? (id)userInfo : (id)[NSNull null], kUserInfoKey,
        nil];
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:record];

    if (data == nil) {
        return;
    }

    int fd = open(_logPath(), O_WRONLY | O_CREAT | O_APPEND, 0666);

    if (fd < 0) {
        return;
    }
    flock(fd, LOCK_EX);

    struct stat info;

    if (fstat(fd, &info) == 0 && info.st_size > kMaximumLogBytes) {
        ftruncate(fd, 0);
    }

    uint32_t header[2] = { kRecordMagic, (uint32_t)[data length] };

    if (write(fd, header, sizeof(header)) == (ssize_t)sizeof(header)) {
        write(fd, [data bytes], [data length]);
    }
    flock(fd, LOCK_UN);
    close(fd);

    notify_post(kDistributedNotifyName);
}

- (void)postNotificationName:(NSNotificationName)name
                      object:(NSString *)object
                    userInfo:(NSDictionary *)userInfo
          deliverImmediately:(BOOL)deliverImmediately {
    [self postNotificationName:name object:object userInfo:userInfo];
}

- (void)postNotificationName:(NSNotificationName)name
                      object:(NSString *)object
                    userInfo:(NSDictionary *)userInfo
                     options:(NSDistributedNotificationOptions)options {
    [self postNotificationName:name object:object userInfo:userInfo];
}

- (void)setSuspended:(BOOL)suspended {
    _suspended = suspended;
    if (!suspended) {
        [self _drainLog];
    }
}

- (BOOL)isSuspended {
    return _suspended;
}

@end
