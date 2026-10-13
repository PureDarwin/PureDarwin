// /bin/sh: runs the shell /private/var/select/sh points at, as "sh" so bash and zsh take their POSIX modes
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SELECT_LINK "/private/var/select/sh"

static const char *fallbacks[] = { "/bin/bash", "/bin/zsh", NULL };

int main(int argc, char **argv)
{
	char target[PATH_MAX];
	ssize_t n = readlink(SELECT_LINK, target, sizeof(target) - 1);

	(void)argc;
	argv[0] = "sh";
	if (n > 0) {
		target[n] = 0;
		// a relative link is relative to the link's directory
		if (target[0] != '/') {
			char full[PATH_MAX];
			snprintf(full, sizeof(full), "/private/var/select/%s", target);
			strlcpy(target, full, sizeof(target));
		}
		if (strcmp(target, "/bin/sh") != 0)
			execv(target, argv);
	}
	for (int i = 0; fallbacks[i]; i++)
		execv(fallbacks[i], argv);
	fprintf(stderr, "sh: no shell to run: %s\n", strerror(errno));
	return 127;
}
