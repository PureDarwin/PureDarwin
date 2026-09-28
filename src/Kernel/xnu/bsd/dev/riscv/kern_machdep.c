/*
 * Copyright (c) 2000-2024 Apple Inc. All rights reserved.
 */
/*
 *	Copyright (C) 1990,  NeXT, Inc.
 *
 *	File:	next/kern_machdep.c
 *	Author:	John Seamons
 *
 *	Machine-specific kernel routines.
 */

#include        <sys/types.h>
#include        <mach/machine.h>
#include        <kern/cpu_number.h>
#include        <libkern/libkern.h>
#include        <machine/exec.h>

int ml_grade_binary(cpu_type_t, cpu_subtype_t, cpu_subtype_t, bool);

// Routine: ml_grade_binary(), relative preference for exectypes and execsubtypes in fat files.
// The higher the grade, the higher the preference. A grade of 0 means not acceptable.
int
ml_grade_binary(cpu_type_t exectype, cpu_subtype_t execsubtype, cpu_subtype_t execfeatures __unused, bool allow_simulator_binary __unused)
{
	// only rv64 processes exist, and there is a single subtype so far
	switch (exectype) {
	case CPU_TYPE_RISCV64:
		switch (execsubtype) {
		case CPU_SUBTYPE_RISCV_ALL:
			return 10;
		}
		break;
	}

	return 0;
}
