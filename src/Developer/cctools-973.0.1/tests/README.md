# INTRODUCTION

The cctools/tests directory contains a number of functional tests designed to
exercise cctools commands. The test system can be used as part of a test-driven
development workflow or simply as a regression test suite. Tests generally
compile from source rather than relying on pre-built binaries so that test cases
can be easily built and run for different platforms and architectures.

Tests can be run using the "run-tests" test driver:

    % ./run-tests
    1a_harness_test                  PASS
    lipo-cpusubtype-order            PASS
    bitcode_strip_arm64_32           FAIL WATCHOS
    codesign_allocate_arm64_32       FAIL WATCHOS
    libtool-static                   PASS
    strings-stdin                    PASS
    segedit-extract                  PASS
    segedit-replace                  PASS
    lipo-info                        PASS
    ### 7 of 9 unit-tests passed (77.8 percent)

The cctools test system is organized around Platforms and Tests.

# PLATFORMS

The cctools test driver has a well-known list of platforms. This list currently
includes macOS, iOS, watchOS, and tvOS. A platform currently implies three
things:

  * A list of valid architecture flags (e.g., "i386 x86_64")
  * A default architecture flag (e.g., "x86_64")
  * A path to the SDK root.

Tests declare the platforms for which they are relevant and are run once for
each relevant platform. The test will pass only if it succeeds on all of its
target platforms. Tests must support a platform.

# TESTS

Each test is a directory containing a Makefile and any additional files required
by the test. The Makefile is required to provide the following things:

  * A declaration of platforms supported by the test. This declaration takes
    the form of a comment typically found at the top of the Makefile:

      # PLATFORM: MACOS IOS WATCHOS

  * A default target that performs the test for a requested platform. The test
    driver will run make once for each platform specified in the platform
    declaration, passing the platform as a make varaible. Makefiles will use the
    platform to divine which architectures are supported, which SDK to use, etc.

  * A "clean" target that removes all derived files. The test will be cleaned
    before each run and after the test concludes.

In addition, tests are required to print either "PASS" or "FAIL" as the only
word on a line to STDOUT in order to indicate the test result. Tests are free to
compile whatever code and run whatever tools they need in order to perform the
test.

Tests are allowed to write into their current working directory, which is
set to the test directory at runtime.

A sample valid test Makefile is:

    # PLATFORM: MACOS

    PLATFORM = MACOS
    TESTROOT = ../..
    include ${TESTROOT}/include/common.makefile

    .PHONY: all clean

    all:
    	${PASS_IFF} true

    clean:

This test begins with a platform declaration, indicating this test is
appropriate for macOS. It also sets the default platform to MACOS; while
unnecessary for the test driver, it aids in running the test manually from the
command-line.

Then the test locates and loads "common.makefile", which provides initialization
logic common to all tests, including:

  * evaluating the platform
  * locating the cctools binaries to use for the test
  * providing utility commands to aid in PASS/FAIL reporting.

Finally the test runs a utility $(PASS_IFF} which prints PASS if the following
command returns a 0 status and prints FAIL if that command returns non-0.

# TEST DRIVER

The test driver, run-tests, runs some or all of the test-cases against tools
in the host system, or against a separate root of tools. Tests are copied into
temporary storage, so that they can write intermediate results to a local,
writable filesystem. The results are not cleaned automatically when the test
ends, so that the results can be further examined.

run-tests supports a number of helpful options:

    usage: run-tests [-a] [-C] [-c cctools_root] [-t test] [-v]
      -a        - run tests asynchronously, potentially boosting performance.
                  This value is currently on by default.
      -C        - clean all of the local test directories.
      -c <dir>  - use cctools installed in <dir>. The directory must be a root
                  filesystem; i.e., one containing ./usr/bin. By default,
                  cctools will all run through "xcrun", although individual
                  tests have the final say.
      -t <test> - run only the test named <test> from the test repository. Test
                  may be the name of a test, or a regex pattern matching one or
                  more tests.
      -v        - verbose, print a line for each phase of the test.

Typical usage runs the tests against the cctools found in the current Xcode
toolchain:

    ./run-tests

You can also specify a directory of cctools to use instead of relying on the
Xcode toolchain. The tools directory needs to be a cctools project build root,
with tools installed in ./usr/bin and ./usr/local/bin:

    ./run-tests -c /tmp/cctools-922.roots/BuildRecords/cctools-922_install/Root/Applications/Xcode.app/Contents/Developer/Toolchains/OSX10.15.xctoolchain

Other options are useful when developing and debugging tests.

# DIRECTORY STRUCTURE

All of the cctools tests data live in a single repository, organized into a
number of important subdirectories.

    bin       - tools and scripts, both to be called by individual tests and
                by the user, including the run-tests.pl test driver and
                check.pl output diffing tool.
    include   - files to be directly included by tests, providing standard
                common implementation.
    src       - cctools tests run on a variety of output files which are built
                from source stored in this directory. By building files under
                test from source, cctools tests are not bound to a specific
                platform or architecture. it also means a full toolchain is
                necessary to run cctools tests, which is a reasonable
                requirement for cctools. if a test does require a pre-built
                binary, that binary can be included in the test directory.
    test-cases- individual test cases.
