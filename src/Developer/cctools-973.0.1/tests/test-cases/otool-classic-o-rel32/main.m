#import <Foundation/Foundation.h>
#import "hi.h"

int main(void)
{
  @autoreleasepool {
    Hi* hi = [[Hi alloc] init];
    [hi greetings];
  }
  return 0;
}
