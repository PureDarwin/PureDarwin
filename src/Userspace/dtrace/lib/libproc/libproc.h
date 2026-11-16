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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _LIBPROC_H
#define _LIBPROC_H

#include "rtld_db.h"
#include <sys/utsname.h>
#include <sys/bitmap.h>
#include <dlfcn.h>
#include <gelf.h>

#include "procfs.h"

#ifdef	__cplusplus

extern "C" {
#endif

/* From Sun's link.h */
#define LM_ID_BASE              0x00

/*
 * Opaque structure tag reference to a process control structure.
 * Clients of libproc cannot look inside the process control structure.
 * The implementation of struct ps_prochandle can change w/o affecting clients.
 */

struct ps_prochandle;

/* State values returned by Pstate() */
#define PS_RUN          1       /* process is running */
#define PS_STOP         2       /* process is stopped */
#define PS_LOST         3       /* process is lost to control (EAGAIN) */
#define PS_UNDEAD       4       /* process is terminated (zombie) */
#define PS_DEAD         5       /* process is terminated (core file) */
#define PS_IDLE         6       /* process has not been run */

/* Flags accepted by Pgrab() */
#define PGRAB_RETAIN    0x01    /* Retain tracing flags, else clear flags */
#define PGRAB_FORCE     0x02    /* Open the process w/o O_EXCL */
#define PGRAB_RDONLY    0x04    /* Open the process or core w/ O_RDONLY */
#define PGRAB_NOSTOP    0x08    /* Open the process but do not stop it */

/* Flags accepted by Prelease */
#define PRELEASE_RETAIN 0x20    /* Retain final tracing flags */
#define PRELEASE_HANG   0x40    /* Leave the process stopped */
#define PRELEASE_KILL   0x80    /* Terminate the process */

/*
 * Function prototypes for routines in the process control package.
 */
extern struct ps_prochandle *Pxcreate(const char *, char *const *,
                                     char *const *, int *, char *,
                                     size_t, cpu_type_t);
extern const char *Pcreate_error(int);

extern struct ps_prochandle *Pgrab(pid_t, int, int *);
extern const char *Pgrab_error(int);

extern  void    Prelease(struct ps_prochandle *, int);

extern  int     Pstate(struct ps_prochandle *);
extern  const pstatus_t *Pstatus(struct ps_prochandle *);
extern	int		Psetrun(struct ps_prochandle *, int, int);
extern  ssize_t Pread(struct ps_prochandle *, void *, size_t, mach_vm_address_t);
extern  int     Psetbkpt(struct ps_prochandle *, uintptr_t, ulong_t *);
extern  int     Pdelbkpt(struct ps_prochandle *, uintptr_t, ulong_t);
extern  int     Psetflags(struct ps_prochandle *, long);
extern  int     Punsetflags(struct ps_prochandle *, long);

/*
 * Symbol table interfaces.
 */

/*
 * Pseudo-names passed to Plookup_by_name() for well-known load objects.
 */
#define PR_OBJ_EXEC     ((const char *)0)       /* search the executable file */
#define PR_OBJ_LDSO     ((const char *)1)       /* search ld.so.1 */
#define PR_OBJ_EVERY    ((const char *)-1)      /* search every load object */

/*
 * Special Lmid_t passed to Plookup_by_lmid() to search all link maps.  The
 * special values LM_ID_BASE and LM_ID_LDSO from <link.h> may also be used.
 * If PR_OBJ_EXEC is used as the object name, the lmid must be PR_LMID_EVERY
 * or LM_ID_BASE in order to return a match.  If PR_OBJ_LDSO is used as the
 * object name, the lmid must be PR_LMID_EVERY or LM_ID_LDSO to return a match.
 */
#define PR_LMID_EVERY   ((Lmid_t)-1UL)          /* search every link map */

/*
 * 'object_name' is the name of a load object obtained from an
 * iteration over the process's address space mappings (Pmapping_iter),
 * or an iteration over the process's mapped objects (Pobject_iter),
 * or else it is one of the special PR_OBJ_* values above.
*/

extern int Plookup_by_name(struct ps_prochandle *, const char *, const char *, GElf_Sym *);

extern int Plookup_by_addr(struct ps_prochandle *, mach_vm_address_t, char *, size_t, GElf_Sym *);

typedef struct prsyminfo {
          const char      *prs_object;            /* object name */
          const char      *prs_name;              /* symbol name */
          Lmid_t          prs_lmid;               /* link map id */
} prsyminfo_t;

#define PLOOKUP_NOT_FOUND	-1
#define PLOOKUP_WRONG_GEN	1

extern int Pxlookup_by_name(struct ps_prochandle *,
    Lmid_t, const char *, const char *, GElf_Sym *, prsyminfo_t *);
extern int Pxlookup_by_name_new_syms(struct ps_prochandle *,
    Lmid_t, const char *, const char *, GElf_Sym *, prsyminfo_t *);


extern int Pxlookup_by_addr(struct ps_prochandle *,
    mach_vm_address_t, char *, size_t, GElf_Sym *, prsyminfo_t *);

typedef int proc_map_f(void *, const prmap_t *, const char *);

extern int Pobject_iter(struct ps_prochandle *, proc_map_f *, void *);
extern int Pobject_iter_new_syms(struct ps_prochandle *, proc_map_f *, void *);

/*
 * Apple NOTE: These differ from their solaris counterparts by taking a prmap_t pointer argument.
 * This is to manage thread local storage of the prmap_t.
 */
extern const prmap_t *Paddr_to_map(struct ps_prochandle *, mach_vm_address_t, prmap_t*);
extern const prmap_t *Pname_to_map(struct ps_prochandle *, const char *, prmap_t*);
extern const prmap_t *Plmid_to_map(struct ps_prochandle *, Lmid_t, const char *, prmap_t*);

extern char *Pobjname(struct ps_prochandle *, mach_vm_address_t, char *, size_t);
extern int Plmid(struct ps_prochandle *, mach_vm_address_t, Lmid_t *);

extern void Pcheckpoint_syms(struct ps_prochandle *);

/*
 * Apple only objc iteration interface
 */

typedef int proc_objc_f(void *, const GElf_Sym *, const char *, const char *);
extern int Pobjc_method_iter(struct ps_prochandle *, proc_objc_f* , void *);
extern int Pobjc_method_iter_new_syms(struct ps_prochandle *, proc_objc_f* , void *);

/*
 * Symbol table iteration interface.  The special lmid constants LM_ID_BASE,
 * LM_ID_LDSO, and PR_LMID_EVERY may be used with Psymbol_iter_by_lmid.
 */
typedef int proc_sym_f(void *, const GElf_Sym *, const char *);

extern int Psymbol_iter_by_addr(struct ps_prochandle *, const char *, int, int, proc_sym_f *, void *);
extern int Psymbol_iter_by_addr_new_syms(struct ps_prochandle *, const char *, int, int, proc_sym_f *, void *);

/*
 * 'which' selects which symbol table and can be one of the following.
 */
#define PR_SYMTAB       1
#define PR_DYNSYM       2

/*
 * 'type' selects the symbols of interest by binding and type.  It is a bit-
 * mask of one or more of the following flags, whose order MUST match the
 * order of STB and STT constants in <sys/elf.h>.
 */
#define BIND_LOCAL      0x0001
#define BIND_GLOBAL     0x0002
#define BIND_WEAK       0x0004
#define BIND_ANY (BIND_LOCAL|BIND_GLOBAL|BIND_WEAK)
#define TYPE_NOTYPE     0x0100
#define TYPE_OBJECT     0x0200
#define TYPE_FUNC       0x0400
#define TYPE_SECTION    0x0800
#define TYPE_FILE       0x1000
#define TYPE_ANY (TYPE_NOTYPE|TYPE_OBJECT|TYPE_FUNC|TYPE_SECTION|TYPE_FILE)

/*
 * This returns the rtld_db agent handle for the process.
 * The handle will become invalid at the next successful exec() and
 * must not be used beyond that point (see Preset_maps(), below).
 */
extern rd_agent_t *Prd_agent(struct ps_prochandle *);

/*
 * This should be called when an RD_DLACTIVITY event with the
 * RD_CONSISTENT state occurs via librtld_db's event mechanism.
 * This makes libproc's address space mappings and symbol tables current.
 * The variant Pupdate_syms() can be used to preload all symbol tables as well.
 */
extern void Pupdate_maps(struct ps_prochandle *);
extern void Pupdate_syms(struct ps_prochandle *);

/*
 * Given an address, Ppltdest() determines if this is part of a PLT, and if
 * so returns a pointer to the symbol name that will be used for resolution.
 * If the specified address is not part of a PLT, the function returns NULL.
*/
extern const char *Ppltdest(struct ps_prochandle *, mach_vm_address_t);

/*
 * We have a hideous three thread of control system.
 *
 * There is the main dtrace thread, the per-proc control thread, and the per symbolicator notification thread.
 * The symbolicator notification thread and main thread may queue events for processing by the control thread.
 * Pcreate_sync_proc_activity will not return until the control thread has processed the activity. 
 */
extern void Pcreate_async_proc_activity(struct ps_prochandle*, rd_event_e);
extern void Pcreate_sync_proc_activity(struct ps_prochandle*, rd_event_e);
	
extern void* Pdequeue_proc_activity(struct ps_prochandle*);
extern void Pdestroy_proc_activity(void*);
	
#ifdef  __cplusplus
}
#endif

#endif  /* _LIBPROC_H */
