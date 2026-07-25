/*
 * Minimal portable stand-in for Apple's copyfile.h, for host-native cctools
 * builds (see nix/pkgs/host-otool.nix). install_name_tool.c only uses
 * copyfile(src, dst, NULL, COPYFILE_ALL | COPYFILE_MOVE) to atomically
 * replace the original file with its rewritten temp copy - plain rename(2)
 * does exactly that (both files live on the same tmpfs during our build).
 */
#ifndef PD_FOREIGN_COPYFILE_H
#define PD_FOREIGN_COPYFILE_H

#include <stdio.h>

#define COPYFILE_ALL 0
#define COPYFILE_MOVE 0

static inline int
copyfile(const char *src, const char *dst, void *state, int flags)
{
	(void)state;
	(void)flags;
	return rename(src, dst);
}

#endif /* PD_FOREIGN_COPYFILE_H */
