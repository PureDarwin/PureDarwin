// PureDarwin WebKit: the classes apps link, views that load nothing yet
#import <AppKit/AppKit.h>

@class WKUserContentController;

@interface WKUserScript : NSObject <NSCopying>
- (instancetype)initWithSource:(NSString *)source injectionTime:(NSInteger)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly;
@property (nonatomic, readonly, copy) NSString *source;
@property (nonatomic, readonly) NSInteger injectionTime;
@property (nonatomic, readonly, getter=isForMainFrameOnly) BOOL forMainFrameOnly;
@end

@interface WKUserContentController : NSObject
@property (nonatomic, readonly, copy) NSArray *userScripts;
- (void)addUserScript:(WKUserScript *)userScript;
- (void)removeAllUserScripts;
- (void)addScriptMessageHandler:(id)handler name:(NSString *)name;
- (void)removeScriptMessageHandlerForName:(NSString *)name;
@end

@interface WKWebViewConfiguration : NSObject <NSCopying>
@property (nonatomic, retain) WKUserContentController *userContentController;
@end

@interface WKWebView : NSView
- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration *)configuration;
@property (nonatomic, readonly, copy) WKWebViewConfiguration *configuration;
@property (nonatomic, assign) id navigationDelegate;
@property (nonatomic, assign) id UIDelegate;
@property (nonatomic, readonly, getter=isLoading) BOOL loading;
- (id)loadHTMLString:(NSString *)string baseURL:(NSURL *)baseURL;
- (id)loadRequest:(NSURLRequest *)request;
- (id)loadFileURL:(NSURL *)URL allowingReadAccessToURL:(NSURL *)readAccessURL;
- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler;
- (id)stopLoading;
@end

@interface WebPreferences : NSObject
+ (WebPreferences *)standardPreferences;
@end

@interface WebView : NSView
@end
