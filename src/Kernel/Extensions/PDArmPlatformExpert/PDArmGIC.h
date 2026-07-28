#ifndef _PUREDARWIN_PDARMGIC_H
#define _PUREDARWIN_PDARMGIC_H

#include <IOKit/IOTypes.h>

/* Program the GICv3 distributor + this CPU's redistributor and route/enable the
 * generic virtual-timer PPI. Returns true on success. Idempotent. */
bool PDArmGIC_init(void);

#endif /* _PUREDARWIN_PDARMGIC_H */
