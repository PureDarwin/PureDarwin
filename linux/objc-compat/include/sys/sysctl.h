// sys/sysctl.h for Argon on Linux: the names PureDarwin's apps read, answered from sysconf and /proc
#pragma once
#include <stddef.h>
#include <sys/time.h>
#define CTL_KERN 1
#define KERN_PROC 14
#define KERN_PROC_ALL 0
#define KERN_BOOTTIME 21
// linux keeps 15 characters of a command name, Darwin 16
#define MAXCOMLEN 15
#define SIDL 1
#define SRUN 2
#define SSLEEP 3
#define SSTOP 4
#define SZOMB 5
// the fields of Darwin's kinfo_proc that callers read, filled from /proc
struct extern_proc { int p_pid; char p_stat; char p_comm[MAXCOMLEN + 1]; };
struct eproc { int e_ppid; unsigned int e_uid; };
struct kinfo_proc { struct extern_proc kp_proc; struct eproc kp_eproc; };
#ifdef __cplusplus
extern "C" {
#endif
int sysctlbyname(const char *name, void *oldp, size_t *oldlenp, const void *newp, size_t newlen);
int sysctl(const int *name, unsigned int namelen, void *oldp, size_t *oldlenp, const void *newp, size_t newlen);
#ifdef __cplusplus
}
#endif
