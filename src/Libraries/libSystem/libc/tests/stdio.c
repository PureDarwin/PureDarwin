#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <sys/sysctl.h>

#include <darwintest.h>

#define FILE_LIMIT 100

/*
 * Validate:
 * (1) the maximum number of fds allowed open per process
 * implemented by the kernel matches what sysconf expects:
 *  32 bit: OPEN_MAX
 *  64 bit: RLIM_INFINITY
 * (2) fopen does not fail when NOFILE is unlimited.
 */
T_DECL(stdio_PR_63187147_SC_STREAM_MAX, "_SC_STREAM_MAX test")
{
	struct rlimit rlim;
	long stream_max, saved_stream_max, open_count, i;
	int maxfilesperproc, err;
	size_t maxfilesperproc_size = sizeof(maxfilesperproc);
	FILE **fp = NULL;
	const char *filename = "fopen_test";

	T_SETUPBEGIN;

	saved_stream_max = sysconf(_SC_STREAM_MAX);
	T_LOG("Initial stream_max %ld", saved_stream_max);

	/* Decide the maximum number of fds allowed by the kernel */
	err = sysctlbyname("kern.maxfilesperproc", &maxfilesperproc, &maxfilesperproc_size, NULL, 0);
	T_EXPECT_POSIX_SUCCESS(err, "sysctlbyname(\"kern.maxfilesperproc\") returned %d", err);
	T_LOG("kern.maxfilesperproc %d", maxfilesperproc);

	/*
	 * Raise RLIMIT_NOFILE to RLIM_INFINITY, note that this does NOT update
	 * __stream_max in findfp.c
	 */
	err = getrlimit(RLIMIT_NOFILE, &rlim);
	T_EXPECT_POSIX_SUCCESS(err, "getrlimit(RLIMIT_NOFILE)");
	T_LOG("Initial RLIMIT_NOFILE rlim.cur: 0x%llx", rlim.rlim_cur);
	rlim.rlim_cur = RLIM_INFINITY;
	err = setrlimit(RLIMIT_NOFILE, &rlim);
	T_EXPECT_POSIX_SUCCESS(err, "setrlimit(RLIMIT_NOFILE) to RLIM_INFINITY");
	err = getrlimit(RLIMIT_NOFILE, &rlim);
	T_EXPECT_POSIX_SUCCESS(err, "New RLIMIT_NOFILE rlim_cur: 0x%llx", rlim.rlim_cur);

	T_SETUPEND;

	/*
	 * Test 1 (sysconf with _SC_STREAM_MAX): the largest value sysconf
	 * returns for _SC_STREAM_MAX is OPEN_MAX (32 bit) or
	 * RLIM_INFINITY (64 bit)
	 */
	stream_max = sysconf(_SC_STREAM_MAX);
	T_EXPECT_NE_LONG((long)-1, stream_max, "stream_max %ld", stream_max);
#if __LP64__
	T_EXPECT_EQ((long)RLIM_INFINITY, stream_max, "sysconf returned 0x%lx", stream_max);
#else
	T_EXPECT_EQ((long)OPEN_MAX, stream_max, "sysconf returned 0x%lx", stream_max);
#endif

	/*
	 * Test 2 (__stream_max in findfp.c): exercise __sfp by calling fopen
	 * saved_stream_max + 1 times. Note that we call fopen() up to
	 * maxfilesperproc times in case fopen() goes nuts.
	 */
	fp = malloc(sizeof(FILE *) * (size_t)maxfilesperproc);
	T_EXPECT_NOTNULL(fp, "Allocated %d FILE pointers", maxfilesperproc);
	for (i = 0; i < saved_stream_max + 1 && i < maxfilesperproc; i++) {
	    if (i == saved_stream_max) {
		T_LOG("The very next fopen should trigger __sfp to update __stream_max and fopen shouldn't fail ");
	    }
	    fp[i] = fopen(filename, "r");
	    T_QUIET; T_EXPECT_NOTNULL(fp, "%ld: fopen(%s, \"r\")", i, filename);
	}
	open_count = i;

	for (i = 0; i < open_count; i++) {
	    fclose(fp[i]);
	}
	free(fp);

	T_LOG("saved_stream_max %ld stream_max %ld fopen %ld files", saved_stream_max, stream_max, open_count);
}

T_DECL(stdio_PR_22813396, "STREAM_MAX is affected by changes to RLIMIT_NOFILE")
{
	struct rlimit theLimit;
	getrlimit( RLIMIT_NOFILE, &theLimit );
	theLimit.rlim_cur = FILE_LIMIT;
	setrlimit( RLIMIT_NOFILE, &theLimit );

	long stream_max = sysconf(_SC_STREAM_MAX);
	T_EXPECT_EQ_LONG(stream_max, (long)FILE_LIMIT, "stream_max = FILE_LIMIT");

	FILE *f;
	for(int i = 3; i < stream_max; i++) {
		if((f = fdopen(0, "r")) == NULL) {
			T_FAIL("Failed after %d streams", i);
		}
	}

	f = fdopen(0, "r");
	T_EXPECT_NULL(f, "fdopen fail after stream_max streams");

	theLimit.rlim_cur = FILE_LIMIT + 1;
	setrlimit( RLIMIT_NOFILE, &theLimit );

	f = fdopen(0, "r");
	T_EXPECT_NOTNULL(f, "fdopen succeed after RLIMIT_NOFILE increased");
}

T_DECL(stdio_PR_22813396_close, "STREAM_MAX is enforced properly after fclose")
{
	struct rlimit theLimit;
	getrlimit( RLIMIT_NOFILE, &theLimit );
	theLimit.rlim_cur = FILE_LIMIT;
	setrlimit( RLIMIT_NOFILE, &theLimit );

	long stream_max = sysconf(_SC_STREAM_MAX);
	T_EXPECT_EQ_LONG(stream_max, (long)FILE_LIMIT, "stream_max = FILE_LIMIT");

	FILE *f;
	for(int i = 3; i < stream_max - 1; i++) {
		if((f = fdopen(0, "r")) == NULL) {
			T_FAIL("Failed after %d streams", i);
		}
	}

	// the last stream is for dup(0), it needs to be fclose'd
	FILE *dupf = NULL;
	T_EXPECT_NOTNULL(dupf = fdopen(dup(0), "r"), NULL);

	T_EXPECT_NULL(f = fdopen(0, "r"), "fdopen fail after stream_max streams");

	T_EXPECT_POSIX_ZERO(fclose(dupf), "fclose succeeds");

	f = fdopen(0, "r");
	T_WITH_ERRNO; T_EXPECT_NOTNULL(f, "fdopen succeed after fclose");
}

