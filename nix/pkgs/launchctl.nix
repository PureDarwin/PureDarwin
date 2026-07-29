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
  pname = "puredarwin-launchctl";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

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
    platforms = platforms.linux;
  };
}
