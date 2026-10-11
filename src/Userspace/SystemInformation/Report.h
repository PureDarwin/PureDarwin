/*
 * Report.h - gathers the hardware and software report from the I/O Registry, sysctl and the network stack.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <Foundation/Foundation.h>

// a report is an array of sections, @{ @"name", @"heading", @"build" }, build being a block that returns the groups
// each group is @{ @"title", @"rows" } with rows as @[ key, value ] pairs, a nil title for loose rows
typedef NSArray *(^PDReportBuilder)(void);
NSArray *PDReportSections(void);
