// NSProgress.m - progress of a piece of work, its children and its user info
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// there is no progress server for Finder to watch, so publishing only marks the object as published
#import <Foundation/NSProgress.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSKeyValueObserving.h>
#include <pthread.h>

NSProgressKind const NSProgressKindFile = @"NSProgressKindFile";
NSProgressUserInfoKey const NSProgressEstimatedTimeRemainingKey = @"NSProgressEstimatedTimeRemainingKey";
NSProgressUserInfoKey const NSProgressThroughputKey = @"NSProgressThroughputKey";
NSProgressUserInfoKey const NSProgressFileOperationKindKey = @"NSProgressFileOperationKindKey";
NSProgressFileOperationKind const NSProgressFileOperationKindDownloading = @"NSProgressFileOperationKindDownloading";
NSProgressFileOperationKind const NSProgressFileOperationKindDecompressingAfterDownloading = @"NSProgressFileOperationKindDecompressingAfterDownloading";
NSProgressFileOperationKind const NSProgressFileOperationKindReceiving = @"NSProgressFileOperationKindReceiving";
NSProgressFileOperationKind const NSProgressFileOperationKindCopying = @"NSProgressFileOperationKindCopying";
NSProgressFileOperationKind const NSProgressFileOperationKindUploading = @"NSProgressFileOperationKindUploading";
NSProgressFileOperationKind const NSProgressFileOperationKindDuplicating = @"NSProgressFileOperationKindDuplicating";
NSProgressUserInfoKey const NSProgressFileURLKey = @"NSProgressFileURLKey";
NSProgressUserInfoKey const NSProgressFileTotalCountKey = @"NSProgressFileTotalCountKey";
NSProgressUserInfoKey const NSProgressFileCompletedCountKey = @"NSProgressFileCompletedCountKey";

// the current progress is per thread
static pthread_key_t currentKey;
static pthread_once_t currentOnce = PTHREAD_ONCE_INIT;

static void makeCurrentKey(void) {
    pthread_key_create(&currentKey, NULL);
}

@interface NSProgress (Private)
- (void)_childChanged;
@end

@implementation NSProgress

+ (NSProgress *)currentProgress {
    pthread_once(&currentOnce, makeCurrentKey);
    return pthread_getspecific(currentKey);
}

+ (NSProgress *)progressWithTotalUnitCount:(int64_t)unitCount {
    NSProgress *progress = [[[self alloc] initWithParent:[self currentProgress] userInfo:nil] autorelease];

    [progress setTotalUnitCount:unitCount];
    return progress;
}

+ (NSProgress *)discreteProgressWithTotalUnitCount:(int64_t)unitCount {
    NSProgress *progress = [[[self alloc] initWithParent:nil userInfo:nil] autorelease];

    [progress setTotalUnitCount:unitCount];
    return progress;
}

+ (NSProgress *)progressWithTotalUnitCount:(int64_t)unitCount parent:(NSProgress *)parent pendingUnitCount:(int64_t)portion {
    NSProgress *progress = [self discreteProgressWithTotalUnitCount:unitCount];

    [parent addChild:progress withPendingUnitCount:portion];
    return progress;
}

// a parent given here must be the current progress, which hands over its pending units
- (instancetype)initWithParent:(NSProgress *)parent userInfo:(NSDictionary *)userInfo {
    if ((self = [super init]) != nil) {
        _userInfo = userInfo ? [userInfo mutableCopy] : [[NSMutableDictionary alloc] init];
        _children = [[NSMutableArray alloc] init];
        _cancellable = YES;
        if (parent != nil && parent == [NSProgress currentProgress]) {
            [parent addChild:self withPendingUnitCount:parent->_pendingUnitCountForCurrent];
            parent->_pendingUnitCountForCurrent = 0;
        }
    }
    return self;
}

- (instancetype)init {
    return [self initWithParent:nil userInfo:nil];
}

- (void)dealloc {
    [_children release];
    [_userInfo release];
    [_kind release];
    [_localizedDescription release];
    [_localizedAdditionalDescription release];
    [_cancellationHandler release];
    [_pausingHandler release];
    [_resumingHandler release];
    [super dealloc];
}

- (void)becomeCurrentWithPendingUnitCount:(int64_t)unitCount {
    pthread_once(&currentOnce, makeCurrentKey);
    _pendingUnitCountForCurrent = unitCount;
    _previousCurrent = pthread_getspecific(currentKey);
    pthread_setspecific(currentKey, self);
}

// units nobody claimed while this was current count as done
- (void)resignCurrent {
    if (_pendingUnitCountForCurrent > 0)
        [self setCompletedUnitCount:_completedUnitCount + _pendingUnitCountForCurrent];
    _pendingUnitCountForCurrent = 0;
    pthread_setspecific(currentKey, _previousCurrent);
    _previousCurrent = nil;
}

- (void)addChild:(NSProgress *)child withPendingUnitCount:(int64_t)unitCount {
    @synchronized(self) {
        child->_parent = self;
        child->_pendingUnitCountInParent = unitCount;
        [_children addObject:child];
    }
    [self _childChanged];
}

- (void)_notifyFraction {
    [self willChangeValueForKey:@"fractionCompleted"];
    [self didChangeValueForKey:@"fractionCompleted"];
    [_parent _childChanged];
}

// a finished child's units move into this progress's own completed count
- (void)_childChanged {
    NSArray *children;
    int64_t done = 0;

    @synchronized(self) {
        children = [[_children copy] autorelease];
    }
    for (NSProgress *child in children) {
        if ([child isFinished]) {
            done += child->_pendingUnitCountInParent;
            @synchronized(self) {
                [_children removeObjectIdenticalTo:child];
            }
            child->_parent = nil;
        }
    }
    if (done > 0)
        [self setCompletedUnitCount:_completedUnitCount + done];
    else
        [self _notifyFraction];
}

- (int64_t)totalUnitCount {
    return _totalUnitCount;
}

- (void)setTotalUnitCount:(int64_t)count {
    [self willChangeValueForKey:@"totalUnitCount"];
    _totalUnitCount = count;
    [self didChangeValueForKey:@"totalUnitCount"];
    [self _notifyFraction];
}

- (int64_t)completedUnitCount {
    return _completedUnitCount;
}

- (void)setCompletedUnitCount:(int64_t)count {
    [self willChangeValueForKey:@"completedUnitCount"];
    _completedUnitCount = count;
    [self didChangeValueForKey:@"completedUnitCount"];
    [self _notifyFraction];
}

- (double)fractionCompleted {
    double fraction;
    NSArray *children;

    if (_totalUnitCount <= 0)
        return 0;
    @synchronized(self) {
        children = [[_children copy] autorelease];
    }
    fraction = (double)_completedUnitCount;
    for (NSProgress *child in children)
        fraction += [child fractionCompleted] * child->_pendingUnitCountInParent;
    fraction /= _totalUnitCount;
    return fraction > 1 ? 1 : fraction;
}

- (BOOL)isIndeterminate {
    return _totalUnitCount < 0 || _completedUnitCount < 0 || (_totalUnitCount == 0 && _completedUnitCount == 0);
}

- (BOOL)isFinished {
    return _totalUnitCount > 0 && _completedUnitCount >= _totalUnitCount;
}

- (NSString *)localizedDescription {
    if (_localizedDescription != nil)
        return _localizedDescription;
    if ([self isIndeterminate])
        return @"";
    return [NSString stringWithFormat:@"%d%% completed", (int)([self fractionCompleted] * 100)];
}

- (void)setLocalizedDescription:(NSString *)description {
    NSString *old = _localizedDescription;
    _localizedDescription = [description copy];
    [old release];
}

- (NSString *)localizedAdditionalDescription {
    if (_localizedAdditionalDescription != nil)
        return _localizedAdditionalDescription;
    if ([self isIndeterminate])
        return @"";
    return [NSString stringWithFormat:@"%lld of %lld", (long long)_completedUnitCount, (long long)_totalUnitCount];
}

- (void)setLocalizedAdditionalDescription:(NSString *)description {
    NSString *old = _localizedAdditionalDescription;
    _localizedAdditionalDescription = [description copy];
    [old release];
}

- (BOOL)isCancellable { return _cancellable; }
- (void)setCancellable:(BOOL)flag { _cancellable = flag; }
- (BOOL)isPausable { return _pausable; }
- (void)setPausable:(BOOL)flag { _pausable = flag; }
- (BOOL)isCancelled { return _cancelled || [_parent isCancelled]; }
- (BOOL)isPaused { return _paused || [_parent isPaused]; }

- (void (^)(void))cancellationHandler { return _cancellationHandler; }

- (void)setCancellationHandler:(void (^)(void))handler {
    id old = _cancellationHandler;
    _cancellationHandler = [handler copy];
    [old release];
    // a handler set after cancelling still runs, as on macOS
    if (_cancelled && handler != nil)
        handler();
}

- (void (^)(void))pausingHandler { return _pausingHandler; }

- (void)setPausingHandler:(void (^)(void))handler {
    id old = _pausingHandler;
    _pausingHandler = [handler copy];
    [old release];
}

- (void (^)(void))resumingHandler { return _resumingHandler; }

- (void)setResumingHandler:(void (^)(void))handler {
    id old = _resumingHandler;
    _resumingHandler = [handler copy];
    [old release];
}

- (void)cancel {
    NSArray *children;
    void (^handler)(void);

    if (_cancelled)
        return;
    [self willChangeValueForKey:@"cancelled"];
    _cancelled = YES;
    [self didChangeValueForKey:@"cancelled"];
    handler = [[_cancellationHandler retain] autorelease];
    if (handler != nil)
        handler();
    @synchronized(self) {
        children = [[_children copy] autorelease];
    }
    [children makeObjectsPerformSelector:@selector(cancel)];
}

- (void)pause {
    NSArray *children;
    void (^handler)(void);

    if (_paused)
        return;
    [self willChangeValueForKey:@"paused"];
    _paused = YES;
    [self didChangeValueForKey:@"paused"];
    handler = [[_pausingHandler retain] autorelease];
    if (handler != nil)
        handler();
    @synchronized(self) {
        children = [[_children copy] autorelease];
    }
    [children makeObjectsPerformSelector:@selector(pause)];
}

- (void)resume {
    NSArray *children;
    void (^handler)(void);

    if (!_paused)
        return;
    [self willChangeValueForKey:@"paused"];
    _paused = NO;
    [self didChangeValueForKey:@"paused"];
    handler = [[_resumingHandler retain] autorelease];
    if (handler != nil)
        handler();
    @synchronized(self) {
        children = [[_children copy] autorelease];
    }
    [children makeObjectsPerformSelector:@selector(resume)];
}

- (NSProgressKind)kind {
    return _kind;
}

- (void)setKind:(NSProgressKind)kind {
    NSString *old = _kind;
    _kind = [kind copy];
    [old release];
}

- (NSDictionary *)userInfo {
    @synchronized(self) {
        return [[_userInfo copy] autorelease];
    }
}

- (void)setUserInfoObject:(id)object forKey:(NSProgressUserInfoKey)key {
    [self willChangeValueForKey:@"userInfo"];
    @synchronized(self) {
        if (object != nil)
            [_userInfo setObject:object forKey:key];
        else
            [_userInfo removeObjectForKey:key];
    }
    [self didChangeValueForKey:@"userInfo"];
}

- (id)_userInfoObjectForKey:(NSString *)key {
    @synchronized(self) {
        return [[[_userInfo objectForKey:key] retain] autorelease];
    }
}

- (NSProgressFileOperationKind)fileOperationKind { return [self _userInfoObjectForKey:NSProgressFileOperationKindKey]; }
- (void)setFileOperationKind:(NSProgressFileOperationKind)kind { [self setUserInfoObject:kind forKey:NSProgressFileOperationKindKey]; }
- (NSURL *)fileURL { return [self _userInfoObjectForKey:NSProgressFileURLKey]; }
- (void)setFileURL:(NSURL *)url { [self setUserInfoObject:url forKey:NSProgressFileURLKey]; }
- (NSNumber *)estimatedTimeRemaining { return [self _userInfoObjectForKey:NSProgressEstimatedTimeRemainingKey]; }
- (void)setEstimatedTimeRemaining:(NSNumber *)value { [self setUserInfoObject:value forKey:NSProgressEstimatedTimeRemainingKey]; }
- (NSNumber *)throughput { return [self _userInfoObjectForKey:NSProgressThroughputKey]; }
- (void)setThroughput:(NSNumber *)value { [self setUserInfoObject:value forKey:NSProgressThroughputKey]; }
- (NSNumber *)fileTotalCount { return [self _userInfoObjectForKey:NSProgressFileTotalCountKey]; }
- (void)setFileTotalCount:(NSNumber *)value { [self setUserInfoObject:value forKey:NSProgressFileTotalCountKey]; }
- (NSNumber *)fileCompletedCount { return [self _userInfoObjectForKey:NSProgressFileCompletedCountKey]; }
- (void)setFileCompletedCount:(NSNumber *)value { [self setUserInfoObject:value forKey:NSProgressFileCompletedCountKey]; }

- (void)publish {
    _published = YES;
}

- (void)unpublish {
    _published = NO;
}

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key {
    // the setters above announce their own changes
    if ([key isEqualToString:@"totalUnitCount"] || [key isEqualToString:@"completedUnitCount"] || [key isEqualToString:@"userInfo"])
        return NO;
    return [super automaticallyNotifiesObserversForKey:key];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %p> : Parent: %p / Fraction completed: %.4f / Completed: %lld of %lld", [self class],
                                      self, _parent, [self fractionCompleted], (long long)_completedUnitCount, (long long)_totalUnitCount];
}

@end
