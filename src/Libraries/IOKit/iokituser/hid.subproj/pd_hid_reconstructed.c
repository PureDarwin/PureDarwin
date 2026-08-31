/*
 * Definitions hid.subproj references but the IOKitUser drop does not provide.
 */

#include <stdint.h>

/*
 * Debug tracing bitmask, referenced by IOHIDLibPrivate.c but defined in one of
 * the drop's stripped sources. Off: the trace/perf paths it gates need the
 * event system, which is not here either.
 */
uint32_t gIOHIDDebugConfig = 0;
