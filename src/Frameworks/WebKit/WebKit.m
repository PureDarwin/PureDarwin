// WebKit's classes as views that load nothing, so apps that show web content in one window still start.
// Navigation delegates are never called, the way a page that is still loading behaves
#import <WebKit/WebKit.h>

@implementation WKUserScript {
    NSString *_source;
    NSInteger _injectionTime;
    BOOL _forMainFrameOnly;
}

- (instancetype)initWithSource:(NSString *)source injectionTime:(NSInteger)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly
{
    if ((self = [super init])) {
        _source = [source copy];
        _injectionTime = injectionTime;
        _forMainFrameOnly = forMainFrameOnly;
    }
    return self;
}

- (void)dealloc
{
    [_source release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (NSString *)source
{
    return _source;
}

- (NSInteger)injectionTime
{
    return _injectionTime;
}

- (BOOL)isForMainFrameOnly
{
    return _forMainFrameOnly;
}
@end

@implementation WKUserContentController {
    NSMutableArray *_scripts;
}

- (instancetype)init
{
    if ((self = [super init]))
        _scripts = [NSMutableArray new];
    return self;
}

- (void)dealloc
{
    [_scripts release];
    [super dealloc];
}

- (NSArray *)userScripts
{
    return [[_scripts copy] autorelease];
}

- (void)addUserScript:(WKUserScript *)userScript
{
    if (userScript)
        [_scripts addObject:userScript];
}

- (void)removeAllUserScripts
{
    [_scripts removeAllObjects];
}

- (void)addScriptMessageHandler:(id)handler name:(NSString *)name
{
}

- (void)removeScriptMessageHandlerForName:(NSString *)name
{
}
@end

@implementation WKWebViewConfiguration
@synthesize userContentController = _userContentController;

- (instancetype)init
{
    if ((self = [super init]))
        _userContentController = [WKUserContentController new];
    return self;
}

- (void)dealloc
{
    [_userContentController release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    WKWebViewConfiguration *copy = [[WKWebViewConfiguration allocWithZone:zone] init];

    copy.userContentController = _userContentController;
    return copy;
}

// keys apps set on the configuration and its preferences, accepted and ignored
- (void)setValue:(id)value forUndefinedKey:(NSString *)key
{
}

- (id)valueForUndefinedKey:(NSString *)key
{
    return nil;
}
@end

@implementation WKWebView {
    WKWebViewConfiguration *_configuration;
}
@synthesize navigationDelegate = _navigationDelegate;
@synthesize UIDelegate = _UIDelegate;

- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if ((self = [super initWithFrame:frame]))
        _configuration = configuration ? [configuration copy] : [WKWebViewConfiguration new];
    return self;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    return [self initWithFrame:frame configuration:nil];
}

- (void)dealloc
{
    [_configuration release];
    [super dealloc];
}

- (WKWebViewConfiguration *)configuration
{
    return [[_configuration copy] autorelease];
}

- (BOOL)isLoading
{
    return NO;
}

- (id)loadHTMLString:(NSString *)string baseURL:(NSURL *)baseURL
{
    return nil;
}

- (id)loadRequest:(NSURLRequest *)request
{
    return nil;
}

- (id)loadFileURL:(NSURL *)URL allowingReadAccessToURL:(NSURL *)readAccessURL
{
    return nil;
}

- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler
{
    if (completionHandler)
        completionHandler(nil, [NSError errorWithDomain:@"WKErrorDomain" code:4 userInfo:nil]);
}

- (id)stopLoading
{
    return nil;
}

- (void)setValue:(id)value forUndefinedKey:(NSString *)key
{
}

- (id)valueForUndefinedKey:(NSString *)key
{
    return nil;
}
@end

@implementation WebPreferences

+ (WebPreferences *)standardPreferences
{
    static WebPreferences *standard;

    if (!standard)
        standard = [WebPreferences new];
    return standard;
}

- (void)setValue:(id)value forUndefinedKey:(NSString *)key
{
}

- (id)valueForUndefinedKey:(NSString *)key
{
    return nil;
}
@end

@implementation WebView
@end
