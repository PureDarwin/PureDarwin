{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, iokit
, src
}:

let
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };
in
stdenv.mkDerivation {
  pname = "puredarwin-systemstarter";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang \
      -isysroot "$DARWIN_SDK_ROOT" \
      -I${src}/src/Libraries/XPC/launchd/SystemStarter \
      -I${libSystem}/pd-xpc-dev/include \
      -I${corefoundation}/include \
      -I${iokit}/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libxpc_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libinfo_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libnv_static.a \
      -Wl,-fixup_chains \
      -lCoreFoundation -lIOKitCF -lSystem \
      ${src}/src/Libraries/XPC/launchd/SystemStarter/SystemStarter.c \
      ${src}/src/Libraries/XPC/launchd/SystemStarter/StartupItems.c \
      ${src}/src/Libraries/XPC/launchd/SystemStarter/IPC.c \
      ${src}/src/Libraries/XPC/launchd/SystemStarter/pd_NSSystemDirectories.c \
      ${libSystem}/pd-xpc-dev/lib/libXPC_launchd_mig_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libCrashReporterClient.a \
      -o SystemStarter

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/sbin
    cp SystemStarter $out/sbin/
    mkdir -p $out/System/Library/LaunchDaemons
    cp ${src}/src/Libraries/XPC/launchd/SystemStarter/com.apple.SystemStarter.plist \
      $out/System/Library/LaunchDaemons/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Real PureDarwin/XPC SystemStarter + classic StartupItems support";
    platforms = platforms.linux;
  };
}
