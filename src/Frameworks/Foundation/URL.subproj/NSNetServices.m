/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * Bonjour over the DNSService API in libSystem, which talks to mdnsd. Each
 * operation owns a DNSServiceRef; its socket is serviced on a dispatch queue
 * because NSRunLoop is select()-driven and has no Mach or fd source that
 * DNSServiceRefSockFD could be attached to from here.
 */

#import <Foundation/NSNetServices.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>
#include <dns_sd.h>
#include <dispatch/dispatch.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

NSString * const NSNetServicesErrorCode = @"NSNetServicesErrorCode";
NSString * const NSNetServicesErrorDomain = @"NSNetServicesErrorDomain";

/* Pumps a DNSServiceRef's socket until the ref is deallocated. */
static dispatch_source_t _startServicing(DNSServiceRef ref, dispatch_queue_t queue) {
    int fd = DNSServiceRefSockFD(ref);

    if (fd < 0) {
        return NULL;
    }

    dispatch_source_t source =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)fd, 0, queue);

    if (source == NULL) {
        return NULL;
    }
    dispatch_source_set_event_handler(source, ^{
        DNSServiceProcessResult(ref);
    });
    dispatch_resume(source);
    return source;
}

static NSDictionary *_errorDictionary(DNSServiceErrorType error) {
    return [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:(int)error]
                                       forKey:NSNetServicesErrorCode];
}

@implementation NSNetService {
    dispatch_queue_t _queue;
}

- (instancetype)initWithDomain:(NSString *)domain type:(NSString *)type
                          name:(NSString *)name {
    return [self initWithDomain:domain type:type name:name port:-1];
}

- (instancetype)initWithDomain:(NSString *)domain type:(NSString *)type
                          name:(NSString *)name port:(int)port {
    self = [super init];
    if (self != nil) {
        _domain = [domain copy];
        _type = [type copy];
        _name = [name copy];
        _port = port;
        _addresses = [[NSMutableArray alloc] init];
        _queue = dispatch_queue_create("org.puredarwin.NSNetService", NULL);
    }
    return self;
}

- (void)dealloc {
    [self stop];
    [_domain release];
    [_type release];
    [_name release];
    [_hostName release];
    [_addresses release];
    [_txtRecord release];
    if (_queue != NULL) {
        dispatch_release(_queue);
    }
    [super dealloc];
}

- (NSString *)domain { return _domain; }
- (NSString *)type { return _type; }
- (NSString *)name { return _name; }
- (NSString *)hostName { return _hostName; }
- (NSArray *)addresses { return _addresses; }
- (NSInteger)port { return _port; }

- (void)setDelegate:(id<NSNetServiceDelegate>)delegate { _delegate = delegate; }
- (id<NSNetServiceDelegate>)delegate { return _delegate; }

- (NSData *)TXTRecordData { return _txtRecord; }

- (BOOL)setTXTRecordData:(NSData *)recordData {
    [recordData retain];
    [_txtRecord release];
    _txtRecord = recordData;
    return YES;
}

static void _registerReply(DNSServiceRef ref, DNSServiceFlags flags,
                           DNSServiceErrorType error, const char *name,
                           const char *type, const char *domain, void *context) {
    NSNetService *service = (NSNetService *)context;

    if (error == kDNSServiceErr_NoError) {
        if ([[service delegate] respondsToSelector:@selector(netServiceDidPublish:)]) {
            [[service delegate] netServiceDidPublish:service];
        }
    } else if ([[service delegate] respondsToSelector:@selector(netService:didNotPublish:)]) {
        [[service delegate] netService:service didNotPublish:_errorDictionary(error)];
    }
}

- (void)publish {
    [self publishWithOptions:0];
}

- (void)publishWithOptions:(NSNetServiceOptions)options {
    if (_sdRef != NULL) {
        return;
    }
    if ([_delegate respondsToSelector:@selector(netServiceWillPublish:)]) {
        [_delegate netServiceWillPublish:self];
    }

    DNSServiceRef ref = NULL;
    DNSServiceFlags flags = (options & NSNetServiceNoAutoRename) ? kDNSServiceFlagsNoAutoRename : 0;
    DNSServiceErrorType error = DNSServiceRegister(&ref, flags, 0,
        [_name UTF8String], [_type UTF8String],
        ([_domain length] > 0) ? [_domain UTF8String] : NULL, NULL,
        htons((uint16_t)_port),
        (uint16_t)[_txtRecord length],
        [_txtRecord length] > 0 ? [_txtRecord bytes] : NULL,
        _registerReply, self);

    if (error != kDNSServiceErr_NoError) {
        if ([_delegate respondsToSelector:@selector(netService:didNotPublish:)]) {
            [_delegate netService:self didNotPublish:_errorDictionary(error)];
        }
        return;
    }
    _sdRef = ref;
    _source = (id)_startServicing(ref, _queue);
}

static void _resolveReply(DNSServiceRef ref, DNSServiceFlags flags,
                          uint32_t interfaceIndex, DNSServiceErrorType error,
                          const char *fullname, const char *hosttarget,
                          uint16_t port, uint16_t txtLen,
                          const unsigned char *txtRecord, void *context) {
    NSNetService *service = (NSNetService *)context;

    if (error != kDNSServiceErr_NoError) {
        if ([[service delegate] respondsToSelector:@selector(netService:didNotResolve:)]) {
            [[service delegate] netService:service didNotResolve:_errorDictionary(error)];
        }
        return;
    }
    [service _resolvedHost:hosttarget port:ntohs(port)
                       txt:[NSData dataWithBytes:txtRecord length:txtLen]];
}

- (void)_resolvedHost:(const char *)host port:(uint16_t)port txt:(NSData *)txt {
    NSString *name = (host != NULL) ? [NSString stringWithUTF8String:host] : nil;

    [name retain];
    [_hostName release];
    _hostName = name;
    _port = port;
    [self setTXTRecordData:txt];

    if ([_delegate respondsToSelector:@selector(netServiceDidResolveAddress:)]) {
        [_delegate netServiceDidResolveAddress:self];
    }
}

- (void)resolve {
    [self resolveWithTimeout:0.0];
}

- (void)resolveWithTimeout:(NSTimeInterval)timeout {
    if (_sdRef != NULL) {
        return;
    }
    if ([_delegate respondsToSelector:@selector(netServiceWillResolve:)]) {
        [_delegate netServiceWillResolve:self];
    }

    DNSServiceRef ref = NULL;
    DNSServiceErrorType error = DNSServiceResolve(&ref, 0, 0,
        [_name UTF8String], [_type UTF8String],
        ([_domain length] > 0) ? [_domain UTF8String] : "local.",
        _resolveReply, self);

    if (error != kDNSServiceErr_NoError) {
        if ([_delegate respondsToSelector:@selector(netService:didNotResolve:)]) {
            [_delegate netService:self didNotResolve:_errorDictionary(error)];
        }
        return;
    }
    _sdRef = ref;
    _source = (id)_startServicing(ref, _queue);
}

- (void)stop {
    if (_source != NULL) {
        dispatch_source_cancel((dispatch_source_t)_source);
        dispatch_release((dispatch_source_t)_source);
        _source = NULL;
    }
    if (_sdRef != NULL) {
        DNSServiceRefDeallocate((DNSServiceRef)_sdRef);
        _sdRef = NULL;
    }
    if ([_delegate respondsToSelector:@selector(netServiceDidStop:)]) {
        [_delegate netServiceDidStop:self];
    }
}

/* The DNSServiceRef is pumped by dispatch, so run-loop scheduling is a no-op. */
- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode {
}

- (void)removeFromRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode {
}

+ (NSDictionary *)dictionaryFromTXTRecordData:(NSData *)txtData {
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    const unsigned char *bytes = [txtData bytes];
    NSUInteger length = [txtData length];
    NSUInteger offset = 0;

    /* Each entry is a length byte followed by "key=value". */
    while (offset < length) {
        NSUInteger entryLength = bytes[offset++];

        if (entryLength == 0 || offset + entryLength > length) {
            break;
        }

        const unsigned char *entry = bytes + offset;
        const unsigned char *equals = memchr(entry, '=', entryLength);

        if (equals != NULL) {
            NSString *key = [[[NSString alloc] initWithBytes:entry
                                                      length:(NSUInteger)(equals - entry)
                                                    encoding:NSUTF8StringEncoding] autorelease];
            NSData *value = [NSData dataWithBytes:equals + 1
                                           length:entryLength - (NSUInteger)(equals - entry) - 1];

            if (key != nil) {
                [result setObject:value forKey:key];
            }
        }
        offset += entryLength;
    }
    return result;
}

+ (NSData *)dataFromTXTRecordDictionary:(NSDictionary *)txtDictionary {
    NSMutableData *result = [NSMutableData data];

    for (NSString *key in [txtDictionary allKeys]) {
        id value = [txtDictionary objectForKey:key];
        NSData *valueData = [value isKindOfClass:[NSData class]]
            ? value : [[value description] dataUsingEncoding:NSUTF8StringEncoding];
        NSData *keyData = [key dataUsingEncoding:NSUTF8StringEncoding];
        NSUInteger total = [keyData length] + 1 + [valueData length];

        if (total > 255) {
            continue;
        }

        unsigned char prefix = (unsigned char)total;

        [result appendBytes:&prefix length:1];
        [result appendData:keyData];
        [result appendBytes:"=" length:1];
        [result appendData:valueData];
    }
    return result;
}

@end

@implementation NSNetServiceBrowser {
    dispatch_queue_t _queue;
}

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        _services = [[NSMutableArray alloc] init];
        _queue = dispatch_queue_create("org.puredarwin.NSNetServiceBrowser", NULL);
    }
    return self;
}

- (void)dealloc {
    [self stop];
    [_services release];
    if (_queue != NULL) {
        dispatch_release(_queue);
    }
    [super dealloc];
}

- (void)setDelegate:(id<NSNetServiceBrowserDelegate>)delegate { _delegate = delegate; }
- (id<NSNetServiceBrowserDelegate>)delegate { return _delegate; }

static void _browseReply(DNSServiceRef ref, DNSServiceFlags flags,
                         uint32_t interfaceIndex, DNSServiceErrorType error,
                         const char *name, const char *type, const char *domain,
                         void *context) {
    NSNetServiceBrowser *browser = (NSNetServiceBrowser *)context;

    if (error != kDNSServiceErr_NoError) {
        if ([[browser delegate] respondsToSelector:@selector(netServiceBrowser:didNotSearch:)]) {
            [[browser delegate] netServiceBrowser:browser
                                     didNotSearch:_errorDictionary(error)];
        }
        return;
    }
    [browser _service:name type:type domain:domain
                added:(flags & kDNSServiceFlagsAdd) ? YES : NO
              moreComing:(flags & kDNSServiceFlagsMoreComing) ? YES : NO];
}

- (void)_service:(const char *)name type:(const char *)type
          domain:(const char *)domain added:(BOOL)added moreComing:(BOOL)moreComing {
    NSNetService *service = [[[NSNetService alloc]
        initWithDomain:(domain != NULL) ? [NSString stringWithUTF8String:domain] : @""
                  type:(type != NULL) ? [NSString stringWithUTF8String:type] : @""
                  name:(name != NULL) ? [NSString stringWithUTF8String:name] : @""] autorelease];

    if (added) {
        [_services addObject:service];
        if ([_delegate respondsToSelector:@selector(netServiceBrowser:didFindService:moreComing:)]) {
            [_delegate netServiceBrowser:self didFindService:service moreComing:moreComing];
        }
    } else {
        if ([_delegate respondsToSelector:@selector(netServiceBrowser:didRemoveService:moreComing:)]) {
            [_delegate netServiceBrowser:self didRemoveService:service moreComing:moreComing];
        }
        [_services removeObject:service];
    }
}

- (void)searchForServicesOfType:(NSString *)type inDomain:(NSString *)domain {
    if (_sdRef != NULL) {
        return;
    }
    if ([_delegate respondsToSelector:@selector(netServiceBrowserWillSearch:)]) {
        [_delegate netServiceBrowserWillSearch:self];
    }

    DNSServiceRef ref = NULL;
    DNSServiceErrorType error = DNSServiceBrowse(&ref, 0, 0,
        [type UTF8String], ([domain length] > 0) ? [domain UTF8String] : "local.",
        _browseReply, self);

    if (error != kDNSServiceErr_NoError) {
        if ([_delegate respondsToSelector:@selector(netServiceBrowser:didNotSearch:)]) {
            [_delegate netServiceBrowser:self didNotSearch:_errorDictionary(error)];
        }
        return;
    }
    _sdRef = ref;
    _source = (id)_startServicing(ref, _queue);
}

/* Domain enumeration is not wired to DNSServiceEnumerateDomains yet; report
 * no domains rather than appearing to search. */
- (void)searchForBrowsableDomains {
    if ([_delegate respondsToSelector:@selector(netServiceBrowser:didNotSearch:)]) {
        [_delegate netServiceBrowser:self
                       didNotSearch:_errorDictionary(kDNSServiceErr_Unsupported)];
    }
}

- (void)searchForRegistrationDomains {
    [self searchForBrowsableDomains];
}

- (void)stop {
    if (_source != NULL) {
        dispatch_source_cancel((dispatch_source_t)_source);
        dispatch_release((dispatch_source_t)_source);
        _source = NULL;
    }
    if (_sdRef != NULL) {
        DNSServiceRefDeallocate((DNSServiceRef)_sdRef);
        _sdRef = NULL;
    }
    if ([_delegate respondsToSelector:@selector(netServiceBrowserDidStopSearch:)]) {
        [_delegate netServiceBrowserDidStopSearch:self];
    }
}

- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode {
}

- (void)removeFromRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode {
}

@end
