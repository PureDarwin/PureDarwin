#ifndef _PD_SG2002_H
#define _PD_SG2002_H

// sophgo sg2002 on its cortex-a53, the licheerv nano

// the loader tags /arm-io with device_type "sg2002-io"
bool PDSg2002_isPlatform(void);

// keeps a watchdog u-boot armed from resetting the board, pdwdt=<seconds> stops feeding after that
void PDSg2002Watchdog_start(void);

// nubs for the on-chip controllers that have drivers, sd0 and gmac0
class IOService;
void PDSg2002_publish(IOService *parent);

#endif
