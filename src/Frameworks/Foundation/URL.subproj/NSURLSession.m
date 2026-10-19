// NSURLSession.m - URL loading sessions, file URLs load from disk
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// other schemes fail with NSURLErrorUnsupportedURL, there is no network URL loading system here
#import <Foundation/NSURLSession.h>
#import <Foundation/NSURLFileTypeMappings.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSData.h>
#import <Foundation/NSError.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSOperation.h>
#import <Foundation/NSFileManager.h>
#include <dispatch/dispatch.h>
#include <stdatomic.h>

NSErrorDomain const NSURLErrorDomain = @"NSURLErrorDomain";
NSString *const NSURLErrorFailingURLErrorKey = @"NSErrorFailingURLKey";
NSString *const NSURLErrorFailingURLStringErrorKey = @"NSErrorFailingURLStringKey";

@interface NSObject (NSURLSessionDelegates)
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task didReceiveData:(NSData *)data;
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error;
- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(NSError *)error;
@end

@implementation NSURLSessionConfiguration

+ (NSURLSessionConfiguration *)defaultSessionConfiguration {
    NSURLSessionConfiguration *configuration = [[[self alloc] init] autorelease];

    configuration->_timeoutIntervalForRequest = 60;
    configuration->_timeoutIntervalForResource = 7 * 24 * 60 * 60;
    configuration->_allowsCellularAccess = YES;
    return configuration;
}

+ (NSURLSessionConfiguration *)ephemeralSessionConfiguration {
    return [self defaultSessionConfiguration];
}

+ (NSURLSessionConfiguration *)backgroundSessionConfigurationWithIdentifier:(NSString *)identifier {
    NSURLSessionConfiguration *configuration = [self defaultSessionConfiguration];

    configuration->_identifier = [identifier copy];
    return configuration;
}

- (void)dealloc {
    [_identifier release];
    [_HTTPAdditionalHeaders release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    NSURLSessionConfiguration *copy = [[NSURLSessionConfiguration alloc] init];

    copy->_identifier = [_identifier copy];
    copy->_timeoutIntervalForRequest = _timeoutIntervalForRequest;
    copy->_timeoutIntervalForResource = _timeoutIntervalForResource;
    copy->_requestCachePolicy = _requestCachePolicy;
    copy->_HTTPAdditionalHeaders = [_HTTPAdditionalHeaders copy];
    copy->_allowsCellularAccess = _allowsCellularAccess;
    return copy;
}

- (NSString *)identifier { return _identifier; }
- (NSTimeInterval)timeoutIntervalForRequest { return _timeoutIntervalForRequest; }
- (void)setTimeoutIntervalForRequest:(NSTimeInterval)value { _timeoutIntervalForRequest = value; }
- (NSTimeInterval)timeoutIntervalForResource { return _timeoutIntervalForResource; }
- (void)setTimeoutIntervalForResource:(NSTimeInterval)value { _timeoutIntervalForResource = value; }
- (NSURLRequestCachePolicy)requestCachePolicy { return _requestCachePolicy; }
- (void)setRequestCachePolicy:(NSURLRequestCachePolicy)policy { _requestCachePolicy = policy; }
- (NSDictionary *)HTTPAdditionalHeaders { return _HTTPAdditionalHeaders; }

- (void)setHTTPAdditionalHeaders:(NSDictionary *)headers {
    NSDictionary *old = _HTTPAdditionalHeaders;
    _HTTPAdditionalHeaders = [headers copy];
    [old release];
}

- (BOOL)allowsCellularAccess { return _allowsCellularAccess; }
- (void)setAllowsCellularAccess:(BOOL)flag { _allowsCellularAccess = flag; }

@end

@interface NSURLSession (Tasks)
- (void)_runTask:(NSURLSessionTask *)task;
@end

@implementation NSURLSessionTask

- (instancetype)_initWithSession:(NSURLSession *)session request:(NSURLRequest *)request completion:(id)completion {
    static atomic_uint nextIdentifier;

    if ((self = [super init]) != nil) {
        _taskIdentifier = atomic_fetch_add(&nextIdentifier, 1) + 1;
        _session = [session retain];
        _originalRequest = [request copy];
        _completion = [completion copy];
        _state = NSURLSessionTaskStateSuspended;
    }
    return self;
}

- (void)dealloc {
    [_session release];
    [_originalRequest release];
    [_response release];
    [_error release];
    [_taskDescription release];
    [_completion release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (NSUInteger)taskIdentifier { return _taskIdentifier; }
- (NSURLRequest *)originalRequest { return _originalRequest; }
- (NSURLRequest *)currentRequest { return _originalRequest; }
- (NSURLResponse *)response { return _response; }
- (NSError *)error { return _error; }
- (int64_t)countOfBytesReceived { return _countOfBytesReceived; }

- (NSURLSessionTaskState)state {
    @synchronized(self) {
        return _state;
    }
}

- (NSString *)taskDescription { return _taskDescription; }

- (void)setTaskDescription:(NSString *)description {
    NSString *old = _taskDescription;
    _taskDescription = [description copy];
    [old release];
}

- (void)resume {
    @synchronized(self) {
        if (_state != NSURLSessionTaskStateSuspended)
            return;
        _state = NSURLSessionTaskStateRunning;
    }
    [_session _runTask:self];
}

- (void)suspend {
}

- (void)cancel {
    @synchronized(self) {
        if (_state == NSURLSessionTaskStateCompleted)
            return;
        _state = NSURLSessionTaskStateCanceling;
    }
    if (_state == NSURLSessionTaskStateCanceling)
        [_session _runTask:self];
}

@end

@implementation NSURLSessionDataTask
@end

@implementation NSURLSessionUploadTask
@end

@implementation NSURLSessionDownloadTask
@end

static NSError *urlError(NSInteger code, NSURL *url, NSString *description) {
    NSMutableDictionary *info = [NSMutableDictionary dictionaryWithObject:description forKey:NSLocalizedDescriptionKey];

    if (url != nil) {
        [info setObject:url forKey:NSURLErrorFailingURLErrorKey];
        [info setObject:[url absoluteString] forKey:NSURLErrorFailingURLStringErrorKey];
    }
    return [NSError errorWithDomain:NSURLErrorDomain code:code userInfo:info];
}

@implementation NSURLSession

+ (NSURLSession *)sharedSession {
    static NSURLSession *shared;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        shared = [[NSURLSession alloc] _initWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration]
                                                     delegate:nil delegateQueue:nil];
    });
    return shared;
}

+ (NSURLSession *)sessionWithConfiguration:(NSURLSessionConfiguration *)configuration {
    return [self sessionWithConfiguration:configuration delegate:nil delegateQueue:nil];
}

+ (NSURLSession *)sessionWithConfiguration:(NSURLSessionConfiguration *)configuration delegate:(id)delegate
                             delegateQueue:(NSOperationQueue *)queue {
    return [[[self alloc] _initWithConfiguration:configuration delegate:delegate delegateQueue:queue] autorelease];
}

- (instancetype)_initWithConfiguration:(NSURLSessionConfiguration *)configuration delegate:(id)delegate
                         delegateQueue:(NSOperationQueue *)queue {
    if ((self = [super init]) != nil) {
        _configuration = [configuration copy];
        // the session keeps its delegate until it is invalidated, as on macOS
        _delegate = [delegate retain];
        if (queue == nil) {
            queue = [[[NSOperationQueue alloc] init] autorelease];
            [queue setMaxConcurrentOperationCount:1];
        }
        _delegateQueue = [queue retain];
    }
    return self;
}

- (void)dealloc {
    [_configuration release];
    [_delegate release];
    [_delegateQueue release];
    [_sessionDescription release];
    [super dealloc];
}

- (NSOperationQueue *)delegateQueue { return _delegateQueue; }
- (id)delegate { return _delegate; }
- (NSURLSessionConfiguration *)configuration { return [[_configuration copy] autorelease]; }
- (NSString *)sessionDescription { return _sessionDescription; }

- (void)setSessionDescription:(NSString *)description {
    NSString *old = _sessionDescription;
    _sessionDescription = [description copy];
    [old release];
}

- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request {
    return [self dataTaskWithRequest:request completionHandler:nil];
}

- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url {
    return [self dataTaskWithURL:url completionHandler:nil];
}

- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request
                            completionHandler:(void (^)(NSData *, NSURLResponse *, NSError *))completionHandler {
    return [[[NSURLSessionDataTask alloc] _initWithSession:self request:request completion:completionHandler] autorelease];
}

- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url
                        completionHandler:(void (^)(NSData *, NSURLResponse *, NSError *))completionHandler {
    return [self dataTaskWithRequest:[NSURLRequest requestWithURL:url] completionHandler:completionHandler];
}

- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromData:(NSData *)bodyData
                                completionHandler:(void (^)(NSData *, NSURLResponse *, NSError *))completionHandler {
    NSMutableURLRequest *upload = [[request mutableCopy] autorelease];

    [upload setHTTPBody:bodyData];
    return [[[NSURLSessionUploadTask alloc] _initWithSession:self request:upload completion:completionHandler] autorelease];
}

// file URLs are read from disk, anything else has nowhere to go
- (void)_load:(NSURLSessionTask *)task data:(NSData **)data response:(NSURLResponse **)response error:(NSError **)error {
    NSURL *url = [[task originalRequest] URL];
    BOOL directory = NO;

    *data = nil;
    *response = nil;
    *error = nil;
    if (url == nil) {
        *error = urlError(NSURLErrorBadURL, nil, @"The URL is not valid.");
        return;
    }
    if (![url isFileURL]) {
        *error = urlError(NSURLErrorUnsupportedURL, url, @"URL loading over the network is not available on this system.");
        return;
    }
    if (![[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory:&directory]) {
        *error = urlError(NSURLErrorFileDoesNotExist, url, @"The requested file does not exist.");
        return;
    }
    if (directory) {
        *error = urlError(NSURLErrorFileIsDirectory, url, @"The requested URL is a folder.");
        return;
    }
    *data = [NSData dataWithContentsOfFile:[url path]];
    if (*data == nil) {
        *error = urlError(NSURLErrorNoPermissionsToReadFile, url, @"The file could not be read.");
        return;
    }
    NSString *type = [[NSURLFileTypeMappings sharedMappings] MIMETypeForExtension:[url pathExtension]];
    *response = [[[NSURLResponse alloc] initWithURL:url MIMEType:type ? type : @"application/octet-stream"
                              expectedContentLength:[*data length] textEncodingName:nil] autorelease];
}

- (void)_runTask:(NSURLSessionTask *)task {
    [task retain];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSData *data;
        NSURLResponse *response;
        NSError *error;

        if ([task state] == NSURLSessionTaskStateCanceling) {
            data = nil;
            response = nil;
            error = urlError(NSURLErrorCancelled, [[task originalRequest] URL], @"cancelled");
        } else {
            [self _load:task data:&data response:&response error:&error];
        }
        @synchronized(task) {
            if (task->_state == NSURLSessionTaskStateCompleted) {
                [task release];
                return;
            }
            task->_state = NSURLSessionTaskStateCompleted;
        }
        task->_response = [response retain];
        task->_error = [error retain];
        task->_countOfBytesReceived = [data length];
        [data retain];
        [_delegateQueue addOperationWithBlock:^{
            void (^completion)(NSData *, NSURLResponse *, NSError *) = task->_completion;

            if (completion != nil) {
                completion(data, response, error);
            } else {
                if (data != nil && [_delegate respondsToSelector:@selector(URLSession:dataTask:didReceiveData:)])
                    [_delegate URLSession:self dataTask:(NSURLSessionDataTask *)task didReceiveData:data];
                if ([_delegate respondsToSelector:@selector(URLSession:task:didCompleteWithError:)])
                    [_delegate URLSession:self task:task didCompleteWithError:error];
            }
            [data release];
            [task release];
        }];
    });
}

- (void)finishTasksAndInvalidate {
    [self invalidateAndCancel];
}

- (void)invalidateAndCancel {
    id delegate;

    if (self == [NSURLSession sharedSession] || _invalidated)
        return;
    _invalidated = YES;
    delegate = _delegate;
    _delegate = nil;
    [_delegateQueue addOperationWithBlock:^{
        if ([delegate respondsToSelector:@selector(URLSession:didBecomeInvalidWithError:)])
            [delegate URLSession:self didBecomeInvalidWithError:nil];
        [delegate release];
    }];
}

@end
