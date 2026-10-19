// CoreLocation.m - a location manager on a system with no location services
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// authorization reads restricted, and starting updates reports kCLErrorDenied to the delegate

#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

typedef double CLLocationAccuracy;
typedef double CLLocationDistance;

const CLLocationAccuracy kCLLocationAccuracyBestForNavigation = -2;
const CLLocationAccuracy kCLLocationAccuracyBest = -1;
const CLLocationAccuracy kCLLocationAccuracyNearestTenMeters = 10;
const CLLocationAccuracy kCLLocationAccuracyHundredMeters = 100;
const CLLocationAccuracy kCLLocationAccuracyKilometer = 1000;
const CLLocationAccuracy kCLLocationAccuracyThreeKilometers = 3000;
const CLLocationDistance kCLDistanceFilterNone = -1;
NSString *const kCLErrorDomain = @"kCLErrorDomain";

enum { kCLErrorLocationUnknown = 0, kCLErrorDenied = 1 };

typedef NS_ENUM(int, CLAuthorizationStatus) {
    kCLAuthorizationStatusNotDetermined = 0,
    kCLAuthorizationStatusRestricted = 1,
    kCLAuthorizationStatusDenied = 2,
    kCLAuthorizationStatusAuthorizedAlways = 3,
};

@interface CLLocationManager : NSObject {
    id _delegate;
    CLLocationAccuracy _accuracy;
    CLLocationDistance _distanceFilter;
}
@end

@interface NSObject (CLLocationManagerDelegate)
- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error;
- (void)locationManager:(CLLocationManager *)manager didChangeAuthorizationStatus:(CLAuthorizationStatus)status;
- (void)locationManagerDidChangeAuthorization:(CLLocationManager *)manager;
@end

@implementation CLLocationManager

+ (BOOL)locationServicesEnabled { return NO; }
+ (CLAuthorizationStatus)authorizationStatus { return kCLAuthorizationStatusRestricted; }
+ (BOOL)significantLocationChangeMonitoringAvailable { return NO; }
+ (BOOL)headingAvailable { return NO; }

- (instancetype)init
{
    if ((self = [super init])) {
        _accuracy = kCLLocationAccuracyBest;
        _distanceFilter = kCLDistanceFilterNone;
    }
    return self;
}

- (id)delegate { return _delegate; }
- (void)setDelegate:(id)delegate { _delegate = delegate; }
- (CLAuthorizationStatus)authorizationStatus { return kCLAuthorizationStatusRestricted; }
- (CLLocationAccuracy)desiredAccuracy { return _accuracy; }
- (void)setDesiredAccuracy:(CLLocationAccuracy)accuracy { _accuracy = accuracy; }
- (CLLocationDistance)distanceFilter { return _distanceFilter; }
- (void)setDistanceFilter:(CLLocationDistance)filter { _distanceFilter = filter; }
- (id)location { return nil; }

// the status is already final, delegates only hear it again
- (void)requestWhenInUseAuthorization
{
    [self retain];
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([_delegate respondsToSelector:@selector(locationManagerDidChangeAuthorization:)])
            [_delegate locationManagerDidChangeAuthorization:self];
        else if ([_delegate respondsToSelector:@selector(locationManager:didChangeAuthorizationStatus:)])
            [_delegate locationManager:self didChangeAuthorizationStatus:kCLAuthorizationStatusRestricted];
        [self release];
    });
}

- (void)requestAlwaysAuthorization { [self requestWhenInUseAuthorization]; }

- (void)startUpdatingLocation
{
    [self retain];
    dispatch_async(dispatch_get_main_queue(), ^{
        NSError *error = [NSError errorWithDomain:kCLErrorDomain code:kCLErrorDenied userInfo:nil];
        if ([_delegate respondsToSelector:@selector(locationManager:didFailWithError:)])
            [_delegate locationManager:self didFailWithError:error];
        [self release];
    });
}

- (void)requestLocation { [self startUpdatingLocation]; }
- (void)stopUpdatingLocation { }
- (void)startUpdatingHeading { }
- (void)stopUpdatingHeading { }
- (void)startMonitoringSignificantLocationChanges { [self startUpdatingLocation]; }
- (void)stopMonitoringSignificantLocationChanges { }

@end
