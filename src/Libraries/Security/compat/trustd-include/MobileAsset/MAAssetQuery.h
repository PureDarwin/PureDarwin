// MAAssetQuery.h - see MAAsset.h, MobileAsset is weak-linked and absent here
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <MobileAsset/MAAsset.h>

__attribute__((weak_import))
@interface MAAssetQuery : NSObject
@property (nonatomic, readonly) NSArray *results;
- (instancetype)initWithType:(NSString *)type;
- (void)returnTypes:(MAQueryReturnTypes)types;
- (void)augmentResultsWithState:(BOOL)augment;
- (MAQueryResult)queryMetaDataSync;
@end
