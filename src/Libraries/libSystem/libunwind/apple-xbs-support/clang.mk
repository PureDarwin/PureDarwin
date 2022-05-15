# apple-xbs-support/clang.mk
# Apple Internal B&I makefile for clang.

################################################################################
# Apple clang default B&I configuration.
################################################################################

Clang_Use_Assertions   := 0
Clang_Use_Optimized    := 1
# FIXME: remove, stop hardcoding it.
Clang_Version          := 11.0.0

# Use LTO for clang but not clang_device
ifeq ($(APPLE_XBS_SUPPORT_COMPUTED_RC_ProjectName),clang)
Clang_Enable_LTO := THIN
else
Clang_Enable_LTO := 0
endif

################################################################################
# Apple clang XBS targets.
################################################################################

# Default target.
all: help

help:
	@echo "usage: make [{VARIABLE=VALUE}*] <target>"
	@echo
	@echo "The Apple Clang makefile is primarily intended for use with XBS."
	@echo
	@echo "Supported B&I related targets are:"
	@echo "  installsrc    -- Copy source files from the current" \
	      "directory to the SRCROOT."
	@echo "  clean         -- Does nothing, just for XBS support."
	@echo "  installhdrs   -- Does nothing, just for XBS support."
	@echo "  install       -- Alias for install-clang."
	@echo "  install-clang -- Build the Apple Clang compiler."

# Default is to build Clang.
install: clang

# Install source uses the shared helper, but also fetches PGO data during
# submission to a B&I train.
installsrc-paths :=    \
    llvm               \
    clang-tools-extra  \
    clang-shims        \
    compiler-rt        \
    libcxx             \
    libcxxabi          \
    clang
include apple-xbs-support/helpers/installsrc.mk

# FIXME: Fetch PGO data if we can.
installsrc: installsrc-helper

# The clean target is run after installing sources, but we do nothing because
# the expectation is that we will just avoid copying in cruft during the
# installsrc phase.
clean:

# We do not need to do anything for the install headers phase.
installhdrs:

################################################################################
# Apple clang build targets.
################################################################################

# The clang build target invokes CMake and ninja in the `build_clang` script.
BUILD_CLANG = $(SRCROOT)/clang/utils/buildit/build_clang

# FIXME (Alex): export the Git version.
clang: $(OBJROOT) $(SYMROOT) $(DSTROOT)
	cd $(OBJROOT) && \
		export LLVM_REPOSITORY=$(LLVM_REPOSITORY) && \
		$(BUILD_CLANG) $(Clang_Use_Assertions) $(Clang_Use_Optimized) $(Clang_Version) $(Clang_Enable_LTO)

clang-libs: $(OBJROOT) $(SYMROOT) $(DSTROOT)
	cd $(OBJROOT) && \
		export LLVM_REPOSITORY=$(LLVM_REPOSITORY) && \
		$(BUILD_CLANG) $(Clang_Use_Assertions) $(Clang_Use_Optimized) $(Clang_Version) $(Clang_Enable_LTO) build_libs

BUILD_COMPILER_RT = $(SRCROOT)/compiler-rt/utils/buildit/build_compiler_rt

compiler-rt: $(OBJROOT) $(SYMROOT) $(DSTROOT)
	cd $(OBJROOT) && \
		$(BUILD_COMPILER_RT) $(Clang_Use_Assertions) $(Clang_Use_Optimized) $(Clang_Version)

# FIXME: Workaround <rdar://problem/57402006> DT_TOOLCHAIN_DIR is incorrectly
# set when -developerDir is used or `DEVELOPER_DIR` environment variable is set
# when targeting non-macOS trains.
XCODE_TOOLCHAINS_DIR = Applications/Xcode.app/Contents/Developer/Toolchains
XCODE_TOOLCHAIN_DIR = $(XCODE_TOOLCHAINS_DIR)/$(notdir $(DT_TOOLCHAIN_DIR))
XCODE_TOOLCHAIN_CLANG_LIBS_DIR = $(XCODE_TOOLCHAIN_DIR)/usr/lib/clang/$(Clang_Version)/lib/darwin
COMPILER_RT_PROJECT_DIR = $(RC_EMBEDDEDPROJECT_DIR)/$(APPLE_XBS_SUPPORT_VARIANT_PREFIX)clang_compiler_rt
COMPILER_RT_TOOLCHAIN_DYLIB_DIR = $(COMPILER_RT_PROJECT_DIR)/$(XCODE_TOOLCHAIN_CLANG_LIBS_DIR)
$(info COMPILER_RT_TOOLCHAIN_DYLIB_DIR:$(COMPILER_RT_TOOLCHAIN_DYLIB_DIR))
COMPILER_RT_DYLIBS = $(filter-out %sim_dynamic.dylib,$(wildcard $(COMPILER_RT_TOOLCHAIN_DYLIB_DIR)/*_dynamic.dylib))
$(info COMPILER_RT_DYLIBS:$(COMPILER_RT_DYLIBS))
ifeq ($(findstring OSX,$(DT_TOOLCHAIN_DIR)),)
    OS_DYLIBS = $(filter-out %osx_dynamic.dylib,$(COMPILER_RT_DYLIBS))
else
    OS_DYLIBS = $(COMPILER_RT_DYLIBS)
endif

# TODO(dliew): Remove support for old path (rdar://problem/57350767).
include apple-xbs-support/helpers/train_detection.mk
OLD_COMPILER_RT_OS_INSTALL_PATH=/usr/local/lib/sanitizers
NEW_COMPILER_RT_OS_INSTALL_PATH=/usr/appleinternal/lib/sanitizers/
NEW_COMPILER_RT_INSTALL_OS_VERSIONS := OSX-11.0 iOS-14.0 WatchOS-7.0 AppleTVOS-14.0 BridgeOS-5.0
COMPILER_RT_OS_DYLIB_PERMISSIONS := u=rwx,g=rx,o=rx

compiler-rt-os:
	mkdir -p $(DSTROOT)$(OLD_COMPILER_RT_OS_INSTALL_PATH)
	ditto $(OS_DYLIBS) $(DSTROOT)$(OLD_COMPILER_RT_OS_INSTALL_PATH)
	chmod -vv $(COMPILER_RT_OS_DYLIB_PERMISSIONS) $(DSTROOT)$(OLD_COMPILER_RT_OS_INSTALL_PATH)/*.dylib
	@if [ "X$(call os-train-version-is-at-least,$(NEW_COMPILER_RT_INSTALL_OS_VERSIONS))" = "X1" ]; then \
		echo Installing compiler-rt dylibs to new path: $(NEW_COMPILER_RT_OS_INSTALL_PATH); \
		mkdir -p $(DSTROOT)$(NEW_COMPILER_RT_OS_INSTALL_PATH); \
		ditto $(OS_DYLIBS) $(DSTROOT)$(NEW_COMPILER_RT_OS_INSTALL_PATH); \
		chmod -vv $(COMPILER_RT_OS_DYLIB_PERMISSIONS) $(DSTROOT)$(NEW_COMPILER_RT_OS_INSTALL_PATH)/*.dylib; \
	fi
