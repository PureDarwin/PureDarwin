/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSRunLoop.h>
#import <Foundation/NSPort.h>
#import <Foundation/NSSelectInputSource.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSTimer.h>
#import <Foundation/NSThread-Private.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFFileDescriptor.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <dispatch/dispatch.h>
#include <sys/select.h>
#include <pthread.h>

// a select input source's descriptor as a CFFileDescriptor in the CFRunLoop, kept for the source's lifetime
@interface NSRunLoopDescriptorWatch : NSObject {
@public
    NSSelectInputSource *_source;
    CFFileDescriptorRef _descriptor;
    CFRunLoopSourceRef _runLoopSource;
    NSSelectEventMask _happened;
}
- (void)armForMask:(NSSelectEventMask)mask runLoop:(CFRunLoopRef)runLoop mode:(CFStringRef)mode;
- (NSSelectEventMask)disarmFromRunLoop:(CFRunLoopRef)runLoop mode:(CFStringRef)mode;
@end

static void descriptorCallBack(CFFileDescriptorRef descriptor, CFOptionFlags types, void *info) {
    NSRunLoopDescriptorWatch *watch = info;

    if (types & kCFFileDescriptorReadCallBack) {
        watch->_happened |= NSSelectReadEvent;
    }
    if (types & kCFFileDescriptorWriteCallBack) {
        watch->_happened |= NSSelectWriteEvent;
    }
}

@implementation NSRunLoopDescriptorWatch

- (instancetype)initWithSource:(NSSelectInputSource *)source {
    CFFileDescriptorContext context = { 0, self, NULL, NULL, NULL };

    if ((self = [super init]) == nil) {
        return nil;
    }
    _source = source;
    _descriptor = CFFileDescriptorCreate(kCFAllocatorDefault, [source fileDescriptor], false, descriptorCallBack, &context);
    if (_descriptor == NULL) {
        [self release];
        return nil;
    }
    _runLoopSource = CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault, _descriptor, 0);
    return self;
}

- (void)dealloc {
    if (_runLoopSource != NULL) {
        CFRunLoopSourceInvalidate(_runLoopSource);
        CFRelease(_runLoopSource);
    }
    if (_descriptor != NULL) {
        CFFileDescriptorInvalidate(_descriptor);
        CFRelease(_descriptor);
    }
    [super dealloc];
}

- (void)armForMask:(NSSelectEventMask)mask runLoop:(CFRunLoopRef)runLoop mode:(CFStringRef)mode {
    CFOptionFlags types = 0;

    _happened = 0;
    if (mask & NSSelectReadEvent) {
        types |= kCFFileDescriptorReadCallBack;
    }
    if (mask & NSSelectWriteEvent) {
        types |= kCFFileDescriptorWriteCallBack;
    }
    CFFileDescriptorEnableCallBacks(_descriptor, types);
    CFRunLoopAddSource(runLoop, _runLoopSource, mode);
}

- (NSSelectEventMask)disarmFromRunLoop:(CFRunLoopRef)runLoop mode:(CFStringRef)mode {
    CFRunLoopRemoveSource(runLoop, _runLoopSource, mode);
    CFFileDescriptorDisableCallBacks(_descriptor, kCFFileDescriptorReadCallBack | kCFFileDescriptorWriteCallBack);
    return _happened;
}

@end

@interface NSRunLoopPerform : NSObject {
@public
    SEL selector;
    id target;
    id argument;
    NSArray *modes;
    NSUInteger order;
}
@end

@interface NSTimer (NSRunLoopScheduling)
- (void)_scheduleOnQueue:(dispatch_queue_t)queue;
@end

@implementation NSRunLoopPerform
- (void)dealloc {
    [target release];
    [argument release];
    [modes release];
    [super dealloc];
}
@end

NSRunLoopMode const NSDefaultRunLoopMode = @"kCFRunLoopDefaultMode";
NSRunLoopMode const NSRunLoopCommonModes = @"kCFRunLoopCommonModes";
NSRunLoopMode const NSEventTrackingRunLoopMode = @"NSEventTrackingRunLoopMode";
NSRunLoopMode const NSModalPanelRunLoopMode = @"NSModalPanelRunLoopMode";

// sources are file descriptors as Cocotron's AppKit expects, waited on as CFFileDescriptors in the
// thread's CFRunLoop so CF sources and cross-thread wake-ups end the wait. NSTimer uses libdispatch.

static pthread_key_t runLoopKey;
static pthread_once_t runLoopOnce = PTHREAD_ONCE_INIT;
static NSRunLoop *mainRunLoop = nil;

static void createRunLoopKey(void) {
    pthread_key_create(&runLoopKey, NULL);
}

@implementation NSRunLoop

+ (NSRunLoop *)currentRunLoop {
    pthread_once(&runLoopOnce, createRunLoopKey);

    NSRunLoop *loop = pthread_getspecific(runLoopKey);
    if (loop == nil) {
        loop = [[self alloc] init];
        pthread_setspecific(runLoopKey, loop);
        if (mainRunLoop == nil) {
            mainRunLoop = loop;
        }
    }
    return loop;
}

+ (NSRunLoop *)mainRunLoop {
    if (mainRunLoop == nil) {
        (void)[self currentRunLoop];
    }
    return mainRunLoop;
}

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _modeToSources = [[NSMutableDictionary alloc] init];
    _timers = [[NSMutableArray alloc] init];
    _performs = [[NSMutableArray alloc] init];
    _currentMode = [NSDefaultRunLoopMode copy];
    // made by +currentRunLoop on the thread it serves
    _cfRunLoop = (struct __CFRunLoop *)CFRetain(CFRunLoopGetCurrent());
    // AppKit's tracking and modal modes are common modes, so main-thread performs run during a drag or a modal panel
    CFRunLoopAddCommonMode((CFRunLoopRef)_cfRunLoop, (CFStringRef)NSEventTrackingRunLoopMode);
    CFRunLoopAddCommonMode((CFRunLoopRef)_cfRunLoop, (CFStringRef)NSModalPanelRunLoopMode);
    return self;
}

- (struct __CFRunLoop *)getCFRunLoop {
    return _cfRunLoop;
}

- (void)dealloc {
    [_modeToSources release];
    [_timers release];
    [_performs release];
    [_currentMode release];
    if (_cfRunLoop != NULL)
        CFRelease(_cfRunLoop);
    [super dealloc];
}

- (NSString *)currentMode {
    return _currentMode;
}

- (NSMutableArray *)_sourcesForMode:(NSRunLoopMode)mode {
    NSMutableArray *sources = [_modeToSources objectForKey:mode];

    if (sources == nil) {
        sources = [NSMutableArray array];
        [_modeToSources setObject:sources forKey:mode];
    }
    return sources;
}

- (void)addInputSource:(NSInputSource *)source forMode:(NSRunLoopMode)mode {
    [[self _sourcesForMode:mode] addObject:source];
}

- (void)removeInputSource:(NSInputSource *)source forMode:(NSRunLoopMode)mode {
    [[self _sourcesForMode:mode] removeObjectIdenticalTo:source];
}

// every mode shares one timer: it is armed once, on the queue of this run loop's thread
- (void)addTimer:(NSTimer *)timer forMode:(NSRunLoopMode)mode {
    (void)mode;
    for (NSUInteger i = [_timers count]; i > 0; i--) {
        if (![[_timers objectAtIndex:i - 1] isValid])
            [_timers removeObjectAtIndex:i - 1];
    }
    if (![_timers containsObject:timer])
        [_timers addObject:timer];
    [timer _scheduleOnQueue:self == mainRunLoop ? dispatch_get_main_queue()
                                                : dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
}

- (void)performSelector:(SEL)selector target:(id)target argument:(id)argument
                  order:(NSUInteger)order modes:(NSArray *)modes {
    NSRunLoopPerform *perform = [NSRunLoopPerform new];
    perform->selector = selector;
    perform->target = [target retain];
    perform->argument = [argument retain];
    perform->modes = [modes copy];
    perform->order = order;
    [_performs addObject:perform];
    [perform release];
}

- (void)cancelPerformSelector:(SEL)selector target:(id)target argument:(id)argument {
    for(NSUInteger index = [_performs count]; index > 0; --index) {
        NSRunLoopPerform *perform = [_performs objectAtIndex:index - 1];
        if(perform->selector == selector && perform->target == target &&
           perform->argument == argument)
            [_performs removeObjectAtIndex:index - 1];
    }
}

- (BOOL)_performSelectorsForMode:(NSRunLoopMode)mode {
    BOOL fired = NO;

    for(;;) {
        NSUInteger selected = NSNotFound;
        NSUInteger selectedOrder = (NSUInteger)-1;
        for(NSUInteger index = 0; index < [_performs count]; ++index) {
            NSRunLoopPerform *perform = [_performs objectAtIndex:index];
            if(([perform->modes containsObject:mode] ||
                [perform->modes containsObject:NSRunLoopCommonModes]) &&
               perform->order < selectedOrder) {
                selected = index;
                selectedOrder = perform->order;
            }
        }
        if(selected == NSNotFound)
            break;

        NSRunLoopPerform *perform = [[_performs objectAtIndex:selected] retain];
        [_performs removeObjectAtIndex:selected];
        typedef void (*PerformIMP)(id, SEL, id);
        PerformIMP implementation = (PerformIMP)[perform->target
            methodForSelector:perform->selector];
        implementation(perform->target, perform->selector, perform->argument);
        [perform release];
        fired = YES;
    }
    return fired;
}

// one watch per source and descriptor, a new descriptor on the same source gets a new watch
- (NSRunLoopDescriptorWatch *)_watchForSource:(NSSelectInputSource *)source {
    static char key;
    NSRunLoopDescriptorWatch *watch = objc_getAssociatedObject(source, &key);

    if (watch == nil || CFFileDescriptorGetNativeDescriptor(watch->_descriptor) != [source fileDescriptor]) {
        watch = [[[NSRunLoopDescriptorWatch alloc] initWithSource:source] autorelease];
        objc_setAssociatedObject(source, &key, watch, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return watch;
}

- (NSDate *)limitDateForMode:(NSRunLoopMode)mode {
    return [NSDate distantFuture];
}

- (BOOL)runMode:(NSRunLoopMode)mode beforeDate:(NSDate *)date {
    NSString *previousMode = _currentMode;
    _currentMode = [mode copy];

    // performSelectorOnMainThread: and NSFileHandle queue blocks on this thread's CFRunLoop,
    // which nothing else runs here, so give it one non-blocking pass
    CFRunLoopRunInMode((CFStringRef)mode, 0, true);

    BOOL fired = [self _performSelectorsForMode:mode];
    if(fired) {
        [_currentMode release];
        _currentMode = previousMode;
        return YES;
    }

    // the wait is the CFRunLoop's, so blocks, sources and wake-ups from other threads end it too,
    // the select sources take part as CFFileDescriptors that report what became ready
    NSTimeInterval remaining = (date != nil) ? [date timeIntervalSinceNow] : 0.0;
    if (remaining < 0.0) {
        remaining = 0.0;
    }
    if (remaining > 86400.0) {
        remaining = 86400.0;
    }

    // read now: the pass and the performs above run code that can remove sources from this mode
    NSArray *sources = [[[self _sourcesForMode:mode] copy] autorelease];
    NSMutableArray *watched = [NSMutableArray array];
    for (id source in sources) {
        if (![source isKindOfClass:[NSSelectInputSource class]] || [source fileDescriptor] < 0) {
            continue;
        }
        NSRunLoopDescriptorWatch *watch = [self _watchForSource:source];
        if (watch != nil) {
            [watch armForMask:[source selectEventMask] runLoop:_cfRunLoop mode:(CFStringRef)mode];
            [watched addObject:watch];
        }
    }

    SInt32 result = CFRunLoopRunInMode((CFStringRef)mode, remaining, true);

    for (NSRunLoopDescriptorWatch *watch in watched) {
        NSSelectEventMask happened = [watch disarmFromRunLoop:_cfRunLoop mode:(CFStringRef)mode];

        if (happened != 0) {
            [watch->_source processImmediateEvents:happened];
            fired = YES;
        }
    }
    if ([self _performSelectorsForMode:mode]) {
        fired = YES;
    }

    [_currentMode release];
    _currentMode = previousMode;
    // as on macOS, NO only when the mode has nothing that could end a wait
    return fired || result != kCFRunLoopRunFinished;
}

/* Ports are scheduled through the port itself; see -[NSPort scheduleInRunLoop:]. */
- (void)addPort:(NSPort *)port forMode:(NSRunLoopMode)mode {
    [port scheduleInRunLoop:self forMode:mode];
}

- (void)removePort:(NSPort *)port forMode:(NSRunLoopMode)mode {
    [port removeFromRunLoop:self forMode:mode];
}

- (void)runUntilDate:(NSDate *)date {
    while ([date timeIntervalSinceNow] > 0.0) {
        if (![self runMode:NSDefaultRunLoopMode beforeDate:date]) {
            break;
        }
    }
}

- (void)run {
    [self runUntilDate:[NSDate distantFuture]];
}

@end

// a delayed perform: a one-shot timer that sends SELECTOR to the target with the argument
// both are retained until it fires or is cancelled, and the pending ones are kept for cancelPrevious...
@interface NSDelayedPerform : NSObject {
@public
    id _target;
    SEL _selector;
    id _argument;
    NSTimer *_timer;
}
- (void)fire:(NSTimer *)timer;
@end

static NSMutableArray *pendingPerforms(void) {
    static NSMutableArray *pending = nil;

    if (pending == nil)
        pending = [[NSMutableArray alloc] init];
    return pending;
}

@implementation NSDelayedPerform

- (void)dealloc {
    [_target release];
    [_argument release];
    [super dealloc];
}

- (void)fire:(NSTimer *)timer {
    [self retain];
    @synchronized (pendingPerforms()) {
        [pendingPerforms() removeObjectIdenticalTo:self];
    }
    ((void (*)(id, SEL, id))objc_msgSend)(_target, _selector, _argument);
    [self release];
}

@end

@implementation NSObject (NSDelayedPerforming)

- (void)performSelector:(SEL)selector withObject:(id)object afterDelay:(NSTimeInterval)delay {
    [self performSelector:selector withObject:object afterDelay:delay inModes:nil];
}

// timers here run in every mode, so the modes only matter for cancelling
- (void)performSelector:(SEL)selector withObject:(id)object afterDelay:(NSTimeInterval)delay
                inModes:(NSArray *)modes {
    NSDelayedPerform *perform = [[NSDelayedPerform alloc] init];

    perform->_target = [self retain];
    perform->_selector = selector;
    perform->_argument = [object retain];
    perform->_timer = [NSTimer timerWithTimeInterval:(delay > 0 ? delay : 0)
                                              target:perform
                                            selector:@selector(fire:)
                                            userInfo:nil
                                             repeats:NO];
    @synchronized (pendingPerforms()) {
        [pendingPerforms() addObject:perform];
    }
    [[NSRunLoop currentRunLoop] addTimer:perform->_timer forMode:NSDefaultRunLoopMode];
    [perform release];
}

+ (void)cancelPreviousPerformRequestsWithTarget:(id)target {
    @synchronized (pendingPerforms()) {
        NSMutableArray *pending = pendingPerforms();

        for (NSUInteger i = [pending count]; i > 0; i--) {
            NSDelayedPerform *perform = [pending objectAtIndex:i - 1];

            if (perform->_target != target)
                continue;
            [perform->_timer invalidate];
            [pending removeObjectAtIndex:i - 1];
        }
    }
}

// the argument matches by -isEqual:, as on macOS
+ (void)cancelPreviousPerformRequestsWithTarget:(id)target selector:(SEL)selector object:(id)object {
    @synchronized (pendingPerforms()) {
        NSMutableArray *pending = pendingPerforms();

        for (NSUInteger i = [pending count]; i > 0; i--) {
            NSDelayedPerform *perform = [pending objectAtIndex:i - 1];

            if (perform->_target != target || perform->_selector != selector)
                continue;
            if (perform->_argument != object && ![perform->_argument isEqual:object])
                continue;
            [perform->_timer invalidate];
            [pending removeObjectAtIndex:i - 1];
        }
    }
}

@end
