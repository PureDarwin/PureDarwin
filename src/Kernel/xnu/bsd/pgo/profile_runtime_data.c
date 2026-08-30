#include <mach/vm_param.h>

#ifndef __BUILDING_XNU_LIBRARY__
/*
 * This tells compiler_rt not to include userspace-specific stuff writing
 * profile data to a file.
 * When building userspace unit-test we don't do that because we do want
 * the normal file-saving coverage mechanism to work as usual in a
 */
int __llvm_profile_runtime = 0;

#endif /* __BUILDING_XNU_LIBRARY__ */

/* compiler-rt requires this.  It uses it to page-align
 * certain things inside its buffers.
 */

extern int getpagesize(void);

int
getpagesize()
{
	return PAGE_SIZE;
}
