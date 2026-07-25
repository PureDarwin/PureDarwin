// TEST_CFLAGS -framework Foundation
/*
TEST_RUN_OUTPUT
foo
bar
bar
foo
END
*/

// NOTE: This test won't catch problems when running against a root, so it's of
// limited utility, but it would at least catch things when testing against the
// shared cache.

#include <Foundation/Foundation.h>
#include <objc/runtime.h>

@interface NSBlock: NSObject @end

// NSBlock is a conveniently accessible superclass that (currently) has a constant cache.
@interface MyBlock: NSBlock
+(void)foo;
+(void)bar;
@end
@implementation MyBlock
+(void)foo {
  printf("foo\n");
}
+(void)bar {
  printf("bar\n");
}
@end

int main() {
  [MyBlock foo];
  [MyBlock bar];
  
  Method m1 = class_getClassMethod([MyBlock class], @selector(foo));
  Method m2 = class_getClassMethod([MyBlock class], @selector(bar));
  method_exchangeImplementations(m1, m2);
  
  [MyBlock foo];
  [MyBlock bar];
}
