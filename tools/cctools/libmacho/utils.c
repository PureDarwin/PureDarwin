/*
 * macOS 13 mach-o/utils.h entry points, over the NXArchInfo table in arch.c.
 */

#include <mach-o/utils.h>
#include <mach-o/arch.h>
#include <errno.h>
#include <stddef.h>

const char *
macho_arch_name_for_cpu_type(cpu_type_t cputype, cpu_subtype_t cpusubtype)
{
	const NXArchInfo *info;

	/* NXGetArchInfoFromCpuType already treats CPU_SUBTYPE_MULTIPLE as
	 * "most general entry for this cputype", which is what the caller
	 * passing it means. */
	info = NXGetArchInfoFromCpuType(cputype,
	    cpusubtype & ~CPU_SUBTYPE_MASK);
	return (info != NULL) ? info->name : NULL;
}

int
macho_cpu_type_for_arch_name(const char *name, cpu_type_t *cputype,
    cpu_subtype_t *cpusubtype)
{
	const NXArchInfo *info;

	if (name == NULL) {
		return EINVAL;
	}
	if ((info = NXGetArchInfoFromName(name)) == NULL) {
		return EINVAL;
	}
	if (cputype != NULL) {
		*cputype = info->cputype;
	}
	if (cpusubtype != NULL) {
		*cpusubtype = info->cpusubtype;
	}
	return 0;
}

const char *
macho_arch_name_for_mach_header(const struct mach_header *mh)
{
	if (mh == NULL) {
		/* NULL means the running process. Taking that from the local
		 * arch rather than _mh_execute_header keeps this linkable into
		 * libSystem, where that symbol belongs to the main executable
		 * and would be undefined. */
		const NXArchInfo *info = NXGetLocalArchInfo();

		return (info != NULL) ? info->name : NULL;
	}
	return macho_arch_name_for_cpu_type(mh->cputype, mh->cpusubtype);
}
