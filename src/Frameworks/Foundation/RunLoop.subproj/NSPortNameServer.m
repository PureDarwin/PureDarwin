// NSPortNameServer.m - name servers that NSConnection looks names up in
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// NSConnection rides on CFMessagePort names, so every lookup goes to that registry on this machine
#import <Foundation/NSPortNameServer.h>
#import <Foundation/NSPort.h>
#import <Foundation/NSString.h>
#include <dispatch/dispatch.h>

@implementation NSPortNameServer

+ (NSPortNameServer *)systemDefaultPortNameServer {
    return [NSMessagePortNameServer sharedInstance];
}

- (NSPort *)portForName:(NSString *)name {
    return [self portForName:name host:nil];
}

- (NSPort *)portForName:(NSString *)name host:(NSString *)host {
    return nil;
}

- (BOOL)registerPort:(NSPort *)port name:(NSString *)name {
    return NO;
}

- (BOOL)removePortForName:(NSString *)name {
    return NO;
}

@end

#define SHARED_INSTANCE(cls) \
    + (id)sharedInstance { \
        static cls *shared; \
        static dispatch_once_t once; \
        dispatch_once(&once, ^{ \
            shared = [[cls alloc] init]; \
        }); \
        return shared; \
    }

@implementation NSMachBootstrapServer
SHARED_INSTANCE(NSMachBootstrapServer)
@end

@implementation NSMessagePortNameServer
SHARED_INSTANCE(NSMessagePortNameServer)
@end

@implementation NSSocketPortNameServer
SHARED_INSTANCE(NSSocketPortNameServer)

- (uint16_t)defaultNameServerPortNumber { return _defaultNameServerPortNumber; }
- (void)setDefaultNameServerPortNumber:(uint16_t)portNumber { _defaultNameServerPortNumber = portNumber; }

- (BOOL)registerPort:(NSPort *)port name:(NSString *)name nameServerPortNumber:(uint16_t)portNumber {
    return [self registerPort:port name:name];
}

- (NSPort *)portForName:(NSString *)name host:(NSString *)host nameServerPortNumber:(uint16_t)portNumber {
    return [self portForName:name host:host];
}

@end
