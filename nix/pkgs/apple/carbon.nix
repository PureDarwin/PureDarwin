{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, libobjc
, corefoundation
, coregraphics
, coreservices
, src
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# Carbon.framework: the HIToolbox calls Cocoa apps still make, text input sources, TSM documents and theme drawing.
# As on macOS the code lives in the HIToolbox subframework, which AppKit links and apps find as com.apple.HIToolbox,
# and Carbon re-exports it and CoreServices, so -framework Carbon brings CoreFoundation too
let
  hitoolboxName = "/System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/HIToolbox";
in
stdenv.mkDerivation {
  pname = "puredarwin-carbon";
  version = "0.1";

  inherit src;
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -O2 -Wall -Wno-multichar -x objective-c \
      -I${libSystem}/usr/include -I${libobjc}/usr/include -I${corefoundation}/include -I${coregraphics}/usr/include -F${coregraphics}/System/Library/Frameworks \
      -c $src/Carbon.c -o Carbon.o
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -O2 -Wall \
      -I${libSystem}/usr/include -I${corefoundation}/include \
      -c $src/KeyLayouts.c -o KeyLayouts.o
    LD="$CC -fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot $SDK -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib -L${coregraphics}/usr/lib -dynamiclib"
    $LD -install_name ${hitoolboxName} -compatibility_version 1 -current_version 2000 \
      -Wl,-reexport_library,${coreservices}/System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices \
      Carbon.o KeyLayouts.o -lCoreGraphics -lCoreFoundation -lobjc -lSystem -o HIToolbox
    echo > umbrella.c
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -c umbrella.c -o umbrella.o
    $LD -install_name /System/Library/Frameworks/Carbon.framework/Versions/A/Carbon \
      -compatibility_version 2 -current_version 170 \
      -Wl,-reexport_library,HIToolbox \
      -Wl,-dylib_file,${hitoolboxName}:$PWD/HIToolbox \
      -Wl,-reexport_library,${coreservices}/System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices \
      umbrella.o -lSystem -o Carbon
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    F=$out/System/Library/Frameworks/Carbon.framework
    install -Dm755 Carbon $F/Versions/A/Carbon
    install -Dm644 $src/Carbon.h $F/Versions/A/Headers/Carbon.h
    ln -s A $F/Versions/Current
    ln -s Versions/Current/Carbon $F/Carbon
    ln -s Versions/Current/Headers $F/Headers
    H=$F/Versions/A/Frameworks/HIToolbox.framework
    install -Dm755 HIToolbox $H/Versions/A/HIToolbox
    install -d $H/Versions/A/Resources
    cat > $H/Versions/A/Resources/Info.plist <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>HIToolbox</string>
	<key>CFBundleIdentifier</key>
	<string>com.apple.HIToolbox</string>
	<key>CFBundleName</key>
	<string>HIToolbox</string>
	<key>CFBundlePackageType</key>
	<string>FMWK</string>
	<key>CFBundleShortVersionString</key>
	<string>2.1.1</string>
</dict>
</plist>
PLIST
    ln -s A $H/Versions/Current
    ln -s Versions/Current/HIToolbox $H/HIToolbox
    ln -s Versions/Current/Resources $H/Resources
    ln -s Versions/Current/Frameworks $F/Frameworks
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "Carbon HIToolbox subset for PureDarwin";
    platforms = lib.platforms.unix;
  };
}
