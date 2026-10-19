// Vision.m - image requests with no recognition models on this system
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// performing a request fails with VNErrorDomain's unsupported-request error, the results stay empty

#import <Foundation/Foundation.h>

NSString *const VNErrorDomain = @"com.apple.Vision";

enum { VNErrorUnsupportedRequest = 6 };

typedef NS_ENUM(NSInteger, VNRequestTextRecognitionLevel) {
    VNRequestTextRecognitionLevelAccurate = 0,
    VNRequestTextRecognitionLevelFast = 1,
};

@interface VNRequest : NSObject {
    void (^_completion)(VNRequest *, NSError *);
}
@end

@implementation VNRequest

- (instancetype)initWithCompletionHandler:(void (^)(VNRequest *request, NSError *error))handler
{
    if ((self = [super init]))
        _completion = [handler copy];
    return self;
}

- (void)dealloc
{
    [_completion release];
    [super dealloc];
}

- (NSArray *)results { return nil; }
- (void (^)(VNRequest *, NSError *))completionHandler { return _completion; }
- (NSUInteger)revision { return 1; }
- (void)setRevision:(NSUInteger)revision { }
- (BOOL)preferBackgroundProcessing { return NO; }
- (void)setPreferBackgroundProcessing:(BOOL)prefer { }
- (void)cancel { }

@end

@interface VNImageBasedRequest : VNRequest
@end

@implementation VNImageBasedRequest
@end

@interface VNRecognizeTextRequest : VNImageBasedRequest {
    VNRequestTextRecognitionLevel _level;
    NSArray *_languages;
    BOOL _correction;
}
@end

@implementation VNRecognizeTextRequest

+ (NSArray *)supportedRecognitionLanguagesForTextRecognitionLevel:(VNRequestTextRecognitionLevel)level
                                                         revision:(NSUInteger)revision error:(NSError **)error
{
    return @[];
}

- (void)dealloc
{
    [_languages release];
    [super dealloc];
}

- (NSArray *)supportedRecognitionLanguagesAndReturnError:(NSError **)error { return @[]; }
- (VNRequestTextRecognitionLevel)recognitionLevel { return _level; }
- (void)setRecognitionLevel:(VNRequestTextRecognitionLevel)level { _level = level; }
- (NSArray *)recognitionLanguages { return _languages ? _languages : @[]; }
- (void)setRecognitionLanguages:(NSArray *)languages
{
    [_languages autorelease];
    _languages = [languages copy];
}
- (BOOL)usesLanguageCorrection { return _correction; }
- (void)setUsesLanguageCorrection:(BOOL)correction { _correction = correction; }
- (BOOL)automaticallyDetectsLanguage { return NO; }
- (void)setAutomaticallyDetectsLanguage:(BOOL)detects { }
- (float)minimumTextHeight { return 0; }
- (void)setMinimumTextHeight:(float)height { }

@end

@interface VNImageRequestHandler : NSObject
@end

@implementation VNImageRequestHandler

- (instancetype)initWithCGImage:(void *)image options:(NSDictionary *)options { return [super init]; }
- (instancetype)initWithCGImage:(void *)image orientation:(uint32_t)orientation options:(NSDictionary *)options { return [super init]; }
- (instancetype)initWithCVPixelBuffer:(void *)pixelBuffer options:(NSDictionary *)options { return [super init]; }
- (instancetype)initWithData:(NSData *)data options:(NSDictionary *)options { return [super init]; }
- (instancetype)initWithURL:(NSURL *)url options:(NSDictionary *)options { return [super init]; }

- (BOOL)performRequests:(NSArray *)requests error:(NSError **)outError
{
    NSError *error = [NSError errorWithDomain:VNErrorDomain code:VNErrorUnsupportedRequest
                                     userInfo:@{ NSLocalizedDescriptionKey : @"No Vision request is supported on this system." }];
    for (VNRequest *r in requests) {
        if ([r completionHandler])
            [r completionHandler](r, error);
    }
    if (outError)
        *outError = error;
    return NO;
}

@end
