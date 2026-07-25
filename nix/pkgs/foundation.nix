{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, libobjc
, corefoundation
, src
}:

# Cross-builds the small real slice of Foundation vendored at
# src/Libraries/Foundation into /usr/lib/libFoundation.dylib: NSString /
# NSCFString (the CFString toll-free bridge target registered by
# NSCFString.m) and NSAttributedString's minimal declaration. NSObject
# itself lives in libobjc (objc4's runtime/NSObject.mm), not here - matches
# real Darwin, where NSObject is the objc runtime's root class.

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

  cc = "${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang";
  mmSrcs = [
    "String.subproj/NSString"
    "String.subproj/NSCFString"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-foundation";
  version = "1";

  inherit src;

  nativeBuildInputs = [ stdenv.cc ];

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    # Stage a "Foundation/" include dir (same trick corefoundation.nix uses)
    # so `#import <Foundation/NSString.h>` etc. resolve like a real
    # framework search path would. Scoped to Runtime.subproj/String.subproj
    # only (not `.` broadly) - the SDK tarball just got extracted alongside
    # these sources and its own real Foundation.framework/*.h must not be
    # picked up here.
    mkdir -p foundation-headers/Foundation
    find Runtime.subproj String.subproj -name '*.h' -exec cp {} foundation-headers/Foundation/ \;

    # corefoundation.nix installs its headers flattened into $out/include
    # (no "CoreFoundation/" subdirectory) - stage the same
    # "<CoreFoundation/Foo.h>" layout our sources expect.
    mkdir -p cf-headers
    ln -s ${corefoundation}/include cf-headers/CoreFoundation

    CFLAGS="-x objective-c -fno-objc-arc -fPIC -Os -DNDEBUG -D__PUREDARWIN__=1 \
      -DDEPLOYMENT_RUNTIME_OBJC=1 -DINCLUDE_OBJC=1 \
      -isysroot $DARWIN_SDK_ROOT \
      -Ifoundation-headers \
      -Icf-headers \
      -I${libSystem}/usr/include \
      -I${libobjc}/usr/include \
      -I${corefoundation}/include"

    objs=""
    for s in ${lib.concatStringsSep " " mmSrcs}; do
      objfile="$(basename $s).o"
      ${cc} $CFLAGS -c "$s.m" -o "$objfile"
      objs="$objs $objfile"
    done

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-install_name,/usr/lib/libFoundation.dylib \
      -Wl,-fixup_chains \
      -lobjc -lCoreFoundation -lSystem \
      -o libFoundation.dylib $objs

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $out/usr/include/Foundation
    cp libFoundation.dylib $out/usr/lib/
    find Runtime.subproj String.subproj -name '*.h' -exec cp {} $out/usr/include/Foundation/ \;
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "PureDarwin Foundation (NSString/NSCFString real toll-free bridge slice), cross-built as /usr/lib/libFoundation.dylib";
    platforms = platforms.linux;
  };
}
