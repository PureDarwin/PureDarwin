//
//  SOSTrustedDeviceAttributes.h
//  Security
//

#ifndef SOSTrustedDeviceAttributes_h
#define SOSTrustedDeviceAttributes_h

#import <Foundation/Foundation.h>
#define MACHINEID @"machineID"
#define SERIALNUMBER @"serialNumber"

NS_ASSUME_NONNULL_BEGIN

#if __OBJC2__

@interface SOSTrustedDeviceAttributes : NSObject <NSSecureCoding>

@property NSString *machineID;
@property NSString *serialNumber;

@end
#endif /* __OBJC2__ */

NS_ASSUME_NONNULL_END

#endif /* SOSTrustedDeviceAttributes_h */
