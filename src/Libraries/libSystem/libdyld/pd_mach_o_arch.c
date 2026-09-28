/*
 * PureDarwin: getsegbyname()/NXGetLocalArchInfo()/NXFindBestFatArch() - the
 * three real Darwin ABI functions CoreFoundation's CFBundle_Grok.c needs and
 * that were still missing after wiring up dyld_priv.h. Real cctools has full
 * implementations of these (tools/cctools/libmacho/getsegbyname.c and
 * arch.c), but those are meant for the cctools binaries themselves (RLD/
 * OPENSTEP legacy branches, malloc'd NXArchInfo tables with hundreds of
 * entries) - this target is always x86_64-apple-darwin, so a small direct
 * implementation covers what's actually needed.
 */

#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/arch.h>
#include <mach/machine.h>
#include <string.h>
#include <stdlib.h>

extern struct mach_header_64 *_NSGetMachExecuteHeader(void);

const struct segment_command_64 *
getsegbyname(const char *segname)
{
	struct mach_header_64 *mh = _NSGetMachExecuteHeader();
	struct load_command *lc = (struct load_command *)((char *)mh + sizeof(*mh));

	for (uint32_t i = 0; i < mh->ncmds; i++) {
		if (lc->cmd == LC_SEGMENT_64) {
			struct segment_command_64 *sg = (struct segment_command_64 *)lc;
			if (strncmp(sg->segname, segname, sizeof(sg->segname)) == 0)
				return sg;
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	return NULL;
}

/*
 * Real Darwin's NXGetLocalArchInfo() returns one of a large static table of
 * NXArchInfo entries selected by host_info(). This build only ever targets
 * one architecture, so the "local arch" is always this one.
 */
// the arch this libdyld was built for
static const NXArchInfo pd_local_arch_info = {
#if defined(__riscv) && __riscv_xlen == 64
	.name = "riscv64",
	.cputype = CPU_TYPE_RISCV64,
	.cpusubtype = CPU_SUBTYPE_RISCV_ALL,
	.byteorder = NX_LittleEndian,
	.description = "riscv64",
#elif defined(__arm64__)
	.name = "arm64",
	.cputype = CPU_TYPE_ARM64,
	.cpusubtype = CPU_SUBTYPE_ARM64_ALL,
	.byteorder = NX_LittleEndian,
	.description = "arm64",
#else
	.name = "x86_64",
	.cputype = CPU_TYPE_X86_64,
	.cpusubtype = CPU_SUBTYPE_X86_64_ALL,
	.byteorder = NX_LittleEndian,
	.description = "x86_64",
#endif
};

const NXArchInfo *
NXGetLocalArchInfo(void)
{
	return &pd_local_arch_info;
}

/*
 * Real NXFindBestFatArch() does generic-vs-specific cpusubtype scoring
 * across every slice. CFBundle_Grok.c only ever asks for this build's own
 * cputype/cpusubtype, so an exact-match scan (falling back to a cputype-only
 * match, matching real Darwin's behavior for CPU_SUBTYPE_MULTIPLE) is
 * equivalent for every case this build can hit.
 */
struct fat_arch *
NXFindBestFatArch(cpu_type_t cputype, cpu_subtype_t cpusubtype,
    struct fat_arch *fat_archs, uint32_t nfat_archs)
{
	struct fat_arch *cputype_match = NULL;

	for (uint32_t i = 0; i < nfat_archs; i++) {
		if (fat_archs[i].cputype != cputype)
			continue;
		if (fat_archs[i].cpusubtype == cpusubtype)
			return &fat_archs[i];
		if (cputype_match == NULL)
			cputype_match = &fat_archs[i];
	}
	return cputype_match;
}
