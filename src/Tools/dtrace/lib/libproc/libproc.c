/*
 * Copyright (c) 2006-2008 Apple Computer, Inc.  All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#if DTRACE_USE_CORESYMBOLICATION
#include <CoreSymbolication/CoreSymbolication.h>
#include <CoreSymbolication/CoreSymbolicationPrivate.h>
#endif /* DTRACE_USE_CORESYMBOLICATION */

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/mach_error.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/sysctl.h>

#include <sys/proc_info.h>
#include <sys/codesign.h>
#include <sys/fasttrap_isa.h>

#if DTRACE_TARGET_APPLE_MAC
#include <sys/csr.h>
#include <sandbox/rootless.h>
#endif /* DTRACE_TARGET_APPLE_MAC */

#include "libproc.h"
#include "libproc_apple.h"
#include "libproc_internal.h"

#include <spawn.h>
#include <pthread.h>

#include <os/log.h>

extern int _dtrace_disallow_dsym;
extern int _dtrace_mangled;

#define APPLE_PCREATE_BAD_SYMBOLICATOR		0x0F000001
#define APPLE_EXECUTABLE_RESTRICTED		0x0F000003
#define APPLE_EXECUTABLE_NOT_ATTACHABLE		0x0F000004
#define APPLE_TRANSLATED_NOTSUP			0x0F000005

#if DTRACE_USE_CORESYMBOLICATION
/*
 * This is a helper method, it does extended lookups following roughly these rules
 *
 * 1. An exact match (i.e. a full pathname): "/usr/lib/libc.so.1"
 * 2. An exact basename match: "libc.so.1"
 * 3. An initial basename match up to a '.' suffix: "libc.so" or "libc"
 * 4. The literal string "a.out" is an alias for the executable mapping
 */
static int forEachSymbolOwner(CSSymbolicatorRef symbolicator, const char* name, void (^cb)(CSSymbolOwnerRef owner)) {
	// Check for a.out specifically
	if (strcmp(name, "a.out") == 0) {
		if (CSSymbolicatorForeachSymbolOwnerWithFlagsAtTime(symbolicator, kCSSymbolOwnerIsAOut, kCSNow, ^(CSSymbolOwnerRef t) { cb(t); }) == 1) {
			return 0;
		}
		return -1;
	}

	// Try path based matching. Multile matches are legal, take the first.
	if (CSSymbolicatorForeachSymbolOwnerWithPathAtTime(symbolicator, name, kCSNow, ^(CSSymbolOwnerRef t) { cb(t); }) > 0)
		return 0;

	// Try name based matching. Multiple matches are legal, take the first.
	if (CSSymbolicatorForeachSymbolOwnerWithNameAtTime(symbolicator, name, kCSNow, ^(CSSymbolOwnerRef t) { cb(t); }) > 0)
		return 0;

	// Strip off extensions. We know there are no direct matches now.
	size_t nameLength = strlen(name);
	__block bool found_match = false;
	CSSymbolicatorForeachSymbolOwnerAtTime(symbolicator, kCSNow, ^(CSSymbolOwnerRef candidate) {
		const char* candidateName = CSSymbolOwnerGetName(candidate);
		size_t candidateNameLength = strlen(candidateName);

		// We're going to cheat a bit.
		//
		// A match at this point will always be a prefix match. I.E. libSystem match against libSystem.B.dylib
		// We make the following assertions
		// 1) For a match to be possible, the candidate must always be longer than the search name
		// 2) The match must always begin at the root of the candidate name
		// 3) The next character in the candidate must be a '.'
		if (nameLength < candidateNameLength) {
			if (strstr(candidateName, name) == candidateName) {
				if (candidateName[nameLength] == '.') {
					found_match = true;
					// Its a match!
					cb(candidate);
				}
			}
		}
	});
	return found_match ? 0 : -1;
}

static bool
procIsTranslated(pid_t pid)
{
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
	struct kinfo_proc info;
	size_t size = sizeof(info);

	if (sysctl(mib, (unsigned)(sizeof(mib) / sizeof(int)),
		&info, &size, NULL, 0) == 0 && size >= sizeof(info)) {
		return info.kp_proc.p_flag & P_TRANSLATED;
	}

	return false;
}


//
// Helper function so that Pcreate & Pgrab can use the same code.
//
// NOTE!
//
// We're doing something really hideous here.
//
// For each target process, there are *two* threads created. A dtrace control thread, and a CoreSymbolication
// dyld listener thread. The listener thread does not directly call into dtrace. The reason is that the thread
// calling into dtrace sometimes gets put to sleep. If dtrace decides to release/deallocate due to a notice
// from the listener thread, it deadlocks waiting for the listener to acknowledge that it has shut down.
static struct ps_prochandle* createProcAndSymbolicator(pid_t pid, task_t task, int* perr, bool read_only) {
	static os_log_t proc_log;
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		proc_log = os_log_create("com.apple.dtrace", "libproc");
	});

	if (procIsTranslated(pid) && !read_only) {
		*perr = APPLE_TRANSLATED_NOTSUP;
		return NULL;
	}

	bool should_queue_proc_activity_notices = !read_only;
	// The symbolicator block captures proc, and actually uses it before completing.
	// We allocate and initialize it first.
	struct ps_prochandle* proc = calloc(sizeof(struct ps_prochandle), 1);
	proc->current_symbol_owner_generation = 1; // MUST start with generation of 1 or higher.	
	proc->status.pr_pid = pid;
			
	(void) pthread_mutex_init(&proc->proc_activity_queue_mutex, NULL);
	(void) pthread_cond_init(&proc->proc_activity_queue_cond, NULL);
	
	// Only enable this if we're going to generate events...
	if (should_queue_proc_activity_notices)
		proc->proc_activity_queue_enabled = true;
	uint32_t flags = kCSSymbolicatorTrackDyldActivity;

	if (_dtrace_disallow_dsym)
		flags |= kCSSymbolicatorDisallowDsymData;

	CSSymbolicatorRef symbolicator = CSSymbolicatorCreateWithTaskFlagsAndNotification(task, flags, ^(uint32_t notification_type, CSNotificationData data) {
		switch (notification_type) {
			case kCSNotificationTaskMain:
				os_log(proc_log, "pid %d: kCSNotificationTaskMain", CSSymbolicatorGetPid(data.symbolicator));
				// We're faking a "POSTINIT" breakpoint here.
				if (should_queue_proc_activity_notices)
					Pcreate_sync_proc_activity(proc, RD_POSTINIT);
				break;

			case kCSNotificationPing:
				os_log(proc_log, "pid %d: kCSNotificationPing (value: %d)", CSSymbolicatorGetPid(data.symbolicator), data.u.ping.value);
				// We're faking a "POSTINIT" breakpoint here.
				if (should_queue_proc_activity_notices)
					Pcreate_sync_proc_activity(proc, RD_POSTINIT);
				break;
				
			case kCSNotificationInitialized:
				os_log(proc_log, "pid %d: kCSNotificationInitialized", CSSymbolicatorGetPid(data.symbolicator));
				// We're faking a "PREINIT" breakpoint here. NOTE! The target is not actually suspended at this point. Racey!
				if (should_queue_proc_activity_notices)
					Pcreate_async_proc_activity(proc, RD_PREINIT);
				break;
				
			case kCSNotificationDyldLoad:
				os_log(proc_log, "pid %d: kCSNotificationDyldLoad %s", CSSymbolicatorGetPid(data.symbolicator), CSSymbolOwnerGetPath(data.u.dyldLoad.symbolOwner));
				if (should_queue_proc_activity_notices)
					Pcreate_sync_proc_activity(proc, RD_DLACTIVITY);
				break;
				
			case kCSNotificationDyldUnload:
				os_log(proc_log, "pid %d: kCSNotificationDyldUnload %s", CSSymbolicatorGetPid(data.symbolicator), CSSymbolOwnerGetPath(data.u.dyldLoad.symbolOwner));
				break;
				
			case kCSNotificationTimeout:
				os_log(proc_log, "pid %d: kCSNotificationTimeout", CSSymbolicatorGetPid(data.symbolicator));
				if (should_queue_proc_activity_notices)
					Pcreate_async_proc_activity(proc, RD_DYLD_LOST);
				break;
				
			case kCSNotificationTaskExit:
				os_log(proc_log, "pid %d: kCSNotificationTaskExit", CSSymbolicatorGetPid(data.symbolicator));
				if (should_queue_proc_activity_notices)
					Pcreate_async_proc_activity(proc, RD_DYLD_EXIT);
				break;
				
			case kCSNotificationFini:
				os_log(proc_log, "pid %d: kCSNotificationFini", CSSymbolicatorGetPid(data.symbolicator));
				break;
				
			default:
				os_log(proc_log, "pid %d: 0x%x UNHANDLED notification from CoreSymbolication", CSSymbolicatorGetPid(data.symbolicator), notification_type);
		}
	});
	if (!CSIsNull(symbolicator)) {
		CSSymbolicatorSubscribeToTaskMainNotification(symbolicator);
		proc->symbolicator = symbolicator; // Starts with a retain count of 1
		proc->status.pr_dmodel = CSArchitectureIs64Bit(CSSymbolicatorGetArchitecture(symbolicator)) ? PR_MODEL_LP64 : PR_MODEL_ILP32;
	} else {
		free(proc);
		proc = NULL;
		*perr = APPLE_PCREATE_BAD_SYMBOLICATOR;
	}
	
	return proc;
}
#endif /* DTRACE_USE_CORESYMBOLICATION */

struct ps_prochandle *
Pxcreate(const char *file,	/* executable file name */
        char *const *argv,	/* argument vector */
        char *const *envp,	/* environment */
        int *perr,		    /* pointer to error return code */
        char *path,		    /* if non-null, holds exec path name on return */
        size_t len, 	    /* size of the path buffer */
        cpu_type_t arch)    /* architecture to launch */
{
#pragma unused(path, len)
#if DTRACE_USE_CORESYMBOLICATION
	struct ps_prochandle* proc = NULL;

	int pid;
	posix_spawnattr_t attr;
	task_t task;

	*perr = posix_spawnattr_init(&attr);
	if (0 != *perr) goto destroy_attr;
	
	if (arch != CPU_TYPE_ANY) {
		*perr = posix_spawnattr_setbinpref_np(&attr, 1, &arch, NULL);
		if (0 != *perr) goto destroy_attr;
	}
	
	*perr = posix_spawnattr_setflags(&attr, POSIX_SPAWN_START_SUSPENDED);
	if (0 != *perr) goto destroy_attr;
	*perr = posix_spawnp(&pid, file, NULL, &attr, argv, envp);
	
destroy_attr:
	posix_spawnattr_destroy(&attr);
	
	if (0 == *perr) {
#if DTRACE_TARGET_APPLE_MAC
		/*
		 * <rdar://problem/13969762>:
		 * If the process is signed with restricted entitlements, the libdtrace_dyld
		 * library will not be injected in the process. In this case we kill the
		 * process and report an error.
		 */
		if (csr_check(CSR_ALLOW_UNRESTRICTED_DTRACE) != 0 && csops(pid, CS_OPS_STATUS, &flags, sizeof(flags)) != -1
			&& (flags & CS_RESTRICT))
		{
			if (kill(pid, SIGKILL))
				perror("kill()");
			*perr = APPLE_EXECUTABLE_RESTRICTED;
			return NULL;
		}
#endif /* DTRACE_TARGET_APPLE_MAC */
		*perr = task_read_for_pid(mach_task_self(), pid, &task);
		if (*perr == KERN_SUCCESS) {
			proc = createProcAndSymbolicator(pid, task, perr, false);
		} else {
			if (kill(pid, SIGKILL))
				perror("kill()");
			*perr = -(*perr); // Make room for mach errors
		}
	}

	return proc;
#else
	return NULL;
#endif /* DTRACE_USE_CORESYMBOLICATION */
}

/*
 * Return a printable string corresponding to a Pcreate() error return.
 */
const char *
Pcreate_error(int error)
{
	const char *str;
	
	switch (error) {
		case APPLE_PCREATE_BAD_SYMBOLICATOR:
			str = "Could not create symbolicator for task";
			break;
		case APPLE_EXECUTABLE_RESTRICTED:
			str = "DTrace cannot control executables signed with restricted entitlements";
			break;
		case APPLE_EXECUTABLE_NOT_ATTACHABLE:
			str = "the current security restriction (system integrity protection enabled) prevents dtrace from attaching to an executable not signed "
			     "with the [com.apple.security.get-task-allow] entitlement";
			break;
		case APPLE_TRANSLATED_NOTSUP:
			str = "DTrace cannot instrument translated processes";
			break;
		default:
			if (error < 0)
				str = mach_error_string(-error);
			else
				str = strerror(error);
			break;
	}
	
	return (str);
}


/*
 * Grab an existing process.
 * Return an opaque pointer to its process control structure.
 *
 * pid:		UNIX process ID.
 * flags:
 *	PGRAB_RETAIN	Retain tracing flags (default clears all tracing flags).
 *	PGRAB_FORCE	Grab regardless of whether process is already traced.
 *	PGRAB_RDONLY	Open the address space file O_RDONLY instead of O_RDWR,
 *                      and do not open the process control file.
 *	PGRAB_NOSTOP	Open the process but do not force it to stop.
 * perr:	pointer to error return code.
 */
/*
 * APPLE NOTE:
 *
 * We don't seem to have an equivalent to the tracing
 * flag(s), so we're also going to punt on them for now.
 */

#define APPLE_PGRAB_BAD_SYMBOLICATOR  0x0F0F0F0E
#define APPLE_PGRAB_UNSUPPORTED_FLAGS 0x0F0F0F0F

struct ps_prochandle *Pgrab(pid_t pid, int flags, int *perr) {
	struct ps_prochandle* proc = NULL;
#if DTRACE_USE_CORESYMBOLICATION
	if (flags & PGRAB_RDONLY || (0 == flags)) {	
		task_t task;

		*perr = task_read_for_pid(mach_task_self(), pid, &task);
		if (*perr == KERN_SUCCESS) {
			if (0 == (flags & PGRAB_RDONLY))
				(void)task_suspend(task);
			
			proc = createProcAndSymbolicator(pid, task, perr, flags & PGRAB_RDONLY);
		}
	} else {
		*perr = APPLE_PGRAB_UNSUPPORTED_FLAGS;
	}
#endif /* DTRACE_USE_CORESYMBOLICATION */
	return proc;
}

const char *Pgrab_error(int err) {
	const char* str;

	switch (err) {
		case APPLE_PGRAB_BAD_SYMBOLICATOR:
			str = "Pgrab could not create symbolicator for pid";
			break;
		case APPLE_PGRAB_UNSUPPORTED_FLAGS:
			str = "Pgrab was called with unsupported flags";
			break;
		case APPLE_EXECUTABLE_NOT_ATTACHABLE:
			str = "the current security restriction (system integrity protection enabled) prevents dtrace from attaching to an executable not signed "
			      "with the [com.apple.security.get-task-allow] entitlement";
			break;
		case APPLE_TRANSLATED_NOTSUP:
			str = "DTrace cannot instrument translated processes";
			break;
		default:
			str = mach_error_string(err);
	}
	
	return str;
}

/*
 * Release the process.  Frees the process control structure.
 * flags:
 *	PRELEASE_HANG	Leave the process stopped and abandoned.
 *	PRELEASE_KILL	Terminate the process with SIGKILL.
 */
/*
 * APPLE NOTE:
 *
 * We're ignoring most flags for now. They will eventually need to be honored.
 */
void Prelease(struct ps_prochandle *P, int flags)
{
#if DTRACE_USE_CORESYMBOLICATION
	if (0 == flags) {
		if (P->status.pr_flags & PR_KLC)
			(void)kill(P->status.pr_pid, SIGKILL);
	} else if (flags & PRELEASE_KILL) {
		(void)kill(P->status.pr_pid, SIGKILL);
	} else if (flags & PRELEASE_HANG)
		(void)kill(P->status.pr_pid, SIGSTOP);

	// Shouldn't be leaking events. Do this before releasing the symbolicator,
	// so the dyld activity thread isn't blocked waiting on an event.
	pthread_mutex_lock(&P->proc_activity_queue_mutex);
	// Prevent any new events from being queue'd
	P->proc_activity_queue_enabled = false;
	// Destroy any existing events.
	struct ps_proc_activity_event* temp = P->proc_activity_queue;
	while (temp != NULL) {
		struct ps_proc_activity_event* next = temp->next;
		Pdestroy_proc_activity(temp);
		temp = next;
	}
	pthread_mutex_unlock(&P->proc_activity_queue_mutex);
	
	// We don't have to check for kCSNull...
	CSRelease(P->symbolicator);
	
	(void) pthread_mutex_destroy(&P->proc_activity_queue_mutex);
	(void) pthread_cond_destroy(&P->proc_activity_queue_cond);

	free(P);
#endif /* DTRACE_USE_CORESYMBOLICATION */
}

/*
 * APPLE NOTE:
 *
 * These low-level breakpoint functions are no-ops. We expect dyld to make RPC
 * calls to give us roughly the same functionality.
 *
 */
int
Psetbkpt(struct ps_prochandle *P, uintptr_t addr, ulong_t *instr)
{
#pragma unused(P, addr, instr)
	return 0;
}

int
Pdelbkpt(struct ps_prochandle *P, uintptr_t addr, ulong_t instr)
{
#pragma unused(P, addr, instr)
	return 0;
}


/*
 * APPLE NOTE:
 *
 * Psetflags/Punsetflags has three caller values at this time.
 * PR_KLC - proc kill on last close
 * PR_RLC - proc resume/run on last close
 * PR_BPTAD - x86 only, breakpoint adjust eip
 *
 * We are not supporting any of these at this time.
 */

int Psetflags(struct ps_prochandle *P, long flags) {
	P->status.pr_flags |= flags;
	return 0;
}

int Punsetflags(struct ps_prochandle *P, long flags) {
	P->status.pr_flags &= ~flags;
	return 0;
}

#if DTRACE_USE_CORESYMBOLICATION
/*
 * When processing new modules loaded in a process, we only want to look at
 * what has changed since the last processing attempt.
 *
 * We work through "generations of symbol owners. Dyld may load library after
 * library with the same load timestamp. So we mark the symbol owners with a "generation"
 * and only look at those that are unmarked, or are the current generation.
 *
 * This is a Darwin-specific optimization.
 */
static int
update_check_symbol_owner_gen(struct ps_prochandle *P, CSSymbolOwnerRef owner, bool err_on_mismatched_gen)
{
	uintptr_t generation = CSSymbolOwnerGetTransientUserData(owner);
	if (generation == 0) {
		CSSymbolOwnerSetTransientUserData(owner, P->current_symbol_owner_generation);
		return 0;
	}
	if (err_on_mismatched_gen && generation != P->current_symbol_owner_generation) {
		return -1;
	}
	return 0;
}

static int
update_check_symbol_gen(struct ps_prochandle *P, CSSymbolRef symbol, bool err_on_mismatched_gen)
{
	CSSymbolOwnerRef owner = CSSymbolGetSymbolOwner(symbol);
	return update_check_symbol_owner_gen(P, owner, err_on_mismatched_gen);
}

#if defined(__arm__) || defined(__arm64__)
static uint32_t
get_arm_arch_subinfo(CSSymbolRef symbol)
{
	assert(!CSIsNull(symbol));
	CSSymbolOwnerRef owner = CSSymbolGetSymbolOwner(symbol);
	CSArchitecture arch = CSSymbolOwnerGetArchitecture(owner);

	if (CSArchitectureMatchesArchitecture(arch, kCSArchitectureArm64_32)) {
		return FASTTRAP_FN_ARM64_32;
	}
	else if (CSArchitectureMatchesArchitecture(arch, kCSArchitectureArm64)) {
		return FASTTRAP_FN_ARM64;
	}
	else if (CSSymbolIsThumb(symbol)) {
		return FASTTRAP_FN_THUMB;
	}
	else if (CSSymbolIsArm(symbol)) {
		return FASTTRAP_FN_ARM;
	}
	return 0;
}
#endif
#endif /* DTRACE_USE_CORESYMBOLICATION */


/*
 * Search the process symbol tables looking for a symbol whose name matches the
 * specified name and whose object and link map optionally match the specified
 * parameters.  On success, the function returns 0 and fills in the GElf_Sym
 * symbol table entry.  On failure, -1 is returned.
 */
/*
 * APPLE NOTE:
 *
 * We're completely blowing off the lmid.
 *
 * It looks like the only GElf_Sym entries used are value & size.
 * Most of the time, prsyminfo_t is null, and when it is used, only
 * prs_lmid is set.
 */
static int
Pxlookup_by_name_sym(
		     struct ps_prochandle *P,
		     Lmid_t lmid,		/* link map to match, or -1 (PR_LMID_EVERY) for any */
		     const char *oname,		/* load object name */
		     const char *sname,		/* symbol name */
		     GElf_Sym *symp,		/* returned symbol table entry */
		     prsyminfo_t *sip,		/* returned symbol info */
		     bool only_current_gen)
{
#pragma unused(lmid)
#if DTRACE_USE_CORESYMBOLICATION
	__block CSSymbolRef symbol = kCSNull;
	if (oname != NULL) {
		if (_dtrace_mangled) {
			forEachSymbolOwner(P->symbolicator, oname, ^(CSSymbolOwnerRef owner) {
				CSSymbolOwnerForeachSymbolWithMangledName(owner, sname, ^(CSSymbolRef s) { if (CSIsNull(symbol)) symbol = s; });
			});
		} else {
			forEachSymbolOwner(P->symbolicator, oname, ^(CSSymbolOwnerRef owner) {
				CSSymbolOwnerForeachSymbolWithName(owner, sname, ^(CSSymbolRef s) { if (CSIsNull(symbol)) symbol = s; });
			});
		}
	} else {
		if (_dtrace_mangled) {
			CSSymbolicatorForeachSymbolWithMangledNameAtTime(P->symbolicator, sname, kCSNow, ^(CSSymbolRef s) { if (CSIsNull(symbol)) symbol = s; });
		} else {
			CSSymbolicatorForeachSymbolWithNameAtTime(P->symbolicator, sname, kCSNow, ^(CSSymbolRef s) { if (CSIsNull(symbol)) symbol = s; });
		}
	}

	if (CSIsNull(symbol)) {
		return PLOOKUP_NOT_FOUND;
	}

	// Filter out symbols we do not want to instrument
	if (CSSymbolIsDyldStub(symbol) || !CSSymbolIsFunction(symbol)) {
		return PLOOKUP_NOT_FOUND;
	}

	 // Check that the symbol we found is part of the current generation
	if (update_check_symbol_gen(P, symbol, only_current_gen) != 0) {
		return PLOOKUP_WRONG_GEN;
	}

	if (symp) {
		CSRange addressRange = CSSymbolGetRange(symbol);

		symp->st_name = 0;
		symp->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_FUNC));
#if defined(__arm__) || defined(__arm64__)
		symp->st_arch_subinfo = get_arm_arch_subinfo(symbol);
#endif
		symp->st_other = 0;
		symp->st_shndx = SHN_MACHO;
		symp->st_value = addressRange.location;
		symp->st_size = addressRange.length;
	}

	if (sip) {
		sip->prs_lmid = LM_ID_BASE;
	}
	return 0;
#else
	return PLOOKUP_NOT_FOUND;
#endif /* DTRACE_USE_CORESYMBOLICATION */
}

int
Pxlookup_by_name_new_syms(
		     struct ps_prochandle *P,
		     Lmid_t lmid,		/* link map to match, or -1 (PR_LMID_EVERY) for any */
		     const char *oname,		/* load object name */
		     const char *sname,		/* symbol name */
		     GElf_Sym *symp,		/* returned symbol table entry */
		     prsyminfo_t *sip)		/* returned symbol info */
{
	return Pxlookup_by_name_sym(P, lmid, oname, sname, symp, sip, true);
}


int
Pxlookup_by_name(
		     struct ps_prochandle *P,
		     Lmid_t lmid,		/* link map to match, or -1 (PR_LMID_EVERY) for any */
		     const char *oname,		/* load object name */
		     const char *sname,		/* symbol name */
		     GElf_Sym *symp,		/* returned symbol table entry */
		     prsyminfo_t *sip)		/* returned symbol info */
{
	return Pxlookup_by_name_sym(P, lmid, oname, sname, symp, sip, false);
}
/*
 * Search the process symbol tables looking for a symbol whose
 * value to value+size contain the address specified by addr.
 * Return values are:
 *	sym_name_buffer containing the symbol name
 *	GElf_Sym symbol table entry
 *	prsyminfo_t ancillary symbol information
 * Returns 0 on success, -1 on failure.
 */
/*
 * APPLE NOTE:
 *
 * This function is called directly by the plockstat binary and it passes the
 * psyminfo_t argument We only set fields that plockstat actually uses.
 * sip->prs_table and sip->prs_id are not used by plockstat so we don't
 * attempt to set them.
 */
int
Pxlookup_by_addr(
                 struct ps_prochandle *P,
                 mach_vm_address_t addr,			/* process address being sought */
                 char *sym_name_buffer,		/* buffer for the symbol name */
                 size_t bufsize,		/* size of sym_name_buffer */
                 GElf_Sym *symbolp,		/* returned symbol table entry */
                 prsyminfo_t *sip)		/* returned symbol info (used only by plockstat) */
{
	int err = -1;

#if DTRACE_USE_CORESYMBOLICATION
	CSSymbolRef symbol = CSSymbolicatorGetSymbolWithAddressAtTime(P->symbolicator, (mach_vm_address_t)addr, kCSNow);
	
	// See comments in Ppltdest()
	// Filter out symbols we do not want to instrument
	// if ([symbol isDyldStub]) symbol = nil;
	// if (![symbol isFunction]) symbol = nil;
	
	if (!CSIsNull(symbol)) {
		if (CSSymbolIsUnnamed(symbol)) {
			if (CSArchitectureIs64Bit(CSSymbolOwnerGetArchitecture(CSSymbolGetSymbolOwner(symbol))))
				snprintf(sym_name_buffer, bufsize, "0x%016llx", CSSymbolGetRange(symbol).location);
			else
				snprintf(sym_name_buffer, bufsize, "0x%08llx", CSSymbolGetRange(symbol).location);
		} else {
			if (_dtrace_mangled) {
				const char *mangledName = CSSymbolGetMangledName(symbol);
				if (strlen(mangledName) >= 3 &&
				    mangledName[0] == '_' &&
				    mangledName[1] == '_' &&
				    mangledName[2] == 'Z') {
					// mangled name - use it
					strncpy(sym_name_buffer, mangledName, bufsize);
				} else {
					strncpy(sym_name_buffer, CSSymbolGetName(symbol), bufsize);
				}
			} else
				strncpy(sym_name_buffer, CSSymbolGetName(symbol), bufsize);
		}
		err = 0;
		
		if (symbolp) {
			CSRange addressRange = CSSymbolGetRange(symbol);
			
			symbolp->st_name = 0;
			symbolp->st_info = GELF_ST_INFO((STB_GLOBAL), (STT_FUNC));
			symbolp->st_other = 0;
#if defined(__arm__) || defined(__arm64__)
			symbolp->st_arch_subinfo = get_arm_arch_subinfo(symbol);

#endif
			symbolp->st_shndx = SHN_MACHO;
			symbolp->st_value = addressRange.location;
			symbolp->st_size = addressRange.length;
		}
		
		if (sip) {
			CSSymbolOwnerRef owner = CSSymbolGetSymbolOwner(symbol);
			sip->prs_name = (bufsize == 0 ? NULL : sym_name_buffer);
			
			sip->prs_object = CSSymbolOwnerGetName(owner);
			
			// APPLE: The following fields set by Solaris code are not used by
			// plockstat, hence we don't return them.
			//sip->prs_id = (symp == sym1p) ? i1 : i2;
			//sip->prs_table = (symp == sym1p) ? PR_SYMTAB : PR_DYNSYM;
			
			// FIXME:!!!
			//sip->prs_lmid = (fptr->file_lo == NULL) ? LM_ID_BASE : fptr->file_lo->rl_lmident;
			sip->prs_lmid = LM_ID_BASE;
		}
	}
#endif /* DTRACE_USE_CORESYMBOLICATION */
	return err;
}


int Plookup_by_addr(struct ps_prochandle *P, mach_vm_address_t addr, char *buf, size_t size, GElf_Sym *symp) {
	return Pxlookup_by_addr(P, addr, buf, size, symp, NULL);
}

/*
 * APPLE NOTE:
 *
 * We're just calling task_resume(). Returns 0 on success, -1 on failure.
 */
int Psetrun(struct ps_prochandle *P,
	    int sig,	/* Ignored in OS X. Nominally supposed to be the signal passed to the target process */
	    int flags	/* Ignored in OS X. PRSTEP|PRSABORT|PRSTOP|PRCSIG|PRCFAULT */)
{
#pragma unused(sig, flags)
#if DTRACE_USE_CORESYMBOLICATION
	/* If PR_KLC is set, we created the process with posix_spawn(); otherwise we grabbed it with task_suspend. */
	if (P->status.pr_flags & PR_KLC)
		return kill(P->status.pr_pid, SIGCONT); // Advances BSD p_stat from SSTOP to SRUN
	else
		return (int)task_resume(CSSymbolicatorGetTask(P->symbolicator));
#else
	return (0);
#endif /* DTRACE_USE_CORESYMBOLICATION */
}

ssize_t Pread(struct ps_prochandle *P, void *buf, size_t nbyte, mach_vm_address_t address) {
	vm_offset_t mapped_address;
	mach_msg_type_number_t mapped_size;
	ssize_t bytes_read = 0;

#if DTRACE_USE_CORESYMBOLICATION
	kern_return_t err = mach_vm_read(CSSymbolicatorGetTask(P->symbolicator), (mach_vm_address_t)address, (mach_vm_size_t)nbyte, &mapped_address, &mapped_size);
	if (! err) {
		bytes_read = nbyte;
		memcpy(buf, (void*)mapped_address, nbyte);
		vm_deallocate(mach_task_self(), (vm_address_t)mapped_address, (vm_size_t)mapped_size);
	}
#endif /* DTRACE_USE_CORESYMBOLICATION */
	return bytes_read;
}

static int
Pobject_iter_symbols(struct ps_prochandle *P, proc_map_f *func, void *cd, bool only_current_gen)
{
	__block int err = 0;
#if DTRACE_USE_CORESYMBOLICATION
	CSSymbolicatorForeachSymbolOwnerAtTime(P->symbolicator, kCSNow, ^(CSSymbolOwnerRef owner) {
		if (update_check_symbol_owner_gen(P, owner, only_current_gen) == 0) {
			if (err) return; // skip everything after error

			prmap_t map;
			const char* name = CSSymbolOwnerGetName(owner);
			map.pr_vaddr = CSSymbolOwnerGetBaseAddress(owner);
			map.pr_mflags = MA_READ;

			err = func(cd, &map, name);
		}
	});
#endif /* DTRACE_USE_CORESYMBOLICATION */
	return err;
}

int Pobject_iter(struct ps_prochandle *P, proc_map_f *func, void *cd) {
	return Pobject_iter_symbols(P, func, cd, false);
}

int Pobject_iter_new_syms(struct ps_prochandle *P, proc_map_f *func, void *cd) {
	return Pobject_iter_symbols(P, func, cd, true);
}

// The solaris version of XYZ_to_map() didn't require the prmap_t* map argument.
// They relied on their backing store to allocate and manage the prmap_t's. We don't
// have an equivalent, and these are cheaper to fill in on the fly than to store.

const prmap_t *Paddr_to_map(struct ps_prochandle *P, mach_vm_address_t addr, prmap_t* map) {        
#if DTRACE_USE_CORESYMBOLICATION
	CSSymbolOwnerRef owner = CSSymbolicatorGetSymbolOwnerWithAddressAtTime(P->symbolicator, addr, kCSNow);
	
	// <rdar://problem/4877551>
	if (!CSIsNull(owner)) {	
		map->pr_vaddr = CSSymbolOwnerGetBaseAddress(owner);                        
		map->pr_mflags = MA_READ; // Anything we get from a symbolicator is readable
		
		return map;
	}
#endif /* DTRACE_USE_CORESYMBOLICATION */
	return NULL;
}

const prmap_t *Pname_to_map(struct ps_prochandle *P, const char *name, prmap_t* map) {
	return (Plmid_to_map(P, PR_LMID_EVERY, name, map));
}

/*
 * Given a shared object name, return the map_info_t for it.  If no matching
 * object is found, return NULL.  Normally, the link maps contain the full
 * object pathname, e.g. /usr/lib/libc.so.1.  We allow the object name to
 * take one of the following forms:
 *
 * 1. An exact match (i.e. a full pathname): "/usr/lib/libc.so.1"
 * 2. An exact basename match: "libc.so.1"
 * 3. An initial basename match up to a '.' suffix: "libc.so" or "libc"
 * 4. The literal string "a.out" is an alias for the executable mapping
 *
 * The third case is a convenience for callers and may not be necessary.
 *
 * As the exact same object name may be loaded on different link maps (see
 * dlmopen(3DL)), we also allow the caller to resolve the object name by
 * specifying a particular link map id.  If lmid is PR_LMID_EVERY, the
 * first matching name will be returned, regardless of the link map id.
 */
/*
 * APPLE NOTE:
 * 
 * It appears there are only 2 uses of this currently. A check for ld.so (dtrace fails against static exe's),
 * and a test for a.out'ness. dtrace looks up the map for a.out, and the map for the module, and makes sure
 * they share the same v_addr.
 */

const prmap_t*
Plmid_to_map(struct ps_prochandle *P, Lmid_t ignored, const char *cname, prmap_t* map)
{
#pragma unused(ignored)
#if DTRACE_USE_CORESYMBOLICATION
	// Need to handle some special case defines
	if (cname == PR_OBJ_LDSO)
		cname = "dyld";

	__block CSSymbolOwnerRef owner = kCSNull;

	int err = forEachSymbolOwner(P->symbolicator, cname, ^(CSSymbolOwnerRef o) {
		if (CSIsNull(owner)) {
			owner = o;
		}
	});

	if (err) {
		return NULL;
	}

	map->pr_vaddr = CSSymbolOwnerGetBaseAddress(owner);
	map->pr_mflags = MA_READ; // Anything we get from a symbolicator is readable

	return map;
#else
	return NULL;
#endif /* DTRACE_USE_CORESYMBOLICATION */
}

/*
 * Given a virtual address, return the name of the underlying
 * mapped object (file), as provided by the dynamic linker.
 * Return NULL on failure (no underlying shared library).
 */
char
*Pobjname(struct ps_prochandle *P, mach_vm_address_t addr, char *buffer, size_t bufsize)
{
#if DTRACE_USE_CORESYMBOLICATION
	CSSymbolOwnerRef owner = CSSymbolicatorGetSymbolOwnerWithAddressAtTime(P->symbolicator, addr, kCSNow);
	if (!CSIsNull(owner)) {
		strncpy(buffer, CSSymbolOwnerGetPath(owner), bufsize);
		buffer[bufsize-1] = 0; // Make certain buffer is NULL terminated.
		return buffer;
	}
#endif /* DTRACE_USE_CORESYMBOLICATION */
	buffer[0] = 0;
	return NULL;
}

/*
 * Given a virtual address, return the link map id of the underlying mapped
 * object (file), as provided by the dynamic linker.  Return -1 on failure.
 */
/*
 * APPLE NOTE:
 *
 * We are treating everything as being in the base map, so no work is needed.
 */
int
Plmid(struct ps_prochandle *P, mach_vm_address_t addr, Lmid_t *lmidp)
{
#pragma unused(P, addr)
	*lmidp = LM_ID_BASE;
	return 0;
}

/*
 * This is an Apple only proc method. It is used by the objc provider,
 * to iterate all classes and methods.
 */
static int
Pobjc_method_iter_syms(struct ps_prochandle *P, proc_objc_f *func, void *cd, bool only_current_gen) {
	__block int err = 0;
#if DTRACE_USE_CORESYMBOLICATION
	CSSymbolicatorForeachSymbolOwnerAtTime(P->symbolicator, kCSNow, ^(CSSymbolOwnerRef owner) {
		if (update_check_symbol_owner_gen(P, owner, only_current_gen) == 0) {
			if (err) return; // Have to bail out on error condition
			
			CSSymbolOwnerForeachSymbol(owner, ^(CSSymbolRef symbol) {

				if (err) return; // Have to bail out on error condition
				
				if (CSSymbolIsObjcMethod(symbol)) {
					GElf_Sym gelf_sym;
					CSRange addressRange = CSSymbolGetRange(symbol);

					gelf_sym.st_name = 0;
					gelf_sym.st_info = GELF_ST_INFO((STB_GLOBAL), (STT_FUNC));
					gelf_sym.st_other = 0;
#if defined(__arm__) || defined(__arm64__)
					gelf_sym.st_arch_subinfo = get_arm_arch_subinfo(symbol);
#endif
					gelf_sym.st_shndx = SHN_MACHO;
					gelf_sym.st_value = addressRange.location;
					gelf_sym.st_size = addressRange.length;

					const char* symbolName = CSSymbolGetName(symbol);
					size_t symbolNameLength = strlen(symbolName);

					// First find the split point
					size_t split_index = 0;
					while (symbolName[split_index] != ' ' && symbolName[split_index] != 0)
						split_index++;
					
					if (split_index < symbolNameLength) {
						// We know the combined length will be +1 byte for an extra NULL, and -3 for  no '[', ']', or ' '
						char backingStore[256];
						char* className = (symbolNameLength < sizeof(backingStore)) ? backingStore : malloc(symbolNameLength);
						
						// Class name range is [2, split_index)
						size_t classNameLength = &symbolName[split_index] - &symbolName[2];
						strncpy(className, &symbolName[2], classNameLength);
						
						// method name range is [split_index+1, length-1)
						char* methodName = &className[classNameLength];
						*methodName++ = 0; // Null terminate the className string;
						*methodName++ = symbolName[0]; // Apply the -/+ instance/class modifier.
						size_t methodNameLength = &symbolName[symbolNameLength] - &symbolName[split_index+1] - 1;
						strncpy(methodName, &symbolName[split_index+1], methodNameLength);
						methodName[methodNameLength] = 0; // Null terminate!
						methodName -= 1; // Move back to cover the modifier.
						
						err = func(cd, &gelf_sym, className, methodName);
						
						// Free any memory we had to allocate
						if (className != backingStore)
							free(className);					
					}
				}
			});
		}
	});
#endif /* DTRACE_USE_CORESYMBOLICATION */
	return err;
}

int
Pobjc_method_iter_new_syms(struct ps_prochandle *P, proc_objc_f *func, void *cd) {
	return Pobjc_method_iter_syms(P, func, cd, true);
}

int
Pobjc_method_iter(struct ps_prochandle *P, proc_objc_f *func, void *cd) {
	return Pobjc_method_iter_syms(P, func, cd, false);
}
/*
 * APPLE NOTE: 
 *
 * object_name == CSSymbolOwner name
 * which == PR_SYMTAB || PR_DYNSYM 
 * mask == BIND_ANY | TYPE_FUNC (Binding type and func vs data?)
 * cd = caller data, pass through
 *
 * If which is not PR_SYMTAB, return success without doing any work
 * We're ignoring the binding type, but honoring TYPE_FUNC
 *
 * Note that we do not actually iterate in address order!
 */

static int
Psymbol_iter_by_addr_syms(struct ps_prochandle *P, const char *object_name,
     int which, int mask, proc_sym_f *func, void *cd, bool only_current_gen)
{
	__block int err = 0;
#if DTRACE_USE_CORESYMBOLICATION
	if (which != PR_SYMTAB)
		return err;

	err = forEachSymbolOwner(P->symbolicator, object_name, ^(CSSymbolOwnerRef owner) {
		if (update_check_symbol_owner_gen(P, owner, only_current_gen) == 0) {
			CSSymbolOwnerForeachSymbol(owner, ^(CSSymbolRef symbol) {
				if (err)
					return; // Bail out on error.
				
				if (CSSymbolIsDyldStub(symbol)) // We never instrument dyld stubs
					return;
				
				if (CSSymbolIsUnnamed(symbol)) // Do not use symbols with NULL names
					return;
				
				if ((mask & TYPE_FUNC) && !CSSymbolIsFunction(symbol))
					return;
				GElf_Sym gelf_sym;
				CSRange addressRange = CSSymbolGetRange(symbol);
				
				gelf_sym.st_name = 0;
				gelf_sym.st_info = GELF_ST_INFO((STB_GLOBAL), (STT_FUNC));
				gelf_sym.st_other = 0;
#if defined(__arm__) || defined(__arm64__)
				gelf_sym.st_arch_subinfo = get_arm_arch_subinfo(symbol);
#endif
				gelf_sym.st_shndx = SHN_MACHO;
				gelf_sym.st_value = addressRange.location;
				gelf_sym.st_size = addressRange.length;
				
				const char *mangledName;
				if (_dtrace_mangled &&
				    (mangledName = CSSymbolGetMangledName(symbol)) &&
				    strlen(mangledName) >= 3 &&
				    mangledName[0] == '_' &&
				    mangledName[1] == '_' &&
				    mangledName[2] == 'Z') {
					// mangled name - use it
					err = func(cd, &gelf_sym, mangledName);
				} else {
					err = func(cd, &gelf_sym, CSSymbolGetName(symbol));
				}
			});
		}
	});
#endif /* DTRACE_USE_CORESYMBOLICATION */
	return err;
}

int
Psymbol_iter_by_addr_new_syms(struct ps_prochandle *P, const char *object_name,
     int which, int mask, proc_sym_f *func, void *cd)
{
	return Psymbol_iter_by_addr_syms(P, object_name, which, mask, func,
	       cd, true);
}

int
Psymbol_iter_by_addr(struct ps_prochandle *P, const char *object_name,
     int which, int mask, proc_sym_f *func, void *cd)
{
	return Psymbol_iter_by_addr_syms(P, object_name, which, mask, func,
	       cd, false);
}

void
Pupdate_maps(struct ps_prochandle *P)
{
#pragma unused(P)
}

//
// This method is called after dtrace has handled dyld activity.
// It should "checkpoint" the timestamp of the most recently processed library.
// Following invocations to "iter_by_xyz()" should only process libraries newer
// than the checkpoint time.
//
void
Pcheckpoint_syms(struct ps_prochandle *P)
{
	// In future iterations of the symbolicator, we will only process symbol owners older than what we have already seen.
	P->current_symbol_owner_generation++;
}

void Pupdate_syms(struct ps_prochandle *P) {    
#pragma unused(P)
}

/*
 * Given an address, Ppltdest() determines if this is part of a PLT, and if
 * so returns a pointer to the symbol name that will be used for resolution.
 * If the specified address is not part of a PLT, the function returns NULL.
 */
/*
 * APPLE NOTE:
 *
 * I believe a PLT is equivalent to our dyld stubs. This method is difficult
 * to implement properly, there isn't a good place to free a string allocated
 * here. Currently, the only use of this method does not actually use the
 * returned string, it simply checks !NULL.
 *
 * We're cheating in one other way. Plookup_by_addr() is used in dt_pid.c to
 * match functions to instrument in the pid provider. For that use, we want to
 * screen out (!functions && dyldStubs). However, the stack backtrace also uses
 * Plookup_by_addr(), and should be able to find dyldStubs. 
 * 
 * Because dt_pid.c calls this method just before lookups, we're going to overload
 * it to also screen out !functions.
 */
const char *Ppltdest(struct ps_prochandle *P, mach_vm_address_t addr) {
	const char* err = NULL;
#if DTRACE_USE_CORESYMBOLICATION
	CSSymbolRef symbol = CSSymbolicatorGetSymbolWithAddressAtTime(P->symbolicator, addr, kCSNow);
	
	// Do not allow dyld stubs, !functions, or commpage entries (<rdar://problem/4877551>)
	if (!CSIsNull(symbol) && (CSSymbolIsDyldStub(symbol) || !CSSymbolIsFunction(symbol)))
		err = "Ppltdest is not implemented";
#endif /* DTRACE_USE_CORESYMBOLICATION */
	return err;
}

rd_agent_t *Prd_agent(struct ps_prochandle *P) {
	return (rd_agent_t *)P; // rd_agent_t is type punned onto ps_proc_handle (see below).
}

static void
Penqueue_proc_activity(struct ps_prochandle* P, struct ps_proc_activity_event* activity)
{
	pthread_mutex_lock(&P->proc_activity_queue_mutex);
	
	if (P->proc_activity_queue_enabled) {
		// Events are processed in order. Add the new activity to the end.
		if (P->proc_activity_queue) {
			struct ps_proc_activity_event* temp = P->proc_activity_queue;
			while (temp->next != NULL) {
				temp = temp->next;
			}
			temp->next = activity;
		} else {
			P->proc_activity_queue = activity;
		}
	} else {
		// The queue is disabled. destroy the events.
		Pdestroy_proc_activity(activity);
	}
	
	pthread_cond_broadcast(&P->proc_activity_queue_cond);
	pthread_mutex_unlock(&P->proc_activity_queue_mutex);
}

void Pcreate_async_proc_activity(struct ps_prochandle* P, rd_event_e type)
{
	struct ps_proc_activity_event* activity = malloc(sizeof(struct ps_proc_activity_event));
	
	activity->rd_event.type = type;
	activity->rd_event.u.state = RD_CONSISTENT;
	activity->synchronous = false;
	activity->destroyed = false;
	activity->next = NULL;
	
	Penqueue_proc_activity(P, activity);	
}

void Pcreate_sync_proc_activity(struct ps_prochandle* P, rd_event_e type)
{
	// Sync proc activity can use stack allocation.
	struct ps_proc_activity_event activity;
	
	activity.rd_event.type = type;
	activity.rd_event.u.state = RD_CONSISTENT;
	activity.synchronous = true;
	activity.destroyed = false;
	activity.next = NULL;
	
	pthread_mutex_init(&activity.synchronous_mutex, NULL);
	pthread_cond_init(&activity.synchronous_cond, NULL);
	
	Penqueue_proc_activity(P, &activity);	

	// Now wait for the activity to be processed.
	pthread_mutex_lock(&activity.synchronous_mutex);
	while (!activity.destroyed) {
		pthread_cond_wait(&activity.synchronous_cond, &activity.synchronous_mutex);
	}
	pthread_mutex_unlock(&activity.synchronous_mutex);

	pthread_mutex_destroy(&activity.synchronous_mutex);
	pthread_cond_destroy(&activity.synchronous_cond);
	
	return;
}

void* Pdequeue_proc_activity(struct ps_prochandle* P)
{
	struct ps_proc_activity_event* ret = NULL;
	
	pthread_mutex_lock(&P->proc_activity_queue_mutex);
	
	while (P->proc_activity_queue_enabled && !P->proc_activity_queue)
		pthread_cond_wait(&P->proc_activity_queue_cond, &P->proc_activity_queue_mutex);

	// Did we get anything?
	if ((ret = P->proc_activity_queue)) {
		P->proc_activity_queue = ret->next;
		P->rd_event = ret->rd_event;		
	}
	
	pthread_mutex_unlock(&P->proc_activity_queue_mutex);
	
	// RD_NONE is used to wakeup the control thread.
	// Return NULL to indicate that.
	if (ret && (ret->rd_event.type == RD_NONE)) {
		Pdestroy_proc_activity((void*)ret);
		ret = NULL;
	}
	
	return ret;	
}

void Pdestroy_proc_activity(void* opaque)
{
	if (opaque) {
		struct ps_proc_activity_event* activity = (struct ps_proc_activity_event*)opaque;
		
		// sync events own their memory, we just notify them.
		if (activity->synchronous) {
			pthread_mutex_lock(&activity->synchronous_mutex);
			activity->destroyed = true;
			pthread_cond_broadcast(&activity->synchronous_cond);
			pthread_mutex_unlock(&activity->synchronous_mutex);
		} else {
			free(activity);
		}
	}
}

//
// Shims for Solaris run-time loader SPI.
// The (opaque) rd_agent type is type punned onto ps_prochandle.
// "breakpoint" addresses corresponding to rd_events are mapped to the event type (a small enum value),
// that's sufficient to uniquely identify the event.
//

rd_err_e rd_event_getmsg(rd_agent_t *oo7, rd_event_msg_t *rdm) {
	struct ps_prochandle *P = (struct ps_prochandle *)oo7;
	
	*rdm = P->rd_event;
	return RD_OK;
}


psaddr_t rd_event_mock_addr(struct ps_prochandle *P);
/*
 * Procedural replacement called from dt_proc_bpmatch() to simulate "psp->pr_reg[R_PC]"
 */
psaddr_t rd_event_mock_addr(struct ps_prochandle *P)
{
	return (psaddr_t)(P->rd_event.type);
}

rd_err_e
rd_event_enable(rd_agent_t *oo7, int onoff)
{
#pragma unused(oo7, onoff)
	return RD_OK;
}

rd_err_e rd_event_addr(rd_agent_t *oo7, rd_event_e ev, rd_notify_t *rdn)
{
#pragma unused(oo7)
	rdn->type = RD_NOTIFY_BPT;
	rdn->u.bptaddr = (psaddr_t)ev;
	
	return RD_OK;
}

char *rd_errstr(rd_err_e err) {
	switch (err) {
		case RD_OK:
			return "RD_OK";
		default:
			return "RD_UNKNOWN";
	}
	/* NOTREACHED */
}

extern int proc_pidinfo(int pid, int flavor, uint64_t arg,  void *buffer, int buffersize);

int Pstate(struct ps_prochandle *P) {
	int retval = 0;
	struct proc_bsdinfo pbsd;
	
	// Overloaded usage. If we received an explicity LOST/EXIT, dont querry proc_pidinfo
	if (P->rd_event.type == RD_DYLD_LOST)
		return PS_LOST;
	
	if (P->rd_event.type == RD_DYLD_EXIT)
		return PS_UNDEAD;
	
	retval = proc_pidinfo(P->status.pr_pid, PROC_PIDTBSDINFO, (uint64_t)0, &pbsd, sizeof(struct proc_bsdinfo));
	if (retval == -1) {
		return -1;
	} else if (retval == 0) {
		return PS_LOST;
	} else {
		switch(pbsd.pbi_status) {
			case SIDL:
				return PS_IDLE;
			case SRUN:
				return PS_RUN;
			case SSTOP:
				return PS_STOP;
			case SZOMB:
				return PS_UNDEAD;
			case SSLEEP:
				return PS_RUN;
			default:
				return -1;
		}
	}
	/* NOTREACHED */
}

const pstatus_t *Pstatus(struct ps_prochandle *P) {
	return &P->status;
}
