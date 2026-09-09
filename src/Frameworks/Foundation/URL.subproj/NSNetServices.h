/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSNetServices_h
#define NSNetServices_h

#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>

@class NSString, NSArray, NSData, NSDictionary, NSMutableArray, NSRunLoop;
@class NSNetService, NSNetServiceBrowser;

FOUNDATION_EXPORT NSString * const NSNetServicesErrorCode;
FOUNDATION_EXPORT NSString * const NSNetServicesErrorDomain;

typedef NS_ENUM(NSInteger, NSNetServicesError) {
    NSNetServicesUnknownError = -72000,
    NSNetServicesCollisionError = -72001,
    NSNetServicesNotFoundError = -72002,
    NSNetServicesActivityInProgress = -72003,
    NSNetServicesBadArgumentError = -72004,
    NSNetServicesCancelledError = -72005,
    NSNetServicesInvalidError = -72006,
    NSNetServicesTimeoutError = -72007
};

typedef NS_OPTIONS(NSUInteger, NSNetServiceOptions) {
    NSNetServiceNoAutoRename = 1 << 0,
    NSNetServiceListenForConnections = 1 << 1
};

@protocol NSNetServiceDelegate <NSObject>
@optional
- (void)netServiceWillPublish:(NSNetService *)sender;
- (void)netServiceDidPublish:(NSNetService *)sender;
- (void)netService:(NSNetService *)sender didNotPublish:(NSDictionary *)errorDict;
- (void)netServiceWillResolve:(NSNetService *)sender;
- (void)netServiceDidResolveAddress:(NSNetService *)sender;
- (void)netService:(NSNetService *)sender didNotResolve:(NSDictionary *)errorDict;
- (void)netServiceDidStop:(NSNetService *)sender;
@end

@protocol NSNetServiceBrowserDelegate <NSObject>
@optional
- (void)netServiceBrowserWillSearch:(NSNetServiceBrowser *)browser;
- (void)netServiceBrowserDidStopSearch:(NSNetServiceBrowser *)browser;
- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
             didNotSearch:(NSDictionary *)errorDict;
- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
           didFindService:(NSNetService *)service
               moreComing:(BOOL)moreComing;
- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
         didRemoveService:(NSNetService *)service
               moreComing:(BOOL)moreComing;
@end

@interface NSNetService : NSObject {
    NSString *_domain;
    NSString *_type;
    NSString *_name;
    NSString *_hostName;
    NSMutableArray *_addresses;
    NSData *_txtRecord;
    NSInteger _port;
    id _delegate;
    void *_sdRef;
    id _source;
}

- (instancetype)initWithDomain:(NSString *)domain type:(NSString *)type
                          name:(NSString *)name;
- (instancetype)initWithDomain:(NSString *)domain type:(NSString *)type
                          name:(NSString *)name port:(int)port;

- (NSString *)domain;
- (NSString *)type;
- (NSString *)name;
- (NSString *)hostName;
- (NSArray *)addresses;
- (NSInteger)port;

- (void)setDelegate:(id<NSNetServiceDelegate>)delegate;
- (id<NSNetServiceDelegate>)delegate;

- (void)publish;
- (void)publishWithOptions:(NSNetServiceOptions)options;
- (void)resolve;
- (void)resolveWithTimeout:(NSTimeInterval)timeout;
- (void)stop;

- (NSData *)TXTRecordData;
- (BOOL)setTXTRecordData:(NSData *)recordData;

- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode;
- (void)removeFromRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode;

+ (NSDictionary *)dictionaryFromTXTRecordData:(NSData *)txtData;
+ (NSData *)dataFromTXTRecordDictionary:(NSDictionary *)txtDictionary;

@end

@interface NSNetServiceBrowser : NSObject {
    id _delegate;
    void *_sdRef;
    id _source;
    NSMutableArray *_services;
}

- (void)setDelegate:(id<NSNetServiceBrowserDelegate>)delegate;
- (id<NSNetServiceBrowserDelegate>)delegate;

- (void)searchForServicesOfType:(NSString *)type inDomain:(NSString *)domain;
- (void)searchForBrowsableDomains;
- (void)searchForRegistrationDomains;
- (void)stop;

- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode;
- (void)removeFromRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode;

@end

#endif /* NSNetServices_h */
