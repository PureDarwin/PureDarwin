//
//  SecTapToRadarTests.m
//  TrustedPeersTests
//

#import <TargetConditionals.h>

#if !TARGET_OS_BRIDGE

#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import "utilities/SecTapToRadar.h"

@interface SecTapToRadarTests : XCTestCase
@property BOOL isRateLimited;
@property BOOL userSay;

@property bool ttrDidAppear;
@property id mockTTR;

@end

@implementation SecTapToRadarTests

- (void)triggerTapToRadar:(SecTapToRadar *)ttrRequest
{
    self.ttrDidAppear = true;
}

- (BOOL)isRateLimited:(SecTapToRadar *)ttrRequest
{
    return self.isRateLimited;
}

- (BOOL)askUserIfTTR:(SecTapToRadar *)ttrRequest
{
    return self.userSay;
}

- (void)setUp {

    self.ttrDidAppear = NO;
    self.isRateLimited = NO;
    self.userSay = YES;

    self.mockTTR = OCMClassMock([SecTapToRadar class]);
    OCMStub([self.mockTTR triggerTapToRadar:[OCMArg any]]).andCall(self, @selector(triggerTapToRadar:));
    OCMStub([self.mockTTR isRateLimited:[OCMArg any]]).andCall(self, @selector(isRateLimited:));
    OCMStub([self.mockTTR askUserIfTTR:[OCMArg any]]).andCall(self, @selector(askUserIfTTR:));
}

- (void)testSecTTRNormal {

    SecTapToRadar *ttr = [[SecTapToRadar alloc] initTapToRadar:@"alert" description:@"test" radar:@"1"];

    [ttr trigger];
    XCTAssertTrue(self.ttrDidAppear, "should have appeared");
}

- (void)testSecTTRRateLimit {
    SecTapToRadar *ttr = [[SecTapToRadar alloc] initTapToRadar:@"alert" description:@"test" radar:@"1"];

    self.isRateLimited = YES;
    [ttr trigger];
    XCTAssertFalse(self.ttrDidAppear, "should not have appered");
}

- (void)testSecTTRUserSupress {

    SecTapToRadar *ttr = [[SecTapToRadar alloc] initTapToRadar:@"alert" description:@"test" radar:@"1"];

    self.userSay = false;
    [ttr trigger];
    XCTAssertFalse(self.ttrDidAppear, "should not have appered");

    self.userSay = true;
    [ttr trigger];
    XCTAssertTrue(self.ttrDidAppear, "should have appeared");
}

- (void)testSecTTRRateLimiter {

    SecTapToRadar *ttr = [[SecTapToRadar alloc] initTapToRadar:@"alert" description:@"test" radar:@"1"];
    NSString *key = [SecTapToRadar keyname:ttr];
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];

    [ttr clearRetryTimestamp];

    XCTAssertFalse([ttr isRateLimited], @"should not be rate-limited for first request");
    [ttr updateRetryTimestamp];
    XCTAssertTrue([ttr isRateLimited], @"should be rate-limited after first request");

    [ttr clearRetryTimestamp];
    XCTAssertFalse([ttr isRateLimited], @"should not be ratelitmied after clear");

    // check invalid settings
    [defaults setObject:@"invalid" forKey:key];
    XCTAssertNotNil([defaults objectForKey:key], "should have cleared setting");
    XCTAssertFalse([ttr isRateLimited], @"should not be rate-limited if invalid type");
    XCTAssertNil([defaults objectForKey:key], "should have cleared setting");


}


@end

#endif /* TARGET_OS_BRIDGE */
