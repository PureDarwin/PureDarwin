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
