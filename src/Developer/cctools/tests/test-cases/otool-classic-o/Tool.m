// cc -framework Foundation Tool.m Dylib.dylib

#import <Foundation/Foundation.h>
#import "Dylib.h"

@interface Dylib (Category) <Verbing>
+ (void)categoryClass;
- (void)categoryAddition;
- (void)genericMethod;
@property (nonatomic, readonly) int categoryProperty;
@end

@implementation Dylib (Category)

+ (void)categoryClass
{
  NSLog(@"categoryClass");
}

- (void)categoryAddition
{
  NSLog(@"categoryAddition");
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wobjc-protocol-method-implementation"

- (void)genericMethod
{
  NSLog(@"genericMethod override");
}

#pragma GCC diagnostic pop

- (int)categoryProperty
{
  return 0;
}

@end

@interface DylibSubclass : Dylib
@end

@implementation DylibSubclass
@end

@interface Tool : NSObject <Verbing>
{
  int m_intProperty;
}
@end

@implementation Tool

+ (void)classVerb
{
  NSLog(@"classVerb");
}

- (void)verb
{
  NSLog(@"verb");
}

@synthesize intProperty = m_intProperty;

@end

int main(int argc, const char * argv[])
{
  @autoreleasepool {
    DylibSubclass* dylib = [[DylibSubclass alloc] init];
  }
  return 0;
}
