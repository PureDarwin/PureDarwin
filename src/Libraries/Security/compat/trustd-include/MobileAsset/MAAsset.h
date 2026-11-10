// MAAsset.h - the MobileAsset declarations trustd compiles against
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// trustd weak-links them and checks [MAAsset class], so with no MobileAsset trust store updates are off
#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, MADownLoadResult) {
    MADownloadSuccessful = 0,
    MADownloadDaemonNotReady = 21,
};

typedef NS_ENUM(NSInteger, MAQueryResult) {
    MAQuerySuccessful = 0,
};

typedef NS_ENUM(NSInteger, MAPurgeResult) {
    MAPurgeSucceeded = 0,
};

typedef NS_ENUM(NSInteger, MAAssetState) {
    MAUnknown = 0,
    MANotPresent = 1,
    MAInstalled = 2,
    MADownloading = 3,
};

typedef NS_ENUM(NSInteger, MAQueryReturnTypes) {
    MAInstalledOnly = 1,
};

__attribute__((weak_import))
@interface MADownloadOptions : NSObject
@property (nonatomic) BOOL allowsCellularAccess;
@property (nonatomic) BOOL discretionary;
@end

__attribute__((weak_import))
@interface MAAsset : NSObject
@property (nonatomic, readonly) MAAssetState state;
@property (nonatomic, readonly) NSDictionary *attributes;
+ (void)startCatalogDownload:(NSString *)type options:(MADownloadOptions *)options then:(void (^)(MADownLoadResult result))handler;
- (void)startDownload:(MADownloadOptions *)options then:(void (^)(MADownLoadResult result))handler;
- (void)purge:(void (^)(MAPurgeResult result))handler;
- (NSURL *)getLocalFileUrl;
- (id)assetProperty:(NSString *)key;
@end

__attribute__((weak_import))
@interface ASAsset : NSObject
@end

__attribute__((weak_import))
@interface ASAssetQuery : NSObject
@end
