{ stdenv
, lib
, cmake
, ninja
, darwinCrossToolchain
, nativeLd
, libSystem
, kcToolsSrc
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  kextList = import ../../lib/kc-kexts.nix;
in
stdenv.mkDerivation {
  pname = "puredarwin-kc-tools-guest";
  version = "0.1";

  src = kcToolsSrc;

  nativeBuildInputs = [ cmake ninja ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    cmake -B build -G Ninja \
      -DCMAKE_SYSTEM_NAME=Darwin \
      -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
      -DCMAKE_C_COMPILER=${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -DCMAKE_AR=${darwinCrossToolchain}/bin/${targetTriple}-ar \
      -DCMAKE_RANLIB=${darwinCrossToolchain}/bin/${targetTriple}-ranlib \
      -DCMAKE_C_FLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -I${libSystem}/usr/include" \
      -DCMAKE_EXE_LINKER_FLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 -lSystem" \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DCMAKE_BUILD_TYPE=Release

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    DESTDIR="$out" ninja -C build install

    mkdir -p "$out/usr/sbin"
    cat > "$out/usr/sbin/pd-rebuild-kc" <<'PDKCEOF'
#!/bin/sh
# Rebuild the boot kernel collection from what is installed on this system.
#
#   pd-rebuild-kc [output]     write the KC to <output> (default /var/tmp/kernel)
#   pd-rebuild-kc -i           build it and install it onto the ESP
#
# The kext list below is generated from nix/lib/kc-kexts.nix, the same list the
# image's KC is built from, so the two cannot drift.
#
#  -kext     real, linked kexts (code goes into the KC)
#  -codeless Info.plist-only KPI kexts, present so the OSBundleLibraries
#            dependencies of the real kexts resolve
set -e

KEXTS=/System/Library/Extensions
ESP_DEV=''${PD_ESP_DEV:-/dev/disk0s1}
ESP_MNT=''${PD_ESP_MNT:-/var/tmp/esp}

install=no
outfile=/var/tmp/kernel
mounted_here=no
case "$1" in
    -i|--install) install=yes ;;
    "") ;;
    *) outfile=$1 ;;
esac

KERNEL_BIN=$(ls /System/Library/Kernels/kernel* 2>/dev/null | head -n1)
if [ -z "$KERNEL_BIN" ]; then
    echo "pd-rebuild-kc: no kernel in /System/Library/Kernels" >&2
    exit 1
fi

set -- -kernel "$KERNEL_BIN"
PDKCEOF

    # The kext arguments, from the shared list.
${lib.concatMapStringsSep "\n" (k: ''
    echo 'set -- "$@" -kext "$KEXTS/${k}"' >> "$out/usr/sbin/pd-rebuild-kc"'') kextList}

    cat >> "$out/usr/sbin/pd-rebuild-kc" <<'PDKCEOF'

for p in "$KEXTS"/System.kext/PlugIns/*.kext; do
    [ -e "$p" ] || continue
    set -- "$@" -codeless "$p"
done

echo "pd-rebuild-kc: building from $KERNEL_BIN"
/usr/bin/kc-builder "$@" -o "$outfile"
echo "pd-rebuild-kc: wrote $outfile"

if [ "$install" = yes ]; then
    # The KC the firmware loads lives on the ESP, not the root filesystem.
    if ! mount | grep -q " $ESP_MNT "; then
        /bin/mkdir -p "$ESP_MNT"
        echo "pd-rebuild-kc: mounting $ESP_DEV at $ESP_MNT"
        mount -t msdos "$ESP_DEV" "$ESP_MNT"
        mounted_here=yes
    fi
    # Keep the previous KC: if the new one does not boot, the bootloader path
    # is the only way back and there is no second chance to save a copy.
    if [ -f "$ESP_MNT/EFI/BOOT/kernel" ] && [ ! -f "$ESP_MNT/EFI/BOOT/kernel.prev" ]; then
        cp "$ESP_MNT/EFI/BOOT/kernel" "$ESP_MNT/EFI/BOOT/kernel.prev"
        echo "pd-rebuild-kc: saved previous KC as kernel.prev"
    fi
    cp "$outfile" "$ESP_MNT/EFI/BOOT/kernel"
    sync
    echo "pd-rebuild-kc: installed to $ESP_MNT/EFI/BOOT/kernel"
    if [ "$mounted_here" = yes ]; then
        umount "$ESP_MNT"
    fi
fi
PDKCEOF
    chmod +x "$out/usr/sbin/pd-rebuild-kc"

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "kc-tools (kc-builder, prelink-builder) built to run on PureDarwin, plus pd-rebuild-kc";
    platforms = platforms.linux;
  };
}
