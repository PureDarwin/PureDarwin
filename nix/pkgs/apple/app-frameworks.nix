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
, corevideo
, libz
, zlibHeaders
# the framework source directories by name, e.g. CoreMedia = ./src/Frameworks/CoreMedia
, srcs
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# frameworks Xcode-built apps link for services this system does not run (Game Center, notifications,
# screen capture), with CoreMedia time values, type identifiers, ImageIO over CoreGraphics and ModelIO
let
  frameworks = [ "CoreMedia" "UniformTypeIdentifiers" "ImageIO" "UserNotifications" "ScreenCaptureKit" "GameKit" "ModelIO"
    "VideoToolbox" "AVFoundation" "MediaPlayer" "CoreMIDI" "AuthenticationServices" "LocalAuthentication" "CoreLocation"
    "Vision" "ServiceManagement" "IOBluetooth" "Accessibility" "AddressBook" "CFNetwork" "GSS" "ColorSync" "WebKit"
    "GameController" "ForceFeedback" "CoreHaptics" ];
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
      cp ${srcs.${f}}/*.h include/${f}/ 2>/dev/null || true
      rm -f include/${f}/MDLPrivate.h include/${f}/USDCrate.h
    '') frameworks}

    CFLAGS="-isysroot $SDK -mmacosx-version-min=26.5 -O2 -Wall -Wno-deprecated-declarations \
      -fno-objc-arc -fobjc-exceptions -fblocks -D_DARWIN_C_SOURCE \
      -Iinclude -I${libSystem}/usr/include -I${libobjc}/usr/include -I${corefoundation}/include \
      -I${foundation}/usr/include -I${onyx2d}/usr/include -I${coretext}/usr/include \
      -I${quartzcore}/usr/include -I${applicationservices}/usr/include \
      -I${coregraphics}/usr/include -I${coreservices}/usr/include -I${windowserver}/usr/include \
      -I${mesa}/usr/include -I${corevideo}/usr/include \
      -F${appkit}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks"
    LDFLAGS="-isysroot $SDK -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib -dynamiclib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib -L${foundation}/usr/lib \
      -L${coregraphics}/usr/lib -L${appkit}/usr/lib -L${corevideo}/usr/lib -L${quartzcore}/usr/lib \
      -L${coreservices}/usr/lib -L${coretext}/usr/lib -L. \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -compatibility_version 1 -current_version 1"
    fw() { echo "-install_name /System/Library/Frameworks/$1.framework/Versions/A/$1"; }

    $CC $CFLAGS -c ${srcs.CoreMedia}/CMTime.c -o CMTime.o
    $CC $CFLAGS -c ${srcs.CoreMedia}/CMSampleBuffer.c -o CMSampleBuffer.o
    $CC $LDFLAGS $(fw CoreMedia) CMTime.o CMSampleBuffer.o -lCoreVideo -lCoreFoundation -lSystem -o CoreMedia

    $CC $CFLAGS -x objective-c -c ${srcs.UniformTypeIdentifiers}/UTType.m -o UTType.o
    $CC $LDFLAGS $(fw UniformTypeIdentifiers) UTType.o -lFoundation -lCoreFoundation -lobjc -lSystem -o UniformTypeIdentifiers

    $CC $CFLAGS -c ${srcs.ImageIO}/ImageIOKeys.c -o ImageIOKeys.o
    $CC $LDFLAGS $(fw ImageIO) \
      -Wl,-reexport_library,${coregraphics}/System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics \
      ImageIOKeys.o -lCoreFoundation -lSystem -o ImageIO

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

    # VideoToolbox finds no codecs, AVFoundation no devices, voices or demuxers
    $CC $CFLAGS -c ${srcs.VideoToolbox}/VideoToolbox.c -o VideoToolbox.o
    $CC $LDFLAGS $(fw VideoToolbox) VideoToolbox.o -lCoreMedia -lCoreVideo -lCoreFoundation -lSystem -o VideoToolbox
    $CC $CFLAGS -x objective-c -c ${srcs.AVFoundation}/AVFoundation.m -o AVFoundation.o
    $CC $LDFLAGS $(fw AVFoundation) AVFoundation.o -lQuartzCore -lCoreMedia -lCoreVideo -lCoreGraphics -lFoundation \
      -lCoreFoundation -lobjc -lSystem -o AVFoundation
    $CC $CFLAGS -x objective-c -c ${srcs.MediaPlayer}/MediaPlayer.m -o MediaPlayer.o
    $CC $LDFLAGS $(fw MediaPlayer) MediaPlayer.o -lFoundation -lCoreFoundation -lobjc -lSystem -o MediaPlayer
    # CFNetwork: the proxy API, over SystemConfiguration's proxy settings
    $CC $CFLAGS -c ${srcs.CFNetwork}/CFProxySupport.c -o CFProxySupport.o
    $CC $LDFLAGS $(fw CFNetwork) CFProxySupport.o -lCoreFoundation -lSystem -o CFNetwork
    # GSS: no security mechanisms, ColorSync: display UUIDs, WebKit: views that load nothing
    $CC $CFLAGS -c ${srcs.GSS}/GSS.c -o GSS.o
    $CC $LDFLAGS $(fw GSS) GSS.o -lSystem -o GSS
    $CC $CFLAGS -c ${srcs.ColorSync}/ColorSync.c -o ColorSync.o
    $CC $LDFLAGS $(fw ColorSync) ColorSync.o -lCoreGraphics -lCoreFoundation -lSystem -o ColorSync
    $CC $CFLAGS -x objective-c -c ${srcs.WebKit}/WebKit.m -o WebKit.o
    $CC $LDFLAGS $(fw WebKit) WebKit.o -lAppKit -lFoundation -lCoreFoundation -lobjc -lSystem -o WebKit
    # game controllers that never connect, no force feedback devices, haptics that never play
    $CC $CFLAGS -c ${srcs.ForceFeedback}/ForceFeedback.c -o ForceFeedback.o
    $CC $LDFLAGS $(fw ForceFeedback) ForceFeedback.o -lSystem -o ForceFeedback
    $CC $CFLAGS -c ${srcs.CoreMIDI}/CoreMIDI.c -o CoreMIDI.o
    $CC $LDFLAGS $(fw CoreMIDI) CoreMIDI.o -lCoreFoundation -lSystem -o CoreMIDI
    ${lib.concatMapStringsSep "\n" (f: ''
      $CC $CFLAGS -x objective-c -c ${srcs.${f}}/${f}.m -o ${f}.o
      $CC $LDFLAGS $(fw ${f}) ${f}.o -lFoundation -lCoreFoundation -lobjc -lSystem -o ${f}
    '') [ "AuthenticationServices" "LocalAuthentication" "CoreLocation" "Vision" "IOBluetooth" "GameController" "CoreHaptics" ]}
    ${lib.concatMapStringsSep "\n" (f: ''
      $CC $CFLAGS -c ${srcs.${f}}/${f}.c -o ${f}.o
      $CC $LDFLAGS $(fw ${f}) ${f}.o -lCoreFoundation -lSystem -o ${f}
    '') [ "AddressBook" ]}
    # these two also carry ObjC classes
    $CC $CFLAGS -c ${srcs.ServiceManagement}/ServiceManagement.c -o ServiceManagement.o
    $CC $CFLAGS -x objective-c -c ${srcs.ServiceManagement}/SMAppService.m -o SMAppService.o
    $CC $LDFLAGS $(fw ServiceManagement) ServiceManagement.o SMAppService.o -lFoundation -lCoreFoundation -lobjc -lSystem \
      -o ServiceManagement
    $CC $CFLAGS -c ${srcs.Accessibility}/Accessibility.c -o Accessibility.o
    $CC $CFLAGS -x objective-c -c ${srcs.Accessibility}/AXCustomContent.m -o AXCustomContent.o
    $CC $LDFLAGS $(fw Accessibility) Accessibility.o AXCustomContent.o -lFoundation -lCoreFoundation -lobjc -lSystem -o Accessibility

    # ApplicationServices: HIServices and PrintCore here, the rest re-exported from their frameworks
    $CC $CFLAGS -c ${srcs.HIServices}/HIServices.c -o HIServices.o
    $CC $CFLAGS -x objective-c -c ${srcs.HIServices}/PrintCore.c -o PrintCore.o
    $CC $LDFLAGS $(fw ApplicationServices) HIServices.o PrintCore.o \
      -Wl,-reexport_library,${coregraphics}/System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics \
      -Wl,-reexport_library,${coretext}/System/Library/Frameworks/CoreText.framework/Versions/A/CoreText \
      -Wl,-reexport_library,ImageIO \
      ${coreservices}/System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices \
      -lCoreFoundation -lobjc -lSystem -o ApplicationServices

    # libraries apps link by path: a CUPS client with no server, and libbsm's calls from libSystem
    $CC $CFLAGS -I${zlibHeaders}/include -c ${srcs.libcompression}/compression.c -o compression.o
    $CC $LDFLAGS -install_name /usr/lib/libcompression.dylib compression.o -L${libz}/usr/lib -lz -lSystem -o libcompression.dylib
    $CC $CFLAGS -c ${srcs.libcups}/libcups.c -o libcups.o
    $CC $LDFLAGS -install_name /usr/lib/libcups.2.dylib -compatibility_version 3 -current_version 3 libcups.o -lSystem -o libcups.2.dylib
    echo "// re-exports only" > empty.c
    $CC $CFLAGS -c empty.c -o empty.o
    $CC $LDFLAGS -install_name /usr/lib/libbsm.0.dylib empty.o -Wl,-reexport-lSystem -o libbsm.0.dylib
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    for f in ${lib.concatStringsSep " " frameworks}; do
      F=$out/System/Library/Frameworks/$f.framework
      install -Dm755 $f $F/Versions/A/$f
      mkdir -p $F/Versions/A/Headers $F/Versions/A/Resources
      cp include/$f/*.h $F/Versions/A/Headers/ 2>/dev/null || true
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
    # only the binary: the headers and Info.plist come with the applicationservices package
    F=$out/System/Library/Frameworks/ApplicationServices.framework
    install -Dm755 ApplicationServices $F/Versions/A/ApplicationServices
    ln -s A $F/Versions/Current
    ln -s Versions/Current/ApplicationServices $F/ApplicationServices
    install -Dm755 libcompression.dylib $out/usr/lib/libcompression.dylib
    install -Dm644 ${srcs.libcompression}/compression.h $out/usr/include/compression.h
    install -Dm755 libcups.2.dylib $out/usr/lib/libcups.2.dylib
    ln -s libcups.2.dylib $out/usr/lib/libcups.dylib
    install -Dm755 libbsm.0.dylib $out/usr/lib/libbsm.0.dylib
    ln -s libbsm.0.dylib $out/usr/lib/libbsm.dylib
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "CoreMedia, UniformTypeIdentifiers, ImageIO, UserNotifications, ScreenCaptureKit and GameKit for PureDarwin";
    platforms = lib.platforms.unix;
  };
}
