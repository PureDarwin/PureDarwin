@protocol Verbing
+ (void)classVerb;
- (void) verb;
@property (nonatomic, readonly) int intProperty;

@optional
+ (void)classOptionalVerb;
- (void)optionalVerb;
@end