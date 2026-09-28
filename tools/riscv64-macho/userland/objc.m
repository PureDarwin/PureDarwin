// an objc image: a root class, a category ld64 merges into it, a message send and a class ref
__attribute__((objc_root_class))
@interface A
- (int)base;
+ (id)make;
@end
@implementation A
- (int)base { return 1; }
+ (id)make { return (id)0; }
@end
@interface A (Extra)
- (int)extra;
@end
@implementation A (Extra)
- (int)extra { return 2; }
@end
int use(A *a) { return [a base] + [a extra] + ([A make] != 0); }
