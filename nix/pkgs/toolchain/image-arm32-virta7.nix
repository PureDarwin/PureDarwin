{ stdenv
, lib
, e2fsprogs
, fakeroot
, cpio
, darwinCrossToolchain
, appleSdk
, libSystem
, libobjc
, libcxxabi
, icu
, corefoundation
, iokit
, launchd
, launchctl
, zsh
, toybox
, userland
, prelinked
, shim
  # more programs (their bin/) and the libraries they load (their usr/lib/*.dylib)
, extraBins ? [ ]
, extraLibs ? [ ]
, ramdiskMB ? 64
, bootArgs ? "debug=0x14e serial=3 -v keepsyms=1 wdt=-1 no_lock_timeouts=1 rd=md0"
  # swap libicudata for an empty data package, 31 MB smaller, for boards with little ram
, icuDataStub ? false
  # also write payload.bin for pdrkboot: the shim padded to 64 KB, then the cpio
, pdrkbootPayload ? false
  # the adb daemon and its launchd job, for boards with a usb gadget
, pdadbd ? null
  # a u-boot with an SPL (u-boot-sunxi-with-spl.bin) to write an SD card image around
, uboot ? null
, ubootTools
, dosfstools
, mtools
, util-linux
}:

# Minimal generic Cortex-A7 image: the prelinked kernel plus an ext4 ramdisk holding
# dyld, libSystem, libobjc, launchd as PID 1, its console login job, zsh and toybox.
#
# $out/initrd.cpio is what the arm32 shim reads (EFI/BOOT/kernel, boot-args.txt,
# ramdisk.img); $out/zImage is the shim itself; $out/run-qemu.sh boots both.
stdenv.mkDerivation {
  pname = "puredarwin-image-arm32-virta7";
  version = "0.1";

  dontUnpack = true;
  nativeBuildInputs = [ e2fsprogs fakeroot cpio ]
    ++ lib.optionals (uboot != null) [ ubootTools dosfstools mtools util-linux ];

  buildPhase = ''
    runHook preBuild
    sdk=$(ls -d ${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)

    # PID 1, built the way the userland tools are: host cross clang, -arch armv6
    # Headers from the SDK; the link resolves libSystem's re-exports against the
    # armv6 libSystem itself, so it is the sysroot there
    cc=${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang
    $cc -arch armv6 -isysroot "$sdk" -O2 -fblocks -c -o pd-mini-init.o ${./pd-mini-init.c}
    $cc -arch armv6 -isysroot ${libSystem} -o pd-mini-init pd-mini-init.o -lSystem

    root=$PWD/root
    mkdir -p $root/usr/lib/system $root/bin $root/sbin $root/dev $root/tmp $root/var/tmp \
      $root/etc $root/System/Library/CoreServices
    cp -L ${libSystem}/usr/lib/dyld ${libSystem}/usr/lib/libSystem.B.dylib $root/usr/lib/
    ln -s libSystem.B.dylib $root/usr/lib/libSystem.dylib
    # libSystem's fork path calls into libobjc (__objc_atfork_child); libobjc needs libc++abi
    cp -L ${libobjc}/usr/lib/libobjc.A.dylib $root/usr/lib/
    ln -s libobjc.A.dylib $root/usr/lib/libobjc.dylib
    cp -L ${libcxxabi}/usr/lib/libc++abi*.dylib $root/usr/lib/
    # launchd links CoreFoundation (which pulls ICU) and IOKitCF
    for d in ${icu} ${corefoundation} ${iokit}; do
      for f in $d/usr/lib/*.dylib; do cp -P "$f" $root/usr/lib/; done
    done
    cp -L ${libSystem}/usr/lib/system/*.dylib $root/usr/lib/system/
    cp -L ${userland}/bin/* $root/bin/
    # PID 1 is pd-mini-init: it checks the primitives launchd needs, then execs the real
    # launchd, which runs org.puredarwin.console-login and with it /bin/zsh
    install -m755 ${launchd}/pd-sbin/launchd $root/sbin/launchd.real
    install -m755 pd-mini-init $root/sbin/launchd
    # launchd bootstraps the system domain by running /bin/launchctl bootstrap -S System
    install -m755 ${launchctl}/bin/launchctl $root/bin/launchctl
    mkdir -p $root/usr/libexec $root/System/Library/LaunchDaemons $root/var/root $root/var/run $root/var/log
    install -m755 ${launchd}/usr/libexec/pd-console-login $root/usr/libexec/
    install -m644 ${launchd}/System/Library/LaunchDaemons/org.puredarwin.console-login.plist \
      $root/System/Library/LaunchDaemons/
    install -m755 pd-mini-init $root/usr/libexec/pd-mini-init
    install -m755 ${zsh}/bin/zsh $root/bin/zsh
    ln -s zsh $root/bin/sh
    # toybox supplies ls, cp, mkdir and the rest; the armv6 userland tools keep their names
    install -m755 ${toybox}/bin/toybox $root/bin/toybox
    for a in $(cat ${toybox}/applets.txt); do
      [ -e "$root/bin/$a" ] || ln -s toybox "$root/bin/$a"
    done
    for d in ${lib.concatStringsSep " " (map toString extraLibs)}; do
      for f in $d/usr/lib/*.dylib; do cp -P "$f" $root/usr/lib/; done
    done
    for d in ${lib.concatStringsSep " " (map toString extraBins)}; do
      for f in $d/bin/*; do install -m755 "$f" $root/bin/; done
    done
    ${lib.optionalString (pdadbd != null) ''
      install -Dm755 ${pdadbd}/usr/sbin/pdadbd $root/usr/sbin/pdadbd
      install -m644 ${pdadbd}/System/Library/LaunchDaemons/org.puredarwin.pdadbd.plist \
        $root/System/Library/LaunchDaemons/
    ''}
    ${lib.optionalString icuDataStub ''
      # same install name and versions as the real one, so libicuuc binds to it unchanged
      [ -e $root/usr/lib/libicudata.76.1.dylib ] || { echo "no libicudata.76.1.dylib to replace"; exit 1; }
      $cc -arch armv6 -dynamiclib -nostdlib -install_name /usr/lib/libicudata.76.dylib \
        -compatibility_version 76.0.0 -current_version 76.1.0 \
        -o libicudata-stub.dylib ${./icudata-stub.c}
      rm -f $root/usr/lib/libicudata.76.1.dylib
      install -m755 libicudata-stub.dylib $root/usr/lib/libicudata.76.1.dylib
    ''}
    chmod -R u+w $root
    chmod 1777 $root/tmp $root/var/tmp
    cat > $root/System/Library/CoreServices/SystemVersion.plist <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>ProductBuildVersion</key>
	<string>20F71</string>
	<key>ProductCopyright</key>
	<string>1983-2021 PureDarwin. All rights reserved.</string>
	<key>ProductName</key>
	<string>PureDarwin</string>
	<key>ProductUserVisibleVersion</key>
	<string>11.4</string>
	<key>ProductVersion</key>
	<string>11.4</string>
</dict>
</plist>
EOF

    # Same mke2fs shape as image.nix's ramdisk: our ext4 driver's feature set
    fakeroot bash <<FAKESCRIPT
    chown -R 0:0 "$root"
    mke2fs -q -F -t ext4 -b 4096 -O ^orphan_file -L darwin-ramdisk \
      -d "$root" ramdisk.img $(( ${toString ramdiskMB} * 256 )) >/dev/null
FAKESCRIPT

    mkdir -p cpio/EFI/BOOT
    cp -L ${prelinked}/System/Library/Kernels/kernel.virta7 cpio/EFI/BOOT/kernel
    printf '%s\n' ${lib.escapeShellArg bootArgs} > cpio/EFI/BOOT/boot-args.txt
    cp ramdisk.img cpio/EFI/BOOT/ramdisk.img
    # kernel ahead of the ramdisk: the shim can then load it low and slide the ramdisk down
    (cd cpio && printf '%s\n' EFI EFI/BOOT EFI/BOOT/kernel EFI/BOOT/boot-args.txt EFI/BOOT/ramdisk.img \
      | cpio -o -H newc --quiet) > initrd.cpio
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp initrd.cpio ramdisk.img $out/
    cp -L ${shim}/boot/xnu-loader-arm32.zImage $out/zImage
    ${lib.optionalString pdrkbootPayload ''
      # the shim finds a cpio 64 KB past its own start when entered through u-boot's go
      [ $(stat -c %s $out/zImage) -le 65536 ] || { echo "shim over 64 KB"; exit 1; }
      cp $out/zImage $out/payload.bin
      chmod u+w $out/payload.bin
      truncate -s 65536 $out/payload.bin
      cat initrd.cpio >> $out/payload.bin
    ''}
    ${lib.optionalString (uboot != null) ''
      # u-boot's own control fdt is the board's device tree, which is what the shim reads
      cat > boot.cmd <<'SCRIPT'
load mmc 0:1 0x42000000 zImage
load mmc 0:1 0x4a000000 initrd.cpio
setenv initrd_size $filesize
bootz 0x42000000 0x4a000000:$initrd_size $fdtcontroladdr
SCRIPT
      mkimage -C none -A arm -T script -d boot.cmd boot.scr

      # 64 MB of slack past the payload; one fat16 partition at 1 MB, spl at 8 KB
      payload=$(( $(stat -c %s $out/zImage) + $(stat -c %s initrd.cpio) ))
      mb=$(( 1 + payload / 1048576 + 64 ))
      truncate -s "''${mb}M" sdcard.img
      sfdisk --quiet sdcard.img <<TABLE
label: dos
start=2048, type=6, bootable
TABLE
      truncate -s $(( (mb - 1) * 1048576 )) boot.fat
      mkfs.vfat -F 16 -n PDBOOT boot.fat >/dev/null
      mcopy -i boot.fat $out/zImage ::zImage
      mcopy -i boot.fat initrd.cpio ::initrd.cpio
      mcopy -i boot.fat boot.scr ::boot.scr
      dd if=boot.fat of=sdcard.img bs=1M seek=1 conv=notrunc status=none
      # the sunxi brom reads the spl from 8 KB in, clear of both the table and the partition
      dd if=${uboot}/u-boot-sunxi-with-spl.bin of=sdcard.img bs=1024 seek=8 conv=notrunc status=none
      cp sdcard.img boot.scr $out/
    ''}
    cat > $out/run-qemu.sh <<EOF
#!/bin/sh
# Boot the minimal image on QEMU's virt board with a Cortex-A7, serial on stdio
exec qemu-system-arm -M virt -cpu cortex-a7 -m 1024 -kernel $out/zImage \\
  -initrd $out/initrd.cpio -display none -serial stdio -monitor none "\$@"
EOF
    chmod +x $out/run-qemu.sh
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin minimal generic Cortex-A7 image (ramdisk root, QEMU virt)";
    platforms = platforms.linux;
  };
}
