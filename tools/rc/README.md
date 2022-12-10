# rc

## The PureDarwin build system

This repository contains the PureDarwin build automation system. It's
essentially a simpler version of darwinbuild that uses modern concepts
and best practices.

Currently, this project contains two scripts: `setup.sh` and `rc`, both written
as standard UNIX shell scripts. The `setup.sh` scripts is meant to be executed
in the user's shell (usually with the `source` builtin), and has been tested in
both bash and zsh. The `setup.sh` script sets up the user's build environment
for building PureDarwin, and installs the `rc` script into the user's PATH
(which is updated to include the `${RC_HOST_BIN}` directory).

The `rc` script is used to invoke PureDarwin toolchain and system builds. It
uses the provided command line arguments and the environment variables set by
the `setup.sh` script or manually by the user to configure, build, and install
all components of the PureDarwin distribution. The available options and
environment are described below.

## Quick Start

To get started quickly with the default configuration, navigate to the
PureDarwin repository root, ensure you've cloned this submodule properly, and
execute `source tools/dbuild/setup.sh`. Some default build variables should be
output. You can now run `rc` or `rc root` from anywhere to return to the
PureDarwin root directory.

Now, to build the host toolchain, run `rc --make-host-toolchain` (this will
take many minutes, even on the best machine).

And finally, run `rc build`. This should invoke a full system build. The built
products will be installed to the `${RC_SYSTEM_ROOT}` directory.

## The build environment

Similarly to Apple's internal build system, the Darwin build system uses various
environment variables with the `RC_*` prefix. I document some of these here:

* `RC_HOST_ARCH`: The architecture of the host machine
* `RC_HOST_TYPE`: The name of the host environment (currently, `Darwin` or
    `Linux`)
* `RC_DARWIN_ROOT`: The path of root of the PureDarwin repository.
* `RC_BUILD_ROOT`: The directory in which to place temporary build files.
* `RC_SOURCE_DIR`: The directory where system sources live.
* `RC_PRODUCT_DIR`: The directory in which to place build products (host tools,
    system root directory, etc.)
* `RC_SYSTEM_ROOT`: The directory in which to place the built system.
* `RC_TOOLS_DIR`: The directory where host tool sources live (we live here)
* `RC_HOST_BIN`: The directory where toolchain binaries should be placed
* `RC_BUILD_JOBS`: The number of concurrent threads to build with.
* `RC_VERBOSE`: Tell the build system to output more messages.

## The `rc` command

Currently, the following options are supported for the `rc` command:

* `--force-rebuild-llvm`: Rebuild host compiler + LLVM-related tools
* `--make-host-toolchain`: Build the host toolchain
* `build`: Build the target system
* `root`: Navigate to the PureDarwin root directory.

## TODO

* Why is this repository called dbuild?
* Allow for building LLVM/ninja/tools without git checkout
* Figure out how best to use/set RC_NONARCH_CFLAGS/RC_NONARCH_CXXFLAGS (this
    really shouldn't be managed by `rc`)
* Support more architectures (I have an M1 mac, I can hopefully do this)
* Generate a system ISO? (This is more of a long-term goal)
* `rc clean {,build,tools,all}`
