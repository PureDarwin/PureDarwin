#include <unistd.h>

int
main(int argc, char **argv)
{
	/*
	 * This is just a process signed in such a way that it appears to the system
	 * as a DriverKit extension.
	 * This is used to ensure that dexts get the MTE policy we expect just by virtue
	 * of being a dext.
	 * sleep() so we eventually terminate too (and don't bother with anything
	 *  fancy for synchronizing.)
	 */
	sleep(5);
	return 0;
}
