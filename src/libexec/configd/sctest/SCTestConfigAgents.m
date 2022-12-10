/*
 * Copyright (c) 2016, 2017, 2019, 2021 Apple Inc. All rights reserved.
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
#import <Network/Network_Private.h>
#import <CoreFoundation/CFXPCBridge.h>
#import <config_agent_info.h>

#define TEST_DOMAIN	"apple.biz"

@interface SCTestConfigAgent : SCTest
@property NSString *serviceID;
@property NSString *proxyKey;
@property NSArray<NSArray<NSDictionary *> *> *testProxy;
@property (copy) NSArray<NSArray<NSDictionary *> *> *pathProxy;
@property NSString *dnsKey;
@property NSArray<NWEndpoint *> *testDNS;
@property (copy) NSArray<NWEndpoint *> *pathDNS;
@property (atomic, retain) dispatch_semaphore_t doneOrTimeoutCondition;
@property (atomic) timerInfo *doneOrTimeoutElapsed;
@end

@implementation SCTestConfigAgent

- (instancetype)initWithOptions:(NSDictionary *)options
{
	self = [super initWithOptions:options];
	if (self) {
		_serviceID = @"8F66B505-EAEF-4611-BD4D-C523FD9451F0";
		_proxyKey = (__bridge_transfer NSString *)[self copyStateKeyWithServiceID:(__bridge CFStringRef)(self.serviceID) forEntity:kSCEntNetProxies];
		_dnsKey = (__bridge_transfer NSString *)[self copyStateKeyWithServiceID:(__bridge CFStringRef)(self.serviceID) forEntity:kSCEntNetDNS];
	}
	return self;
}

- (void)dealloc
{
	if (self.doneOrTimeoutElapsed != nil) {
		free(self.doneOrTimeoutElapsed);
	}
}

+ (NSString *)command
{
	return @"config_agent";
}

+ (NSString *)commandDescription
{
	return @"Tests the Config Agent code path";
}

- (void)start
{
	if (self.options[kSCTestConfigAgentRemoveProxy]) {
		[self removeFromSCDynamicStore:self.proxyKey];
	}

	if (self.options[kSCTestConfigAgentRemoveDNS]) {
		[self removeFromSCDynamicStore:self.dnsKey];
	}

	NSDictionary *proxyConfig = [self parseProxyAgentOptions];
	if (proxyConfig != nil) {
		[self publishToSCDynamicStore:self.proxyKey value:proxyConfig];
		self.testProxy = @[@[proxyConfig]];
	}

	NSDictionary *dnsConfig = [self parseDNSAgentOptions];
	if (dnsConfig != nil) {
		[self publishToSCDynamicStore:self.dnsKey value:dnsConfig];
		self.testDNS = [self createDNSArray:dnsConfig];
	}

	[self cleanupAndExitWithErrorCode:0];
}

- (NWPathEvaluator *)pathEvaluatorWithHostname:(nonnull NSString *)hostname
					  port:(nonnull NSString *)port
{
	NWHostEndpoint	*endpoint;
	NWPathEvaluator	*pathEvaluator;

	endpoint = [NWHostEndpoint endpointWithHostname:hostname port:port];
	pathEvaluator = [[NWPathEvaluator alloc] initWithEndpoint:endpoint parameters:NULL];
	[pathEvaluator addObserver:self
			forKeyPath:@"path"
			   options:(NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew)
			   context:nil];

	return pathEvaluator;
}

- (void)startPathChange
{
	if (self.doneOrTimeoutCondition == nil) {
		self.doneOrTimeoutCondition = dispatch_semaphore_create(0);
	}

	if (self.doneOrTimeoutElapsed == nil) {
		self.doneOrTimeoutElapsed = malloc(sizeof(timerInfo));
	}
	timerStart(self.doneOrTimeoutElapsed);
}

- (BOOL)waitForPathChange
{
	BOOL	changed	= NO;
	long	status;

	status = dispatch_semaphore_wait(self.doneOrTimeoutCondition,
					 dispatch_time(DISPATCH_TIME_NOW, (uint64_t)(30 * NSEC_PER_SEC)));
	if (status == 0) {
		changed = YES;
	}

	timerEnd(self.doneOrTimeoutElapsed);

	return changed;
}

- (CFStringRef)copyStateKeyWithServiceID:(CFStringRef)serviceID
			       forEntity:(CFStringRef)entity
{
	return SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault,
							   kSCDynamicStoreDomainState,
							   serviceID,
							   entity);
}

- (NSArray<NWEndpoint *> *)createDNSArray:(NSDictionary *)dnsConfig
{
	NSArray<NSString *> *dnsServers;
	NSMutableArray<NWEndpoint *> *dnsArray;

	dnsServers = [dnsConfig objectForKey:(__bridge NSString *)kSCPropNetDNSServerAddresses];
	if (dnsServers == nil || [dnsServers count] == 0) {
		return nil;
	}

	dnsArray = [[NSMutableArray alloc] init];
	for (NSString *server in dnsServers) {
		NWEndpoint *endpoint = (NWEndpoint *)[NWAddressEndpoint endpointWithHostname:server port:@"0"];
		[dnsArray addObject:endpoint];
	}

	return dnsArray;
}

- (void)publishToSCDynamicStore:(NSString *)key
			  value:(NSDictionary *)value
{

	BOOL ok = SCDynamicStoreSetValue(NULL, (__bridge CFStringRef)key, (__bridge CFPropertyListRef _Nonnull)(value));
	if (!ok) {
		int error = SCError();
		if (error == kSCStatusNoKey) {
			return;
		}
		SCTestLog("Could not set SCDynamicStore key: %@, Error: %s", key, SCErrorString(error));
		return;
	}
}

- (void)removeFromSCDynamicStore:(NSString *)key
{
	BOOL ok = SCDynamicStoreRemoveValue(NULL, (__bridge CFStringRef)key);
	if (!ok) {
		int error = SCError();
		if (error == kSCStatusNoKey) {
			return;
		}
		SCTestLog("Could not remove SCDynamicStore key: %@, Error: %s", key, SCErrorString(error));
		return;
	}
}

- (NSDictionary *)parseProxyAgentOptions
{
	NSMutableDictionary *proxyConfig = [[NSMutableDictionary alloc] init];

#define NS_NUMBER(x) [NSNumber numberWithInt:x]

#define SET_PROXY_CONFIG(proxyType)														\
	do {																	\
		if (self.options[kSCTestConfigAgent ## proxyType ## Proxy] != nil) {								\
			NSString *serverAndPortString = self.options[kSCTestConfigAgent ## proxyType ## Proxy];					\
			NSArray<NSString *> *serverAndPortArray = [serverAndPortString componentsSeparatedByString:@":"];			\
			if ([serverAndPortArray count] != 2) {											\
				SCTestLog("server address or port missing");									\
				ERR_EXIT;													\
			}															\
			NSString *server = [serverAndPortArray objectAtIndex:0];								\
			NSString *port = [serverAndPortArray objectAtIndex:1];									\
			[proxyConfig setObject:server forKey:(__bridge NSString *)kSCPropNetProxies ## proxyType ## Proxy];			\
			[proxyConfig setObject:NS_NUMBER(port.intValue) forKey:(__bridge NSString *)kSCPropNetProxies ## proxyType ## Port];	\
			[proxyConfig setObject:NS_NUMBER(1) forKey:(__bridge NSString *)kSCPropNetProxies ## proxyType ## Enable];		\
		}																\
	} while(0);

	SET_PROXY_CONFIG(HTTP);
	SET_PROXY_CONFIG(HTTPS);
	SET_PROXY_CONFIG(FTP);
	SET_PROXY_CONFIG(Gopher);
	SET_PROXY_CONFIG(SOCKS);

	if ([proxyConfig count] > 0) {

		NSArray<NSString *> *domains = nil;
		NSString *matchDomains = self.options[kSCTestConfigAgentProxyMatchDomain];
		if (matchDomains == nil) {
			domains = @[@TEST_DOMAIN];
		} else {
			domains = [matchDomains componentsSeparatedByString:@","];
		}

		[proxyConfig setObject:domains forKey:(__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains];
	} else {
		proxyConfig = nil;
	}

	return proxyConfig;
#undef SET_PROXY_CONFIG
}

- (NSDictionary *)parseDNSAgentOptions
{
	NSMutableDictionary *dnsConfig;
	NSString *dnsServerString;
	NSString *dnsDomainString;
	NSArray<NSString *> *dnsServers;
	NSArray<NSString *> *dnsDomains;

	dnsConfig = [[NSMutableDictionary alloc] init];
	dnsServerString = self.options[kSCTestConfigAgentDNSServers];
	if (dnsServerString == nil) {
		return nil;
	}

	dnsDomainString = self.options[kSCTestConfigAgentDNSDomains];
	if (dnsDomainString == nil) {
		dnsDomainString = @TEST_DOMAIN;
	}

	dnsServers = [dnsServerString componentsSeparatedByString:@","];
	[dnsConfig setObject:dnsServers forKey:(__bridge NSString *)kSCPropNetDNSServerAddresses];

	dnsDomains = [dnsDomainString componentsSeparatedByString:@","];
	[dnsConfig setObject:dnsDomains forKey:(__bridge NSString *)kSCPropNetDNSSupplementalMatchDomains];

	return dnsConfig;
}

- (void)cleanupAndExitWithErrorCode:(int)error
{
	[super cleanupAndExitWithErrorCode:error];
}

- (BOOL)setup
{
	return YES;
}

- (BOOL)unitTest
{
	if(![self setup]) {
		return NO;
	}

	BOOL allUnitTestsPassed = YES;
	allUnitTestsPassed &= [self unitTestInstallProxy];
	allUnitTestsPassed &= [self unitTestInstallProxyWithLargeConfig];
	allUnitTestsPassed &= [self unitTestInstallProxyWithConflictingDomain];
	allUnitTestsPassed &= [self unitTestInstallDNS];
	allUnitTestsPassed &= [self unitTestInstallDNSWithConflictingDomain];

	if(![self tearDown]) {
		return NO;
	}

	return allUnitTestsPassed;
}

- (BOOL)unitTestInstallProxy
{
	BOOL changed;
	BOOL success = NO;
	SCTestConfigAgent *test;
	NSDictionary *proxyConfig;
	NSString *hostname;
	NSNumber *port;
	NWPathEvaluator *pathEvaluator;
	NSMutableDictionary *dict;

	test = [[SCTestConfigAgent alloc] initWithOptions:self.options];
	proxyConfig = [test parseProxyAgentOptions];
	if (proxyConfig == nil) {
		// Use default options
		proxyConfig = @{(__bridge NSString *)kSCPropNetProxiesHTTPEnable:@(1),
				(__bridge NSString *)kSCPropNetProxiesHTTPPort:@(80),
				(__bridge NSString *)kSCPropNetProxiesHTTPProxy:@"10.10.10.100",
				(__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains:@[@TEST_DOMAIN],
				};
	}

	hostname = [[proxyConfig objectForKey:(__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains] objectAtIndex:0];
	port = [proxyConfig objectForKey:(__bridge NSString *)kSCPropNetProxiesHTTPPort];
	pathEvaluator = [test pathEvaluatorWithHostname:hostname port:port.stringValue];

	do {
		// ==================================================

		[test startPathChange];

		dict = [NSMutableDictionary dictionaryWithDictionary:proxyConfig];
		[dict removeObjectForKey:(__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains];
		test.testProxy = @[@[dict]];
		[test publishToSCDynamicStore:test.proxyKey value:proxyConfig];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, change not applied after %@s. Test: %@",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed),
				  test.testProxy);
			[test removeFromSCDynamicStore:test.dnsKey];
			break;
		}

		if (![test.testProxy isEqualToArray:test.pathProxy]) {
			SCTestLog("test proxy and applied proxy do not match. Test: %@, Applied: %@",
				  test.testProxy,
				  test.pathProxy);
			[test removeFromSCDynamicStore:test.proxyKey];
			break;
		}

		// ==================================================

		[test startPathChange];

		[test removeFromSCDynamicStore:test.proxyKey];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, cleanup not applied after %@s.",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed));
			break;
		}

		// ==================================================

		SCTestLog("Verified the configured proxy is the same as applied proxy");

		success = YES;
	} while(0);

	[pathEvaluator removeObserver:test
			   forKeyPath:@"path"];

	return success;
}

- (BOOL)unitTestInstallDNS
{
	BOOL changed;
	BOOL success = NO;
	SCTestConfigAgent *test;
	NSDictionary *dnsConfig;
	NSString *hostname;
	NWPathEvaluator *pathEvaluator;

	test = [[SCTestConfigAgent alloc] initWithOptions:self.options];
	dnsConfig = [test parseDNSAgentOptions];
	if (dnsConfig == nil) {
		dnsConfig = @{	(__bridge NSString *)kSCPropNetDNSServerAddresses:@[@"10.10.10.101", @"10.10.10.102", @"10.10.10.103"],
				(__bridge NSString *)kSCPropNetDNSSupplementalMatchDomains:@[@TEST_DOMAIN],
				};
	}

	hostname = [[dnsConfig objectForKey:(__bridge NSString *)kSCPropNetDNSSupplementalMatchDomains] objectAtIndex:0];
	pathEvaluator = [test pathEvaluatorWithHostname:hostname port:@"80"];

	do {
		// ==================================================

		[test startPathChange];

		test.testDNS = [test createDNSArray:dnsConfig];
		[test publishToSCDynamicStore:test.dnsKey value:dnsConfig];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, change not applied after %@s. Test: %@",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed),
				  test.testDNS);
			[test removeFromSCDynamicStore:test.dnsKey];
			break;
		}

		if (![test.testDNS isEqualToArray:test.pathDNS]) {
			SCTestLog("test DNS and applied DNS do not match. Test: %@, Applied: %@",
				  test.testDNS,
				  test.pathDNS);
			[test removeFromSCDynamicStore:test.dnsKey];
			break;
		}

		// ==================================================

		[test startPathChange];

		[test removeFromSCDynamicStore:test.dnsKey];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, cleanup not applied after %@s.",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed));
		}

		// ==================================================

		SCTestLog("Verified that the configured DNS is same as applied DNS");

		success = YES;
	} while (0);

	[pathEvaluator removeObserver:test
			   forKeyPath:@"path"];

	return success;
}

- (BOOL)unitTestInstallProxyWithLargeConfig
{
	BOOL changed;
	BOOL success = NO;
	SCTestConfigAgent *test;
	NSString *str = @"0123456789";
	NSMutableString *largeStr;
	NSDictionary *proxyConfig;
	NSString *hostname;
	NSNumber *port;
	NWPathEvaluator *pathEvaluator;
	NSMutableDictionary *dict;

	test = [[SCTestConfigAgent alloc] initWithOptions:self.options];
	largeStr = [[NSMutableString alloc] init];
	for (int i = 0; i < 200; i++) {
		[largeStr appendString:str];
	}

	// We imitate a proxy config worth 2K bytes.
	proxyConfig = @{(__bridge NSString *)kSCPropNetProxiesHTTPEnable:@(1),
			(__bridge NSString *)kSCPropNetProxiesHTTPPort:@(80),
			(__bridge NSString *)kSCPropNetProxiesHTTPProxy:@"10.10.10.100",
			(__bridge NSString *)kSCPropNetProxiesProxyAutoConfigJavaScript:largeStr,
			(__bridge NSString *)kSCPropNetProxiesProxyAutoConfigEnable:@(1),
			(__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains:@[@TEST_DOMAIN],
			};

	hostname = [[proxyConfig objectForKey:(__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains] objectAtIndex:0];
	port = [proxyConfig objectForKey:(__bridge NSString *)kSCPropNetProxiesHTTPPort];
	pathEvaluator = [test pathEvaluatorWithHostname:hostname port:port.stringValue];

	do {
		// ==================================================

		[test startPathChange];

		dict = [NSMutableDictionary dictionaryWithDictionary:proxyConfig];
		[dict removeObjectForKey:(__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains];
		test.testProxy = @[@[dict]];
		[test publishToSCDynamicStore:test.proxyKey value:proxyConfig];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, change not applied after %@s. Test: %@",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed),
				  test.testProxy);
			[test removeFromSCDynamicStore:test.dnsKey];
			break;
		}

		if ([test.testProxy isEqualToArray:test.pathProxy]) {
			SCTestLog("applied proxy does not contain Out of Band Agent UUID");
			[test removeFromSCDynamicStore:test.proxyKey];
			break;
		}

		// ==================================================

		// Now we verify that we are able to fetch the proxy configuration from configd
		for (NSArray<NSDictionary *> *config in test.pathProxy) {
			xpc_object_t xpcConfig = _CFXPCCreateXPCObjectFromCFObject((__bridge CFArrayRef)config);
			xpc_object_t fetchedConfig = config_agent_update_proxy_information(xpcConfig);
			if (fetchedConfig != nil) {
				NSArray *nsConfig = (__bridge_transfer NSArray *)(_CFXPCCreateCFObjectFromXPCObject(fetchedConfig));
				test.pathProxy = @[nsConfig];
				break;
			}
		}

		if (![test.testProxy isEqualToArray:test.pathProxy]) {
			SCTestLog("Could not fetch proxy configuration from configd. Test: %@, Applied: %@", test.testProxy, test.pathProxy);
			[test removeFromSCDynamicStore:test.proxyKey];
			break;
		}

		// ==================================================

		[test startPathChange];

		[test removeFromSCDynamicStore:test.proxyKey];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, cleanup not applied after %@s.",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed));
			break;
		}

		// ==================================================

		SCTestLog("Verified that the proxy configuration is successfully fetched from configd");

		success = YES;
	} while (0);

	[pathEvaluator removeObserver:test
			   forKeyPath:@"path"];

	return success;
}

- (BOOL)unitTestInstallDNSWithConflictingDomain
{
	BOOL changed;
	BOOL success = NO;
	SCTestConfigAgent *test;
	NSDictionary *dnsConfig;
	NSString *hostname;
	NWPathEvaluator *pathEvaluator;

	test = [[SCTestConfigAgent alloc] initWithOptions:self.options];
	dnsConfig = @{(__bridge NSString *)kSCPropNetDNSServerAddresses:@[@"10.10.10.101", @"10.10.10.102", @"10.10.10.103"],
		      (__bridge NSString *)kSCPropNetDNSSupplementalMatchDomains:@[@TEST_DOMAIN],
		     };

	hostname = [[dnsConfig objectForKey:(__bridge NSString *)kSCPropNetDNSSupplementalMatchDomains] objectAtIndex:0];
	pathEvaluator = [test pathEvaluatorWithHostname:hostname port:@"80"];

	do {
		NSDictionary *anotherDNSConfig;
		NSString *anotherDNSKey;
		NSSet *testDNSSet;
		NSSet *pathDNSSet;

		// ==================================================

		[test startPathChange];

		test.testDNS = [test createDNSArray:dnsConfig];
		[test publishToSCDynamicStore:test.dnsKey value:dnsConfig];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, change not applied after %@s. Test: %@",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed),
				  test.testDNS);
			[test removeFromSCDynamicStore:test.dnsKey];
			break;
		}

		if (![test.testDNS isEqualToArray:test.pathDNS]) {
			SCTestLog("test DNS and applied DNS for conflicting domains do not match. Test: %@, Applied: %@",
				  test.testDNS,
				  test.pathDNS);
			[test removeFromSCDynamicStore:test.dnsKey];
			break;
		}

		// ==================================================

		[test startPathChange];

		// Now install the conflicting DNS configuration
		anotherDNSConfig = @{(__bridge NSString *)kSCPropNetDNSServerAddresses:@[@"10.10.10.104", @"10.10.10.105", @"10.10.10.106"],
				     (__bridge NSString *)kSCPropNetDNSSupplementalMatchDomains:@[@TEST_DOMAIN],
				    };

		anotherDNSKey = (__bridge_transfer NSString *)[self copyStateKeyWithServiceID:(__bridge CFStringRef)[NSUUID UUID].UUIDString
										    forEntity:kSCEntNetDNS];
		test.testDNS = [test.testDNS arrayByAddingObjectsFromArray:[test createDNSArray:anotherDNSConfig]];
		[test publishToSCDynamicStore:anotherDNSKey value:anotherDNSConfig];

		changed = [test waitForPathChange];
		if (!changed) {
			// if timeout
			SCTestLog("timeout, change not applied after %@s. Test: %@",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed),
				  test.testDNS);
			[test removeFromSCDynamicStore:anotherDNSKey];
			[test removeFromSCDynamicStore:test.dnsKey];
			break;
		}

		// compare requested with actual (use NSSet for unordered comparison)
		testDNSSet = [NSSet setWithArray:test.testDNS];
		pathDNSSet = [NSSet setWithArray:test.pathDNS];
		success = [testDNSSet isEqualToSet:pathDNSSet];
		if (!success) {
			SCTestLog("test DNS and applied DNS for conflicting domains do not match. Test: %@, Applied: %@",
				  test.testDNS,
				  test.pathDNS);
			[test removeFromSCDynamicStore:anotherDNSKey];
			[test removeFromSCDynamicStore:test.dnsKey];
			break;
		}

		// ==================================================

		[test startPathChange];

		[test removeFromSCDynamicStore:anotherDNSKey];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, cleanup (1) not applied after %@s.",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed));
		}

		// ==================================================

		[test startPathChange];

		[test removeFromSCDynamicStore:test.dnsKey];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, cleanup (2) not applied after %@s.",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed));
		}

		// ==================================================

		SCTestLog("Verified that the configured DNS with conflicting domains is same as applied DNS");

		success = YES;

	} while (0);

	[pathEvaluator removeObserver:test
			   forKeyPath:@"path"];

	return success;
}

- (BOOL)unitTestInstallProxyWithConflictingDomain
{
	BOOL changed;
	BOOL success = NO;
	SCTestConfigAgent *test;
	NSDictionary *proxyConfig;
	NSString *hostname;
	NSNumber *port;
	NWPathEvaluator *pathEvaluator;

	test = [[SCTestConfigAgent alloc] initWithOptions:self.options];
	proxyConfig = @{(__bridge NSString *)kSCPropNetProxiesHTTPEnable:@(1),
			(__bridge NSString *)kSCPropNetProxiesHTTPPort:@(80),
			(__bridge NSString *)kSCPropNetProxiesHTTPProxy:@"10.10.10.100",
			(__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains:@[@TEST_DOMAIN],
			};

	hostname = [[proxyConfig objectForKey:(__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains] objectAtIndex:0];
	port = [proxyConfig objectForKey:(__bridge NSString *)kSCPropNetProxiesHTTPPort];
	pathEvaluator = [test pathEvaluatorWithHostname:hostname port:port.stringValue];

	do {
		NSMutableDictionary *dict1;
		NSMutableDictionary *dict2;
		NSDictionary *anotherProxyConfig;
		NSString *anotherProxyKey;
		NSSet *testProxySet;
		NSSet *pathProxySet;

		// ==================================================

		[test startPathChange];

		dict1 = [NSMutableDictionary dictionaryWithDictionary:proxyConfig];
		[dict1 removeObjectForKey:(__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains];
		test.testProxy = @[@[dict1]];
		[test publishToSCDynamicStore:test.proxyKey value:proxyConfig];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, change not applied after %@s. Test: %@",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed),
				  test.testProxy);
			[test removeFromSCDynamicStore:test.proxyKey];
			break;
		}

		if (![test.testProxy isEqualToArray:test.pathProxy]) {
			SCTestLog("test proxy and applied proxy do not match. Test: %@, Applied: %@",
				  test.testProxy,
				  test.pathProxy);
			[test removeFromSCDynamicStore:test.proxyKey];
			break;
		}

		// ==================================================

		[test startPathChange];

		// Now install the conflicting Proxy configuration
		anotherProxyConfig = @{(__bridge NSString *)kSCPropNetProxiesHTTPSEnable:@(1),
				       (__bridge NSString *)kSCPropNetProxiesHTTPSPort:@(8080),
				       (__bridge NSString *)kSCPropNetProxiesHTTPSProxy:@"10.10.10.101",
				       (__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains:@[@TEST_DOMAIN],
				      };
		anotherProxyKey = (__bridge_transfer NSString *)[self copyStateKeyWithServiceID:(__bridge CFStringRef)[NSUUID UUID].UUIDString
										      forEntity:kSCEntNetProxies];

		dict2 = [NSMutableDictionary dictionaryWithDictionary:anotherProxyConfig];
		[dict2 removeObjectForKey:(__bridge NSString *)kSCPropNetProxiesSupplementalMatchDomains];
		test.testProxy = @[@[dict1],@[dict2]];
		[test publishToSCDynamicStore:anotherProxyKey value:anotherProxyConfig];;

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, change not applied after %@s. Test: %@",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed),
				  test.testProxy);
			[test removeFromSCDynamicStore:anotherProxyKey];
			[test removeFromSCDynamicStore:test.proxyKey];
			break;
		}

		// Use NSSet for unordered comparison
		testProxySet = [NSSet setWithArray:test.testProxy];
		pathProxySet = [NSSet setWithArray:test.pathProxy];
		success = [testProxySet isEqualToSet:pathProxySet];
		if (!success) {
			SCTestLog("test proxy and applied proxy for conflicting domains do not match. Test: %@, Applied: %@", test.testDNS, test.pathDNS);
			[test removeFromSCDynamicStore:anotherProxyKey];
			[test removeFromSCDynamicStore:test.proxyKey];
			break;
		}

		// ==================================================

		[test startPathChange];

		test.testProxy = nil;
		[test removeFromSCDynamicStore:anotherProxyKey];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, cleanup (1) not applied after %@s.",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed));
			break;
		}

		// ==================================================

		[test startPathChange];

		[test removeFromSCDynamicStore:test.proxyKey];

		changed = [test waitForPathChange];
		if (!changed) {
			SCTestLog("timeout, cleanup (2) not applied after %@s.",
				  createUsageStringForTimer(self.doneOrTimeoutElapsed));
			break;
		}

		// ==================================================

		SCTestLog("Verified the configured proxy with conflicting domains is the same as applied proxy");

		success = YES;

	} while(0);

	[pathEvaluator removeObserver:test
			   forKeyPath:@"path"];

	return success;
}

- (BOOL)tearDown
{
	[self removeFromSCDynamicStore:self.proxyKey];
	[self removeFromSCDynamicStore:self.dnsKey];
	return YES;
}

- (void)observeValueForKeyPath:(NSString *)keyPath
		      ofObject:(id)object
			change:(NSDictionary *)change
		       context:(void *)context
{
#pragma unused(change)
#pragma unused(context)
	NWPathEvaluator *pathEvaluator = (NWPathEvaluator *)object;
	if ([keyPath isEqualToString:@"path"]) {
		self.pathProxy = pathEvaluator.path.proxySettings;
		self.pathDNS = pathEvaluator.path.dnsServers;
		if (self.doneOrTimeoutCondition != NULL) {
			dispatch_semaphore_signal(self.doneOrTimeoutCondition);
		}
	}
}

@end
