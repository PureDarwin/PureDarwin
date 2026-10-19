// NSURLSession.h - URL loading sessions: file URLs load, other schemes fail with an NSURLErrorDomain error
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSObject.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSError.h>

@class NSURL, NSData, NSError, NSString, NSDictionary, NSOperationQueue, NSURLResponse;

FOUNDATION_EXPORT NSErrorDomain const NSURLErrorDomain;
FOUNDATION_EXPORT NSString *const NSURLErrorFailingURLErrorKey;
FOUNDATION_EXPORT NSString *const NSURLErrorFailingURLStringErrorKey;

enum {
    NSURLErrorUnknown = -1,
    NSURLErrorCancelled = -999,
    NSURLErrorBadURL = -1000,
    NSURLErrorTimedOut = -1001,
    NSURLErrorUnsupportedURL = -1002,
    NSURLErrorCannotFindHost = -1003,
    NSURLErrorCannotConnectToHost = -1004,
    NSURLErrorNotConnectedToInternet = -1009,
    NSURLErrorFileDoesNotExist = -1100,
    NSURLErrorFileIsDirectory = -1101,
    NSURLErrorNoPermissionsToReadFile = -1102,
};

typedef NS_ENUM(NSInteger, NSURLSessionTaskState) {
    NSURLSessionTaskStateRunning = 0,
    NSURLSessionTaskStateSuspended = 1,
    NSURLSessionTaskStateCanceling = 2,
    NSURLSessionTaskStateCompleted = 3,
};

@interface NSURLSessionConfiguration : NSObject <NSCopying> {
    NSString *_identifier;
    NSTimeInterval _timeoutIntervalForRequest;
    NSTimeInterval _timeoutIntervalForResource;
    NSURLRequestCachePolicy _requestCachePolicy;
    NSDictionary *_HTTPAdditionalHeaders;
    BOOL _allowsCellularAccess;
}

@property (class, readonly, copy) NSURLSessionConfiguration *defaultSessionConfiguration;
@property (class, readonly, copy) NSURLSessionConfiguration *ephemeralSessionConfiguration;
+ (NSURLSessionConfiguration *)backgroundSessionConfigurationWithIdentifier:(NSString *)identifier;

@property (readonly, copy) NSString *identifier;
@property NSTimeInterval timeoutIntervalForRequest;
@property NSTimeInterval timeoutIntervalForResource;
@property NSURLRequestCachePolicy requestCachePolicy;
@property (copy) NSDictionary *HTTPAdditionalHeaders;
@property BOOL allowsCellularAccess;

@end

@interface NSURLSessionTask : NSObject <NSCopying> {
@package
    NSUInteger _taskIdentifier;
    NSURLRequest *_originalRequest;
    NSURLResponse *_response;
    NSError *_error;
    NSURLSessionTaskState _state;
    NSString *_taskDescription;
    id _session;
    id _completion;
    int64_t _countOfBytesReceived;
}

@property (readonly) NSUInteger taskIdentifier;
@property (readonly, copy) NSURLRequest *originalRequest;
@property (readonly, copy) NSURLRequest *currentRequest;
@property (readonly, copy) NSURLResponse *response;
@property (readonly, copy) NSError *error;
@property (readonly) NSURLSessionTaskState state;
@property (copy) NSString *taskDescription;
@property (readonly) int64_t countOfBytesReceived;

- (void)resume;
- (void)suspend;
- (void)cancel;

@end

@interface NSURLSessionDataTask : NSURLSessionTask
@end

@interface NSURLSessionUploadTask : NSURLSessionDataTask
@end

@interface NSURLSessionDownloadTask : NSURLSessionTask
@end

@interface NSURLSession : NSObject {
    NSURLSessionConfiguration *_configuration;
    id _delegate;
    NSOperationQueue *_delegateQueue;
    NSString *_sessionDescription;
    BOOL _invalidated;
}

@property (class, readonly, retain) NSURLSession *sharedSession;
+ (NSURLSession *)sessionWithConfiguration:(NSURLSessionConfiguration *)configuration;
+ (NSURLSession *)sessionWithConfiguration:(NSURLSessionConfiguration *)configuration delegate:(id)delegate
                             delegateQueue:(NSOperationQueue *)queue;

@property (readonly, retain) NSOperationQueue *delegateQueue;
@property (readonly, retain) id delegate;
@property (readonly, copy) NSURLSessionConfiguration *configuration;
@property (copy) NSString *sessionDescription;

- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request;
- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url;
- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request
                            completionHandler:(void (^)(NSData *data, NSURLResponse *response, NSError *error))completionHandler;
- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url
                        completionHandler:(void (^)(NSData *data, NSURLResponse *response, NSError *error))completionHandler;
- (NSURLSessionUploadTask *)uploadTaskWithRequest:(NSURLRequest *)request fromData:(NSData *)bodyData
                                completionHandler:(void (^)(NSData *data, NSURLResponse *response, NSError *error))completionHandler;

- (void)finishTasksAndInvalidate;
- (void)invalidateAndCancel;

@end
