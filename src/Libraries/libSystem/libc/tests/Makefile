PROJECT := Libc
TEST_DIR := tests/

CUSTOM_TARGETS += osvariantutil subsystem_test_helper

ifeq ($(DEVELOPER_DIR),)
    DEVELOPER_DIR := $(shell xcode-select -p)
endif

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

ifneq ($(PLATFORM),MacOSX)
EXCLUDED_SOURCES += locale.c
EXCLUDED_SOURCES += os_boot_mode.c
endif

FRAMEWORK_CFLAGS := $(patsubst %,-iframework %,$(SYSTEM_FRAMEWORK_SEARCH_PATHS))
WARNING_CFLAGS := -Weverything \
	-Wno-vla -Wno-missing-field-initializers -Wno-padded \
	-Wno-gnu-flexible-array-initializer -Wno-gnu-empty-initializer \
	-Wno-partial-availability -Wno-used-but-marked-unused \
	-Wno-reserved-id-macro -fmacro-backtrace-limit=0 \
	-Wno-c++98-compat -Wno-extra-semi -Wno-language-extension-token
OTHER_CFLAGS := -DDARWINTEST --std=gnu11 $(FRAMEWORK_CFLAGS) $(WARNING_CFLAGS) -I$(SDK_SYSTEM_FRAMEWORK_HEADERS) -L/usr/lib/system -lsystem_collections
DT_LDFLAGS += -ldarwintest_utils
ASAN_DYLIB_PATH := /usr/appleinternal/lib/sanitizers/

OTHER_LDFLAGS := -umbrella System -L/usr/lib/system -lsystem_collections

nxheap: OTHER_CFLAGS += -Wno-cast-align
strlcat: OTHER_CFLAGS += -Wno-pointer-arith
psort: OTHER_CFLAGS += -Wno-cast-qual -Wno-sign-conversion
net: OTHER_CFLAGS += -Wno-sign-conversion -Wno-cast-align -Wno-incompatible-pointer-types-discards-qualifiers -Wno-sign-compare
printf: OTHER_CFLAGS += -Wno-format-nonliteral
strlcpy: OTHER_CFLAGS += -D_FORTIFY_SOURCE=0
realpath_edge: OTHER_CFLAGS += -fsanitize=address -I../fbsdcompat
realpath_edge: OTHER_LDFLAGS += -Wl,-rpath -Wl,$(ASAN_DYLIB_PATH)
qsort freebsd_qsort: OTHER_CFLAGS += -Wno-unused-function
ifeq ($(PLATFORM),MacOSX)
qsort_perf: OTHER_CFLAGS += -Wno-sign-compare -Wno-sign-conversion -Wno-cast-align -Wno-shorten-64-to-32
else
EXCLUDED_SOURCES += qsort_perf.c
endif
os_variant: OTHER_CFLAGS += -DOS_CRASH_ENABLE_EXPERIMENTAL_LIBTRACE

osvariantutil: OTHER_CFLAGS += -Wno-gnu-statement-expression -Wno-covered-switch-default -DOS_CRASH_ENABLE_EXPERIMENTAL_LIBTRACE

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets

OSVARIANTUTIL_INSTALLDIR = $(DSTROOT)/usr/local/bin

install-osvariantutil: osvariantutil
	mkdir -p $(OSVARIANTUTIL_INSTALLDIR)
	cp $(SYMROOT)/osvariantutil $(OSVARIANTUTIL_INSTALLDIR)/
ifeq ($(DWARF_DSYM_FILE_SHOULD_ACCOMPANY_PRODUCT),YES)
	cp -R $(SYMROOT)/osvariantutil.dSYM $(OSVARIANTUTIL_INSTALLDIR)/
endif

subsystem_test: subsystem_test.c subsystem_test-entitlements.plist
subsystem_test: CODE_SIGN_ENTITLEMENTS=subsystem_test-entitlements.plist

install-subsystem_test_helper: subsystem_test_helper
	mkdir -p $(INSTALLDIR)
	cp $(SYMROOT)/subsystem_test_helper $(INSTALLDIR)/
ifeq ($(DWARF_DSYM_FILE_SHOULD_ACCOMPANY_PRODUCT),YES)
	cp -R $(SYMROOT)/subsystem_test_helper.dSYM $(INSTALLDIR)/
endif

