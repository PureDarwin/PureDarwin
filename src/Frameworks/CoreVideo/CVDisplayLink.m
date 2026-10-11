#import <CoreVideo/CVDisplayLink.h>

#include <mach/mach_time.h>
#include <pthread.h>

// the callback runs on the link's own thread at 60 Hz, as on macOS, so an app may block its main thread
// (MoltenVK's vkcube renders from it). Times are host time and a 60 Hz video clock

enum {
    pdTimeScale = 6000,
    pdRefreshPeriod = 100,
    pdTimeStampValid = (1 << 0) | (1 << 1) | (1 << 3) | (1 << 4),
};

@interface CVDisplayLink : NSObject {
@public
    CVDisplayLinkOutputCallback _callback;
    void *_userInfo;
    pthread_t _thread;
    volatile BOOL _running;
    // a stop then a start makes a new thread, the old one sees the generation change and leaves
    volatile unsigned _generation;
}

@end

static void fillTimeStamp(CVTimeStamp *t, uint64_t frame, uint64_t start, uint64_t period) {
    memset(t, 0, sizeof *t);
    t->videoTimeScale = pdTimeScale;
    t->videoTime = (int64_t)frame * pdRefreshPeriod;
    t->hostTime = start + frame * period;
    t->rateScalar = 1.0;
    t->videoRefreshPeriod = pdRefreshPeriod;
    t->flags = pdTimeStampValid;
}

static void *displayLinkThread(void *arg) {
    CVDisplayLink *link = arg;
    mach_timebase_info_data_t tb;
    uint64_t start = mach_absolute_time(), period, frame = 0;
    unsigned generation = link->_generation;

    mach_timebase_info(&tb);
    period = (uint64_t)(1e9 / 60.0 * tb.denom / tb.numer);
    while (link->_running && link->_generation == generation) {
        CVTimeStamp now, out;
        CVOptionFlags flagsOut = 0;

        frame++;
        mach_wait_until(start + frame * period);
        if (!link->_running || link->_generation != generation)
            break;
        fillTimeStamp(&now, frame, start, period);
        fillTimeStamp(&out, frame + 1, start, period);

        NSAutoreleasePool *pool = [NSAutoreleasePool new];
        if (link->_callback != NULL)
            link->_callback(link, &now, &out, 0, &flagsOut, link->_userInfo);
        [pool release];
    }
    [link release];
    return NULL;
}

@implementation CVDisplayLink

-(void)dealloc {
    _running = NO;
    [super dealloc];
}

-(void)start {
    _generation++;
    _running = YES;
    // the thread holds the link until it notices the stop
    [self retain];
    if (pthread_create(&_thread, NULL, displayLinkThread, self) != 0) {
        _running = NO;
        [self release];
        return;
    }
    pthread_detach(_thread);
}

-(void)stop {
    _running = NO;
}

CVReturn CVDisplayLinkCreateWithActiveCGDisplays(CVDisplayLinkRef *result) {
    if (result == NULL)
        return kCVReturnInvalidArgument;

    *result = [[CVDisplayLink alloc] init];
    return *result != nil ? kCVReturnSuccess : kCVReturnAllocationFailed;
}

CVReturn CVDisplayLinkSetOutputCallback(CVDisplayLinkRef self,
    CVDisplayLinkOutputCallback callback, void *userInfo) {
    if (self == nil || callback == NULL)
        return kCVReturnInvalidArgument;

    self->_callback = callback;
    self->_userInfo = userInfo;
    return kCVReturnSuccess;
}

CVReturn CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(CVDisplayLinkRef self,
    CGLContextObj cglContext, CGLPixelFormatObj cglPixelFormat) {
    if (self == nil)
        return kCVReturnInvalidArgument;

    return kCVReturnSuccess;
}

CVReturn CVDisplayLinkStart(CVDisplayLinkRef self) {
    if (self == nil)
        return kCVReturnInvalidArgument;
    if (CVDisplayLinkIsRunning(self))
        return kCVReturnDisplayLinkAlreadyRunning;

    [self start];
    return kCVReturnSuccess;
}

CVReturn CVDisplayLinkStop(CVDisplayLinkRef self) {
    if (self == nil)
        return kCVReturnInvalidArgument;
    if (!CVDisplayLinkIsRunning(self))
        return kCVReturnDisplayLinkNotRunning;

    [self stop];
    return kCVReturnSuccess;
}

Boolean CVDisplayLinkIsRunning(CVDisplayLinkRef self) {
    return self != nil && self->_running;
}

CVDisplayLinkRef CVDisplayLinkRetain(CVDisplayLinkRef self) {
    return [self retain];
}

void CVDisplayLinkRelease(CVDisplayLinkRef self) {
    [self release];
}

@end
