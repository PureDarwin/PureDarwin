#import <QuartzCore/CADisplayLink.h>

// zeros: no preference, the display's own rate
const CAFrameRateRange CAFrameRateRangeDefault = { 0, 0, 0 };

CAFrameRateRange CAFrameRateRangeMake(float minimum, float maximum, float preferred) {
    return (CAFrameRateRange){ minimum, maximum, preferred };
}

BOOL CAFrameRateRangeIsEqualToRange(CAFrameRateRange range, CAFrameRateRange other) {
    return range.minimum == other.minimum && range.maximum == other.maximum && range.preferred == other.preferred;
}

@implementation CADisplayLink {
    id _target;
    SEL _selector;
    NSTimer *_timer;
    NSMutableArray *_runLoops; // pairs of run loop and mode the timer is in
    CFTimeInterval _timestamp, _duration;
    BOOL _paused;
    CAFrameRateRange _range;
}

+ (CADisplayLink *)displayLinkWithTarget:(id)target selector:(SEL)selector {
    CADisplayLink *link = [[[self alloc] init] autorelease];

    link->_target = [target retain];
    link->_selector = selector;
    link->_duration = 1.0 / 60;
    link->_range = (CAFrameRateRange){ 60, 60, 60 };
    link->_runLoops = [[NSMutableArray alloc] init];
    return link;
}

- (void)dealloc {
    [_timer invalidate];
    [_runLoops release];
    [_target release];
    [super dealloc];
}

- (void)_fire:(NSTimer *)timer {
    (void)timer;
    if (_paused)
        return;
    _timestamp = CACurrentMediaTime();
    [_target performSelector:_selector withObject:self];
}

// the timer holds the link, so it is rebuilt on every run loop and rate change
- (void)_schedule {
    [_timer invalidate];
    _timer = nil;
    if (_runLoops.count == 0)
        return;
    _timer = [NSTimer timerWithTimeInterval:_duration target:self selector:@selector(_fire:) userInfo:nil repeats:YES];
    for (NSUInteger i = 0; i + 1 < _runLoops.count; i += 2)
        [[_runLoops objectAtIndex:i] addTimer:_timer forMode:[_runLoops objectAtIndex:i + 1]];
}

- (void)addToRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode {
    [_runLoops addObject:runloop];
    [_runLoops addObject:mode];
    [self _schedule];
}

- (void)removeFromRunLoop:(NSRunLoop *)runloop forMode:(NSRunLoopMode)mode {
    for (NSUInteger i = 0; i + 1 < _runLoops.count; i += 2) {
        if ([_runLoops objectAtIndex:i] == runloop && [[_runLoops objectAtIndex:i + 1] isEqual:mode]) {
            [_runLoops removeObjectsInRange:NSMakeRange(i, 2)];
            break;
        }
    }
    [self _schedule];
}

- (void)invalidate {
    [_runLoops removeAllObjects];
    [self _schedule];
    [_target release];
    _target = nil;
}

- (CFTimeInterval)timestamp {
    return _timestamp;
}

- (CFTimeInterval)duration {
    return _duration;
}

- (CFTimeInterval)targetTimestamp {
    return _timestamp + _duration;
}

- (BOOL)isPaused {
    return _paused;
}

- (void)setPaused:(BOOL)paused {
    _paused = paused;
}

- (NSInteger)preferredFramesPerSecond {
    return (NSInteger)_range.preferred;
}

- (void)setPreferredFramesPerSecond:(NSInteger)fps {
    float f = fps > 0 ? (float)fps : 60;

    self.preferredFrameRateRange = (CAFrameRateRange){ f, f, f };
}

- (CAFrameRateRange)preferredFrameRateRange {
    return _range;
}

- (void)setPreferredFrameRateRange:(CAFrameRateRange)range {
    float fps = range.preferred > 0 ? range.preferred : range.maximum > 0 ? range.maximum : 60;

    _range = range;
    _duration = 1.0 / fps;
    [self _schedule];
}

@end
