/* Copyright (c) 2007 Christopher J. W. Lloyd
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import "NSAnimationContext.h"
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSThread.h>
#import <QuartzCore/CATransaction.h>

// the open groupings of this thread, innermost last
static NSMutableArray *contextStack(void) {
    NSMutableDictionary *thread = [[NSThread currentThread] threadDictionary];
    NSMutableArray *stack = [thread objectForKey:@"NSAnimationContextStack"];

    if (stack == nil) {
        stack = [NSMutableArray array];
        [thread setObject:stack forKey:@"NSAnimationContextStack"];
    }
    return stack;
}

@implementation NSAnimationContext

- (instancetype)init {
    if ((self = [super init]) != nil)
        _duration = 0.25;
    return self;
}

- (void)dealloc {
    [_timingFunction release];
    [_completionHandler release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

+ (void)beginGrouping {
    NSAnimationContext *context = [[self alloc] init];
    NSAnimationContext *outer = [contextStack() lastObject];

    if (outer != nil)
        context->_duration = outer->_duration;
    [contextStack() addObject:context];
    [context release];
    [CATransaction begin];
}

+ (void)endGrouping {
    NSMutableArray *stack = contextStack();
    NSAnimationContext *context = [[stack lastObject] retain];

    if (context == nil)
        return;
    [stack removeLastObject];
    if (context->_completionHandler != nil)
        [CATransaction setCompletionBlock:context->_completionHandler];
    [CATransaction commit];
    [context release];
}

// outside any grouping there is still a context, as on macOS, whose changes apply to the implicit transaction
+ (NSAnimationContext *)currentContext {
    NSAnimationContext *context = [contextStack() lastObject];
    static NSAnimationContext *implicit;

    if (context != nil)
        return context;
    if (implicit == nil)
        implicit = [[self alloc] init];
    return implicit;
}

+ (void)runAnimationGroup:(void (^)(NSAnimationContext *context))changes completionHandler:(void (^)(void))completionHandler {
    [self beginGrouping];
    [[self currentContext] setCompletionHandler:completionHandler];
    if (changes != nil)
        changes([self currentContext]);
    [self endGrouping];
}

+ (void)runAnimationGroup:(void (^)(NSAnimationContext *context))changes {
    [self runAnimationGroup:changes completionHandler:nil];
}

- (void)setDuration:(NSTimeInterval)duration {
    _duration = duration;
    [CATransaction setAnimationDuration:duration];
}

- (NSTimeInterval)duration {
    return _duration;
}

- (CAMediaTimingFunction *)timingFunction {
    return _timingFunction;
}

- (void)setTimingFunction:(CAMediaTimingFunction *)timingFunction {
    [timingFunction retain];
    [_timingFunction release];
    _timingFunction = timingFunction;
    [CATransaction setAnimationTimingFunction:timingFunction];
}

- (void (^)(void))completionHandler {
    return _completionHandler;
}

- (void)setCompletionHandler:(void (^)(void))completionHandler {
    completionHandler = [completionHandler copy];
    [_completionHandler release];
    _completionHandler = completionHandler;
}

- (BOOL)allowsImplicitAnimation {
    return _allowsImplicitAnimation;
}

- (void)setAllowsImplicitAnimation:(BOOL)allows {
    _allowsImplicitAnimation = allows;
}

@end
