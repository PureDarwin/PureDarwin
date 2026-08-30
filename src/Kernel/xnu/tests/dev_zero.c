#include <stdio.h>
#include <fcntl.h>
#include <util.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <darwintest.h>

T_DECL(dev_zero,
    "test reading from /dev/zero",
    T_META_ASROOT(false))
{
	int dev = opendev("/dev/zero", O_RDONLY, 0, NULL);
	char buffer[100];

	for (int i = 0; i < 100; i++) {
		buffer[i] = 0xff;
	}

	int rd_sz = read(dev, buffer, sizeof(buffer));

	T_EXPECT_EQ(rd_sz, 100, "read from /dev/zero failed");

	for (int i = 0; i < 100; i++) {
		if (buffer[i]) {
			T_FAIL("Unexpected non-zero character read from /dev/zero");
		}
	}

	close(dev);
}

T_DECL(dev_zero_permissions,
    "ensure /dev/zero's permissions can't be updated",
    T_META_ASROOT(true))
{
	struct stat sb = {0};
	const char *dev = "/dev/zero";
	int ret = 0;

	ret = stat(dev, &sb);
	T_ASSERT_POSIX_SUCCESS(ret, "stat /dev/zero");
	T_ASSERT_TRUE(sb.st_mode & S_IWOTH, "/dev/zero world writable");

	ret = chmod(dev, 0664);
	T_ASSERT_POSIX_FAILURE(ret, EPERM, "chmod /dev/zero should fail w/ EPERM");

	ret = stat(dev, &sb);
	T_ASSERT_POSIX_SUCCESS(ret, "stat /dev/zero");
	T_ASSERT_TRUE(sb.st_mode & S_IWOTH, "/dev/zero still world writable");
}
