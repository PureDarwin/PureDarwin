{ stdenv
, lib
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
, nativeLd
, libSystem
, corefoundation
, iokit
, src
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-launchctl";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$DARWIN_SDK_ROOT" \
      -DHAVE_LIBAUDITD=0 \
      -DSO_EXECPATH=0x1085 \
      -I${src}/src/Libraries/XPC/launchd \
      -I${src}/src/Libraries/XPC/launchd/SystemStarter \
      -I${src}/src/Libraries/XPC/libxpc \
      -I${src}/src/Libraries/XPC/libinfo \
      -I${src}/src/Libraries/libSystem/libc \
      -I${src}/src/Libraries/libSystem/libplatform/private \
      -I${src}/src/Libraries/libSystem/pthread/compat-include \
      -I${libSystem}/pd-xpc-dev/include \
      -I${corefoundation}/include \
      -F${corefoundation}/System/Library/Frameworks \
      -I${iokit}/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libxpc_static.a \
      -Wl,-U,_OBJC_CLASS_\$_NSObject \
      -Wl,-U,_OBJC_METACLASS_\$_NSObject \
      -Wl,-U,__objc_empty_cache \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libinfo_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libnv_static.a \
      -Wl,-fixup_chains \
      -lCoreFoundation -lIOKitCF -lSystem \
      ${src}/src/Libraries/XPC/launchctl/launchctl.c \
      ${libSystem}/pd-xpc-dev/lib/libXPC_launchd_mig_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libCrashReporterClient.a \
      -o launchctl

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin $out/usr/bin
    cp launchctl $out/usr/bin/
    cp launchctl $out/bin/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Real PureDarwin/XPC launchctl (launchd 842.91.1 lineage)";
    platforms = platforms.unix;
  };
}
