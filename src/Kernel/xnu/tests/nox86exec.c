#include <spawn.h>
#include <sys/wait.h>
#include <darwintest.h>
#include <mach-o/dyld.h>
#include <errno.h>

T_DECL(nox86exec, "make sure the nox86exec boot-arg is honored", T_META_ALL_VALID_ARCHS(false), T_META_BOOTARGS_SET("nox86exec=1"))
{
#if TARGET_OS_OSX && defined(__arm64__)
	int spawn_ret, pid;
	char path[1024];
	uint32_t size = sizeof(path);

	T_ASSERT_EQ(_NSGetExecutablePath(path, &size), 0, NULL);
	T_ASSERT_LT(strlcat(path, "_helper", size), (unsigned long)size, NULL);

	spawn_ret = posix_spawn(&pid, path, NULL, NULL, NULL, NULL);
	if (spawn_ret == 0) {
		int wait_ret = 0;
		waitpid(pid, &wait_ret, 0);
		T_ASSERT_FALSE(WIFEXITED(wait_ret), "x86_64 helper should not run");
	}
#else
	T_SKIP("Skipping. Test only runs on arm64 macOS.");
#endif
}
