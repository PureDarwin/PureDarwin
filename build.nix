{ stdenv
, lib
, cmake
, ninja
, requireFile
, darwinCrossToolchain ? null
, nativeLd ? null
, nativeUnifdef ? null
, nativeMigcom ? null
, pinnedAppleSdk ? null
, openssl
, libxml2
, m4
, bison
, flex
, perl
, bash
, zlib
, ed
, unifdef
, tcsh
, gnustep-base
, pax
, coreutils
, findutils
, gawk
, gnused
, clang
, libuuid
, ruby
, iig
, cctools
, tinycc
, src ? ./.
, pname ? "puredarwin-nix-toolchain"
, version ? "0.1"
, buildTargets ? [ "launchd" ]
, enableProjects ? true
, enableKernel ? true
, enableLibraries ? true
, enableUserspace ? true
, enableTools ? true
, enableTcc ? false
, enableIOGraphicsFamily ? false
, installUserland ? true
, installUserlandTargetsOnly ? false
, installKernel ? false
, installXnuHeaders ? false
, installKexts ? false
, installKextNames ? [ ]
, installLibSystem ? false
, installBaseSystem ? false
, prebuiltLibSystem ? null
, xnuKernelConfig ? "RELEASE"
, xorgDriverIncludes ? null
, extraCmakeFlags ? [ ]
, puredarwinArch ? "x86_64"
, arm64CrossToolchain ? null
, compilerRt ? null
, compilerRtArm64 ? null
}:

let
  isDarwinHost = stdenv.hostPlatform.isDarwin;
  isArm64 = puredarwinArch == "arm64";
  # nix-toolchain.cmake's NIX_DARWIN_TOOLCHAIN_DIR just needs to point at
  # whichever wrapper set matches NIX_DARWIN_HOST below.
  activeCrossToolchain = if isArm64 then arm64CrossToolchain else darwinCrossToolchain;
  # Every component that configures src/Libraries also builds libdyld, which
  # needs compiler-rt for __isPlatformVersionAtLeast (@available's backend).
  # Selected here rather than per-derivation so no component can be missed.
  activeCompilerRt = if isArm64 then compilerRtArm64 else compilerRt;
  nixDarwinHost = if isArm64 then "arm64-apple-darwin20.4" else "x86_64-apple-darwin20.4";
  sdkTarball = if isDarwinHost then null else requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };
  # xar and ctfconvert are host tools, so they want the host's zlib/libxml2. The
  # nix apple-sdk ships neither the headers nor the .tbd stubs, so the SDK paths
  # only work for the Linux cross SDK tarball.
  zlibInclude =
    if isDarwinHost
    then "${zlib.dev}/include"
    else "$DARWIN_SDK_ROOT/usr/include";
  zlibLibrary =
    if isDarwinHost
    then "${zlib.out}/lib/libz.dylib"
    else "$DARWIN_SDK_ROOT/usr/lib/libz.tbd";
  libxml2Include =
    if isDarwinHost
    then "${libxml2.dev}/include/libxml2"
    else "$DARWIN_SDK_ROOT/usr/include/libxml2";
  libxml2Library =
    if isDarwinHost
    then "${libxml2.out}/lib/libxml2.dylib"
    else "$DARWIN_SDK_ROOT/usr/lib/libxml2.tbd";
  opensslCryptoLibrary =
    if isDarwinHost
    then "${openssl.out}/lib/libcrypto.dylib"
    else "${openssl.out}/lib/libcrypto.so";
  opensslSslLibrary =
    if isDarwinHost
    then "${openssl.out}/lib/libssl.dylib"
    else "${openssl.out}/lib/libssl.so";
in
stdenv.mkDerivation ({
  inherit pname version;

  inherit src;

  hardeningDisable = lib.optionals isDarwinHost [ "fortify" "fortify3" ];

  nativeBuildInputs = [
    cmake ninja bison flex perl bash ed unifdef tcsh
    pax coreutils findutils gawk gnused clang ruby iig
    nativeUnifdef nativeMigcom
  ] ++ lib.optionals (!isDarwinHost) [
    activeCrossToolchain gnustep-base
  ] ++ lib.optionals isDarwinHost [
    cctools
  ];

  buildInputs = [ zlib openssl ] ++ lib.optionals (!isDarwinHost) [ libuuid ];

  configurePhase = ''
    runHook preConfigure
  '' + lib.optionalString (!isDarwinHost) ''
    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
  '' + lib.optionalString isDarwinHost ''
    export DEVELOPER_DIR="${pinnedAppleSdk}"
    export DARWIN_SDK_ROOT="$DEVELOPER_DIR/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    # xnu's makefiles otherwise resolve these by shelling out to xcrun again,
    # which fails in the nix sandbox and leaves its error text in $(PLATFORM).
    export SDKROOT="$DARWIN_SDK_ROOT"
    export SDKROOT_RESOLVED="$DARWIN_SDK_ROOT"
    export HOST_SDKROOT_RESOLVED="$DARWIN_SDK_ROOT"
    export PUREDARWIN_SDKROOT="$DARWIN_SDK_ROOT"
    export NIX_CXXSTDLIB_COMPILE=""
    export NIX_CXXSTDLIB_COMPILE_${lib.replaceStrings [ "-" "." ] [ "_" "_" ] stdenv.hostPlatform.config}=""
    export PLATFORM=MacOSX
    # xnu defaults HOST_GM4 to the system gm4, which aborts on CoreFoundation's
    # fork-without-exec guard when flex forks it. Use nixpkgs m4, as Linux does.
    export HOST_GM4=${m4}/bin/m4
  '' + ''

    if [ -e src/Kernel/xnu/Makefile ]; then
      sed -i 's#/bin/pwd#pwd#g' src/Kernel/xnu/Makefile
    fi
  '' + lib.optionalString (!isDarwinHost) ''
    if [ -e src/Kernel/xnu/cmake/MakeInc.cmd.in ]; then
      sed -i "s#/usr/local/osxcross/bin/xcrun#${activeCrossToolchain}/bin/xcrun#g" \
        src/Kernel/xnu/cmake/MakeInc.cmd.in
      sed -i -E 's#(^|[[:space:]=])/(usr/)?bin/([A-Za-z_]+)#\1\3#g' \
        src/Kernel/xnu/cmake/MakeInc.cmd.in
    fi
    if [ -e tools/mig/mig.sh ]; then
      sed -i "s#/usr/local/osxcross/bin/xcrun#${activeCrossToolchain}/bin/xcrun#g" \
        tools/mig/mig.sh
    fi
  '' + ''

    mkdir -p .nix-stubs
    cat > .nix-stubs/sw_vers <<'EOF'
#!/bin/sh
echo 11.3
EOF
    chmod +x .nix-stubs/sw_vers
    export PATH="$PWD/.nix-stubs:$PATH"
    if [ -e src/Kernel/xnu/cmake/make_symbol_aliasing.sh.in ]; then
      sed -i '1s#.*#\#!'"$(command -v bash)"'#' src/Kernel/xnu/cmake/make_symbol_aliasing.sh.in
    fi
    patchShebangs src ${lib.optionalString enableTools "tools"}
    if [ -e tools/mig ]; then
      patchShebangs tools/mig
    fi
    if [ -e src/Kernel/xnu/SETUP/config/doconf ]; then
      sed -i '1c#!${tcsh}/bin/tcsh -f' src/Kernel/xnu/SETUP/config/doconf
    fi

  '' + lib.optionalString (!isDarwinHost) ''
    export NIX_NATIVE_LD_PATH="${nativeLd}/bin/ld"
    export NIX_HOST_CC_PATH="${clang}/bin/clang"
  '' + ''
    export NIX_MIGCOM_PATH="${nativeMigcom}/bin/migcom"
    export NIX_UNIFDEF_PATH="${nativeUnifdef}/bin/unifdef"
  '' + ''

    cmake -S . -B build-nix -G Ninja \
  '' + lib.optionalString (!isDarwinHost) ''
      -DCMAKE_TOOLCHAIN_FILE=cmake/nix-toolchain.cmake \
  '' + ''
      -DCMAKE_BUILD_TYPE=Debug \
      -DOPENSSL_INCLUDE_DIR=${openssl.dev}/include \
      -DOPENSSL_CRYPTO_LIBRARY=${opensslCryptoLibrary} \
      -DOPENSSL_SSL_LIBRARY=${opensslSslLibrary} \
      -DZLIB_INCLUDE_DIR=${zlibInclude} \
      -DZLIB_LIBRARY=${zlibLibrary} \
      -DZLIB_LIBRARY_RELEASE=${zlibLibrary} \
      -DLIBXML2_INCLUDE_DIR=${libxml2Include} \
      -DLIBXML2_LIBRARY=${libxml2Library} \
      -DPUREDARWIN_MACOSX_SDK="$DARWIN_SDK_ROOT" \
  '' + lib.optionalString isDarwinHost ''
      -DCMAKE_OSX_SYSROOT="$DARWIN_SDK_ROOT" \
      -DCMAKE_CXX_FLAGS="-nostdinc++ -isystem $DARWIN_SDK_ROOT/usr/include/c++/v1" \
  '' + ''
      -DPUREDARWIN_ARCH=${puredarwinArch} \
      ${lib.optionalString (activeCompilerRt != null)
        "-DPUREDARWIN_COMPILER_RT_PREFIX=${activeCompilerRt} \\"}
      -DPUREDARWIN_ENABLE_PROJECTS=${if enableProjects then "ON" else "OFF"} \
      -DPUREDARWIN_ENABLE_KERNEL=${if enableKernel then "ON" else "OFF"} \
      -DPUREDARWIN_ENABLE_LIBRARIES=${if enableLibraries then "ON" else "OFF"} \
      -DPUREDARWIN_ENABLE_USERSPACE=${if enableUserspace then "ON" else "OFF"} \
      -DPUREDARWIN_ENABLE_TOOLS=${if enableTools then "ON" else "OFF"} \
      -DPUREDARWIN_ENABLE_TCC=${if enableTcc then "ON" else "OFF"} \
      -DPUREDARWIN_ENABLE_IOGRAPHICS_FAMILY=${if enableIOGraphicsFamily then "ON" else "OFF"} \
      -DPUREDARWIN_XNU_KERNEL_CONFIG=${lib.escapeShellArg xnuKernelConfig} \
      -DPUREDARWIN_TCC_SOURCE=${tinycc.src} \
      ${lib.optionalString (xorgDriverIncludes != null) "-DPUREDARWIN_XORG_INCLUDE_DIRS=${lib.escapeShellArg (lib.concatStringsSep ";" xorgDriverIncludes)}"} \
      ${lib.optionalString (prebuiltLibSystem != null) "-DPUREDARWIN_PREBUILT_LIBSYSTEM_ROOT=${prebuiltLibSystem}"} \
      ${lib.concatStringsSep " \\\n      " extraCmakeFlags}
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    mkdir -p .nix-stubs
    if [ -e tools/mig/mig.sh ]; then
      ln -sf "$PWD/tools/mig/mig.sh" .nix-stubs/mig
    fi
    ln -sf "${nativeMigcom}/bin/migcom" .nix-stubs/migcom
    ln -sf "${nativeUnifdef}/bin/unifdef" .nix-stubs/unifdef
  '' + lib.optionalString (!isDarwinHost) ''
    cat > .nix-stubs/libtool <<'EOF'
#!/bin/sh
if [ "$1" = "-static" ] && [ "$2" = "-o" ]; then
  out="$3"
  shift 3
  exec ${nixDarwinHost}-ar rcs "$out" "$@"
fi
if [ "$1" = "-ca" ]; then
  shift
  out=""
  members=""
  while [ $# -gt 0 ]; do
    case "$1" in
      -filelist)
        members="$members $(cat "$2")"
        shift 2
        ;;
      -o)
        out="$2"
        shift 2
        ;;
      *)
        members="$members $1"
        shift
        ;;
    esac
  done
  exec ${nixDarwinHost}-ar rcs "$out" $members
fi
echo "unsupported libtool invocation: $*" >&2
exit 1
EOF
    chmod +x .nix-stubs/libtool
    export PATH="$PWD/.nix-stubs:$PWD/build-nix/tools/mig:$PWD/build-nix/tools/cctools/misc:$PWD/build-nix/tools/dtrace_ctf/tools:$PATH"
  '' + lib.optionalString isDarwinHost ''
    export PATH="$PWD/.nix-stubs:$PATH"
  '' + ''
    ninja -C build-nix ${lib.escapeShellArgs buildTargets}
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
  '' + lib.optionalString (installUserland && !enableTcc && installUserlandTargetsOnly) ''
    mkdir -p $out/bin
    # Minimal images build a selected target list, while the generated CMake
    # install script still contains install rules for every userspace target.
    # Copy only executable artifacts that Ninja actually produced.
    find build-nix/src/Userspace -type f -perm -0100 -exec cp '{}' $out/bin/ ';'
  '' + lib.optionalString (installUserland && !enableTcc && !installUserlandTargetsOnly) ''
    cmake --install build-nix --component BaseSystem --prefix $out
  '' + lib.optionalString (installUserland && enableTcc) ''
    mkdir -p $out/bin $out/usr/lib/tcc/include
    cp build-nix/src/Userspace/tcc/build/tcc $out/bin/
    cp build-nix/src/Userspace/tcc/build-native/x86_64-osx-libtcc1.a $out/usr/lib/tcc/libtcc1.a
    cp build-nix/src/Userspace/tcc/src/include/*.h $out/usr/lib/tcc/include/
    if [ -f build-nix/src/Userspace/tcc/build-native/runmain.o ]; then
      cp build-nix/src/Userspace/tcc/build-native/runmain.o $out/usr/lib/tcc/
    fi
  '' + lib.optionalString installKernel ''
    cp -R build-nix/src/Kernel/xnu/xnu/. $out/
  '' + lib.optionalString installXnuHeaders ''
    cp -R build-nix/src/Kernel/xnu/xnu_header_install/. $out/
  '' + lib.optionalString installKexts ''
    mkdir -p $out/System/Library/Extensions
  '' + lib.optionalString (installKexts && installKextNames == [ ]) ''
    find build-nix/src/Kernel/Extensions -name '*.kext' -type d -prune \
      -exec cp -R '{}' $out/System/Library/Extensions/ ';'
  '' + lib.optionalString (installKexts && installKextNames != [ ]) ''
    for kext in ${lib.escapeShellArgs installKextNames}; do
      kext_path="$(find build-nix/src/Kernel/Extensions -name "$kext" -type d -print -quit)"
      if [ -z "$kext_path" ]; then
        echo "missing kext bundle: $kext" >&2
        exit 1
      fi
      cp -R "$kext_path" "$out/System/Library/Extensions/"
    done
  '' + lib.optionalString installLibSystem ''
    mkdir -p $out/usr/lib/system
    cp build-nix/src/Libraries/libSystem/stub/libSystem.B.dylib $out/usr/lib/
    cp -P build-nix/src/Libraries/libSystem/stub/libSystem.dylib $out/usr/lib/
    cp build-nix/src/Libraries/libSystem/libdyld/libdyld.dylib $out/usr/lib/system/
    if [ -e build-nix/src/Libraries/libSystem/libsystem_kernel/libsystem_kernel.a ]; then
      cp build-nix/src/Libraries/libSystem/libsystem_kernel/libsystem_kernel.a $out/usr/lib/system/
    fi
    if [ -e build-nix/src/Libraries/libSystem/libsystem_kernel/syscalls.a ]; then
      cp build-nix/src/Libraries/libSystem/libsystem_kernel/syscalls.a $out/usr/lib/system/
    fi

    if [ -e build-nix/src/Libraries/IOKit/libIOKitCF.a ]; then
      cp build-nix/src/Libraries/IOKit/libIOKitCF.a $out/usr/lib/system/
    fi
    cp build-nix/src/Libraries/dyld/dyld $out/usr/lib/

    if [ -e build-nix/src/Libraries/XPC/launchd/launchd_real ]; then
      mkdir -p $out/pd-sbin
      cp build-nix/src/Libraries/XPC/launchd/launchd_real $out/pd-sbin/
      #if [ -e build-nix/src/Libraries/XPC/launchd/launchd_diag_wrapper ]; then
      #  cp build-nix/src/Libraries/XPC/launchd/launchd_diag_wrapper $out/pd-sbin/
      #fi
    fi

    if [ -e build-nix/src/Libraries/XPC/notify/notifyd ]; then
      mkdir -p $out/usr/sbin
      cp build-nix/src/Libraries/XPC/notify/notifyd $out/usr/sbin/
      mkdir -p $out/System/Library/LaunchDaemons
      cp src/Libraries/XPC/notify/com.apple.notifyd.plist $out/System/Library/LaunchDaemons/
    fi

    #if [ -e build-nix/src/Libraries/XPC/logd/logd ]; then
    #  mkdir -p $out/usr/libexec
    #  cp build-nix/src/Libraries/XPC/logd/logd $out/usr/libexec/
    #  mkdir -p $out/System/Library/LaunchDaemons
    #  cp src/Libraries/XPC/logd/com.apple.logd.plist $out/System/Library/LaunchDaemons/
    #fi

    mkdir -p $out/pd-xpc-dev/lib $out/pd-xpc-dev/include
    for a in XPC/launchd/libXPC_launchd_static.a \
             XPC/launchd/libXPC_launchd_mig_static.a \
             XPC/libxpc/libXPC_libxpc_static.a \
             XPC/libinfo/libXPC_libinfo_static.a \
             XPC/libnv/libXPC_libnv_static.a \
             CrashReporterClient/libCrashReporterClient.a; do
      if [ -e "build-nix/src/Libraries/$a" ]; then
        cp "build-nix/src/Libraries/$a" $out/pd-xpc-dev/lib/
      fi
    done
    cp -a src/Libraries/XPC/libxpc/include/. $out/pd-xpc-dev/include/

    if [ -d build-nix/src/Libraries/libSystem/libc/headers/usr/include ]; then
      mkdir -p $out/pd-guest-headers
      if [ -d build-nix/src/Kernel/xnu/xnu_header_install/usr/include ]; then
        cp -RL build-nix/src/Kernel/xnu/xnu_header_install/usr/include/. $out/pd-guest-headers/
      fi
      chmod -R u+w $out/pd-guest-headers
      cp -RL build-nix/src/Libraries/libSystem/libc/headers/usr/include/. $out/pd-guest-headers/
      chmod -R u+w $out/pd-guest-headers
      cp -RL src/Libraries/AvailabilityVersions/include/. $out/pd-guest-headers/
      cp -RL src/Libraries/libSystem/pthread/include/. $out/pd-guest-headers/
      # libc's stdlib.h includes <malloc/_malloc.h>; libmalloc exposes these to
      # the build as an INTERFACE include dir and has no install rule, so
      # without this the guest /usr/include cannot compile stdlib.h.
      cp -RL src/Libraries/libSystem/libmalloc/include/. $out/pd-guest-headers/
      # Apple's libc headers.sh installs only libc's own headers, because on
      # real Darwin these come from separate projects (Libm, Libinfo,
      # libplatform, dyld, libsystem_kernel). Nothing else stages them here, so
      # a guest compiler cannot resolve <math.h>, <netdb.h>, <pwd.h>,
      # <grp.h>, <setjmp.h>, <dlfcn.h> or the <gethostuuid.h> unistd.h pulls in.
      cp -L src/Libraries/libSystem/libc/include/math.h $out/pd-guest-headers/
      cp -L src/Libraries/libSystem/libc/include/netdb.h $out/pd-guest-headers/
      cp -L src/Libraries/libSystem/libc/include/pwd.h $out/pd-guest-headers/
      cp -L src/Libraries/libSystem/libc/include/grp.h $out/pd-guest-headers/
      cp -L src/Libraries/libSystem/libplatform/include/setjmp.h $out/pd-guest-headers/
      cp -L src/Libraries/dyld/upstream/include/dlfcn.h $out/pd-guest-headers/
      cp -L src/Libraries/libSystem/libsystem_kernel/include/gethostuuid.h $out/pd-guest-headers/
      # Same for <sched.h>, which libc++'s __threading_support includes.
      cp -L src/Libraries/libSystem/pthread/include/pthread/sched.h $out/pd-guest-headers/
      # libpthread's header is <pthread/pthread.h>, but every consumer writes
      # #include <pthread.h>; Darwin ships it at both paths.
      cp -L src/Libraries/libSystem/pthread/include/pthread/pthread.h $out/pd-guest-headers/pthread.h
      chmod -R u+w $out/pd-guest-headers
      rm -f $out/pd-guest-headers/sys_pthread_types.modulemap
      chmod -R u+w $out/pd-guest-headers
    fi
  '' + lib.optionalString installBaseSystem ''
    cmake --install build-nix --component BaseSystem --prefix $out
  '' + ''
    runHook postInstall
  '';

  dontCheckForBrokenSymlinks = installKernel || installXnuHeaders || installBaseSystem;

  forceShare = lib.optionals (!installBaseSystem) [ "man" "doc" "info" ];
  dontMoveSbin = installBaseSystem || installUserland;
  dontStrip = installBaseSystem;
  dontPatchELF = installBaseSystem;

  meta = with lib; {
    platforms = platforms.linux ++ platforms.darwin;
  };
} // lib.optionalAttrs (!isDarwinHost) {
  NIX_DARWIN_TOOLCHAIN_DIR = "${activeCrossToolchain}/bin";
  NIX_DARWIN_HOST = nixDarwinHost;
})
