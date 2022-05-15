# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

WIFI_FRAMEWORK=-framework CoreWLAN
ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
CORETELEPHONY=-framework CoreTelephony
ARCHS=arm64 arm64e
WIFI_FRAMEWORK=-framework MobileWiFi 
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
ARCHS=x86_64 arm64e
CORETELEPHONY=
endif

ifeq ($(PLATFORM),watchos)
# watchOS internal SDK
CORETELEPHONY=-framework CoreTelephony
ARCHS=arm64_32
WIFI_FRAMEWORK=-framework MobileWiFi 
endif

ifeq ($(PLATFORM),)
# Mac OS X native SDK
ARCHS=x86_64
CORETELEPHONY=
CC = cc
SYSROOT = /
else
# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc
endif

WARNING_CFLAGS = -Weverything -Wno-missing-variable-declarations -Wno-reserved-id-macro -Wno-gnu-zero-variadic-macro-arguments -Wno-zero-length-array -Wno-cast-align -Wno-cstring-format-directive -Wno-nullability-extension -Wno-format-pedantic -Wno-objc-missing-property-synthesis -Wno-c++98-compat-pedantic -Wno-gnu-empty-initializer -Werror -Wno-error=deprecated-declarations -Wno-error=deprecated-implementations -Wno-error=objc-designated-initializers -Wno-error=nonnull -Wno-error=nullable-to-nonnull-conversion -Wno-error=nullability-declspec -Wno-error=#warnings

PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))
SCPRIV=-DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS

SYSPRIV=-I$(SYSROOT)/System/Library/Frameworks/System.framework/PrivateHeaders


netconfig: netconfig.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSPRIV) -framework SystemConfiguration -framework CoreFoundation $(WARNING_CFLAGS) -g -o $@ $^
	codesign -s - $@

clean:
	rm -f netconfig
	rm -rf *.dSYM *.o

