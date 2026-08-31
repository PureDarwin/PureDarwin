/*
 * Rosetta byte-swap helpers. PureDarwin never runs under Rosetta, so the
 * conditionals are hardwired false; hid.subproj includes this without using it.
 */

#ifndef _PD_SYSTEM_LIBKERN_OSCROSSENDIAN_H
#define _PD_SYSTEM_LIBKERN_OSCROSSENDIAN_H

#define _OSRosettaCheck()	(0)
#define IF_ROSETTA()		if (0)
#define ROSETTA_ONLY(...)	do { } while (0)

#endif /* _PD_SYSTEM_LIBKERN_OSCROSSENDIAN_H */
