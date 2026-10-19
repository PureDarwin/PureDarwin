// NSPortNameServer.h - name servers that NSConnection looks names up in
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSObject.h>

@class NSString, NSPort;

@interface NSPortNameServer : NSObject

+ (NSPortNameServer *)systemDefaultPortNameServer;

- (NSPort *)portForName:(NSString *)name;
- (NSPort *)portForName:(NSString *)name host:(NSString *)host;
- (BOOL)registerPort:(NSPort *)port name:(NSString *)name;
- (BOOL)removePortForName:(NSString *)name;

@end

@interface NSMachBootstrapServer : NSPortNameServer
+ (id)sharedInstance;
@end

@interface NSMessagePortNameServer : NSPortNameServer
+ (id)sharedInstance;
@end

// on macOS these names travel over Bonjour to other machines. Here no network name service
// runs, so only names on this machine resolve, through NSConnection's own registry
@interface NSSocketPortNameServer : NSPortNameServer {
    uint16_t _defaultNameServerPortNumber;
}
+ (id)sharedInstance;
- (BOOL)registerPort:(NSPort *)port name:(NSString *)name nameServerPortNumber:(uint16_t)portNumber;
- (NSPort *)portForName:(NSString *)name host:(NSString *)host nameServerPortNumber:(uint16_t)portNumber;
@property uint16_t defaultNameServerPortNumber;
@end
