#include <os/boot_mode_private.h>
#include <TargetConditionals.h>

#include <darwintest.h>

#if TARGET_OS_OSX
T_DECL(os_boot_mode_basic, "Can't know our exact boot mode, but it should be fetchable")
{
	const char *boot_mode = "??????";
	bool result = os_boot_mode_query(&boot_mode);
	if (result && !boot_mode) {
		boot_mode = "no-particular-mode";
	}
	T_ASSERT_TRUE(result, "os_boot_mode_query() success (%s)", boot_mode);
	T_ASSERT_NE_STR(boot_mode, "??????", "we actually set the result");
}
#endif
