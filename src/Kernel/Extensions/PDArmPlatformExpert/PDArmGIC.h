#ifndef _PUREDARWIN_PDARMGIC_H
#define _PUREDARWIN_PDARMGIC_H

#include <IOKit/IOTypes.h>

/* Program the GICv3 distributor + this CPU's redistributor and route the
 * generic virtual-timer PPI, leaving it masked. Returns true on success.
 * Idempotent. */
bool PDArmGIC_init(void);

/* Unmask the timer PPI and enable Group 1 delivery. Must not run before the
 * CPU interrupt handler is installed. */
bool PDArmGIC_enable(void);

#endif /* _PUREDARWIN_PDARMGIC_H */
