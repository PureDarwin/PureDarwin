#include <unistd.h>

int
main(int argc, char **argv)
{
	/*
	 * This is just a process signed in such a way that it should never be
	 * allowed to receive aliases to tagged memory from elsewhere on the system.
	 * The associated test will try to remap tagged memory into this process, and
	 *  will die in the process.
	 * sleep() so we eventually terminate too (and don't bother with anything
	 *  fancy for synchronizing.)
	 */
	sleep(5);
	return 0;
}
