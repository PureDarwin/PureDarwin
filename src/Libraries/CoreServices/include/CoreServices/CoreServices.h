/*
 * CoreServices umbrella. Only the CarbonCore Multiprocessing entry points
 * exist here, matching MultiprocessingCompat.c; the types are the ones that
 * file defines, so the framework and its header cannot drift.
 */

#ifndef _PD_CORESERVICES_H
#define _PD_CORESERVICES_H

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *MPTaskID;
typedef unsigned char MPBoolean;

extern MPTaskID MPCurrentTaskID(void);
extern MPBoolean MPTaskIsPreemptive(MPTaskID taskID);

#ifdef __cplusplus
}
#endif

#endif /* _PD_CORESERVICES_H */
