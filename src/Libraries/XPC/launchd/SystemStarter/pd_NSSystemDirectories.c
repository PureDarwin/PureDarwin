/*
 * PureDarwin: see NSSystemDirectories.h for why this exists. Real
 * enumeration-state contract: state 0 means "start", each call returns
 * the next state (nonzero) with `path` filled in, or 0 when done. We
 * encode the two real domain roots as small distinct nonzero state
 * values, mirroring how real Apple's own bitmask-driven enumerator would
 * walk NSSystemDomainMask before NSLocalDomainMask.
 */
#include "NSSystemDirectories.h"
#include <string.h>

NSSearchPathEnumerationState
NSStartSearchPathEnumeration(NSSearchPathDirectory dir, NSSearchPathDomainMask mask)
{
	(void)dir;
	return mask & (NSSystemDomainMask | NSLocalDomainMask);
}

NSSearchPathEnumerationState
NSGetNextSearchPathEnumeration(NSSearchPathEnumerationState state, char *path)
{
	if (state & NSSystemDomainMask) {
		strcpy(path, "/System/Library");
		return state & ~(NSSearchPathEnumerationState)NSSystemDomainMask;
	}
	if (state & NSLocalDomainMask) {
		strcpy(path, "/Library");
		return state & ~(NSSearchPathEnumerationState)NSLocalDomainMask;
	}
	return 0;
}
