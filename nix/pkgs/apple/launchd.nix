{ stdenv
, lib
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
, nativeLd
, libSystem
, corefoundation
, iokit
, compilerRt ? null
, isArmv6 ? lib.hasPrefix "armv6-" targetTriple
, src
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-launchd";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$DARWIN_SDK_ROOT" \
      -I${libSystem}/pd-xpc-dev/include \
      -I${corefoundation}/include \
      -I${iokit}/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib \
      -Wl,-dylinker_install_name,/usr/lib/dyld \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_launchd_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_launchd_mig_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libxpc_static.a \
      -Wl,-U,_OBJC_CLASS_\$_NSObject \
      -Wl,-U,_OBJC_METACLASS_\$_NSObject \
      -Wl,-U,__objc_empty_cache \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libinfo_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libnv_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libCrashReporterClient.a \
      ${lib.optionalString (!isArmv6) "-Wl,-fixup_chains"} \
      -lCoreFoundation -lIOKitCF -lSystem ${lib.optionalString (compilerRt != null) "${compilerRt}/lib/libcompiler_rt.a"} \
      ${src}/src/Libraries/XPC/launchd/pd_launchd_main.c \
      -o launchd

    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$DARWIN_SDK_ROOT" \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -L${libSystem}/usr/lib \
      -Wl,-dylinker_install_name,/usr/lib/dyld \
      -Wl,-platform_version,macos,11.0,11.5 \
      ${lib.optionalString (!isArmv6) "-Wl,-fixup_chains"} \
      -lSystem \
      ${src}/src/Libraries/XPC/launchd/pd_console_login.c \
      -o pd-console-login

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/pd-sbin
    cp launchd $out/pd-sbin/
    mkdir -p $out/usr/libexec
    cp pd-console-login $out/usr/libexec/
    mkdir -p $out/System/Library/LaunchDaemons
    cp ${src}/src/Libraries/XPC/launchd/org.puredarwin.console-login.plist \
      $out/System/Library/LaunchDaemons/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "PureDarwin/XPC launchd (bootstrap-namespace PID 1)";
    platforms = platforms.unix;
  };
}
