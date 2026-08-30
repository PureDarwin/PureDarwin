#ifndef XNU_DARWINTEST_UTILS_H
#define XNU_DARWINTEST_UTILS_H

#include <stdbool.h>
#include <sys/types.h>

/* Misc. utility functions for writing darwintests. */
bool is_development_kernel(void);

/* Returns true if the Secure Page Table Monitor (SPTM) is present on the system. */
bool is_sptm_enabled(void);

/* Returns true if system-wide codesigning policy allows programs to use MAP_JIT with mmap(). */
bool is_map_jit_allowed(void);

/*
 * Returns true if the process is translated according to sysctl.proc_translated.
 * For example, Rosetta processes are translated processes.
 */
bool process_is_translated(void);

/* Launches the given helper variant as a managed process. */
pid_t launch_background_helper(
	const char* variant,
	bool start_suspended,
	bool memorystatus_managed);
/*
 * Set the process's managed bit, so that the memorystatus subsystem treats
 * this process like an app instead of a sysproc.
 */
void set_process_memorystatus_managed(pid_t pid);

#define XNU_T_META_SOC_SPECIFIC T_META_TAG("SoCSpecific")

#define XNU_T_META_REQUIRES_DEVELOPMENT_KERNEL T_META_REQUIRES_SYSCTL_EQ("kern.development", 1)
#define XNU_T_META_REQUIRES_RELEASE_KERNEL T_META_REQUIRES_SYSCTL_EQ("kern.development", 0)

#endif /* XNU_DARWINTEST_UTILS_H */
