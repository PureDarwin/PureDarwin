// SPDX-License-Identifier: MIT
// the xnu side of fbsdkpi, built per driver kext with its own PDFBSD_CLASS
// iokit class names must be unique
#ifndef PD_FBSD_XNU_H
#define PD_FBSD_XNU_H

#ifndef PDFBSD_CLASS
#error PDFBSD_CLASS must name the driver's iokit class
#endif

#define PDFBSD_PASTE2(a, b)	a##b
#define PDFBSD_PASTE(a, b)	PDFBSD_PASTE2(a, b)
#define PDFBSD_INTR_CLASS	PDFBSD_PASTE(PDFBSD_CLASS, Intr)

void pdos_xnu_init(void);

#endif
