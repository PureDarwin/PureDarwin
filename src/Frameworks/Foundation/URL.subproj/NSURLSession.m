// NSURLSession.m - URL loading sessions, file URLs load from disk
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// other schemes fail with NSURLErrorUnsupportedURL, there is no network URL loading system here
#import <Foundation/NSURLSession.h>
#import <Foundation/NSCharacterSet.h>
#import <dispatch/dispatch.h>
#include <dlfcn.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSURLFileTypeMappings.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSData.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSError.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSOperation.h>
#import <Foundation/NSFileManager.h>
#include <dispatch/dispatch.h>
#include <stdatomic.h>

const int64_t NSURLSessionTransferSizeUnknown = -1LL;

NSErrorDomain const NSURLErrorDomain = @"NSURLErrorDomain";
NSString *const NSURLErrorFailingURLErrorKey = @"NSErrorFailingURLKey";
NSString *const NSURLErrorFailingURLStringErrorKey = @"NSErrorFailingURLStringKey";

@interface NSObject (NSURLSessionDelegates)
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task didReceiveData:(NSData *)data;
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error;
- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(NSError *)error;
- (void)URLSession:(NSURLSession *)session downloadTask:(NSURLSessionDownloadTask *)task didFinishDownloadingToURL:(NSURL *)location;
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
    [_sourceApplicationAuditTokenData release];
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
    copy->_networkServiceType = _networkServiceType;
    copy->_discretionary = _discretionary;
    copy->_requiresPowerPluggedIn = _requiresPowerPluggedIn;
    copy->_TLSMinimumSupportedProtocol = _TLSMinimumSupportedProtocol;
    copy->_sourceApplicationAuditTokenData = [_sourceApplicationAuditTokenData copy];
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
- (NSURLRequestNetworkServiceType)networkServiceType { return _networkServiceType; }
- (void)setNetworkServiceType:(NSURLRequestNetworkServiceType)type { _networkServiceType = type; }
- (BOOL)isDiscretionary { return _discretionary; }
- (void)setDiscretionary:(BOOL)flag { _discretionary = flag; }
- (BOOL)_requiresPowerPluggedIn { return _requiresPowerPluggedIn; }
- (void)set_requiresPowerPluggedIn:(BOOL)flag { _requiresPowerPluggedIn = flag; }
- (int)TLSMinimumSupportedProtocol { return _TLSMinimumSupportedProtocol; }
- (void)setTLSMinimumSupportedProtocol:(int)protocol { _TLSMinimumSupportedProtocol = protocol; }
- (NSData *)_sourceApplicationAuditTokenData { return _sourceApplicationAuditTokenData; }

- (void)set_sourceApplicationAuditTokenData:(NSData *)data {
    NSData *old = _sourceApplicationAuditTokenData;
    _sourceApplicationAuditTokenData = [data copy];
    [old release];
}

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

// loads read whole files and send nothing, so sent counts are the request body and nothing goes out early
- (int64_t)countOfBytesSent { return 0; }
- (int64_t)countOfBytesExpectedToSend { return (int64_t)[[_originalRequest HTTPBody] length]; }

- (int64_t)countOfBytesExpectedToReceive {
    long long length = _response ? [_response expectedContentLength] : -1;

    return length >= 0 ? length : NSURLSessionTransferSizeUnknown;
}

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

// HTTP and HTTPS go through libcurl, looked up at runtime so Foundation does not link it. The option and info
// numbers are curl's stable ABI values from curl.h
enum {
    PD_CURLOPT_WRITEDATA = 10001, PD_CURLOPT_URL = 10002, PD_CURLOPT_POSTFIELDS = 10015, PD_CURLOPT_USERAGENT = 10018,
    PD_CURLOPT_HTTPHEADER = 10023, PD_CURLOPT_HEADERDATA = 10029, PD_CURLOPT_CUSTOMREQUEST = 10036,
    PD_CURLOPT_CAINFO = 10065, PD_CURLOPT_ACCEPT_ENCODING = 10102, PD_CURLOPT_WRITEFUNCTION = 20011,
    PD_CURLOPT_HEADERFUNCTION = 20079, PD_CURLOPT_POSTFIELDSIZE_LARGE = 30120, PD_CURLOPT_NOBODY = 44,
    PD_CURLOPT_FOLLOWLOCATION = 52, PD_CURLOPT_TIMEOUT_MS = 155, PD_CURLOPT_NOSIGNAL = 99,
    PD_CURLINFO_EFFECTIVE_URL = 0x100001, PD_CURLINFO_RESPONSE_CODE = 0x200002,
};

static struct {
    void *(*init)(void);
    int (*setopt)(void *, int, ...);
    int (*perform)(void *);
    int (*getinfo)(void *, int, ...);
    void (*cleanup)(void *);
    const char *(*strerror)(int);
    void *(*slist_append)(void *, const char *);
    void (*slist_free_all)(void *);
} pdCurl;

static BOOL pdCurlLoad(void) {
    static dispatch_once_t once;
    static BOOL loaded;

    dispatch_once(&once, ^{
        void *lib = dlopen("/usr/lib/libcurl.4.dylib", RTLD_NOW | RTLD_LOCAL);

        if (lib == NULL)
            return;
        pdCurl.init = dlsym(lib, "curl_easy_init");
        pdCurl.setopt = dlsym(lib, "curl_easy_setopt");
        pdCurl.perform = dlsym(lib, "curl_easy_perform");
        pdCurl.getinfo = dlsym(lib, "curl_easy_getinfo");
        pdCurl.cleanup = dlsym(lib, "curl_easy_cleanup");
        pdCurl.strerror = dlsym(lib, "curl_easy_strerror");
        pdCurl.slist_append = dlsym(lib, "curl_slist_append");
        pdCurl.slist_free_all = dlsym(lib, "curl_slist_free_all");
        loaded = pdCurl.init && pdCurl.setopt && pdCurl.perform && pdCurl.getinfo && pdCurl.cleanup &&
                 pdCurl.strerror && pdCurl.slist_append && pdCurl.slist_free_all;
    });
    return loaded;
}

static size_t pdCurlBody(char *bytes, size_t size, size_t count, void *context) {
    [(NSMutableData *)context appendBytes:bytes length:size * count];
    return size * count;
}

// one header line per call. A redirect or a 100 Continue starts a new status line, which clears what came before
static size_t pdCurlHeader(char *bytes, size_t size, size_t count, void *context) {
    NSMutableDictionary *headers = context;
    NSString *line = [[[NSString alloc] initWithBytes:bytes length:size * count encoding:NSISOLatin1StringEncoding] autorelease];
    NSRange colon;

    line = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if ([line hasPrefix:@"HTTP/"]) {
        NSArray *parts = [line componentsSeparatedByString:@" "];

        [headers removeAllObjects];
        [headers setObject:[[parts objectAtIndex:0] substringFromIndex:5] forKey:@"\x01version"];
    } else if ((colon = [line rangeOfString:@":"]).location != NSNotFound) {
        NSString *name = [line substringToIndex:colon.location];
        NSString *value = [[line substringFromIndex:colon.location + 1] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        NSString *previous = [headers objectForKey:name];

        [headers setObject:previous ? [NSString stringWithFormat:@"%@, %@", previous, value] : value forKey:name];
    }
    return size * count;
}

// curl's failures as the NSURLError codes macOS reports for them
static NSInteger pdCurlErrorCode(int code) {
    switch (code) {
    case 1: case 3: return NSURLErrorUnsupportedURL;
    case 5: case 6: return NSURLErrorCannotFindHost;
    case 7: return NSURLErrorCannotConnectToHost;
    case 28: return NSURLErrorTimedOut;
    case 47: return NSURLErrorHTTPTooManyRedirects;
    case 52: return NSURLErrorZeroByteResource;
    case 55: case 56: return NSURLErrorNetworkConnectionLost;
    case 60: case 51: case 77: return NSURLErrorServerCertificateUntrusted;
    case 35: case 58: case 59: case 83: return NSURLErrorSecureConnectionFailed;
    }
    return NSURLErrorUnknown;
}

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

// sessions here keep no background state between runs, so there is never anything to clear
+ (void)_obliterateAllBackgroundSessionsWithCompletionHandler:(void (^)(void))completionHandler {
    if (completionHandler)
        completionHandler();
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

- (NSURLSessionDownloadTask *)downloadTaskWithRequest:(NSURLRequest *)request {
    return [self downloadTaskWithRequest:request completionHandler:nil];
}

- (NSURLSessionDownloadTask *)downloadTaskWithURL:(NSURL *)url {
    return [self downloadTaskWithURL:url completionHandler:nil];
}

- (NSURLSessionDownloadTask *)downloadTaskWithRequest:(NSURLRequest *)request
                                    completionHandler:(void (^)(NSURL *, NSURLResponse *, NSError *))completionHandler {
    return [[[NSURLSessionDownloadTask alloc] _initWithSession:self request:request completion:completionHandler] autorelease];
}

- (NSURLSessionDownloadTask *)downloadTaskWithURL:(NSURL *)url
                                completionHandler:(void (^)(NSURL *, NSURLResponse *, NSError *))completionHandler {
    return [self downloadTaskWithRequest:[NSURLRequest requestWithURL:url] completionHandler:completionHandler];
}

// a finished download's bytes, in a temporary file the caller moves before it returns
static NSURL *downloadFile(NSData *data) {
    NSString *path = [NSTemporaryDirectory() stringByAppendingPathComponent:
        [NSString stringWithFormat:@"CFNetworkDownload_%@.tmp", [[NSProcessInfo processInfo] globallyUniqueString]]];

    return [data writeToFile:path atomically:NO] ? [NSURL fileURLWithPath:path] : nil;
}

- (void)_loadHTTP:(NSURLSessionTask *)task url:(NSURL *)url data:(NSData **)data response:(NSURLResponse **)response error:(NSError **)error {
    NSURLRequest *request = [task originalRequest];
    NSMutableDictionary *fields = [NSMutableDictionary dictionary];
    NSMutableDictionary *received = [NSMutableDictionary dictionary];
    NSMutableData *body = [NSMutableData data];
    NSString *method = [request HTTPMethod] ? [request HTTPMethod] : @"GET";
    NSTimeInterval timeout = [request timeoutInterval] > 0 ? [request timeoutInterval] : [_configuration timeoutIntervalForRequest];
    void *headers = NULL, *curl;
    long status = 0;
    char *effective = NULL;
    int result;

    if (!pdCurlLoad() || (curl = pdCurl.init()) == NULL) {
        *error = urlError(NSURLErrorUnsupportedURL, url, @"URL loading over the network is not available on this system.");
        return;
    }
    // the session's extra headers first, the request's own win
    [fields addEntriesFromDictionary:[_configuration HTTPAdditionalHeaders]];
    [fields addEntriesFromDictionary:[request allHTTPHeaderFields]];
    for (NSString *name in fields)
        headers = pdCurl.slist_append(headers, [[NSString stringWithFormat:@"%@: %@", name, [fields objectForKey:name]] UTF8String]);

    pdCurl.setopt(curl, PD_CURLOPT_URL, [[url absoluteString] UTF8String]);
    pdCurl.setopt(curl, PD_CURLOPT_NOSIGNAL, 1L);
    pdCurl.setopt(curl, PD_CURLOPT_FOLLOWLOCATION, 1L);
    pdCurl.setopt(curl, PD_CURLOPT_ACCEPT_ENCODING, "");
    pdCurl.setopt(curl, PD_CURLOPT_CAINFO, "/etc/ssl/cert.pem");
    if ([fields objectForKey:@"User-Agent"] == nil)
        pdCurl.setopt(curl, PD_CURLOPT_USERAGENT, [[NSString stringWithFormat:@"%@ CFNetwork Darwin",
            [[[NSProcessInfo processInfo] processName] stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]]] UTF8String]);
    pdCurl.setopt(curl, PD_CURLOPT_HTTPHEADER, headers);
    pdCurl.setopt(curl, PD_CURLOPT_WRITEFUNCTION, pdCurlBody);
    pdCurl.setopt(curl, PD_CURLOPT_WRITEDATA, body);
    pdCurl.setopt(curl, PD_CURLOPT_HEADERFUNCTION, pdCurlHeader);
    pdCurl.setopt(curl, PD_CURLOPT_HEADERDATA, received);
    if (timeout > 0)
        pdCurl.setopt(curl, PD_CURLOPT_TIMEOUT_MS, (long)(timeout * 1000));
    if ([method isEqualToString:@"HEAD"])
        pdCurl.setopt(curl, PD_CURLOPT_NOBODY, 1L);
    else if (![method isEqualToString:@"GET"])
        pdCurl.setopt(curl, PD_CURLOPT_CUSTOMREQUEST, [method UTF8String]);
    if ([request HTTPBody] != nil) {
        pdCurl.setopt(curl, PD_CURLOPT_POSTFIELDS, [[request HTTPBody] bytes]);
        pdCurl.setopt(curl, PD_CURLOPT_POSTFIELDSIZE_LARGE, (long long)[[request HTTPBody] length]);
    }

    result = pdCurl.perform(curl);
    if (result != 0) {
        NSString *why = [NSString stringWithUTF8String:pdCurl.strerror(result)];

        *error = urlError(pdCurlErrorCode(result), url, why ? why : @"The network request failed.");
    } else {
        NSString *version = [[[received objectForKey:@"\x01version"] retain] autorelease];
        NSURL *final = url;

        pdCurl.getinfo(curl, PD_CURLINFO_RESPONSE_CODE, &status);
        if (pdCurl.getinfo(curl, PD_CURLINFO_EFFECTIVE_URL, &effective) == 0 && effective != NULL)
            final = [NSURL URLWithString:[NSString stringWithUTF8String:effective]] ?: url;
        [received removeObjectForKey:@"\x01version"];
        *data = body;
        *response = [[[NSHTTPURLResponse alloc] initWithURL:final statusCode:status
            HTTPVersion:version ? [@"HTTP/" stringByAppendingString:version] : @"HTTP/1.1" headerFields:received] autorelease];
    }
    pdCurl.slist_free_all(headers);
    pdCurl.cleanup(curl);
}

// file URLs are read from disk, http and https go to libcurl
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
    if ([[[url scheme] lowercaseString] isEqualToString:@"http"] || [[[url scheme] lowercaseString] isEqualToString:@"https"]) {
        [self _loadHTTP:task url:url data:data response:response error:error];
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

            // a download hands over a file, which goes once the handler or the delegate returns, as on macOS
            if ([task isKindOfClass:[NSURLSessionDownloadTask class]]) {
                void (^downloaded)(NSURL *, NSURLResponse *, NSError *) = task->_completion;
                NSURL *location = error == nil ? downloadFile(data) : nil;

                if (downloaded != nil)
                    downloaded(location, response, error);
                else {
                    if (location != nil && [_delegate respondsToSelector:@selector(URLSession:downloadTask:didFinishDownloadingToURL:)])
                        [_delegate URLSession:self downloadTask:(NSURLSessionDownloadTask *)task didFinishDownloadingToURL:location];
                    if ([_delegate respondsToSelector:@selector(URLSession:task:didCompleteWithError:)])
                        [_delegate URLSession:self task:task didCompleteWithError:error];
                }
                if (location != nil)
                    [[NSFileManager defaultManager] removeItemAtPath:[location path] error:NULL];
            } else if (completion != nil) {
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

@implementation NSURLSessionTaskMetrics

- (void)dealloc {
    [_transactionMetrics release];
    [_taskInterval release];
    [super dealloc];
}

- (NSArray *)transactionMetrics { return _transactionMetrics ?: [NSArray array]; }
- (NSDateInterval *)taskInterval { return _taskInterval; }
- (NSUInteger)redirectCount { return _redirectCount; }

@end
