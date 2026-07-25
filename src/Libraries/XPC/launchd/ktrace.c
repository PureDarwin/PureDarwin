#include "ktrace.h"
#include <sys/kdebug_signpost.h>

void
runtime_ktrace1(runtime_ktrace_code_t code)
{
	void *ra = __builtin_extract_return_addr(__builtin_return_address(1));

	if (launchd_apple_internal) {
		kdebug_signpost(code, 0, 0, 0, (uintptr_t)ra);
	}
}

void
runtime_ktrace0(runtime_ktrace_code_t code)
{
	void *ra = __builtin_extract_return_addr(__builtin_return_address(0));

	if (launchd_apple_internal) {
		kdebug_signpost(code, 0, 0, 0, (uintptr_t)ra);
	}
}

void
runtime_ktrace(runtime_ktrace_code_t code, long a, long b, long c)
{
	void *ra = __builtin_extract_return_addr(__builtin_return_address(0));

	if (launchd_apple_internal) {
		kdebug_signpost(code, a, b, c, (uintptr_t)ra);
	}
}
