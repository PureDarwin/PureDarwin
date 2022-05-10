/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2006 Apple Computer, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_DT_LD_H
#define	_DT_LD_H

#include <libctf.h>
#include <dtrace.h>

 #ifdef __cplusplus
 extern "C" {
 #endif
	
void* dtrace_ld_create_dof(cpu_type_t cpu,             // [provided by linker] target architecture
                           unsigned int typeCount,     // [provided by linker] number of stability or typedef symbol names
                           const char* typeNames[],    // [provided by linker] stability or typedef symbol names
                           unsigned int probeCount,    // [provided by linker] number of probe or isenabled locations
                           const char* probeNames[],   // [provided by linker] probe or isenabled symbol names
                           const char* probeWithin[],  // [provided by linker] function name containing probe or isenabled
                           uint64_t offsetsInDOF[],    // [allocated by linker, populated by DTrace] per-probe offset in the DOF
                           size_t* size);               // [allocated by linker, populated by DTrace] size of the DOF)

char* dt_ld_encode_stability(char* provider_name, dt_provider_t* provider);
char* dt_ld_encode_typedefs(char* provider_name, dt_provider_t* provider);
char* dt_ld_encode_probe(char* provider_name, char* probe_name, dt_probe_t* probe);
char* dt_ld_encode_isenabled(char* provider_name, char* probe_name);

#ifdef __cplusplus
}
#endif

#endif	/* _DT_LD_H */
