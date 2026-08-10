//
//  SecXPCHelper.h
//  Security
//

#ifndef SecXPCHelper_h
#define SecXPCHelper_h

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SecXPCHelper : NSObject
+ (NSSet<Class> *)safeErrorClasses;
+ (NSError * _Nullable)cleanseErrorForXPC:(NSError * _Nullable)error;

/*
 * Some NSError objects contain non-NSSecureCoding-compliant userInfo.
 * When in doubt, use cleanseErrorForXPC: before encodedDataFromError:
 */
+ (NSError * _Nullable)errorFromEncodedData:(NSData *)data;
+ (NSData *)encodedDataFromError:(NSError *)error;
@end

NS_ASSUME_NONNULL_END

#endif // SecXPCHelper_h
