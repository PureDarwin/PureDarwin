{ stdenvNoCC, lib, perl, bash, ed, unifdef, zlib, libxml2 }:

let
  root = ../../..;
  sources = [
    [ "${root}/src/Kernel/xnu/EXTERNAL_HEADERS" "usr/include" ]
    [ "${root}/src/Kernel/xnu/bsd/arm" "usr/include/arm" ]
    [ "${root}/src/Kernel/xnu/bsd/bsm" "usr/include/bsm" ]
    [ "${root}/src/Kernel/xnu/bsd/i386" "usr/include/i386" ]
    [ "${root}/src/Kernel/xnu/bsd/machine" "usr/include/machine" ]
    [ "${root}/src/Kernel/xnu/bsd/net" "usr/include/net" ]
    [ "${root}/src/Kernel/xnu/bsd/netinet" "usr/include/netinet" ]
    [ "${root}/src/Kernel/xnu/bsd/netinet6" "usr/include/netinet6" ]
    [ "${root}/src/Kernel/xnu/bsd/sys" "usr/include/sys" ]
    [ "${root}/src/Kernel/xnu/iokit/IOKit" "System/Library/Frameworks/IOKit.framework/Headers" ]
    [ "${root}/src/Kernel/xnu/libkern/libkern" "usr/include/libkern" ]
    [ "${root}/src/Kernel/xnu/osfmk/mach" "usr/include/mach" ]
    [ "${root}/src/Libraries/AvailabilityVersions/include" "usr/include" ]
    [ "${root}/src/Libraries/dyld/upstream/include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libc/include/mach" "usr/include/mach" ]
    [ "${root}/src/Libraries/syslog/libsystem_asl.tproj/include" "usr/include" ]
    # servers/bootstrap.h, xpc/, launch.h, vproc.h; libsystem_kernel's servers/
    # has only the netname/ls/nm/key defs.
    [ "${root}/src/Libraries/XPC/libxpc/include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libc/libdarwin/os-include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libc/libdarwin/sys-include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libdispatch/os" "usr/include/os" ]
    [ "${root}/src/Libraries/libSystem/libdispatch/dispatch" "usr/include/dispatch" ]
    [ "${root}/src/Libraries/libSystem/libsystem_kernel/include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libmalloc/include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/libplatform/include" "usr/include" ]
    [ "${root}/src/Libraries/libSystem/pthread/include" "usr/include" ]
    [ "${root}/src/Libraries/libcxx/include" "usr/include/c++/v1" ]
    [ "${zlib.dev}/include" "usr/include" ]
    [ "${libxml2.dev}/include/libxml2" "usr/include/libxml2" ]
  ];
  files = [
    [ "${root}/src/Kernel/xnu/EXTERNAL_HEADERS/stdatomic.h" "usr/include/puredarwin/stdatomic.h" ]
    [ "${root}/src/Libraries/libSystem/libdispatch/os/object.h" "usr/include/os/object.h" ]
    [ "${root}/src/Libraries/libSystem/pthread/include/pthread/pthread.h" "usr/include/pthread.h" ]
    [ "${root}/src/Libraries/libSystem/pthread/include/pthread/sched.h" "usr/include/sched.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/grp.h" "usr/include/grp.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/math.h" "usr/include/math.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/netdb.h" "usr/include/netdb.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/pwd.h" "usr/include/pwd.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/MacTypes.h" "usr/include/MacTypes.h" ]
    # The userspace mach.h, not xnu's osfmk one: only this declares slot_name and
    # pulls mach_traps.h. Installed here so it lands after headers.sh's copy.
    [ "${root}/src/Libraries/libSystem/libc/include/mach/mach.h" "usr/include/mach/mach.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/copyfile.h" "usr/include/copyfile.h" ]
    # Individually: pd-compat-include's mach-o/loader.h is worse than dyld's.
    [ "${root}/src/Libraries/libdarwin/pd-compat-include/bsm/libbsm.h" "usr/include/bsm/libbsm.h" ]
    [ "${root}/src/Libraries/libdarwin/pd-compat-include/mach-o/ldsyms.h" "usr/include/mach-o/ldsyms.h" ]
    [ "${root}/src/Libraries/libdarwin/pd-compat-include/mach-o/getsect.h" "usr/include/mach-o/getsect.h" ]
    # FreeBSD's fenv.h renamed; reaches these by angle bracket. Only these - e.g.
    # openlibm_complex.h defines a bare "I".
    [ "${root}/src/Libraries/libSystem/libc/libm/openlibm/include/openlibm_fenv.h" "usr/include/fenv.h" ]
    [ "${root}/src/Libraries/libSystem/libc/libm/openlibm/include/openlibm_fenv_amd64.h" "usr/include/openlibm_fenv_amd64.h" ]
    [ "${root}/src/Libraries/libSystem/libc/libm/openlibm/include/openlibm_fenv_arm.h" "usr/include/openlibm_fenv_arm.h" ]
    # From cctools, not xnu: xnu's copy has no FAT_MAGIC_64/fat_arch_64, and
    # still does not as of xnu-12377. Same for arch.h.
    [ "${root}/tools/cctools/include/mach-o/fat.h" "usr/include/mach-o/fat.h" ]
    [ "${root}/tools/cctools/include/mach-o/arch.h" "usr/include/mach-o/arch.h" ]
    [ "${root}/tools/cctools/include/mach-o/swap.h" "usr/include/mach-o/swap.h" ]
    [ "${root}/tools/cctools/include/mach-o/ranlib.h" "usr/include/mach-o/ranlib.h" ]
    [ "${root}/src/Libraries/libresolv/include/ifaddrs.h" "usr/include/ifaddrs.h" ]
    [ "${root}/src/Kernel/xnu/osfmk/kern/kcdata.h" "usr/include/kern/kcdata.h" ]
    # MIG-generated device_user.h includes this.
    [ "${root}/src/Kernel/xnu/osfmk/device/device_types.h" "usr/include/device/device_types.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/sysdir.h" "usr/include/sysdir.h" ]
    [ "${root}/src/Libraries/XPC/notify/notify_keys.h" "usr/include/notify_keys.h" ]
    # Reconstructed; see the headers for where the layouts came from.
    [ "${root}/src/Libraries/XPC/libinfo/aliasdb.h" "usr/include/aliasdb.h" ]
    [ "${root}/src/Libraries/XPC/libinfo/printerdb.h" "usr/include/printerdb.h" ]
    # libdispatch's userspace BlocksRuntime, not xnu's libkern copy.
    [ "${root}/src/Libraries/libSystem/libdispatch/src/BlocksRuntime/Block.h" "usr/include/Block.h" ]
    [ "${root}/src/Libraries/libSystem/libdispatch/src/BlocksRuntime/Block_private.h" "usr/include/Block_private.h" ]
    [ "${root}/src/Libraries/libresolv/resolv.h" "usr/include/resolv.h" ]
    # Both spellings: resolv.h includes <nameser.h>, everyone else uses the
    # <arpa/nameser.h> Darwin ships (its nameser_compat.h sibling is already there).
    [ "${root}/src/Libraries/libresolv/nameser.h" "usr/include/nameser.h" ]
    [ "${root}/src/Libraries/libresolv/nameser.h" "usr/include/arpa/nameser.h" ]
    [ "${root}/src/Libraries/libresolv/dns.h" "usr/include/dns.h" ]
    [ "${root}/src/Libraries/libresolv/dns_util.h" "usr/include/dns_util.h" ]
    [ "${root}/src/Libraries/libSystem/libplatform/include/setjmp.h" "usr/include/setjmp.h" ]
    [ "${root}/src/Libraries/libcxxabi/config/__config_site" "usr/include/c++/v1/__config_site" ]
    [ "${root}/src/Libraries/libcxx/vendor/llvm/default_assertion_handler.in" "usr/include/c++/v1/__assertion_handler" ]
  ];
  libSystemSymbolSources = [
    "${root}/src/Libraries/libSystem/stub/libSystem.exports"
    "${root}/src/Libraries/libSystem/libc/scripts/legacy_alias.list"
    # x86_64, not i386: the tbd targets 64-bit, and only the i386 map carries the
    # 60 $UNIX2003 aliases that 64-bit does not have.
    "${root}/src/Libraries/libSystem/libsystem_kernel/Platforms/MacOSX/x86_64/syscall.map"
    "${./libSystem-compat.exports}"
  ];
  installSource = source: ''
    mkdir -p "$sdk/${source.target}"
    chmod -R u+w "$sdk/${source.target}"
    cp -RL ${source.path}/. "$sdk/${source.target}/"
  '';
  installFile = source: ''install -Dm644 ${source.path} "$sdk/${source.target}"'';
in
stdenvNoCC.mkDerivation {
  pname = "puredarwin-oss-sdk";
  version = "20.4";

  dontUnpack = true;
  dontConfigure = true;
  dontBuild = true;
  dontFixup = true;
  nativeBuildInputs = [ perl bash ed unifdef ];

  installPhase = ''
    runHook preInstall
    sdk="$out/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    ${lib.concatMapStrings installSource (map (entry: { path = builtins.elemAt entry 0; target = builtins.elemAt entry 1; }) sources)}
    ARCHS=x86_64 SRCROOT=${root}/src/Libraries/libSystem/libc \
      DERIVED_FILES_DIR="$TMPDIR/libc-derived" VARIANT_PLATFORM_NAME=macosx \
      DEPLOYMENT_LOCATION=NO BUILT_PRODUCTS_DIR="$TMPDIR/libc-headers" \
      SDK_INSTALL_HEADERS_ROOT="" bash ${root}/src/Libraries/libSystem/libc/scripts/headers.sh
    chmod -R u+w "$sdk/usr/include"
    cp -RL "$TMPDIR/libc-headers/usr/include/." "$sdk/usr/include/"
    # Hand-written and reconstructed SDK content, laid out as it lands. After
    # the generated headers above so it wins where both supply a file.
    cp -RL ${./oss-sdk}/. "$sdk/"
    chmod -R u+w "$sdk/usr"
    # Apple resolves the PLATFORM_* blocks when it builds an SDK; the xnu copy
    # has none defined, so this reads 0 and 64-bit calls asm-rename to $UNIX2003.
    sed -i '1i #define __DARWIN_ONLY_UNIX_CONFORMANCE 1' "$sdk/usr/include/sys/cdefs.h"
    # Quote-included by the openlibm_fenv_*.h above, and openlibm keeps them in
    # src/ rather than include/.
    for h in cdefs-compat types-compat; do
      install -Dm644 ${root}/src/Libraries/libSystem/libc/libm/openlibm/src/$h.h \
        "$sdk/usr/include/$h.h"
    done
    # launchd resolves <CoreFoundation/*.h> through -F, so CF needs framework
    # shape. Flattened from the subprojects rather than tracking CF's own
    # PUBLIC_HEADERS list; over-inclusive, but that list drifts.
    mkdir -p "$sdk/System/Library/Frameworks/CoreFoundation.framework/Headers"
    # One cp per file: a couple of basenames repeat across subprojects and a
    # single cp refuses to overwrite what it just created.
    for h in ${root}/src/Libraries/CoreFoundation/*.subproj/*.h; do
      cp -Lf "$h" "$sdk/System/Library/Frameworks/CoreFoundation.framework/Headers/"
    done
    # Not a sources entry: runtime/ mixes headers with .mm/.cpp. Same *.h set
    # libobjc.nix flattens into its own <objc/*.h> dir.
    mkdir -p "$sdk/usr/include/objc"
    cp -RL ${root}/src/Libraries/objc4/runtime/*.h "$sdk/usr/include/objc/"
    mkdir -p "$sdk/usr/include/puredarwin"
    ${lib.concatMapStringsSep "\n" installFile (map (entry: { path = builtins.elemAt entry 0; target = builtins.elemAt entry 1; }) files)}
    {
      printf '%s\n' '--- !tapi-tbd' 'tbd-version: 4' \
        'targets: [ x86_64-macos, arm64-macos ]' \
        "install-name: '/usr/lib/libSystem.B.dylib'" \
        'current-version: 1292.100.5' 'compatibility-version: 1' \
        'exports:' '  - targets: [ x86_64-macos, arm64-macos ]'
      printf '    symbols: [ '
      separator=
      for source in ${lib.escapeShellArgs libSystemSymbolSources}; do
        while read -r symbol remainder; do
          case "$symbol" in
            ""|'#'*) continue ;;
          esac
          printf '%s%s' "$separator" "$symbol"
          separator=', '
        done < "$source"
      done
      printf '%s\n' ' ]' '...'
    } > "$sdk/usr/lib/libSystem.tbd"
    for arch in x86_64 arm64; do
      mkdir -p "$sdk/usr/include/$arch"
      ARCHS="$arch" SRCROOT=${root}/src/Libraries/libSystem/libc \
        DERIVED_FILES_DIR="$TMPDIR/libc-features" VARIANT_PLATFORM_NAME=macosx \
        perl ${root}/src/Libraries/libSystem/libc/scripts/generate_features.pl
      cp "$TMPDIR/libc-features/$arch/libc-features.h" "$sdk/usr/include/$arch/"
    done
    ln -s MacOSX.sdk "$out/Platforms/MacOSX.platform/Developer/SDKs/MacOSX20.4.sdk"
    runHook postInstall
  '';

  meta = with lib; {
    description = "Redistributable PureDarwin SDK assembled from open-source headers";
    license = licenses.free;
    platforms = platforms.unix;
  };
}
