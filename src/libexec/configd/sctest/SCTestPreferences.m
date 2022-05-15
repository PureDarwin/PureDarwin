/*
 * Copyright (c) 2016, 2017, 2019 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import "SCTest.h"
#import "SCTestUtils.h"
#import <notify.h>
#import <SystemConfiguration/scprefs_observer.h>

@interface SCTestPreferences : SCTest
@property SCPreferencesRef prefs;
@property dispatch_semaphore_t sem;
@property int counter;
@end

@implementation SCTestPreferences

+ (NSString *)command
{
	return @"preferences";
}

+ (NSString *)commandDescription
{
	return @"Tests the SCPreferences code path";
}

- (instancetype)initWithOptions:(NSDictionary *)options
{
	self = [super initWithOptions:options];
	if (self) {
		_prefs = SCPreferencesCreate(kCFAllocatorDefault, CFSTR("SCTest"), NULL);
	}
	return self;
}

- (void)dealloc
{
	if (self.prefs != NULL) {
		CFRelease(self.prefs);
		self.prefs = NULL;
	}
}

- (void)start
{
	if (self.options[kSCTestPreferencesServiceList]) {
		NSDictionary *services = (__bridge NSDictionary *)SCPreferencesGetValue(self.prefs, kSCPrefNetworkServices);
		if (services != nil) {
			[self printNetworkServicesFromDict:services];
		} else {
			SCTestLog("No services present!");
		}
	}

	if (self.options[kSCTestPreferencesServiceOrder]) {
		SCNetworkSetRef set = SCNetworkSetCopyCurrent(self.prefs);
		NSArray *serviceID = (__bridge NSArray *)SCNetworkSetGetServiceOrder(set);
		NSDictionary *services = (__bridge NSDictionary *)SCPreferencesGetValue(self.prefs, kSCPrefNetworkServices);
		int counter = 1;
		SCTestLog("Network service order");
		for (NSString *key in serviceID) {
			NSDictionary *dict = [services objectForKey:key];
			SCTestLog("\n%d: %@\n\tUserDefinedName: %@", counter++, key, [dict objectForKey:(__bridge NSString *)kSCPropNetServiceUserDefinedName]);
		}
		CFRelease(set);
	}

	[self cleanupAndExitWithErrorCode:0];
}

- (void)printNetworkServicesFromDict:(NSDictionary *)serviceDict
{
	int counter = 1;
	SCTestLog("Network Services");
	for (NSString *key in serviceDict) {
		NSDictionary *dict = [serviceDict objectForKey:key];
		SCTestLog("\n%d: %@\n\tUserDefinedName: %@", counter++, key, [dict objectForKey:(__bridge NSString *)kSCPropNetServiceUserDefinedName]);
	}
}

- (BOOL)unitTest
{
	BOOL allUnitTestsPassed = YES;
	allUnitTestsPassed &= [self unitTestNetworkServicesSanity];
	allUnitTestsPassed &= [self unitTestPreferencesAPI];
	allUnitTestsPassed &= [self unitTestPreferencesNotifications];
	allUnitTestsPassed &= [self unitTestPreferencesObserver];
	allUnitTestsPassed &= [self unitTestPreferencesSession];
	return  allUnitTestsPassed;

}

- (BOOL)unitTestNetworkServicesSanity
{
	// We verify that every service has a unique name, an interface, an IPv4 config method and and IPv6 config method.
	NSArray *sets;
	NSDictionary *services;
	SCTestPreferences *test;

	test = [[SCTestPreferences alloc] initWithOptions:self.options];

	sets = (__bridge_transfer NSArray *)SCNetworkSetCopyAll(test.prefs);
	if (sets == nil || [sets count] == 0) {
		SCTestLog("No sets present!");
		return NO;
	}

	services = (__bridge NSDictionary *)SCPreferencesGetValue(test.prefs, kSCPrefNetworkServices);
	if (services == nil || [services count] == 0) {
		SCTestLog("No services present!");
		return NO;
	}

	for (id setPtr in sets) {
		SCNetworkSetRef set = (__bridge SCNetworkSetRef)setPtr;
		NSArray *serviceArray = nil;
		NSMutableArray *serviceNameArray = nil;
		NSString *setID;

		setID = (__bridge NSString *)SCNetworkSetGetSetID(set);

		serviceArray = (__bridge_transfer NSArray *)SCNetworkSetCopyServices(set);
		if (serviceArray == nil) {
			SCTestLog("No services in set %@!", setID);
			continue;
		}

		serviceNameArray = [[NSMutableArray alloc] init];
		for (id servicePTR in serviceArray) {
			NSDictionary *serviceDict;
			NSDictionary *ipv4Dict;
			NSDictionary *ipv6Dict;
			NSDictionary *ipv4ProtocolConfig;
			NSDictionary *ipv6ProtocolConfig;
			NSString *serviceName;
			NSString *serviceID;
			NSString *interfaceType;
			SCNetworkServiceRef service;
			SCNetworkInterfaceRef interface;
			SCNetworkProtocolRef ipv4Protocol;
			SCNetworkProtocolRef ipv6Protocol;


			service = (__bridge SCNetworkServiceRef)servicePTR;
			serviceID = (__bridge NSString *)SCNetworkServiceGetServiceID(service);

			serviceDict = [services objectForKey:serviceID];
			if (![serviceDict isKindOfClass:[NSDictionary class]]) {
				SCTestLog("Service is not a dictionary");
				return NO;
			}

			serviceName = (__bridge NSString *)SCNetworkServiceGetName(service);
			if (serviceName != nil) {
				// Check if the name is unique
				BOOL namePresent = [serviceNameArray containsObject:serviceName];
				if (!namePresent) {
					[serviceNameArray addObject:serviceName];
				} else {
					SCTestLog("Duplicate services with name %@ exist", serviceName);
					return NO;
				}
			} else {
				SCTestLog("Service ID %@ does not have a name", serviceID);
				return NO;
			}

			interface = SCNetworkServiceGetInterface(service);
			if (interface == nil) {
				SCTestLog("Service %@ does not have an interface", serviceName);
				return NO;
			}

			interfaceType = (__bridge NSString *)SCNetworkInterfaceGetInterfaceType(interface);
			if (interfaceType == nil || [interfaceType length] == 0) {
				SCTestLog("Service %@ does not have an interface type", serviceName);
				return NO;
			}
#if TARGET_OS_IPHONE
			if ([interfaceType containsString:@"CommCenter"]) {
				// CommCenter services typically do not have an ipv4/v6 data OR config method. Skip such services.
				continue;
			}
#endif // TARGET_OS_IPHONE
			ipv4Protocol = SCNetworkServiceCopyProtocol(service, kSCNetworkProtocolTypeIPv4);
			ipv6Protocol = SCNetworkServiceCopyProtocol(service, kSCNetworkProtocolTypeIPv6);

			if (ipv4Protocol != NULL) {
				ipv4ProtocolConfig = (__bridge NSDictionary *)SCNetworkProtocolGetConfiguration(ipv4Protocol);
				if (ipv4ProtocolConfig != nil) {
					ipv4Dict = [ipv4ProtocolConfig copy];
				}
				CFRelease(ipv4Protocol);
			}

			if (ipv6Protocol != NULL) {
				ipv6ProtocolConfig = (__bridge NSDictionary *)SCNetworkProtocolGetConfiguration(ipv6Protocol);
				if (ipv6ProtocolConfig != nil) {
					ipv6Dict = [ipv6ProtocolConfig copy];
				}
				CFRelease(ipv6Protocol);
			}

			// Check that we have at least one IP config method
			if (ipv4Dict == nil && ipv6Dict == nil) {
				SCTestLog("Service %@ does not have an IP dictionary", serviceName);
				return NO;
			}

			if ([ipv4Dict objectForKey:(__bridge NSString *)kSCPropNetIPv4ConfigMethod] == nil &&
			    [ipv6Dict objectForKey:(__bridge NSString *)kSCPropNetIPv6ConfigMethod] == nil) {
				SCTestLog("Service %@ does not have an IP Config Method", serviceName);
				return NO;
			}
		}
	}

	SCTestLog("Verified that the Network Services have valid configurations");

	return YES;
}

static void
myNotificationsCallback(SCPreferencesRef prefs, SCPreferencesNotification notificationType, void *ctx)
{
#pragma unused(prefs)
#pragma unused(notificationType)
	SCTestPreferences *test = (__bridge SCTestPreferences *)ctx;
	test.counter++;
	if (test.sem != NULL) {
		dispatch_semaphore_signal(test.sem);
	}
}

- (BOOL)unitTestPreferencesNotifications
{
	dispatch_queue_t	callbackQ;
	const int		iterations	= 10;
	BOOL			ok		= FALSE;
	SCTestPreferences	*test;
	const int		timeout		= 1;	// second

	test = [[SCTestPreferences alloc] initWithOptions:self.options];
	if (test.prefs != NULL) {
		CFRelease(test.prefs);
		test.prefs = NULL;
	}

	test.sem = dispatch_semaphore_create(0);

	SCPreferencesContext ctx = {0, (__bridge void * _Nullable)(test), CFRetain, CFRelease, NULL};
	NSDictionary *prefsOptions = @{(__bridge NSString *)kSCPreferencesOptionRemoveWhenEmpty:(__bridge NSNumber *)kCFBooleanTrue};
	test.prefs = SCPreferencesCreateWithOptions(NULL,
						    CFSTR("SCTest"),
						    CFSTR("SCTestPreferences.plist"),
						    kSCPreferencesUseEntitlementAuthorization,
						    (__bridge CFDictionaryRef)prefsOptions);
	if (test.prefs == NULL) {
		SCTestLog("Failed to create SCPreferences. Error: %s", SCErrorString(SCError()));
		ok = FALSE;
		goto done;
	}

	ok = SCPreferencesSetCallback(test.prefs, myNotificationsCallback, &ctx);
	if (!ok) {
		SCTestLog("Failed to set callback. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	callbackQ = dispatch_queue_create("SCTestPreferences callback queue", NULL);
	ok = SCPreferencesSetDispatchQueue(test.prefs, callbackQ);
	if (!ok) {
		SCTestLog("Failed to set dispatch queue. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	for (int i = 0; i < iterations; i++) {
		NSUUID *uuid = [NSUUID UUID];
		ok = SCPreferencesSetValue(test.prefs, CFSTR("test"), (__bridge CFStringRef)uuid.UUIDString);
		if (!ok) {
			SCTestLog("Failed to set value. Error: %s", SCErrorString(SCError()));
			goto done;
		}
		ok = SCPreferencesCommitChanges(test.prefs);
		if (!ok) {
			SCTestLog("Failed to commit change. Error: %s", SCErrorString(SCError()));
			goto done;
		}
		if (dispatch_semaphore_wait(test.sem, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_SEC))) {
			SCTestLog("Failed to get SCPreferences notification callback: #%d", i);
			ok = FALSE;
			goto done;
		}
	}

	ok = SCPreferencesRemoveValue(test.prefs, CFSTR("test"));
	if (!ok) {
		SCTestLog("Failed to remove value. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	ok = SCPreferencesCommitChanges(test.prefs);
	if (!ok) {
		SCTestLog("Failed to commit change. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	if (dispatch_semaphore_wait(test.sem, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_SEC))) {
		SCTestLog("Failed to get SCPreferences notification callback: cleanup");
		ok = FALSE;
		goto done;
	}

	ok = SCPreferencesSetDispatchQueue(test.prefs, NULL);
	if (!ok) {
		SCTestLog("Failed to clear dispatch queue. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	SCTestLog("Verified that %d SCPreferences notification callbacks were delivered", iterations);

    done :

	CFRelease(test.prefs);
	test.prefs = NULL;

	return ok;
}

#define	PREFS_OBSERVER_DOMAIN		"com.apple.sctest"
#define	PREFS_OBSERVER_PLIST		PREFS_OBSERVER_DOMAIN ".plist"
#define	MANAGED_PREFERENCES_PATH	"/Library/Managed Preferences"
#if	!TARGET_OS_IPHONE
#define	PREFS_OBSERVER_KEY		"com.apple.MCX._managementStatusChangedForDomains"
#define PREFS_OBSERVER_TYPE		scprefs_observer_type_mcx
#define MANAGED_PREFERENCES_USER	kCFPreferencesAnyUser
#else
#define	PREFS_OBSERVER_KEY		"com.apple.ManagedConfiguration.profileListChanged"
#define PREFS_OBSERVER_TYPE		scprefs_observer_type_global
#define	MANAGED_PREFERENCES_MOBILE_PATH	MANAGED_PREFERENCES_PATH "/mobile"
#define MANAGED_PREFERENCES_USER	CFSTR("mobile")
#endif

#import <CoreFoundation/CFPreferences_Private.h>

- (BOOL)unitTestPreferencesObserver
{
	dispatch_queue_t	callbackQ;
	const int		iterations	= 10;
	scprefs_observer_t	observer;
	Boolean			ok		= FALSE;
	SCTestPreferences	*test;
	const int		timeout		= 1;	// second

	test = [[SCTestPreferences alloc] initWithOptions:self.options];
	if (test.prefs != NULL) {
		CFRelease(test.prefs);
		test.prefs = NULL;
	}

	test.sem = dispatch_semaphore_create(0);

	callbackQ = dispatch_queue_create("SCTestPreferences callback queue", NULL);
	observer = _scprefs_observer_watch(PREFS_OBSERVER_TYPE, PREFS_OBSERVER_PLIST, callbackQ, ^{
		test.counter++;
		if (test.sem != NULL) {
			dispatch_semaphore_signal(test.sem);
		}
	});

	NSDictionary *prefsOptions = @{(__bridge NSString *)kSCPreferencesOptionRemoveWhenEmpty:(__bridge NSNumber *)kCFBooleanTrue};
	test.prefs = SCPreferencesCreateWithOptions(NULL,
						    CFSTR("SCTest"),
						    CFSTR(MANAGED_PREFERENCES_PATH "/" PREFS_OBSERVER_PLIST),
						    NULL,
						    (__bridge CFDictionaryRef)prefsOptions);
	if (test.prefs == NULL) {
		SCTestLog("Failed to create SCPreferences. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	// let's make sure that the "/Library/Managed Configuration[/mobile]" directory exists
	mkdir(MANAGED_PREFERENCES_PATH, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
#if	TARGET_OS_IPHONE
	mkdir(MANAGED_PREFERENCES_MOBILE_PATH, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
#endif	// TARGET_OS_IPHONE

	for (int i = 0; i < iterations; i++) {
		// update prefs
		NSUUID *uuid = [NSUUID UUID];
		NSDictionary *keysAndValues = @{@"test" : uuid.UUIDString};
		ok = _CFPreferencesWriteManagedDomain((__bridge CFDictionaryRef)keysAndValues,
						      MANAGED_PREFERENCES_USER,
						      FALSE,
						      CFSTR(PREFS_OBSERVER_DOMAIN));
		if (!ok) {
			SCTestLog("Failed to write (update) managed preferences");
			goto done;
		}
#if	TARGET_OS_IPHONE
		notify_post(PREFS_OBSERVER_KEY);
#endif	// TARGET_OS_IPHONE
		if (dispatch_semaphore_wait(test.sem, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_SEC))) {
			SCTestLog("Failed to get SCPreferences observer callback: #%d", i);
			ok = FALSE;
			goto done;
		}
	}

	// post w/no changes
	notify_post(PREFS_OBSERVER_KEY);
	[test waitFor:0.01];		// delay after unrelated change

	// zap prefs
	ok = _CFPreferencesWriteManagedDomain((__bridge CFDictionaryRef)[NSDictionary dictionary],
					      MANAGED_PREFERENCES_USER,
					      FALSE,
					      CFSTR(PREFS_OBSERVER_DOMAIN));
	if (!ok) {
		SCTestLog("Failed to write (remove) managed preferences");
		goto done;
	}
#if	TARGET_OS_IPHONE
	notify_post(PREFS_OBSERVER_KEY);
#endif	// TARGET_OS_IPHONE
	if (dispatch_semaphore_wait(test.sem, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_SEC))) {
		SCTestLog("Failed to get SCPreferences observer callback: cleanup");
		ok = FALSE;
		goto done;
	}

	SCTestLog("Verified that %d CF/SCPreferences observer callbacks were delivered", iterations);

    done :

	_scprefs_observer_cancel(observer);

	// cleanup the "/Library/Managed Configuration[/mobile]" directory
#if	TARGET_OS_IPHONE
	rmdir(MANAGED_PREFERENCES_MOBILE_PATH);
#endif	// TARGET_OS_IPHONE
	rmdir(MANAGED_PREFERENCES_PATH);

	return ok;
}

- (BOOL)unitTestPreferencesSession
{
	SCPreferencesRef prefs;

	prefs = SCPreferencesCreate(kCFAllocatorDefault, CFSTR("SCTest"), NULL);
	if (prefs == NULL) {
		SCTestLog("Failed to create SCPreferences. Error: %s", SCErrorString(SCError()));
		return NO;
	}
	CFRelease(prefs);

	prefs = SCPreferencesCreateWithOptions(kCFAllocatorDefault, CFSTR("SCTest"), NULL, kSCPreferencesUseEntitlementAuthorization, NULL);
	if (prefs == NULL) {
		SCTestLog("Failed to create SCPreferences w/options. Error: %s", SCErrorString(SCError()));
		return NO;
	}
	CFRelease(prefs);

	prefs = SCPreferencesCreateWithAuthorization(kCFAllocatorDefault, CFSTR("SCTest"), NULL, kSCPreferencesUseEntitlementAuthorization);
	if (prefs == NULL) {
		SCTestLog("Failed to create SCPreferences w/options. Error: %s", SCErrorString(SCError()));
		return NO;
	}
	CFRelease(prefs);

	SCTestLog("Verified that the preferences session can be created");
	return YES;
}

- (BOOL)unitTestPreferencesAPI
{
	BOOL ok = NO;
	int iterations = 100;
	NSDictionary *prefsOptions;
	NSMutableArray *keys;
	NSMutableArray *values;
	SCTestPreferences *test;
	NSArray *keyList;

	test = [[SCTestPreferences alloc] initWithOptions:self.options];
	if (test.prefs != NULL) {
		CFRelease(test.prefs);
		test.prefs = NULL;
	}

	prefsOptions = @{(__bridge NSString *)kSCPreferencesOptionRemoveWhenEmpty:(__bridge NSNumber *)kCFBooleanTrue};
	test.prefs = SCPreferencesCreateWithOptions(kCFAllocatorDefault,
						    CFSTR("SCTest"),
						    CFSTR("SCTestPreferences.plist"),
						    kSCPreferencesUseEntitlementAuthorization,
						    (__bridge CFDictionaryRef)prefsOptions);
	if (test.prefs == NULL) {
		SCTestLog("Failed to create a preferences session. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	keys = [[NSMutableArray alloc] init];
	values = [[NSMutableArray alloc] init];
	for (int i = 0; i < iterations; i++) {
		NSUUID *uuidKey = [NSUUID UUID];
		NSUUID *uuidValue = [NSUUID UUID];

		ok = SCPreferencesLock(test.prefs, true);
		if (!ok) {
			SCTestLog("Failed to get preferences lock. Error: %s", SCErrorString(SCError()));
			return NO;
		}

		ok = SCPreferencesSetValue(test.prefs, (__bridge CFStringRef)uuidKey.UUIDString, (__bridge CFStringRef)uuidValue.UUIDString);
		if (!ok) {
			SCTestLog("Failed to set preferences value. Error: %s", SCErrorString(SCError()));
			return NO;
		}

		ok = SCPreferencesUnlock(test.prefs);
		if (!ok) {
			SCTestLog("Failed to release preferences lock. Error: %s", SCErrorString(SCError()));
			return NO;
		}

		[keys addObject:uuidKey.UUIDString];
		[values addObject:uuidValue.UUIDString];
	}

	ok = SCPreferencesCommitChanges(test.prefs);
	if (!ok) {
		SCTestLog("Failed to commit preferences. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	CFRelease(test.prefs);
	test.prefs = SCPreferencesCreateWithOptions(kCFAllocatorDefault,
						    CFSTR("SCTest"),
						    CFSTR("SCTestPreferences.plist"),
						    kSCPreferencesUseEntitlementAuthorization,
						    (__bridge CFDictionaryRef)prefsOptions);
	if (test.prefs == NULL) {
		SCTestLog("Failed to create a preferences session. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	keyList = (__bridge_transfer NSArray *)SCPreferencesCopyKeyList(test.prefs);
	if ([keyList count] < [keys count]) {
		SCTestLog("Failed to copy all keys from preferences. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	for (NSString *key in keys) {
		NSString *valueString = (__bridge NSString *)SCPreferencesGetValue(test.prefs, (__bridge CFStringRef)key);
		if (!valueString) {
			SCTestLog("Failed to get value from preferences. Error: %s", SCErrorString(SCError()));
			return NO;
		}

		BOOL ok = [values containsObject:valueString];
		if (!ok) {
			SCTestLog("Incorrect value fetched from preferences");
			return NO;
		}
	}

	ok = SCPreferencesRemoveAllValues(test.prefs);
	if (!ok) {
		SCTestLog("Failed to remove values  preferences. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	ok = SCPreferencesCommitChanges(test.prefs);
	if (!ok) {
		SCTestLog("Failed to commit preferences. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	CFRelease(test.prefs);
	test.prefs = SCPreferencesCreateWithOptions(kCFAllocatorDefault,
						    CFSTR("SCTest"),
						    CFSTR("SCTestPreferences.plist"),
						    kSCPreferencesUseEntitlementAuthorization,
						    (__bridge CFDictionaryRef)prefsOptions);
	if (test.prefs == NULL) {
		SCTestLog("Failed to create a preferences session. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	keyList = (__bridge_transfer NSArray *)SCPreferencesCopyKeyList(test.prefs);
	if ([keyList count] > 0) {
		SCTestLog("Failed to remove all keys from preferences. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	SCTestLog("Verified that SCPreferences APIs behave as expected");
	return ok;
}

- (void)cleanupAndExitWithErrorCode:(int)error
{
	[super cleanupAndExitWithErrorCode:error];
}

@end
