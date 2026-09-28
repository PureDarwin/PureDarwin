#ifndef _PUREDARWIN_PDRISCVPLIC_H
#define _PUREDARWIN_PDRISCVPLIC_H

#include <IOKit/IOTypes.h>

// find and map the plic, then mask every source in each hart's supervisor context
// runs before the cpus register, since secondaries take external interrupts once they start
bool PDRiscvPLIC_init(void);

// publish the plic as the platform's interrupt controller and hang it off the cpu controller
// every cpu must have run initCPU first, the cpu controller blocks registration until they have
bool PDRiscvPLIC_start(void);

#endif /* _PUREDARWIN_PDRISCVPLIC_H */
