/*
 * CoreServices umbrella: the CarbonCore Multiprocessing entry points of
 * MultiprocessingCompat.c (the types are the ones that file defines, so the
 * framework and its header cannot drift) and the FSEvents stream API.
 */

#ifndef _PD_CORESERVICES_H
#define _PD_CORESERVICES_H

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/FSEvents.h>
#include <CoreServices/FileManager.h>
#include <CoreServices/UTType.h>
#include <CoreServices/LaunchServices.h>
#include <CoreServices/SharedFileList.h>
#include <CoreServices/UnicodeUtilities.h>
#include <CoreServices/AppleEvents.h>

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
