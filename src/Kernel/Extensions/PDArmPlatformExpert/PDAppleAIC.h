#ifndef _PUREDARWIN_PDAPPLEAIC_H
#define _PUREDARWIN_PDAPPLEAIC_H

#include <IOKit/IOTypes.h>

/*
 * Bring up the Apple Interrupt Controller found in the device tree and publish
 * it as this platform's interrupt controller. Returns true on success, and is
 * idempotent.
 */
bool PDAppleAIC_init(void);

#endif /* _PUREDARWIN_PDAPPLEAIC_H */
