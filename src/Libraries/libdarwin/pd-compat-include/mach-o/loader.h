/*
 * xnu's EXTERNAL_HEADERS holds the real mach-o/loader.h, but that directory
 * cannot go on this target's include path: its AvailabilityInternal.h is the
 * kernel copy and shadows AvailabilityVersions', which breaks API_AVAILABLE in
 * os/assumes.h. Forward to it by path instead.
 */
#include "../../../../Kernel/xnu/EXTERNAL_HEADERS/mach-o/loader.h"
