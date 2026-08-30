/*
 * CoreEntitlements/V2/API.h - PureDarwin stub. xnu 12377 ships most of
 * CoreEntitlements in EXTERNAL_HEADERS but not the V2/ directory, which
 * libkern/amfi/amfi.h includes. Defer to the real headers and add only the two
 * handle types they do not declare.
 */

#ifndef PD_COREENTITLEMENTS_V2_API_H
#define PD_COREENTITLEMENTS_V2_API_H

#include <CoreEntitlements/CoreEntitlements.h>
#include <CoreEntitlements/Errors.h>

typedef struct _CEContext        CEContext_t;
typedef struct _CEKernelAPI      CEKernelAPI_t;

#endif /* PD_COREENTITLEMENTS_V2_API_H */
