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
    NSData *_sourceApplicationAuditTokenData;
    NSURLRequestNetworkServiceType _networkServiceType;
    BOOL _discretionary;
    BOOL _requiresPowerPluggedIn;
    int _TLSMinimumSupportedProtocol;
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
@property NSURLRequestNetworkServiceType networkServiceType;
@property (getter=isDiscretionary) BOOL discretionary;
// SPI: load only on external power
@property BOOL _requiresPowerPluggedIn;
// an SSLProtocol, Security's enum, which is built after Foundation, held as its int
@property int TLSMinimumSupportedProtocol;
// SPI: the audit token of the application a daemon loads on behalf of
@property (copy) NSData *_sourceApplicationAuditTokenData;

@end

FOUNDATION_EXPORT const int64_t NSURLSessionTransferSizeUnknown;

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
@property (readonly) int64_t countOfBytesSent;
@property (readonly) int64_t countOfBytesExpectedToSend;
@property (readonly) int64_t countOfBytesExpectedToReceive;

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
// SPI: forget every background session this process made, and their files
+ (void)_obliterateAllBackgroundSessionsWithCompletionHandler:(void (^)(void))completionHandler;
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

#import <Foundation/NSURLAuthenticationChallenge.h>
// as on macOS, users of sessions see SecureTransport's protocol versions, Foundation's own build comes first
#if __has_include(<Security/SecureTransport.h>)
#include <Security/SecureTransport.h>
#endif

@class NSHTTPURLResponse;

typedef NS_ENUM(NSInteger, NSURLSessionAuthChallengeDisposition) {
    NSURLSessionAuthChallengeUseCredential = 0,
    NSURLSessionAuthChallengePerformDefaultHandling = 1,
    NSURLSessionAuthChallengeCancelAuthenticationChallenge = 2,
    NSURLSessionAuthChallengeRejectProtectionSpace = 3,
};

typedef NS_ENUM(NSInteger, NSURLSessionResponseDisposition) {
    NSURLSessionResponseCancel = 0,
    NSURLSessionResponseAllow = 1,
    NSURLSessionResponseBecomeDownload = 2,
    NSURLSessionResponseBecomeStream = 3,
};

@protocol NSURLSessionDelegate <NSObject>
@optional
- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(NSError *)error;
- (void)URLSession:(NSURLSession *)session didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
    completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler;
@end

@protocol NSURLSessionTaskDelegate <NSURLSessionDelegate>
@optional
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
    willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request
    completionHandler:(void (^)(NSURLRequest *request))completionHandler;
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
    completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler;
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error;
@end

@protocol NSURLSessionDataDelegate <NSURLSessionTaskDelegate>
@optional
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response
    completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler;
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data;
@end

@class NSDateInterval;

// what a task's loading took, reported to delegates that ask for metrics
@interface NSURLSessionTaskMetrics : NSObject {
    NSArray *_transactionMetrics;
    NSDateInterval *_taskInterval;
    NSUInteger _redirectCount;
}
@property (readonly, copy) NSArray *transactionMetrics;
@property (readonly, copy) NSDateInterval *taskInterval;
@property (readonly) NSUInteger redirectCount;
@end

@protocol NSURLSessionTaskMetricsDelegate
@optional
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didFinishCollectingMetrics:(NSURLSessionTaskMetrics *)metrics;
@end
