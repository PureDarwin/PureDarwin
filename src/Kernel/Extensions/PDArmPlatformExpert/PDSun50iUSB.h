#ifndef _PD_SUN50I_USB_H
#define _PD_SUN50I_USB_H

#include <IOKit/IOService.h>

// true on allwinner h616/h618, from the arm-io device_type the loader sets
bool PDSun50i_isPlatform(void);
// clocks, resets, phy and vbus for the usb1 host port
bool PDSun50iUSB_init(void);
// usb-ehci and usb-ohci nubs for AppleUSBEHCI and AppleUSBOHCI to match
void PDSun50iUSB_publish(IOService *parent);
// sunxi-mmc nub for PDSun50iMMC to match
void PDSun50iMMC_publish(IOService *parent);

#endif
