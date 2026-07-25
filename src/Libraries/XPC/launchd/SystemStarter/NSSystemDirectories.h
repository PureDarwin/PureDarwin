/*
 * PureDarwin: real Apple's NSSystemDirectories.h (historically part of
 * CarbonCore/CoreServicesInternal, never open-sourced) provided a much
 * larger API surface (NSSearchPathDirectory covering ~20 directory kinds,
 * NSAllDomainsMask, etc). SystemStarter.c/StartupItems.c only ever use
 * NSLibraryDirectory + NSSystemDomainMask/NSLocalDomainMask to enumerate
 * "/System/Library" and "/Library" - this header/its .c implement exactly
 * that real, documented subset (same real directory roots, same
 * enumeration-state contract: 0 means "no more results"), not the full
 * historical API PD has no other caller for.
 */
#ifndef PD_NSSYSTEMDIRECTORIES_H
#define PD_NSSYSTEMDIRECTORIES_H

#include <CoreFoundation/CoreFoundation.h>

typedef enum {
	NSLibraryDirectory = 5
} NSSearchPathDirectory;

typedef unsigned long NSSearchPathDomainMask;
typedef unsigned long NSSearchPathEnumerationState;

#define NSSystemDomainMask 2
#define NSLocalDomainMask  4

NSSearchPathEnumerationState NSStartSearchPathEnumeration(NSSearchPathDirectory dir, NSSearchPathDomainMask mask);
NSSearchPathEnumerationState NSGetNextSearchPathEnumeration(NSSearchPathEnumerationState state, char *path);

#endif /* PD_NSSYSTEMDIRECTORIES_H */
