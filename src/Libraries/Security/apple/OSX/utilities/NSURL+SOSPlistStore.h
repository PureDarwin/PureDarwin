//
//  NSURL+SOSPlistStore.h
//

#import <Foundation/NSURL.h>

@interface NSURL (SOSPlistStore)
- (id) readPlist;
- (BOOL) writePlist: (id) plist;
@end
