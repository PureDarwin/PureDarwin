# stuff to include in every test Makefile

SHELL = /bin/sh

# unless overridden by the Makefile, command-line, or environment, assume we
# are building for macOS. This aids developing/debugging tests, as you can run
# a test just by typing "make" at the command shell.
PLATFORM ?= MACOS

# the test name is the directory name
TEST = $(shell basename `pwd`)

# configure platform-specific settings, including the default SDKROOT, default
# arch, and list of valid archs for this platform.
#
# This configuration will change over time as the build trains add and remove
# Mach-O slices.
ifeq ($(PLATFORM), MACOS)
	ARCH        := x86_64
	VALID_ARCHS := x86_64
	SDKROOT     := $(shell xcodebuild -sdk macosx.internal -version Path 2>/dev/null)
	SDKVERS     := $(shell xcodebuild -sdk macosx.internal -version PlatformVersion 2>/dev/null)
	TOOLCHAIN   := OSX${SDKVERS}
endif
ifeq ($(PLATFORM), IOS)
	ARCH        := arm64
	VALID_ARCHS := arm64 arm64e
	SDKROOT     := $(shell xcodebuild -sdk iphoneos.internal -version Path 2>/dev/null)
	SDKVERS     := $(shell xcodebuild -sdk iphoneos.internal -version PlatformVersion 2>/dev/null)
	TOOLCHAIN   := iOS${SDKVERS}
endif
ifeq ($(PLATFORM), WATCHOS)
	ARCH        := arm64_32
	VALID_ARCHS := armv7k arm64_32 arm64e
	SDKROOT     := $(shell xcodebuild -sdk watchos.internal -version Path 2>/dev/null)
	SDKVERS     := $(shell xcodebuild -sdk watchos.internal -version PlatformVersion 2>/dev/null)
	TOOLCHAIN   := WatchOS${SDKVERS}
endif
ifeq ($(PLATFORM), TVOS)
	ARCH        := arm64
	VALID_ARCHS := arm64
	SDKROOT     := $(shell xcodebuild -sdk appletvos.internal -version Path 2>/dev/null)
	SDKVERS     := $(shell xcodebuild -sdk appletvos.internal -version PlatformVersion 2>/dev/null)
	TOOLCHAIN   := AppleTVOS${SDKVERS}
endif

ARCHS_FAT := $(foreach arch,$(VALID_ARCHS),-arch $(arch))
ARCH_THIN := $(foreach arch,$(firstword $(VALID_ARCHS)),-arch $(arch))

# set the command invocations for cctools. If CCTOOLS_ROOT is set and exists
# in the filesystem use cctools from that root. Otherwise, fall back to the
# xcode toolchain.
ifneq ("$(wildcard ${CCTOOLS_ROOT})","")
	AR 	 =	$(CCTOOLS_ROOT)/usr/bin/ar
	AS 	 =	$(CCTOOLS_ROOT)/usr/bin/as
	BITCODE_STRIP = $(CCTOOLS_ROOT)/usr/bin/bitcode_strip
	CHECKSYMS=	$(CCTOOLS_ROOT)/usr/local/bin/checksyms
	CS_ALLOC =	$(CCTOOLS_ROOT)/usr/bin/codesign_allocate
	CTF_INSERT =	$(CCTOOLS_ROOT)/usr/bin/ctf_insert
	LIBTOOL	 =	$(CCTOOLS_ROOT)/usr/bin/libtool
	LIPO	 =	$(CCTOOLS_ROOT)/usr/bin/lipo
	LLOTOOL  =      $(CCTOOLS_ROOT)/usr/bin/llvm-otool
	MTOR	 =	$(CCTOOLS_ROOT)/usr/local/bin/mtor
	NM	 =	`xcrun --sdk $(SDKROOT) -f nm`
	NMC	 =	$(CCTOOLS_ROOT)/usr/bin/nm-classic
	NMEDIT	 =	$(CCTOOLS_ROOT)/usr/bin/nmedit
# 	OTOOL	 =	$(CCTOOLS_ROOT)/usr/bin/otool
	OTOOLC	 =	$(CCTOOLS_ROOT)/usr/bin/otool-classic
	OTOOL	 =	`xcrun --sdk $(SDKROOT) -f otool`
# 	OTOOLC	 =	`xcrun --sdk $(SDKROOT) -f otool-classic`
	PAGESTUFF =	$(CCTOOLS_ROOT)/usr/bin/pagestuff
	RANLIB	 =	$(CCTOOLS_ROOT)/usr/bin/ranlib
	SEGEDIT	 =	$(CCTOOLS_ROOT)/usr/bin/segedit
	SIZEC	 =	$(CCTOOLS_ROOT)/usr/bin/size-classic
	STRINGS	 = 	$(CCTOOLS_ROOT)/usr/bin/strings
	STRIP	 = 	$(CCTOOLS_ROOT)/usr/bin/strip
	VTOOL	 = 	$(CCTOOLS_ROOT)/usr/bin/vtool

	STUFF_TESTS =	$(CCTOOLS_ROOT)/usr/local/bin/cctools/libstuff_test
else
	AR 	 =	`xcrun --sdk $(SDKROOT) -f ar`
	AS 	 =	`xcrun --sdk $(SDKROOT) -f as`
	BITCODE_STRIP = `xcrun --sdk $(SDKROOT) -f bitcode_strip`
	CHECKSYMS=	`xcrun --sdk $(SDKROOT) -f checksyms`
	CS_ALLOC =	`xcrun --sdk $(SDKROOT) -f codesign_allocate`
	CTF_INSERT =	`xcrun --sdk $(SDKROOT) -f ctf_insert`
	LIBTOOL	 =	`xcrun --sdk $(SDKROOT) -f libtool`
	LIPO	 =	`xcrun --sdk $(SDKROOT) -f lipo`
	LLOTOOL  =      `xcrun --sdk $(SDKROOT) -f llvm-otool`
	MTOR	 =      `xcrun --sdk $(SDKROOT) -f mtor`
	NM	 =	`xcrun --sdk $(SDKROOT) -f nm`
	NMC	 =	`xcrun --sdk $(SDKROOT) -f nm-classic`
	NMEDIT	 =	`xcrun --sdk $(SDKROOT) -f nmedit`
	OTOOL	 =	`xcrun --sdk $(SDKROOT) -f otool`
	OTOOLC	 =	`xcrun --sdk $(SDKROOT) -f otool-classic`
	PAGESTUFF =	`xcrun --sdk $(SDKROOT) -f pagestuff`
	RANLIB	 =	`xcrun --sdk $(SDKROOT) -f ranlib`
	SEGEDIT	 =	`xcrun --sdk $(SDKROOT) -f segedit`
	SIZEC	 =	`xcrun --sdk $(SDKROOT) -f size-classic`
	STRINGS	 = 	`xcrun --sdk $(SDKROOT) -f strings`
	STRIP	 = 	`xcrun --sdk $(SDKROOT) -f strip`
	VTOOL	 = 	`xcrun --sdk $(SDKROOT) -f vtool`

	STUFF_TESTS = 	${SDKROOT}/usr/local/bin/cctools/libstuff_test
endif

# set other common tool commands
CC		=	xcrun --toolchain $(TOOLCHAIN) cc -isysroot $(SDKROOT)
CPP		=	xcrun --toolchain $(TOOLCHAIN) c++ -isysroot $(SDKROOT)
LD		=	xcrun --toolchain $(TOOLCHAIN) ld -syslibroot $(SDKROOT)
MKDIRS		=	mkdir -p

# utilites for Makefiles
MYDIR=$(shell cd ../../bin;pwd)
CHECK			:= ${MYDIR}/check.pl $(abspath $(firstword $(MAKEFILE_LIST)))
CHECKSYMSDT		:= ${CHECKSYMS} -dt
PASS_IFF		= ${MYDIR}/pass-iff-exit-zero.pl
PASS_IFF_SUCCESS	= ${PASS_IFF}
PASS_IFF_EMPTY		= ${MYDIR}/pass-iff-no-stdin.pl
PASS_IFF_STDIN		= ${MYDIR}/pass-iff-stdin.pl
FAIL_IFF		= ${MYDIR}/fail-iff-exit-zero.pl
FAIL_IFF_SUCCESS	= ${FAIL_IFF}
PASS_IFF_ERROR		= ${MYDIR}/pass-iff-exit-non-zero.pl
PASS_UNLESS		= ${MYDIR}/pass-iff-exit-non-zero.pl
FAIL_IF_ERROR		= ${MYDIR}/fail-if-exit-non-zero.pl
FAIL_IF_SUCCESS     	= ${MYDIR}/fail-if-exit-zero.pl
FAIL_IF_EMPTY		= ${MYDIR}/fail-if-no-stdin.pl
FAIL_IF_STDIN		= ${MYDIR}/fail-if-stdin.pl
# PASS_IFF_GOOD_MACHO	= ${PASS_IFF} ${MACHOCHECK}
# FAIL_IF_BAD_MACHO	= ${FAIL_IF_ERROR} ${MACHOCHECK}
# FAIL_IF_BAD_OBJ		= ${FAIL_IF_ERROR} ${OBJECTDUMP} >/dev/null
VERIFY_ALIGN_16K	= $(MYDIR)/verify-align.pl -a 0x4000
VERIFY_ALIGN_4K		= $(MYDIR)/verify-align.pl -a 0x1000
