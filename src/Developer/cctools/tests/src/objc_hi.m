#import <Foundation/Foundation.h>
#include <stdio.h>

@interface Hi : NSObject
{
  int payload;
}
- (void)greetings;
- (void)unused;
@end

@implementation Hi
- (void)greetings
{
  printf("hello, Objective-C!\n");
}
- (void)unused
{
}
@end

int main(void)
{
  @autoreleasepool {
    Hi* hi = [[Hi alloc] init];
    [hi greetings];
  }
  return 0;
}
