#import <QuartzCore/CAMetalDisplayLink.h>

@interface CAMetalDisplayLinkUpdate ()
- (instancetype)initWithDrawable:(id<CAMetalDrawable>)drawable target:(CFTimeInterval)target;
@end

@implementation CAMetalDisplayLinkUpdate {
    id<CAMetalDrawable> _drawable;
    CFTimeInterval _target;
}

- (instancetype)initWithDrawable:(id<CAMetalDrawable>)drawable target:(CFTimeInterval)target {
    if ((self = [super init])) {
        _drawable = [drawable retain];
        _target = target;
    }
    return self;
}

- (void)dealloc {
    [_drawable release];
    [super dealloc];
}

- (id<CAMetalDrawable>)drawable {
    return _drawable;
}

- (CFTimeInterval)targetTimestamp {
    return _target;
}

// presented one frame after the one it is drawn for
- (CFTimeInterval)targetPresentationTimestamp {
    return _target;
}

@end

// a CADisplayLink drives it, each tick asks the layer for a drawable and hands it over
@implementation CAMetalDisplayLink {
    CAMetalLayer *_layer;
    CADisplayLink *_link;
}

@synthesize delegate = _delegate, preferredFrameLatency = _preferredFrameLatency;

- (instancetype)initWithMetalLayer:(CAMetalLayer *)layer {
    if ((self = [super init])) {
        _layer = [layer retain];
        _link = [[CADisplayLink displayLinkWithTarget:self selector:@selector(_tick:)] retain];
        _preferredFrameLatency = 2;
    }
    return self;
}

- (void)dealloc {
    [_link invalidate];
    [_link release];
    [_layer release];
    [super dealloc];
}

- (void)_tick:(CADisplayLink *)link {
    id<CAMetalDrawable> drawable;

    if (_delegate == nil || !(drawable = [_layer nextDrawable]))
        return;

    CAMetalDisplayLinkUpdate *update = [[CAMetalDisplayLinkUpdate alloc] initWithDrawable:drawable target:link.targetTimestamp];
    [_delegate metalDisplayLink:self needsUpdate:update];
    [update release];
}

- (void)addToRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode {
    [_link addToRunLoop:runloop forMode:mode];
}

- (void)removeFromRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode {
    [_link removeFromRunLoop:runloop forMode:mode];
}

// the display link holds this object as its target: invalidating breaks that cycle
- (void)invalidate {
    [_link invalidate];
}

- (CAFrameRateRange)preferredFrameRateRange {
    return _link.preferredFrameRateRange;
}

- (void)setPreferredFrameRateRange:(CAFrameRateRange)range {
    _link.preferredFrameRateRange = range;
}

- (BOOL)isPaused {
    return _link.paused;
}

- (void)setPaused:(BOOL)paused {
    _link.paused = paused;
}

@end
