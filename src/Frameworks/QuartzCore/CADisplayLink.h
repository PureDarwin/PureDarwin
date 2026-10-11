// CADisplayLink on PureDarwin: a run loop timer at the preferred rate (60 Hz by default), not tied to vsync yet
#import <Foundation/Foundation.h>
#import <QuartzCore/CABase.h>

typedef struct CAFrameRateRange {
    float minimum;
    float maximum;
    float preferred;
} CAFrameRateRange;

CA_EXPORT const CAFrameRateRange CAFrameRateRangeDefault;
CA_EXPORT CAFrameRateRange CAFrameRateRangeMake(float minimum, float maximum, float preferred);
CA_EXPORT BOOL CAFrameRateRangeIsEqualToRange(CAFrameRateRange range, CAFrameRateRange other);

@interface CADisplayLink : NSObject

// macOS gets one from -[NSView displayLinkWithTarget:selector:]
+ (CADisplayLink *)displayLinkWithTarget:(id)target selector:(SEL)selector;

- (void)addToRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode;
- (void)removeFromRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode;
- (void)invalidate;

@property(readonly, nonatomic) CFTimeInterval timestamp;
@property(readonly, nonatomic) CFTimeInterval duration;
@property(readonly, nonatomic) CFTimeInterval targetTimestamp;
@property(getter=isPaused, nonatomic) BOOL paused;
@property(nonatomic) NSInteger preferredFramesPerSecond;
@property(nonatomic) CAFrameRateRange preferredFrameRateRange;

@end
