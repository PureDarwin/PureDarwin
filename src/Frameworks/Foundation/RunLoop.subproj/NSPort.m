/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#import <Foundation/NSPort.h>
#import <Foundation/NSPortMessage.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSRunLoop.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFRunLoop.h>
#include <stdlib.h>

NSString * const NSPortDidBecomeInvalidNotification = @"NSPortDidBecomeInvalidNotification";

@implementation NSPort

+ (NSPort *)port {
    return [[[NSMachPort alloc] init] autorelease];
}

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        mach_port_t port = MACH_PORT_NULL;

        if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                               &port) != KERN_SUCCESS) {
            [self release];
            return nil;
        }
        if (mach_port_insert_right(mach_task_self(), port, port,
                                   MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) {
            mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
            [self release];
            return nil;
        }
        _machPort = port;
        _ownsPort = YES;
        _valid = YES;
    }
    return self;
}

- (void)dealloc {
    [self invalidate];
    [super dealloc];
}

- (void)_unschedule {
    if (_cfSource != NULL) {
        CFRunLoopSourceInvalidate(_cfSource);
        CFRelease(_cfSource);
        _cfSource = NULL;
    }
    if (_cfPort != NULL) {
        CFMachPortInvalidate(_cfPort);
        CFRelease(_cfPort);
        _cfPort = NULL;
    }
}

- (void)invalidate {
    if (!_valid) {
        return;
    }
    _valid = NO;
    [self _unschedule];

    if (_ownsPort && _machPort != MACH_PORT_NULL) {
        mach_port_mod_refs(mach_task_self(), _machPort, MACH_PORT_RIGHT_RECEIVE, -1);
        mach_port_deallocate(mach_task_self(), _machPort);
    }
    _machPort = MACH_PORT_NULL;

    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSPortDidBecomeInvalidNotification object:self];
}

- (BOOL)isValid {
    return _valid;
}

- (void)setDelegate:(id)delegate {
    _delegate = delegate;
}

- (id)delegate {
    return _delegate;
}

// a received message goes to the delegate: -handleMachMessage: with the raw message, or else
// -handlePortMessage: with its id and components decoded
static void portCallback(CFMachPortRef cfPort, void *msg, CFIndex size, void *info) {
    NSPort *port = info;
    id delegate = [port delegate];
    mach_msg_header_t *header = msg;

    if ([delegate respondsToSelector:@selector(handleMachMessage:)]) {
        [delegate handleMachMessage:msg];
        return;
    }
    if (![delegate respondsToSelector:@selector(handlePortMessage:)])
        return;

    NSMutableArray *components = [NSMutableArray array];
    if (header->msgh_bits & MACH_MSGH_BITS_COMPLEX) {
        mach_msg_body_t *body = (mach_msg_body_t *)(header + 1);
        mach_msg_descriptor_t *descriptor = (mach_msg_descriptor_t *)(body + 1);

        for (mach_msg_size_t i = 0; i < body->msgh_descriptor_count; i++, descriptor++) {
            if (descriptor->type.type == MACH_MSG_OOL_DESCRIPTOR) {
                mach_msg_ool_descriptor_t *ool = &descriptor->out_of_line;
                [components addObject:[NSData dataWithBytes:ool->address length:ool->size]];
                vm_deallocate(mach_task_self(), (vm_address_t)ool->address, ool->size);
            } else if (descriptor->type.type == MACH_MSG_PORT_DESCRIPTOR) {
                [components addObject:[NSMachPort portWithMachPort:descriptor->port.name]];
            }
        }
    }
    NSPort *reply = header->msgh_local_port != MACH_PORT_NULL && header->msgh_local_port != [(NSMachPort *)port machPort]
        ? [NSMachPort portWithMachPort:header->msgh_local_port] : nil;
    NSPortMessage *message = [[NSPortMessage alloc] initWithSendPort:reply receivePort:port components:components];
    [message setMsgid:header->msgh_id];
    // PD_PORT_TRACE=1 logs each message a port delegate gets
    if (getenv("PD_PORT_TRACE") != NULL)
        fprintf(stderr, "NSMachPort: message id 0x%08x, %lu components\n", (unsigned)header->msgh_id, (unsigned long)[components count]);
    [delegate handlePortMessage:message];
    [message release];
}

// the receive right goes into the run loop's CFRunLoop as a source, which -runMode:beforeDate: services
- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSRunLoopMode)mode {
    if (!_valid)
        return;
    if (_cfPort == NULL) {
        CFMachPortContext context = { 0, self, NULL, NULL, NULL };
        Boolean shouldFree = false;

        _cfPort = CFMachPortCreateWithPort(kCFAllocatorDefault, _machPort, portCallback, &context, &shouldFree);
        if (_cfPort == NULL)
            return;
        _cfSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, _cfPort, 0);
    }
    CFRunLoopAddSource([runLoop getCFRunLoop], _cfSource, (CFStringRef)mode);
}

- (void)removeFromRunLoop:(NSRunLoop *)runLoop forMode:(NSRunLoopMode)mode {
    if (_cfSource != NULL)
        CFRunLoopRemoveSource([runLoop getCFRunLoop], _cfSource, (CFStringRef)mode);
}

- (NSUInteger)reservedSpaceLength {
    return 0;
}

- (BOOL)sendBeforeDate:(NSDate *)limitDate components:(NSMutableArray *)components
                  from:(NSPort *)receivePort reserved:(NSUInteger)headerSpaceReserved {
    return [self sendBeforeDate:limitDate msgid:0 components:components from:receivePort reserved:headerSpaceReserved];
}

// components that are NSData travel out of line and NSPort components as send rights
- (BOOL)sendBeforeDate:(NSDate *)limitDate msgid:(NSUInteger)msgID components:(NSMutableArray *)components
                  from:(NSPort *)receivePort reserved:(NSUInteger)headerSpaceReserved {
    if (!_valid)
        return NO;

    NSUInteger count = [components count];
    size_t size = sizeof(mach_msg_header_t) + (count ? sizeof(mach_msg_body_t) + count * sizeof(mach_msg_descriptor_t) : 0);
    mach_msg_header_t *header = calloc(1, size);
    mach_port_t reply = [receivePort isKindOfClass:[NSMachPort class]] ? [(NSMachPort *)receivePort machPort] : MACH_PORT_NULL;

    header->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, reply != MACH_PORT_NULL ? MACH_MSG_TYPE_MAKE_SEND : 0);
    header->msgh_size = (mach_msg_size_t)size;
    header->msgh_remote_port = _machPort;
    header->msgh_local_port = reply;
    header->msgh_id = (mach_msg_id_t)msgID;

    if (count != 0) {
        mach_msg_body_t *body = (mach_msg_body_t *)(header + 1);
        mach_msg_descriptor_t *descriptor = (mach_msg_descriptor_t *)(body + 1);

        header->msgh_bits |= MACH_MSGH_BITS_COMPLEX;
        body->msgh_descriptor_count = (mach_msg_size_t)count;
        for (NSUInteger i = 0; i < count; i++, descriptor++) {
            id component = [components objectAtIndex:i];

            if ([component isKindOfClass:[NSMachPort class]]) {
                descriptor->port.name = [component machPort];
                descriptor->port.disposition = MACH_MSG_TYPE_COPY_SEND;
                descriptor->port.type = MACH_MSG_PORT_DESCRIPTOR;
            } else {
                NSData *data = [component isKindOfClass:[NSData class]] ? component : [NSData data];
                descriptor->out_of_line.address = (void *)[data bytes];
                descriptor->out_of_line.size = (mach_msg_size_t)[data length];
                descriptor->out_of_line.copy = MACH_MSG_VIRTUAL_COPY;
                descriptor->out_of_line.deallocate = FALSE;
                descriptor->out_of_line.type = MACH_MSG_OOL_DESCRIPTOR;
            }
        }
    }

    NSTimeInterval wait = limitDate != nil ? [limitDate timeIntervalSinceNow] : 0;
    mach_msg_option_t options = MACH_SEND_MSG;
    mach_msg_timeout_t timeout = MACH_MSG_TIMEOUT_NONE;
    if (wait < 1e7) {
        options |= MACH_SEND_TIMEOUT;
        timeout = wait > 0 ? (mach_msg_timeout_t)(wait * 1000) : 0;
    }
    kern_return_t result = mach_msg(header, options, (mach_msg_size_t)size, 0, MACH_PORT_NULL, timeout, MACH_PORT_NULL);
    free(header);
    return result == MACH_MSG_SUCCESS;
}

@end

@implementation NSMachPort

+ (NSPort *)portWithMachPort:(uint32_t)machPort {
    return [[[self alloc] initWithMachPort:machPort] autorelease];
}

+ (NSPort *)portWithMachPort:(uint32_t)machPort options:(NSUInteger)options {
    return [self portWithMachPort:machPort];
}

- (instancetype)initWithMachPort:(uint32_t)machPort {
    self = [super init];
    if (self != nil) {
        /* -[NSPort init] allocated one; adopt the caller's instead. */
        if (_ownsPort && _machPort != MACH_PORT_NULL) {
            mach_port_mod_refs(mach_task_self(), _machPort, MACH_PORT_RIGHT_RECEIVE, -1);
            mach_port_deallocate(mach_task_self(), _machPort);
        }
        _machPort = (mach_port_t)machPort;
        _ownsPort = NO;
        _valid = (machPort != MACH_PORT_NULL);
    }
    return self;
}

- (uint32_t)machPort {
    return (uint32_t)_machPort;
}

@end
