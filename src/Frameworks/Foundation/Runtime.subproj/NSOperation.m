// NSOperation.m - operations and operation queues over libdispatch
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// operations and queues over libdispatch: a queue starts each ready operation on a dispatch queue,
// up to its concurrency limit, and is told when an operation finishes.

#import <Foundation/NSOperation.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSException.h>
#import <Foundation/NSInvocation.h>
#import <Foundation/NSKeyValueObserving.h>
#import <Foundation/NSString.h>
#include <pthread.h>
#include <stdlib.h>

const NSInteger NSOperationQueueDefaultMaxConcurrentOperationCount = -1;
NSString * const NSInvocationOperationVoidResultException = @"NSInvocationOperationVoidResultException";
NSString * const NSInvocationOperationCancelledException = @"NSInvocationOperationCancelledException";

enum { kPending, kExecuting, kFinished };

@interface NSOperationQueue (Private)
- (void)_schedule;
- (void)_operationFinished:(NSOperation *)operation;
@end

// every live queue, so a finished operation can wake queues holding operations that depend on it
static pthread_mutex_t queuesLock = PTHREAD_MUTEX_INITIALIZER;
static NSOperationQueue **liveQueues;
static size_t liveCount, liveCapacity;

static pthread_key_t currentQueueKey;
static pthread_once_t currentQueueOnce = PTHREAD_ONCE_INIT;

static void makeCurrentQueueKey(void) {
    pthread_key_create(&currentQueueKey, NULL);
}

static void scheduleAllQueues(void) {
    pthread_mutex_lock(&queuesLock);
    size_t count = liveCount;
    NSOperationQueue **queues = malloc(sizeof(*queues) * (count ? count : 1));
    for (size_t i = 0; i < count; i++)
        queues[i] = [liveQueues[i] retain];
    pthread_mutex_unlock(&queuesLock);

    for (size_t i = 0; i < count; i++) {
        [queues[i] _schedule];
        [queues[i] release];
    }
    free(queues);
}

@implementation NSOperation

- (instancetype)init {
    if ((self = [super init]) == nil)
        return nil;
    _dependencies = [[NSMutableArray alloc] init];
    _queuePriority = NSOperationQueuePriorityNormal;
    _qualityOfService = NSQualityOfServiceDefault;
    _threadPriority = 0.5;
    _done = dispatch_group_create();
    dispatch_group_enter(_done);
    return self;
}

- (void)dealloc {
    if (!_finishedOnce)
        dispatch_group_leave(_done);
    dispatch_release(_done);
    [_dependencies release];
    [_completionBlock release];
    [_name release];
    [super dealloc];
}

- (void)main {
}

- (void)start {
    if (_state == kExecuting)
        [NSException raise:NSInvalidArgumentException format:@"%@: already executing", self];
    if (_state == kFinished)
        return;

    if (!_cancelled) {
        [self willChangeValueForKey:@"isExecuting"];
        _state = kExecuting;
        [self didChangeValueForKey:@"isExecuting"];
        [self main];
    }

    [self willChangeValueForKey:@"isExecuting"];
    [self willChangeValueForKey:@"isFinished"];
    _state = kFinished;
    [self didChangeValueForKey:@"isFinished"];
    [self didChangeValueForKey:@"isExecuting"];
}

// once per operation, whether NSOperation's -start or a subclass's own state says it finished
- (void)_markFinished {
    if (!__sync_bool_compare_and_swap(&_finishedOnce, 0, 1))
        return;

    void (^completion)(void) = [[self completionBlock] retain];
    if (completion != nil) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            completion();
            [completion release];
        });
    }
    NSOperationQueue *queue = [_queue retain];
    dispatch_group_leave(_done);
    [queue _operationFinished:self];
    [queue release];
    scheduleAllQueues();
}

- (void)didChangeValueForKey:(NSString *)key {
    [super didChangeValueForKey:key];
    if (([key isEqualToString:@"isFinished"] || [key isEqualToString:@"finished"]) && [self isFinished])
        [self _markFinished];
}

- (BOOL)isCancelled {
    return _cancelled;
}

- (void)cancel {
    if (_cancelled)
        return;
    [self willChangeValueForKey:@"isCancelled"];
    _cancelled = YES;
    [self didChangeValueForKey:@"isCancelled"];
    // a cancelled operation is ready at once, whatever it depended on
    [_queue _schedule];
}

- (BOOL)isExecuting {
    return _state == kExecuting;
}

- (BOOL)isFinished {
    return _state == kFinished;
}

- (BOOL)isConcurrent {
    return [self isAsynchronous];
}

- (BOOL)isAsynchronous {
    return NO;
}

- (BOOL)isReady {
    if (_cancelled)
        return YES;
    @synchronized (_dependencies) {
        for (NSOperation *dependency in _dependencies) {
            if (![dependency isFinished])
                return NO;
        }
    }
    return YES;
}

- (void)addDependency:(NSOperation *)operation {
    if (operation == nil)
        return;
    [self willChangeValueForKey:@"dependencies"];
    @synchronized (_dependencies) {
        if (![_dependencies containsObject:operation])
            [_dependencies addObject:operation];
    }
    [self didChangeValueForKey:@"dependencies"];
}

- (void)removeDependency:(NSOperation *)operation {
    [self willChangeValueForKey:@"dependencies"];
    @synchronized (_dependencies) {
        [_dependencies removeObject:operation];
    }
    [self didChangeValueForKey:@"dependencies"];
    [_queue _schedule];
}

- (NSArray *)dependencies {
    @synchronized (_dependencies) {
        return [[_dependencies copy] autorelease];
    }
}

- (NSOperationQueuePriority)queuePriority {
    return _queuePriority;
}

- (void)setQueuePriority:(NSOperationQueuePriority)priority {
    _queuePriority = priority;
}

- (void (^)(void))completionBlock {
    @synchronized (self) {
        return [[_completionBlock retain] autorelease];
    }
}

- (void)setCompletionBlock:(void (^)(void))block {
    @synchronized (self) {
        void (^old)(void) = _completionBlock;
        _completionBlock = [block copy];
        [old release];
    }
}

- (void)waitUntilFinished {
    dispatch_group_wait(_done, DISPATCH_TIME_FOREVER);
}

- (double)threadPriority {
    return _threadPriority;
}

- (void)setThreadPriority:(double)priority {
    _threadPriority = priority;
}

- (NSQualityOfService)qualityOfService {
    return _qualityOfService;
}

- (void)setQualityOfService:(NSQualityOfService)quality {
    _qualityOfService = quality;
}

- (NSString *)name {
    @synchronized (self) {
        return [[_name retain] autorelease];
    }
}

- (void)setName:(NSString *)name {
    @synchronized (self) {
        NSString *old = _name;
        _name = [name copy];
        [old release];
    }
}

@end

@implementation NSBlockOperation

+ (instancetype)blockOperationWithBlock:(void (^)(void))block {
    NSBlockOperation *operation = [[[self alloc] init] autorelease];

    [operation addExecutionBlock:block];
    return operation;
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        _blocks = [[NSMutableArray alloc] init];
    return self;
}

- (void)dealloc {
    [_blocks release];
    [super dealloc];
}

- (void)addExecutionBlock:(void (^)(void))block {
    if (block == nil)
        [NSException raise:NSInvalidArgumentException format:@"%@: nil block", self];
    if ([self isExecuting] || [self isFinished])
        [NSException raise:NSInvalidArgumentException format:@"%@: already started", self];

    void (^copy)(void) = [block copy];
    @synchronized (_blocks) {
        [_blocks addObject:copy];
    }
    [copy release];
}

- (NSArray *)executionBlocks {
    @synchronized (_blocks) {
        return [[_blocks copy] autorelease];
    }
}

// the blocks run concurrently with each other, and the operation finishes once all have
- (void)main {
    NSArray *blocks = [self executionBlocks];
    NSUInteger count = [blocks count];

    if (count == 1) {
        ((void (^)(void))[blocks objectAtIndex:0])();
        return;
    }
    dispatch_apply(count, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(size_t i) {
        ((void (^)(void))[blocks objectAtIndex:i])();
    });
}

@end

@implementation NSInvocationOperation

- (instancetype)initWithTarget:(id)target selector:(SEL)selector object:(id)argument {
    NSMethodSignature *signature = [target methodSignatureForSelector:selector];

    if (signature == nil) {
        [self release];
        return nil;
    }
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setTarget:target];
    [invocation setSelector:selector];
    if ([signature numberOfArguments] > 2)
        [invocation setArgument:&argument atIndex:2];
    return [self initWithInvocation:invocation];
}

- (instancetype)initWithInvocation:(NSInvocation *)invocation {
    if ((self = [super init]) != nil) {
        _invocation = [invocation retain];
        [_invocation retainArguments];
    }
    return self;
}

- (void)dealloc {
    [_invocation release];
    [_exception release];
    [super dealloc];
}

- (NSInvocation *)invocation {
    return _invocation;
}

- (void)main {
    @try {
        [_invocation invoke];
    } @catch (NSException *exception) {
        _exception = [exception retain];
    }
}

- (id)result {
    if ([self isCancelled])
        [NSException raise:NSInvocationOperationCancelledException format:@"%@ was cancelled", self];
    if (_exception != nil)
        [_exception raise];

    const char *type = [[_invocation methodSignature] methodReturnType];
    if (type[0] == 'v')
        [NSException raise:NSInvocationOperationVoidResultException format:@"%@ returns void", self];
    if (type[0] != '@')
        return nil;

    id value = nil;
    [_invocation getReturnValue:&value];
    return value;
}

@end

@implementation NSOperationQueue

+ (NSOperationQueue *)mainQueue {
    static NSOperationQueue *main;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        main = [[NSOperationQueue alloc] init];
        main->_isMain = YES;
        main->_maxConcurrent = 1;
        [main setName:@"NSOperationQueue Main Queue"];
    });
    return main;
}

+ (NSOperationQueue *)currentQueue {
    pthread_once(&currentQueueOnce, makeCurrentQueueKey);
    NSOperationQueue *queue = pthread_getspecific(currentQueueKey);

    if (queue == nil && pthread_main_np())
        return [self mainQueue];
    return queue;
}

- (instancetype)init {
    if ((self = [super init]) == nil)
        return nil;
    _operations = [[NSMutableArray alloc] init];
    _maxConcurrent = NSOperationQueueDefaultMaxConcurrentOperationCount;
    _qualityOfService = NSQualityOfServiceDefault;
    _lock = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(_lock, NULL);

    pthread_mutex_lock(&queuesLock);
    if (liveCount == liveCapacity) {
        liveCapacity = liveCapacity ? liveCapacity * 2 : 16;
        liveQueues = realloc(liveQueues, sizeof(*liveQueues) * liveCapacity);
    }
    liveQueues[liveCount++] = self;
    pthread_mutex_unlock(&queuesLock);
    return self;
}

- (void)dealloc {
    pthread_mutex_lock(&queuesLock);
    for (size_t i = 0; i < liveCount; i++) {
        if (liveQueues[i] == self) {
            liveQueues[i] = liveQueues[--liveCount];
            break;
        }
    }
    pthread_mutex_unlock(&queuesLock);

    [_operations release];
    [_name release];
    if (_underlying != NULL)
        dispatch_release(_underlying);
    pthread_mutex_destroy(_lock);
    free(_lock);
    [super dealloc];
}

- (dispatch_queue_t)_targetQueue {
    if (_isMain)
        return dispatch_get_main_queue();
    if (_underlying != NULL)
        return _underlying;
    return dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
}

- (void)_run:(NSOperation *)operation {
    pthread_once(&currentQueueOnce, makeCurrentQueueKey);
    void *previous = pthread_getspecific(currentQueueKey);

    pthread_setspecific(currentQueueKey, self);
    @try {
        [operation start];
    } @finally {
        pthread_setspecific(currentQueueKey, previous);
    }
}

// starts what the concurrency limit allows, highest priority first and otherwise in order
- (void)_schedule {
    NSMutableArray *starting = [NSMutableArray array];

    pthread_mutex_lock(_lock);
    NSInteger limit = _maxConcurrent < 0 ? NSIntegerMax : _maxConcurrent;
    while (!_suspended && _running < limit) {
        NSOperation *best = nil;

        for (NSOperation *operation in _operations) {
            if (operation->_claimed || ![operation isReady])
                continue;
            if (best == nil || [operation queuePriority] > [best queuePriority])
                best = operation;
        }
        if (best == nil)
            break;
        best->_claimed = YES;
        _running++;
        [starting addObject:best];
    }
    pthread_mutex_unlock(_lock);

    for (NSOperation *operation in starting) {
        [operation retain];
        [self retain];
        dispatch_async([self _targetQueue], ^{
            [self _run:operation];
            [operation release];
            [self release];
        });
    }
}

- (void)_operationFinished:(NSOperation *)operation {
    pthread_mutex_lock(_lock);
    NSUInteger index = [_operations indexOfObjectIdenticalTo:operation];
    if (index != NSNotFound) {
        if (operation->_claimed)
            _running--;
        operation->_queue = nil;
        [_operations removeObjectAtIndex:index];
    }
    pthread_mutex_unlock(_lock);
}

- (void)addOperation:(NSOperation *)operation {
    if (operation == nil)
        return;
    if (operation->_queue != nil || [operation isExecuting] || [operation isFinished])
        [NSException raise:NSInvalidArgumentException format:@"%@: operation is already enqueued, running or finished", self];

    [self willChangeValueForKey:@"operations"];
    [self willChangeValueForKey:@"operationCount"];
    pthread_mutex_lock(_lock);
    operation->_queue = self;
    [_operations addObject:operation];
    pthread_mutex_unlock(_lock);
    [self didChangeValueForKey:@"operationCount"];
    [self didChangeValueForKey:@"operations"];
    [self _schedule];
}

- (void)addOperations:(NSArray *)operations waitUntilFinished:(BOOL)wait {
    for (NSOperation *operation in operations)
        [self addOperation:operation];
    if (wait) {
        for (NSOperation *operation in operations)
            [operation waitUntilFinished];
    }
}

- (void)addOperationWithBlock:(void (^)(void))block {
    [self addOperation:[NSBlockOperation blockOperationWithBlock:block]];
}

// runs once everything enqueued before it has finished, and everything after waits for it
- (void)addBarrierBlock:(void (^)(void))barrier {
    NSBlockOperation *operation = [NSBlockOperation blockOperationWithBlock:barrier];

    for (NSOperation *earlier in [self operations])
        [operation addDependency:earlier];
    [self addOperation:operation];
}

- (NSInteger)maxConcurrentOperationCount {
    return _maxConcurrent;
}

- (void)setMaxConcurrentOperationCount:(NSInteger)count {
    if (_isMain)
        return;
    _maxConcurrent = count;
    [self _schedule];
}

- (BOOL)isSuspended {
    return _suspended;
}

- (void)setSuspended:(BOOL)suspended {
    [self willChangeValueForKey:@"isSuspended"];
    _suspended = suspended;
    [self didChangeValueForKey:@"isSuspended"];
    if (!suspended)
        [self _schedule];
}

- (NSString *)name {
    @synchronized (self) {
        return [[_name retain] autorelease];
    }
}

- (void)setName:(NSString *)name {
    @synchronized (self) {
        NSString *old = _name;
        _name = [name copy];
        [old release];
    }
}

- (NSQualityOfService)qualityOfService {
    return _qualityOfService;
}

- (void)setQualityOfService:(NSQualityOfService)quality {
    _qualityOfService = quality;
}

- (dispatch_queue_t)underlyingQueue {
    return _underlying;
}

- (void)setUnderlyingQueue:(dispatch_queue_t)queue {
    if (_isMain)
        [NSException raise:NSInvalidArgumentException format:@"the main queue's underlying queue is fixed"];
    if (queue != NULL)
        dispatch_retain(queue);
    if (_underlying != NULL)
        dispatch_release(_underlying);
    _underlying = queue;
}

- (NSArray *)operations {
    pthread_mutex_lock(_lock);
    NSArray *copy = [[_operations copy] autorelease];
    pthread_mutex_unlock(_lock);
    return copy;
}

- (NSUInteger)operationCount {
    pthread_mutex_lock(_lock);
    NSUInteger count = [_operations count];
    pthread_mutex_unlock(_lock);
    return count;
}

- (void)cancelAllOperations {
    for (NSOperation *operation in [self operations])
        [operation cancel];
}

- (void)waitUntilAllOperationsAreFinished {
    for (;;) {
        NSArray *pending = [self operations];

        if ([pending count] == 0)
            return;
        for (NSOperation *operation in pending)
            [operation waitUntilFinished];
    }
}

@end
