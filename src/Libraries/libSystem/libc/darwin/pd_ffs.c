/*
 * ffs(3) - real implementation (__builtin_ffs), not stubbed.
 *
 * The upstream FreeBSD ffs() lives in libplatform's ffsll.c but is gated
 * behind `#if VARIANT_DYLD && TARGET_OS_SIMULATOR`, which doesn't apply to
 * our static archive build. xlocale.c's querylocale() calls the plain 32-bit
 * ffs(), so provide it directly here rather than perturbing that gate.
 */

int
ffs(int mask)
{
	return __builtin_ffs(mask);
}
