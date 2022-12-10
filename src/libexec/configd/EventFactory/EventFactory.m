/*
 * Copyright (c) 2017-2019 Apple Inc. All rights reserved.
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

/*
 * Modification History
 *
 * November 15, 2017	Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#import <os/log.h>
#import "EventFactory.h"
#import "SCLogParser.h"
#import "InterfaceNamerParser.h"
#import "IPMonitorParser.h"
#import "KernelEventMonitorParser.h"
#import "PreferencesMonitorParser.h"
#import "StateDumpParser.h"
#import "IPConfigurationParser.h"
#import "SCDynamicStoreParser.h"
#import "SCPreferencesParser.h"

#pragma mark -
#pragma mark Logging

os_log_t
__log_Spectacles(void)
{
	static os_log_t	log	= NULL;

	if (log == NULL) {
		log = os_log_create("com.apple.spectacles", "SystemConfiguration");
	}

	return log;
}

#pragma mark -
#pragma mark SystemConfiguratioin Network Event Factory

typedef NS_ENUM(NSInteger, LogAccumulatingState) {
	NOT_ACCUMULATING,
	ACCUMULATING_DNS,
	ACCUMULATING_NWIv4,
	ACCUMULATING_NWIv6,
};

@interface EventFactory ()
@property NSDictionary<NSString *, SCLogParser *> *parserMap;
@property LogAccumulatingState accumulating;
@property EFLogEvent *accumulatingEvent;
@property NSString *accumulatingEventIdentifierString;
@end

@implementation EventFactory

- (void)startWithLogSourceAttributes:(__unused NSDictionary<NSString *, NSObject *> *)attributes
{
	NSMutableDictionary<NSString *, SCLogParser *> *newParserMap = [[NSMutableDictionary alloc] init];
	SCLogParser *parser;

	parser = [[InterfaceNamerParser alloc] init];
	newParserMap[parser.category] = parser;

	parser = [[IPConfigurationParser alloc] init];
	newParserMap[parser.category] = parser;

	parser = [[IPMonitorParser alloc] init];
	newParserMap[parser.category] = parser;

	parser = [[KernelEventMonitorParser alloc] init];
	newParserMap[parser.category] = parser;

	parser = [[PreferencesMonitorParser alloc] init];
	newParserMap[parser.category] = parser;

	parser = [[StateDumpParser alloc] init];
	newParserMap[parser.category] = parser;

	parser = [[SCDynamicStoreParser alloc] init];
	newParserMap[parser.category] = parser;

	parser = [[SCPreferencesParser alloc] init];
	newParserMap[parser.category] = parser;

	_parserMap = [[NSDictionary alloc] initWithDictionary:newParserMap];

	_accumulating = NOT_ACCUMULATING;
}

- (NSString *)logEventIdentifierString:(EFLogEvent *)logEvent
{
	NSString	*identifierString;

	identifierString = [[NSString alloc] initWithFormat:@"%@[%d]:%@:%@",
							logEvent.process,
							logEvent.processIdentifier,
							logEvent.subsystem,
							logEvent.category];

	return identifierString;
}

- (void)handleLogEvent:(EFLogEvent *)logEvent completionHandler:(void (^)(NSArray<EFEvent *> * _Nullable))completionHandler
{
	if ([logEvent.eventType isEqualToString:@"stateEvent"]) {
		logEvent.subsystem = @"com.apple.SystemConfiguration";
		logEvent.category = @"StateDump";
	} else if ([logEvent.subsystem isEqualToString:@"com.apple.IPConfiguration"]) {
		logEvent.category = @"IPConfiguration";
	}

	if (logEvent.category.length == 0) {
		specs_log_debug("Skipped message without a category: %@", logEvent.eventMessage);
		completionHandler(nil);
		return;
	}

	if ([logEvent.subsystem isEqualToString:@"com.apple.SystemConfiguration"] &&
		[logEvent.category  isEqualToString:@"IPMonitor"]) {
		BOOL		appendMessage	= YES;
		BOOL		done			= NO;

		if (_accumulating != NOT_ACCUMULATING) {
			/*
			 * if we are accumulating a block of log messages
			 */
			NSString *logEventIdentifierString = [self logEventIdentifierString:logEvent];
			if ((_accumulating != NOT_ACCUMULATING) &&
				![_accumulatingEventIdentifierString isEqualToString:logEventIdentifierString]) {
				// if the PID changed
				specs_log_debug("Dropped partial message block: %@", _accumulatingEvent.eventMessage);
				_accumulating = NOT_ACCUMULATING;
				_accumulatingEvent = nil;
				appendMessage = NO;
			}
		}

		switch (_accumulating) {
			case NOT_ACCUMULATING : {
				if ([logEvent.eventMessage isEqualToString:@"Updating DNS configuration"]) {
					/*
					  2019-10-10 14:07:32.891719-0400 0x350      Info        0x0                  70     0    configd: [com.apple.SystemConfiguration:IPMonitor] Updating DNS configuration
					  2019-10-10 14:07:32.891722-0400 0x350      Info        0x0                  70     0    configd: [com.apple.SystemConfiguration:IPMonitor] DNS configuration
					  -->
					  2019-10-10 14:03:17.549361-0400 0x0        State       0x2ed40              70     14   configd: DNS Configuration
					  DNS configuration
					*/
					_accumulating = ACCUMULATING_DNS;
					logEvent.eventMessage = @"DNS Configuration";
				} else if ([logEvent.eventMessage isEqualToString:@"Updating network information"]) {
					/*
					  2019-10-10 14:07:32.889595-0400 0x350      Info        0x0                  70     0    configd: [com.apple.SystemConfiguration:IPMonitor] Updating network information
					  2019-10-10 14:07:32.889596-0400 0x350      Info        0x0                  70     0    configd: [com.apple.SystemConfiguration:IPMonitor] Network information (generation 156994061625682 size=1180)
					  -->
					  2019-10-10 14:03:17.549364-0400 0x0        State       0x2ed40              70     14   configd: Network information
					  Network information (generation 55086114928 size=724)
					*/
					_accumulating = ACCUMULATING_NWIv4;
					logEvent.eventMessage = @"Network information";
				}

				if (_accumulating != NOT_ACCUMULATING) {
					// if we are now accumulating a block of messages
					_accumulatingEventIdentifierString = [self logEventIdentifierString:logEvent];
					_accumulatingEvent = logEvent;
					_accumulatingEvent.subsystem = @"com.apple.SystemConfiguration";
					_accumulatingEvent.category = @"StateDump";
					appendMessage = NO;
				}
				break;
			}

			case ACCUMULATING_DNS : {
				if ([logEvent.eventMessage hasPrefix:@"DNS configuration updated: "]) {
					done = YES;
					appendMessage = NO;
				}
				break;
			}

			case ACCUMULATING_NWIv4 : {
				if ([logEvent.eventMessage isEqualToString:@"IPv6 network interface information"]) {
					_accumulating = ACCUMULATING_NWIv6;
				}
				break;
			}

			case ACCUMULATING_NWIv6 : {
				if ([logEvent.eventMessage hasPrefix:@"   REACH : "]) {
					done = YES;
				}
				break;
			}
		}

		if (appendMessage) {
			_accumulatingEvent.eventMessage = [NSString stringWithFormat:@"%@\n%@",
													_accumulatingEvent.eventMessage,
													logEvent.eventMessage];
		}

		if (done) {
			// if we have all we need, pass the [accumulated] event
			logEvent = _accumulatingEvent;
			_accumulating = NOT_ACCUMULATING;
			_accumulatingEvent = nil;
		} else if (_accumulating != NOT_ACCUMULATING) {
			// if we are still (or now) accumulating
			completionHandler(nil);
			return;
		}
	}

	SCLogParser *parser = _parserMap[logEvent.category];
	if (parser == nil) {
		specs_log_debug("Skipped message with an unknown category (%@): %@", logEvent.category, logEvent.eventMessage);
		completionHandler(nil);
		return;
	}

	NSArray<EFEvent *> *completeEvents = [parser.eventParser parseLogEventIntoMultipleEvents:logEvent];
	completionHandler(completeEvents);
}

- (void)finishWithCompletionHandler:(void (^)(NSArray<EFEvent *> * _Nullable))completionHandler
{
	specs_log_notice("Event factory is finishing");
	completionHandler(nil);
}

@end


