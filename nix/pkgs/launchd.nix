{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
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
  pname = "puredarwin-launchd";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$DARWIN_SDK_ROOT" \
      -I${libSystem}/pd-xpc-dev/include \
      -I${corefoundation}/include \
      -I${iokit}/include \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib \
      -Wl,-dylinker_install_name,/usr/lib/dyld \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_launchd_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_launchd_mig_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libxpc_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libinfo_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libXPC_libnv_static.a \
      -Wl,-force_load,${libSystem}/pd-xpc-dev/lib/libCrashReporterClient.a \
      -Wl,-fixup_chains \
      -lCoreFoundation -lIOKitCF -lSystem \
      ${src}/src/Libraries/XPC/launchd/pd_launchd_main.c \
      -o launchd

    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$DARWIN_SDK_ROOT" \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylinker_install_name,/usr/lib/dyld \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-fixup_chains \
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
    platforms = platforms.linux;
  };
}
