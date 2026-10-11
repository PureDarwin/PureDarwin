// NSOperation.h - operations and operation queues
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

#import <Foundation/NSObject.h>
#include <dispatch/dispatch.h>

@class NSArray, NSMutableArray, NSString, NSInvocation, NSException;

typedef NS_ENUM(NSInteger, NSOperationQueuePriority) {
    NSOperationQueuePriorityVeryLow = -8,
    NSOperationQueuePriorityLow = -4,
    NSOperationQueuePriorityNormal = 0,
    NSOperationQueuePriorityHigh = 4,
    NSOperationQueuePriorityVeryHigh = 8
};

#ifndef NSQualityOfServiceDefined
#define NSQualityOfServiceDefined
typedef NS_ENUM(NSInteger, NSQualityOfService) {
    NSQualityOfServiceUserInteractive = 0x21,
    NSQualityOfServiceUserInitiated = 0x19,
    NSQualityOfServiceUtility = 0x11,
    NSQualityOfServiceBackground = 0x09,
    NSQualityOfServiceDefault = -1
};
#endif

FOUNDATION_EXPORT const NSInteger NSOperationQueueDefaultMaxConcurrentOperationCount;
FOUNDATION_EXPORT NSString * const NSInvocationOperationVoidResultException;
FOUNDATION_EXPORT NSString * const NSInvocationOperationCancelledException;

@interface NSOperation : NSObject {
    NSMutableArray *_dependencies;
    void (^_completionBlock)(void);
    NSString *_name;
    NSOperationQueuePriority _queuePriority;
    NSQualityOfService _qualityOfService;
    double _threadPriority;
    dispatch_group_t _done;
    volatile int _state;
    volatile int _finishedOnce;
    BOOL _cancelled;
@public
    // the queue's bookkeeping
    id _queue;
    BOOL _claimed;
}

- (void)start;
- (void)main;

@property (readonly, getter=isCancelled) BOOL cancelled;
- (void)cancel;

@property (readonly, getter=isExecuting) BOOL executing;
@property (readonly, getter=isFinished) BOOL finished;
@property (readonly, getter=isConcurrent) BOOL concurrent;
@property (readonly, getter=isAsynchronous) BOOL asynchronous;
@property (readonly, getter=isReady) BOOL ready;

- (void)addDependency:(NSOperation *)operation;
- (void)removeDependency:(NSOperation *)operation;
@property (readonly, copy) NSArray *dependencies;

@property NSOperationQueuePriority queuePriority;
@property (copy) void (^completionBlock)(void);
- (void)waitUntilFinished;
@property double threadPriority;
@property NSQualityOfService qualityOfService;
@property (copy) NSString *name;

@end

@interface NSBlockOperation : NSOperation {
    NSMutableArray *_blocks;
}

+ (instancetype)blockOperationWithBlock:(void (^)(void))block;
- (void)addExecutionBlock:(void (^)(void))block;
@property (readonly, copy) NSArray *executionBlocks;

@end

@interface NSInvocationOperation : NSOperation {
    NSInvocation *_invocation;
    NSException *_exception;
}

- (instancetype)initWithTarget:(id)target selector:(SEL)selector object:(id)argument;
- (instancetype)initWithInvocation:(NSInvocation *)invocation;
@property (readonly, retain) NSInvocation *invocation;
@property (readonly, retain) id result;

@end

@interface NSOperationQueue : NSObject {
    NSMutableArray *_operations;
    NSInteger _maxConcurrent;
    NSInteger _running;
    BOOL _suspended;
    BOOL _isMain;
    NSString *_name;
    NSQualityOfService _qualityOfService;
    dispatch_queue_t _underlying;
    void *_lock;
}

@property (class, readonly, retain) NSOperationQueue *currentQueue;
@property (class, readonly, retain) NSOperationQueue *mainQueue;

- (void)addOperation:(NSOperation *)operation;
- (void)addOperations:(NSArray *)operations waitUntilFinished:(BOOL)wait;
- (void)addOperationWithBlock:(void (^)(void))block;
- (void)addBarrierBlock:(void (^)(void))barrier;

@property NSInteger maxConcurrentOperationCount;
@property (getter=isSuspended) BOOL suspended;
@property (copy) NSString *name;
@property NSQualityOfService qualityOfService;
@property (assign) dispatch_queue_t underlyingQueue;

@property (readonly, copy) NSArray *operations;
@property (readonly) NSUInteger operationCount;

- (void)cancelAllOperations;
- (void)waitUntilAllOperationsAreFinished;

@end
