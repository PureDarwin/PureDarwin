// CAMetalDisplayLink on PureDarwin: a run loop timer like CADisplayLink's that hands its delegate the layer's next
// drawable for each frame
#import <Foundation/Foundation.h>
#import <QuartzCore/CADisplayLink.h>
#import <QuartzCore/CAMetalLayer.h>

@class CAMetalDisplayLink;

@interface CAMetalDisplayLinkUpdate : NSObject
@property(readonly, nonatomic) id<CAMetalDrawable> drawable;
@property(readonly, nonatomic) CFTimeInterval targetTimestamp;
@property(readonly, nonatomic) CFTimeInterval targetPresentationTimestamp;
@end

@protocol CAMetalDisplayLinkDelegate
- (void)metalDisplayLink:(CAMetalDisplayLink *)link needsUpdate:(CAMetalDisplayLinkUpdate *)update;
@end

@interface CAMetalDisplayLink : NSObject

- (instancetype)initWithMetalLayer:(CAMetalLayer *)layer;
- (void)addToRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode;
- (void)removeFromRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode;
- (void)invalidate;

@property(nonatomic, assign) id<CAMetalDisplayLinkDelegate> delegate;
@property(nonatomic) CAFrameRateRange preferredFrameRateRange;
@property(nonatomic) NSInteger preferredFrameLatency;
@property(getter=isPaused, nonatomic) BOOL paused;

@end
