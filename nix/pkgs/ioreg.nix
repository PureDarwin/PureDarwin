{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, corefoundation
, iokit
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
  pname = "puredarwin-ioreg";
  version = "0.1";

  src = ../../src/Userspace/ioreg;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    CC="${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang"

    CFLAGS="-isysroot $DARWIN_SDK_ROOT -F$DARWIN_SDK_ROOT/System/Library/Frameworks -Icompat -I${libSystem}/usr/include -I${corefoundation}/include -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lIOKitCF -lCoreFoundation -lSystem"

    $CC $CFLAGS -c ioreg.c -o ioreg.o
    $CC $CFLAGS -c term_stub.c -o term_stub.o
    $CC $LDFLAGS -o ioreg ioreg.o term_stub.o

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/sbin
    cp ioreg $out/usr/sbin/ioreg
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Real apple-oss-distributions ioreg (IOKitTools), linked against real CoreFoundation + this project's MIG-backed IOKitLibCF";
    platforms = platforms.linux;
  };
}
