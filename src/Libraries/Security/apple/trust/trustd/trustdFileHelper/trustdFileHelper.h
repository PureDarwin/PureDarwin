//
//  trustdFileHelper.h
//  Security
//
//  Copyright © 2020 Apple Inc. All rights reserved.
//

#ifndef _SECURITY_TRUSTDFILEHELPER_H_
#define _SECURITY_TRUSTDFILEHELPER_H_

#import <Foundation/Foundation.h>
#include "trust/trustd/trustdFileLocations.h"

@interface TrustdFileHelper : NSObject <TrustdFileHelper_protocol>
- (void)fixFiles:(void (^)(BOOL, NSError*))reply;
@end

#endif /* _SECURITY_TRUSTDFILEHELPER_H_ */
