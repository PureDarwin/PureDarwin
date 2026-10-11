{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, libobjc
, corefoundation
, foundation
, coregraphics
, onyx2d
, coretext
, quartzcore
, applicationservices
, coreservices
, windowserver
, appkit
, openglFramework
, mesa
# the framework source directories by name, e.g. CoreMedia = ./src/Frameworks/CoreMedia
, srcs
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# frameworks Xcode-built apps link for services this system does not run (Game Center, notifications,
# screen capture), with CoreMedia time values, type identifiers, ImageIO over CoreGraphics and ModelIO
let
  frameworks = [ "CoreMedia" "UniformTypeIdentifiers" "ImageIO" "UserNotifications" "ScreenCaptureKit" "GameKit" "ModelIO" ];
in
stdenv.mkDerivation {
  pname = "puredarwin-app-frameworks";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk

    # each framework's headers under its own name, the way #import <Name/Header.h> finds them
    mkdir -p include
    ${lib.concatMapStringsSep "\n" (f: ''
      mkdir -p include/${f}
      cp ${srcs.${f}}/*.h include/${f}/
      rm -f include/${f}/MDLPrivate.h include/${f}/USDCrate.h
    '') frameworks}

    CFLAGS="-isysroot $SDK -mmacosx-version-min=26.5 -O2 -Wall -Wno-deprecated-declarations \
      -fno-objc-arc -fobjc-exceptions -fblocks -D_DARWIN_C_SOURCE \
      -Iinclude -I${libSystem}/usr/include -I${libobjc}/usr/include -I${corefoundation}/include \
      -I${foundation}/usr/include -I${onyx2d}/usr/include -I${coretext}/usr/include \
      -I${quartzcore}/usr/include -I${applicationservices}/usr/include \
      -I${coregraphics}/usr/include -I${coreservices}/usr/include -I${windowserver}/usr/include \
      -I${mesa}/usr/include \
      -F${appkit}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks"
    LDFLAGS="-isysroot $SDK -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib -dynamiclib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib -L${foundation}/usr/lib \
      -L${coregraphics}/usr/lib -L${appkit}/usr/lib -L. \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -compatibility_version 1 -current_version 1"
    fw() { echo "-install_name /System/Library/Frameworks/$1.framework/Versions/A/$1"; }

    $CC $CFLAGS -c ${srcs.CoreMedia}/CMTime.c -o CMTime.o
    $CC $LDFLAGS $(fw CoreMedia) CMTime.o -lCoreFoundation -lSystem -o CoreMedia

    $CC $CFLAGS -x objective-c -c ${srcs.UniformTypeIdentifiers}/UTType.m -o UTType.o
    $CC $LDFLAGS $(fw UniformTypeIdentifiers) UTType.o -lFoundation -lCoreFoundation -lobjc -lSystem -o UniformTypeIdentifiers

    echo "// re-exports only" > empty.c
    $CC $CFLAGS -c empty.c -o empty.o
    $CC $LDFLAGS $(fw ImageIO) \
      -Wl,-reexport_library,${coregraphics}/System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics \
      empty.o -lSystem -o ImageIO

    $CC $CFLAGS -x objective-c -c ${srcs.UserNotifications}/UserNotifications.m -o UserNotifications.o
    $CC $LDFLAGS $(fw UserNotifications) UserNotifications.o -lFoundation -lCoreFoundation -lobjc -lSystem -o UserNotifications

    # CoreMedia's CMTime comes from the framework just built
    ln -s CoreMedia libCoreMedia.dylib
    $CC $CFLAGS -x objective-c -c ${srcs.ScreenCaptureKit}/ScreenCaptureKit.m -o ScreenCaptureKit.o
    $CC $LDFLAGS $(fw ScreenCaptureKit) ScreenCaptureKit.o -lCoreMedia -lCoreGraphics -lFoundation -lCoreFoundation -lobjc -lSystem \
      -o ScreenCaptureKit

    # ModelIO: USD crate files read by USDCrate.c, the asset and mesh classes over them
    $CC $CFLAGS -c ${srcs.ModelIO}/USDCrate.c -o USDCrate.o
    $CC $CFLAGS -x objective-c -I${srcs.ModelIO} -c ${srcs.ModelIO}/MDLMesh.m -o MDLMesh.o
    $CC $CFLAGS -x objective-c -I${srcs.ModelIO} -c ${srcs.ModelIO}/MDLAsset.m -o MDLAsset.o
    $CC $LDFLAGS $(fw ModelIO) USDCrate.o MDLMesh.o MDLAsset.o -lCoreGraphics -lFoundation -lCoreFoundation -lobjc -lSystem \
      -o ModelIO

    $CC $CFLAGS -x objective-c -c ${srcs.GameKit}/GameKit.m -o GameKit.o
    $CC $LDFLAGS $(fw GameKit) GameKit.o -lAppKit -lFoundation -lCoreFoundation -lobjc -lSystem -o GameKit
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    for f in ${lib.concatStringsSep " " frameworks}; do
      F=$out/System/Library/Frameworks/$f.framework
      install -Dm755 $f $F/Versions/A/$f
      mkdir -p $F/Versions/A/Headers $F/Versions/A/Resources
      cp include/$f/*.h $F/Versions/A/Headers/
      cat > $F/Versions/A/Resources/Info.plist <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>$f</string>
	<key>CFBundleIdentifier</key>
	<string>com.apple.$f</string>
	<key>CFBundlePackageType</key>
	<string>FMWK</string>
	<key>CFBundleShortVersionString</key>
	<string>1.0</string>
</dict>
</plist>
EOF
      ln -s A $F/Versions/Current
      ln -s Versions/Current/$f $F/$f
      ln -s Versions/Current/Headers $F/Headers
      ln -s Versions/Current/Resources $F/Resources
    done
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "CoreMedia, UniformTypeIdentifiers, ImageIO, UserNotifications, ScreenCaptureKit and GameKit for PureDarwin";
    platforms = lib.platforms.unix;
  };
}
