/*
 * Copyright (c) 2016, 2017, 2020 Apple Inc. All rights reserved.
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

#ifndef SCTestUtils_h
#define SCTestUtils_h

#import <Foundation/Foundation.h>
#import <SystemConfiguration/SCPrivate.h>
#import <objc/objc-runtime.h>

#define SCTestLog(fmt, ...)	SCLog(TRUE, LOG_NOTICE, CFSTR(fmt), ##__VA_ARGS__)

#define ERR_EXIT 		exit(1)

typedef struct {
	uint64_t user;
	uint64_t sys;
	uint64_t idle;
} CPUUsageInfoInner;

typedef struct {
	CPUUsageInfoInner startCPU;
	CPUUsageInfoInner endCPU;
} CPUUsageInfo;

typedef struct {
	struct timespec startTime;
	struct timespec endTime;
} timerInfo;

void timerStart(timerInfo *);
void timerEnd(timerInfo *);
NSString * createUsageStringForTimer(timerInfo *);

void cpuStart(CPUUsageInfo *);
void cpuEnd(CPUUsageInfo *);
NSString * createUsageStringForCPU(CPUUsageInfo *cpu);

NSArray<NSString *> *getTestClasses(void);
NSArray<NSString *> *getUnitTestListForClass(Class base);
NSDictionary *getOptionsDictionary(int argc, const char * const argv[]);

#endif /* SCTestUtils_h */
