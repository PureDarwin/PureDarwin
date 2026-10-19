// NSProgress.h - progress of a piece of work, its children and its user info
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSObject.h>

@class NSString, NSDictionary, NSMutableDictionary, NSMutableArray, NSNumber, NSURL;

typedef NSString *NSProgressKind;
typedef NSString *NSProgressUserInfoKey;
typedef NSString *NSProgressFileOperationKind;

FOUNDATION_EXPORT NSProgressKind const NSProgressKindFile;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressEstimatedTimeRemainingKey;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressThroughputKey;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressFileOperationKindKey;
FOUNDATION_EXPORT NSProgressFileOperationKind const NSProgressFileOperationKindDownloading;
FOUNDATION_EXPORT NSProgressFileOperationKind const NSProgressFileOperationKindDecompressingAfterDownloading;
FOUNDATION_EXPORT NSProgressFileOperationKind const NSProgressFileOperationKindReceiving;
FOUNDATION_EXPORT NSProgressFileOperationKind const NSProgressFileOperationKindCopying;
FOUNDATION_EXPORT NSProgressFileOperationKind const NSProgressFileOperationKindUploading;
FOUNDATION_EXPORT NSProgressFileOperationKind const NSProgressFileOperationKindDuplicating;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressFileURLKey;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressFileTotalCountKey;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressFileCompletedCountKey;

@interface NSProgress : NSObject {
    NSProgress *_parent;
    int64_t _pendingUnitCountInParent;
    int64_t _totalUnitCount;
    int64_t _completedUnitCount;
    NSMutableArray *_children;
    NSMutableDictionary *_userInfo;
    NSString *_kind;
    NSString *_localizedDescription;
    NSString *_localizedAdditionalDescription;
    BOOL _cancellable;
    BOOL _pausable;
    BOOL _cancelled;
    BOOL _paused;
    BOOL _published;
    id _cancellationHandler;
    id _pausingHandler;
    id _resumingHandler;
    // the child that becomeCurrent will attach implicitly, and the units it gets
    int64_t _pendingUnitCountForCurrent;
    NSProgress *_previousCurrent;
}

+ (NSProgress *)currentProgress;
+ (NSProgress *)progressWithTotalUnitCount:(int64_t)unitCount;
+ (NSProgress *)discreteProgressWithTotalUnitCount:(int64_t)unitCount;
+ (NSProgress *)progressWithTotalUnitCount:(int64_t)unitCount parent:(NSProgress *)parent pendingUnitCount:(int64_t)portionOfParentTotalUnitCount;

- (instancetype)initWithParent:(NSProgress *)parentProgressOrNil userInfo:(NSDictionary *)userInfoOrNil;

- (void)becomeCurrentWithPendingUnitCount:(int64_t)unitCount;
- (void)resignCurrent;
- (void)addChild:(NSProgress *)child withPendingUnitCount:(int64_t)inUnitCount;

@property int64_t totalUnitCount;
@property int64_t completedUnitCount;
@property (copy) NSString *localizedDescription;
@property (copy) NSString *localizedAdditionalDescription;
@property (getter=isCancellable) BOOL cancellable;
@property (getter=isPausable) BOOL pausable;
@property (readonly, getter=isCancelled) BOOL cancelled;
@property (readonly, getter=isPaused) BOOL paused;
@property (copy) void (^cancellationHandler)(void);
@property (copy) void (^pausingHandler)(void);
@property (copy) void (^resumingHandler)(void);
@property (readonly, getter=isIndeterminate) BOOL indeterminate;
@property (readonly) double fractionCompleted;
@property (readonly, getter=isFinished) BOOL finished;
@property (copy) NSProgressKind kind;
@property (readonly, copy) NSDictionary *userInfo;
@property (copy) NSProgressFileOperationKind fileOperationKind;
@property (copy) NSURL *fileURL;
@property (copy) NSNumber *estimatedTimeRemaining;
@property (copy) NSNumber *throughput;
@property (copy) NSNumber *fileTotalCount;
@property (copy) NSNumber *fileCompletedCount;

- (void)setUserInfoObject:(id)objectOrNil forKey:(NSProgressUserInfoKey)key;
- (void)cancel;
- (void)pause;
- (void)resume;
- (void)publish;
- (void)unpublish;

@end
