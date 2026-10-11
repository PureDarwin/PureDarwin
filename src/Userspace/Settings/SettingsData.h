/*
 * SettingsData.h - what the Settings panes read from the running system and write back to it.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <Foundation/Foundation.h>

// preference domains the panes change, read live by the apps that own them
extern NSString * const PDDockDomain;
extern NSString * const PDMenuBarDomain;

id PDPref(NSString *domain, NSString *key);
void PDSetPref(NSString *domain, NSString *key, id value);
// nil domain means the global domain every app reads, like AppleAccentColor
id PDGlobalPref(NSString *key);
void PDSetGlobalPref(NSString *key, id value);
// window rounding and shadows default to on, the compositor hears changes through notify(3)
BOOL PDWindowStyleEnabled(NSString *key);
void PDPushWindowStyle(void);

NSString *PDSystemVersion(void);
NSString *PDBuildVersion(void);
NSString *PDKernelVersion(void);
NSString *PDXNUTag(void);
NSString *PDProcessor(void);
NSString *PDMemory(void);
NSString *PDGraphics(void);
NSString *PDHostName(void);
NSString *PDBootArgs(void);
NSString *PDStartupDisk(void);
NSString *PDTimeZone(void);

// names of every running process, for the running dots and status lights
NSSet *PDRunningProcesses(void);

// @{ label, what, path, disabled, running, locked }
NSArray *PDLaunchDaemons(void);
BOOL PDSetDaemonDisabled(NSString *path, BOOL disabled);

// @{ name, family, state }
NSArray *PDKernelExtensions(void);

// @[ name, detail ] pairs
NSArray *PDNetworkInterfaces(void);
NSArray *PDNameServers(void);
NSArray *PDAudioDevices(void);
NSArray *PDInputDevices(void);
// nil when there is no battery
NSDictionary *PDBattery(void);
NSDictionary *PDAccount(void);

// mounts the EFI partition, rewrites boot-args.txt and unmounts, nil on success or the reason it failed
NSString *PDWriteBootArgs(NSString *args);
