#import <Foundation/Foundation.h>

#import "Protocol.h"

@interface Dylib : NSObject <Verbing>
{
  int m_intProperty;
}
- (void)genericMethod;
- (void)targetMethod:(id)sender;
@end
