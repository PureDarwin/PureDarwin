#ifndef _PUREDARWIN_IOKITMIG32_H_
#define _PUREDARWIN_IOKITMIG32_H_

/* 32-bit counterpart of iokitmig64.h. Both are thin shims over the single
 * device.defs MIG output; the 64-bit-only entry points are selected inside
 * IOKitLib.c, not by generating a different interface here. */
#include <device_user.h>

#endif /* _PUREDARWIN_IOKITMIG32_H_ */
