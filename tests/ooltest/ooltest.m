// ooltest.m - NSMachPort messages with out-of-line data of several sizes, received through the run loop
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/Foundation.h>

@interface Receiver : NSObject <NSMachPortDelegate>
@property int got;
@end

@implementation Receiver
- (void)handlePortMessage:(NSPortMessage *)message {
    NSArray *components = [message components];
    NSMutableString *sizes = [NSMutableString string];
    for (id c in components)
        [sizes appendFormat:@" %lu", (unsigned long)([c isKindOfClass:[NSData class]] ? [c length] : 0)];
    printf("OOL got msgid %u components%s\n", [message msgid], [sizes UTF8String]);
    self.got++;
}
@end

int main(void) {
    @autoreleasepool {
        NSMachPort *port = (NSMachPort *)[NSMachPort port];
        Receiver *receiver = [Receiver new];
        [port setDelegate:receiver];
        [[NSRunLoop currentRunLoop] addPort:port forMode:NSDefaultRunLoopMode];

        NSUInteger sizes[] = { 0, 10, 4096, 5000, 100000, 3 };
        for (int round = 0; round < 3; round++) {
            NSMutableArray *components = [NSMutableArray array];
            for (int i = 0; i < 6; i++)
                [components addObject:[NSMutableData dataWithLength:sizes[i]]];
            NSPortMessage *message = [[NSPortMessage alloc] initWithSendPort:port receivePort:nil components:components];
            [message setMsgid:100 + round];
            BOOL sent = [message sendBeforeDate:[NSDate dateWithTimeIntervalSinceNow:2]];
            printf("OOL sent %d\n", sent);
        }
        NSDate *end = [NSDate dateWithTimeIntervalSinceNow:3];
        while (receiver.got < 3 && [end timeIntervalSinceNow] > 0)
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.2]];
        printf("OOLTEST %s\n", receiver.got == 3 ? "PASS" : "FAIL");
    }
    return 0;
}
