//
//  SecABC.h
//  Security
//

#include <CoreFoundation/CoreFoundation.h>

void SecABCTrigger(CFStringRef _Nonnull type,
                   CFStringRef _Nonnull subtype,
                   CFStringRef _Nullable subtypeContext,
                   CFDictionaryRef _Nullable payload);

#if __OBJC__
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SecABC : NSObject

+ (void)triggerAutoBugCaptureWithType:(NSString *)type
                              subType:(NSString *)subType;

+ (void)triggerAutoBugCaptureWithType:(NSString *)type
                              subType:(NSString *)subType
                       subtypeContext:(NSString * _Nullable)subtypeContext
                               events:(NSArray * _Nullable)events
                              payload:(NSDictionary * _Nullable)payload
                      detectedProcess:(NSString * _Nullable)process;


@end

NS_ASSUME_NONNULL_END

#endif
