/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSHost.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

@implementation NSHost

/* Resolves through getaddrinfo and keeps both the canonical name and every
 * address the resolver returned. */
- (instancetype)_initWithName:(NSString *)name {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    struct addrinfo hints;
    struct addrinfo *results = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    if (getaddrinfo([name UTF8String], NULL, &hints, &results) != 0) {
        [self release];
        return nil;
    }

    NSMutableArray *names = [[NSMutableArray alloc] init];
    NSMutableArray *addresses = [[NSMutableArray alloc] init];

    for (struct addrinfo *entry = results; entry != NULL; entry = entry->ai_next) {
        char text[INET6_ADDRSTRLEN];
        const void *raw = NULL;

        if (entry->ai_family == AF_INET) {
            raw = &((struct sockaddr_in *)entry->ai_addr)->sin_addr;
        } else if (entry->ai_family == AF_INET6) {
            raw = &((struct sockaddr_in6 *)entry->ai_addr)->sin6_addr;
        }
        if (raw != NULL && inet_ntop(entry->ai_family, raw, text, sizeof(text)) != NULL) {
            NSString *address = [NSString stringWithUTF8String:text];

            if (![addresses containsObject:address]) {
                [addresses addObject:address];
            }
        }
        if (entry->ai_canonname != NULL) {
            NSString *canonical = [NSString stringWithUTF8String:entry->ai_canonname];

            if (![names containsObject:canonical]) {
                [names addObject:canonical];
            }
        }
    }
    freeaddrinfo(results);

    if ([names count] == 0) {
        [names addObject:name];
    }

    _names = names;
    _addresses = addresses;
    return self;
}

+ (NSHost *)currentHost {
    char hostname[256];

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return nil;
    }
    hostname[sizeof(hostname) - 1] = '\0';
    return [self hostWithName:[NSString stringWithUTF8String:hostname]];
}

+ (NSHost *)hostWithName:(NSString *)name {
    if (name == nil) {
        return nil;
    }
    return [[[self alloc] _initWithName:name] autorelease];
}

+ (NSHost *)hostWithAddress:(NSString *)address {
    return [self hostWithName:address];
}

- (void)dealloc {
    [_names release];
    [_addresses release];
    [super dealloc];
}

- (NSString *)name {
    return [_names count] > 0 ? [_names objectAtIndex:0] : nil;
}

- (NSArray *)names {
    return _names;
}

- (NSString *)address {
    return [_addresses count] > 0 ? [_addresses objectAtIndex:0] : nil;
}

- (NSArray *)addresses {
    return _addresses;
}

- (BOOL)isEqualToHost:(NSHost *)host {
    if (host == self) {
        return YES;
    }
    for (NSString *address in [host addresses]) {
        if ([_addresses containsObject:address]) {
            return YES;
        }
    }
    return NO;
}

@end
