/*
 * Mach-O architecture-name helpers, introduced in macOS 13. PureDarwin reports
 * 26.5, so ports that gate on __MAC_OS_X_VERSION_MIN_REQUIRED >= 130000 expect
 * this header (cmake's cmMachO.cxx is one).
 *
 * Only the name/cputype pair is here. The block-based fat-slice iterators
 * (macho_best_slice, macho_for_each_slice) are a separate piece of work -
 * they need real fat-file parsing - and nothing in the tree calls them yet.
 * Declaring them unimplemented would turn a clear compile error into a
 * confusing link failure.
 */

#ifndef _MACH_O_UTILS_H_
#define _MACH_O_UTILS_H_

#include <sys/cdefs.h>
#include <mach/machine.h>
#include <mach-o/loader.h>
#include <Availability.h>

__BEGIN_DECLS

/*
 * The canonical name for a cputype/cpusubtype pair ("x86_64", "arm64e", ...),
 * or NULL if unknown. Pass CPU_SUBTYPE_MULTIPLE for the most general name.
 * The result is a constant and must not be freed.
 */
__API_AVAILABLE(macos(13.0))
const char *macho_arch_name_for_cpu_type(cpu_type_t cputype,
    cpu_subtype_t cpusubtype);

/*
 * The reverse: fills in cputype/cpusubtype for an architecture name. Returns 0
 * on success, or EINVAL if the name is not known. Either out pointer may be
 * NULL.
 */
__API_AVAILABLE(macos(13.0))
int macho_cpu_type_for_arch_name(const char *name, cpu_type_t *cputype,
    cpu_subtype_t *cpusubtype);

/*
 * The name for a mach_header. Pass NULL for the header of the running process.
 */
__API_AVAILABLE(macos(13.0))
const char *macho_arch_name_for_mach_header(const struct mach_header *mh);

__END_DECLS

#endif /* _MACH_O_UTILS_H_ */
