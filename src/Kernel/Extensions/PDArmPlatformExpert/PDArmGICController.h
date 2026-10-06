#ifndef _PUREDARWIN_PDARMGICCONTROLLER_H
#define _PUREDARWIN_PDARMGICCONTROLLER_H

#include <IOKit/IOTypes.h>

// the gic's spis as an iokit interrupt controller named "PDArmGIC", and a nub's specifier is
// its 32-bit intid
bool PDArmGICController_init(void);

#endif // _PUREDARWIN_PDARMGICCONTROLLER_H
