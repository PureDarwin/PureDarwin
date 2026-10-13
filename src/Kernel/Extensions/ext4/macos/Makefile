# ext4 for macOS: PureDarwin's ext4.kext and its ext4.fs bundle, built with Xcode on an Intel Mac
#
#   make            build/ext4.kext and build/ext4.fs
#   sudo make install   copy both to /Library and stage the kext with kmutil

VERSION  = 1.0.0
ARCHS    = x86_64
MINOS    = 11.0
SDK     := $(shell xcrun --show-sdk-path)
KEXTSRC  = ..
UTILSRC  = ../../../../Userspace/ext4util
OUT      = build

ARCHFLAGS = $(foreach a,$(ARCHS),-arch $(a))
KCFLAGS   = $(ARCHFLAGS) -mkernel -nostdinc -fno-builtin -fno-common -O2 -g \
            -DKERNEL -DAPPLE -DNeXT -isysroot $(SDK) -mmacosx-version-min=$(MINOS) \
            -I$(SDK)/System/Library/Frameworks/Kernel.framework/Headers -Wno-everything
KCXXFLAGS = $(KCFLAGS) -fno-rtti -fno-exceptions -fapple-kext
UCFLAGS   = $(ARCHFLAGS) -O2 -isysroot $(SDK) -mmacosx-version-min=$(MINOS)

KOBJS = $(patsubst $(KEXTSRC)/%.c,$(OUT)/obj/%.o,$(wildcard $(KEXTSRC)/*.c)) \
        $(patsubst $(KEXTSRC)/%.cpp,$(OUT)/obj/%.o,$(wildcard $(KEXTSRC)/*.cpp))
KOBJS += $(OUT)/obj/kmod_info.o
KEXT  = $(OUT)/ext4.kext
FSB   = $(OUT)/ext4.fs

all: $(KEXT) $(FSB)

$(OUT)/obj/%.o: $(KEXTSRC)/%.c $(wildcard $(KEXTSRC)/*.h)
	@mkdir -p $(@D)
	clang $(KCFLAGS) -c -o $@ $<

$(OUT)/obj/%.o: $(KEXTSRC)/%.cpp $(wildcard $(KEXTSRC)/*.h)
	@mkdir -p $(@D)
	clang++ $(KCXXFLAGS) -c -o $@ $<

# what Xcode generates for every kext: kmutil refuses a kext without kmod_info. IOKit
# starts this one, so there is no C start or stop function and libkmod supplies _start and _stop
$(OUT)/obj/kmod_info.c: Makefile
	@mkdir -p $(@D)
	printf '%s\n' '#include <mach/kmod.h>' \
	    'extern kern_return_t _start(kmod_info_t *ki, void *data);' \
	    'extern kern_return_t _stop(kmod_info_t *ki, void *data);' \
	    '__attribute__((visibility("default"))) KMOD_EXPLICIT_DECL(org.puredarwin.kext.ext4, "$(VERSION)", _start, _stop)' \
	    '__private_extern__ kmod_start_func_t *_realmain = 0;' \
	    '__private_extern__ kmod_stop_func_t *_antimain = 0;' \
	    '__private_extern__ int _kext_apple_cc = __APPLE_CC__;' > $@

$(OUT)/obj/kmod_info.o: $(OUT)/obj/kmod_info.c
	clang $(KCFLAGS) -c -o $@ $<

$(KEXT): $(KOBJS) Info.plist
	rm -rf $@
	mkdir -p $@/Contents/MacOS
	sed 's/@VERSION@/$(VERSION)/g' Info.plist > $@/Contents/Info.plist
	clang++ $(ARCHFLAGS) -mkernel -nostdlib -Xlinker -kext -isysroot $(SDK) \
	    -mmacosx-version-min=$(MINOS) -lkmod -lcc_kext -o $@/Contents/MacOS/ext4 $(KOBJS)
	codesign --force --sign - $@

$(FSB): $(UTILSRC)/ext4.util.c mount_ext4.c $(UTILSRC)/ext4.fs/Contents/Info.plist
	rm -rf $@
	mkdir -p $@/Contents/Resources
	cp $(UTILSRC)/ext4.fs/Contents/Info.plist $@/Contents/
	clang $(UCFLAGS) -o $@/Contents/Resources/ext4.util $(UTILSRC)/ext4.util.c
	clang $(UCFLAGS) -o $@/Contents/Resources/mount_ext4 mount_ext4.c
	codesign --force --sign - $@/Contents/Resources/ext4.util $@/Contents/Resources/mount_ext4

install: all
	rm -rf /Library/Extensions/ext4.kext /Library/Filesystems/ext4.fs
	cp -R $(KEXT) /Library/Extensions/
	cp -R $(FSB) /Library/Filesystems/
	chown -R root:wheel /Library/Extensions/ext4.kext /Library/Filesystems/ext4.fs
	kmutil load -p /Library/Extensions/ext4.kext

clean:
	rm -rf $(OUT)

.PHONY: all install clean
