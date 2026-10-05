#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/reboot.h>
#include <unistd.h>

// launchd's reboot2 mig routine; xnu no longer lets root signal pid 1
extern void *reboot2(uint64_t flags);

static const char *
base_name(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

static int
usage(const char *argv0)
{
	fprintf(stderr, "usage: %s [-h | -P | -r]\n", argv0);
	return 1;
}

int
main(int argc, char **argv)
{
	int sig = SIGTERM;
	const char *name = base_name(argv[0]);

	/* SIGINT is reboot and SIGTERM is halt/poweroff; see the handler in
	 * launchd's core.c. NOT SIGUSR1 - launchd has always used that for the
	 * calendar interval timer. */
	if (strcmp(name, "reboot") == 0) {
		sig = SIGINT;
	} else if (strcmp(name, "halt") == 0 ||
	    strcmp(name, "poweroff") == 0 ||
	    strcmp(name, "shutdown") == 0) {
		sig = SIGTERM;
	}

	if (argc > 2) {
		return usage(argv[0]);
	}

	if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-P") == 0 ||
		    strcmp(argv[1], "poweroff") == 0) {
			sig = SIGTERM;
		} else if (strcmp(argv[1], "-r") == 0 ||
		    strcmp(argv[1], "reboot") == 0) {
			sig = SIGINT;
		} else {
			return usage(argv[0]);
		}
	}

	sync();

	if (reboot2(sig == SIGINT ? RB_AUTOBOOT : RB_HALT) == NULL)
		return 0;

	// older launchd without reboot2: the signal still works on old kernels
	if (kill(1, sig) < 0) {
		perror("kill");
		return 1;
	}

	return 0;
}
