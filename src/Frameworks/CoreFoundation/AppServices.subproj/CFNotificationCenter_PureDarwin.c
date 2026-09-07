/*
 * FAKE: returns NULL. Real distributed notifications need a distnoted-equivalent
 * daemon; this exists only so clients that call it (e.g. Wine's ntdll, which
 * discards the result) can link. Callers that actually use the returned center
 * will not work.
 */

#include <CoreFoundation/CoreFoundation.h>
#include "CFNotificationCenter.h"

CFNotificationCenterRef
CFNotificationCenterGetDistributedCenter(void)
{
	return NULL;
}
