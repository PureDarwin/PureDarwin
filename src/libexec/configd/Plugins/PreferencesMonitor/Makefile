# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

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

PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))
SCPRIV=-DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS

SYSPRIV=-I$(SYSROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

updatePreboot: prefsmon_log.c preboot.m
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -framework Foundation -framework DiskManagement -framework DiskArbitration -framework SystemConfiguration $(PF_INC) -DTEST_PREBOOT -Wall -g -DSC_LOG_HANDLE=__log_PreferencesMonitor -o $@ $^
	codesign -s - $@

clean:
	rm -rf updatePreboot preboot.o prefsmon_log.o *.dSYM *~
