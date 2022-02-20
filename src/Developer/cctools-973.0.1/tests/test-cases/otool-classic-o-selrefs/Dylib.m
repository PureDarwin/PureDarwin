// cc -framework Foundation -dynamiclib -o Dylib.dylib Dylib.m

#import "Dylib.h"

@interface Dylib ()
@end

@implementation Dylib

- (void)genericMethod
{
  NSLog(@"genericMethod");
}

- (void)targetMethod:(id)sender
{
  NSLog(@"verb");
}

#pragma mark Protocol Verbing

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
