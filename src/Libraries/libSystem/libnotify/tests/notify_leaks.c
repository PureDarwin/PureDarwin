#include <darwintest_utils.h>

T_DECL(notify_leaks, "checks for leaks in notifyd", T_META_ASROOT(true))
{
	dt_check_leaks_by_name("notifyd");
}
