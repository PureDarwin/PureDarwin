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
    [ "${./stdatomic.h}" "usr/include/stdatomic.h" ]
    [ "${root}/src/Kernel/xnu/EXTERNAL_HEADERS/stdatomic.h" "usr/include/puredarwin/stdatomic.h" ]
    [ "${root}/src/Libraries/libSystem/libdispatch/os/object.h" "usr/include/os/object.h" ]
    [ "${root}/src/Libraries/libSystem/pthread/include/pthread/pthread.h" "usr/include/pthread.h" ]
    [ "${root}/src/Libraries/libSystem/pthread/include/pthread/sched.h" "usr/include/sched.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/grp.h" "usr/include/grp.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/math.h" "usr/include/math.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/netdb.h" "usr/include/netdb.h" ]
    [ "${root}/src/Libraries/libSystem/libc/include/pwd.h" "usr/include/pwd.h" ]
    [ "${./mach_interface.h}" "usr/include/mach/mach_interface.h" ]
    [ "${root}/src/Libraries/libSystem/libplatform/include/setjmp.h" "usr/include/setjmp.h" ]
    [ "${root}/src/Libraries/libcxxabi/config/__config_site" "usr/include/c++/v1/__config_site" ]
    [ "${root}/src/Libraries/libcxx/vendor/llvm/default_assertion_handler.in" "usr/include/c++/v1/__assertion_handler" ]
    [ "${./libc++.tbd}" "usr/lib/libc++.tbd" ]
    [ "${./libz.tbd}" "usr/lib/libz.tbd" ]
  ];
  libSystemSymbolSources = [
    "${root}/src/Libraries/libSystem/stub/libSystem.exports"
    "${root}/src/Libraries/libSystem/libc/scripts/legacy_alias.list"
    "${root}/src/Libraries/libSystem/libsystem_kernel/Platforms/MacOSX/i386/syscall.map"
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
    cp ${./libc-features.h} "$sdk/usr/include/libc-features.h"
    ln -s MacOSX.sdk "$out/Platforms/MacOSX.platform/Developer/SDKs/MacOSX20.4.sdk"
    runHook postInstall
  '';

  meta = with lib; {
    description = "Redistributable PureDarwin SDK assembled from open-source headers";
    license = licenses.free;
    platforms = platforms.unix;
  };
}
