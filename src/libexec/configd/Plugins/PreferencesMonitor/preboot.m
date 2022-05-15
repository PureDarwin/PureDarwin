/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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
 * May 12, 2021		Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import "preboot.h"
#import "prefsmon_log.h"
#import <os/boot_mode_private.h>
#import <limits.h>
#import <uuid/uuid.h>
#import <sys/types.h>
#import <sys/sysctl.h>
#import <string.h>

#import <SystemConfiguration/SystemConfiguration.h>
#import <SystemConfiguration/SCPrivate.h>
#import <SystemConfiguration/SCValidation.h>
#import <DiskArbitration/DiskArbitration.h>
#import <DiskManagement/DMAPFS.h>
#import <DiskManagement/DMManager.h>
#import <DiskManagement/DMManagerInfo.h>

@interface PrebootUpdater : NSObject<DMAsyncDelegate>
@property (nonatomic)BOOL		update_in_progress;
@property (nonatomic)BOOL		request_queued;
@property (strong,nonatomic)DMManager *	manager;
@property (nonatomic)DADiskRef		disk;
@property (strong, nonatomic)DMAPFS *	dmapfs;
@end

#if defined(TEST_PREBOOT)
static bool S_retry;
static bool S_exit_on_completion;
#endif

#define _LOG_PREFIX		"syncNetworkConfigurationToPrebootVolume"

@implementation PrebootUpdater

- (instancetype)initWithManager:(DMManager *)manager disk:(DADiskRef)disk
			 DMAPFS:(DMAPFS *)dmapfs
{
	self = [super init];
	_manager = manager;
	_disk = disk;
	_dmapfs = dmapfs;
	[manager setDelegate:self];
	return (self);
}

- (void)dealloc
{
	if (_manager != nil) {
		[_manager release];
	}
	if (_dmapfs != nil) {
		[_dmapfs release];
	}
	[super dealloc];
	return;
}

- (BOOL) syncNetworkConfiguration
{
	DMDiskErrorType		error;

	if (_update_in_progress) {
		SC_log(LOG_NOTICE, "%s: queueing request", _LOG_PREFIX);
		_request_queued = YES;
		return (YES);
	}
	error = [_dmapfs updatePrebootForVolume:_disk options:nil];
	if (error != 0) {
		SC_log(LOG_NOTICE,
		       "%s: [DMAPFS updatePrebootForVolume] failed %d",
		       _LOG_PREFIX, error);
	}
	else {
		SC_log(LOG_NOTICE, "%s: sync started", _LOG_PREFIX);
		_update_in_progress = YES;
	}
	return (_update_in_progress);
}

- (void)dmAsyncStartedForDisk:(nullable DADiskRef)inDisk
{
}

- (void)dmAsyncProgressForDisk:(nullable DADiskRef)inDisk
		    barberPole:(BOOL)inBarberPole percent:(float)inPercent
{
#if defined(TEST_PREBOOT)
	SC_log(LOG_DEBUG, "%s: %f%% complete", _LOG_PREFIX, inPercent);
	if (S_retry && inPercent > 25) {
		S_retry = false;
		SC_log(LOG_NOTICE, "Test: scheduling sync again");
		[self syncNetworkConfiguration];
	}
#endif
}

- (void)dmAsyncMessageForDisk:(nullable DADiskRef)inDisk
		       string:(NSString *)inString
		   dictionary:(nullable NSDictionary *)inDictionary
{
}

- (void)dmAsyncFinishedForDisk:(nullable DADiskRef)inDisk
		     mainError:(DMDiskErrorType)inMainError
		   detailError:(DMDiskErrorType)inDetailError
		    dictionary:(nullable NSDictionary *)inDictionary
{
	BOOL	sync_started = NO;

	SC_log(LOG_NOTICE, "%s: sync complete", _LOG_PREFIX);
	_update_in_progress = NO;
	if (_request_queued) {
		SC_log(LOG_NOTICE, "%s: starting sync for queued request",
		       _LOG_PREFIX);
		_request_queued = NO;
		sync_started = [self syncNetworkConfiguration];
		if (sync_started) {
			return;
		}
	}
#if defined(TEST_PREBOOT)
	if (!sync_started && S_exit_on_completion) {
		SC_log(LOG_NOTICE, "%s: All done, exiting",
		       _LOG_PREFIX);
		exit(0);
	}
#endif
}

@end

static PrebootUpdater *
allocateUpdater(void)
{
	DADiskRef		disk = NULL;
	DMAPFS *		dmapfs = nil;
	DMDiskErrorType		error;
	BOOL			enabled = false;
	DMManager *		manager = nil;
	DASessionRef		session = NULL;
	static PrebootUpdater * S_updater;

	if (S_updater != nil) {
		return (S_updater);
	}
	session = DASessionCreate(kCFAllocatorDefault);
	if (session == NULL) {
		SC_log(LOG_NOTICE, "%s: DASessionCreate failed", _LOG_PREFIX);
		goto failed;
	}
	manager = [[DMManager alloc] init];
	[manager setDefaultDASession:session];
	disk = [manager copyRootDisk:&error];
	if (disk == NULL) {
		SC_log(LOG_NOTICE, "%s: Failed to copy root disk, %d",
		       _LOG_PREFIX, error);
		goto failed;
	}
	dmapfs = [[DMAPFS alloc] initWithManager:manager];
	[dmapfs isFileVaultEnabled:disk enabled:&enabled];
	if (!enabled) {
		SC_log(LOG_NOTICE, "%s: Filevault not enabled, skipping sync",
		       _LOG_PREFIX);
		goto failed;
	}
	DASessionScheduleWithRunLoop(session,
				     CFRunLoopGetCurrent(),
				     kCFRunLoopDefaultMode);
	S_updater = [[PrebootUpdater alloc]
			    initWithManager:manager
				       disk:disk
				     DMAPFS:dmapfs];
	CFRelease(session);
	return (S_updater);

 failed:
	if (session != NULL) {
		CFRelease(session);
	}
	if (disk != NULL) {
		CFRelease(disk);
	}
	if (dmapfs != nil) {
		[dmapfs release];
	}
	if (manager != nil) {
		[manager release];
	}
	return (nil);
}

static const char *
get_boot_mode(void)
{
	const char *	mode = NULL;
	bool		success;

	success = os_boot_mode_query(&mode);
	if (!success) {
		SC_log(LOG_NOTICE, "os_boot_mode_query failed");
		return (NULL);
	}
	return (mode);
}

bool
syncNetworkConfigurationToPrebootVolume(void)
{
	const char *	boot_mode;
	bool		success = false;

	boot_mode = get_boot_mode();
	if (boot_mode != NULL) {
		SC_log(LOG_NOTICE,
		       "%s: boot mode is %s, not syncing",
		       __func__, boot_mode);
		goto done;
	}
	@autoreleasepool {
		PrebootUpdater *	updater;

		updater = allocateUpdater();
		if (updater != nil) {
			success = [updater syncNetworkConfiguration];
		}
	}
 done:
	return (success);
}

#if defined(TEST_PREBOOT)

#import <stdio.h>
#import <stdlib.h>
#import <unistd.h>

int
main(int argc, char *argv[])
{
	int			ch;

	while ((ch = getopt(argc, argv, "er")) != EOF) {
		switch (ch) {
		case 'e':
			S_exit_on_completion = true;
			break;
		case 'r':
			S_retry = true;
			break;
		default:
			break;
		}
	}
	if (syncNetworkConfigurationToPrebootVolume()) {
		printf("Started...\n");
		CFRunLoopRun();
	}
	exit(0);
	return (0);
}

#endif /* TEST_PREBOOT */
