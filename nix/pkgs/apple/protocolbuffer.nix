{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, libobjc
, corefoundation
, foundation
, src
, targetTriple ? "x86_64-apple-darwin20.4"
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

  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";

  mSrcs = [
    "PBCodable"
    "PBDataReader"
    "PBDataWriter"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-protocolbuffer";
  version = "1";

  inherit src;

  nativeBuildInputs = [ stdenv.cc ];

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    # corefoundation.nix installs its headers flattened into $out/include, so
    # stage the "<CoreFoundation/Foo.h>" layout the sources expect.
    mkdir -p cf-headers
    ln -s ${corefoundation}/include cf-headers/CoreFoundation

    CFLAGS="-x objective-c -fno-objc-arc -fPIC -Os -DNDEBUG -D__PUREDARWIN__=1 \
      -DDEPLOYMENT_RUNTIME_OBJC=1 -DINCLUDE_OBJC=1 \
      -isysroot $DARWIN_SDK_ROOT \
      -Iinclude \
      -Icf-headers \
      -I${libSystem}/usr/include \
      -I${libobjc}/usr/include \
      -I${corefoundation}/include \
      -I${foundation}/usr/include"

    objs=""
    for s in ${lib.concatStringsSep " " mSrcs}; do
      ${cc} $CFLAGS -c "$s.m" -o "$s.o"
      objs="$objs $s.o"
    done

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib \
      -L${corefoundation}/usr/lib -L${foundation}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-install_name,/System/Library/PrivateFrameworks/ProtocolBuffer.framework/Versions/A/ProtocolBuffer \
      -Wl,-fixup_chains \
      -lobjc -lFoundation -lCoreFoundation -lSystem \
      -o libProtocolBuffer.dylib $objs

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    fw="$out/System/Library/PrivateFrameworks/ProtocolBuffer.framework"
    mkdir -p "$fw/Versions/A/Headers"
    cp libProtocolBuffer.dylib "$fw/Versions/A/ProtocolBuffer"
    cp -a include/ProtocolBuffer/. "$fw/Versions/A/Headers/"
    ln -s A "$fw/Versions/Current"
    ln -s Versions/Current/ProtocolBuffer "$fw/ProtocolBuffer"
    ln -s Versions/Current/Headers "$fw/Headers"
    mkdir -p "$out/include"
    cp -a include/ProtocolBuffer "$out/include/"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "PureDarwin ProtocolBuffer (Apple's ObjC protobuf runtime interface), cross-built as a private framework";
    platforms = platforms.unix;
  };
}
