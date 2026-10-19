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
# As on macOS it re-exports CoreServices, so -framework Carbon brings CoreFoundation too
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
    $CC -fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot $SDK -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib -L${coregraphics}/usr/lib \
      -dynamiclib -install_name /System/Library/Frameworks/Carbon.framework/Versions/A/Carbon \
      -compatibility_version 2 -current_version 170 \
      -Wl,-reexport_library,${coreservices}/System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices \
      Carbon.o KeyLayouts.o -lCoreGraphics -lCoreFoundation -lobjc -lSystem -o Carbon
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
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "Carbon HIToolbox subset for PureDarwin";
    platforms = lib.platforms.unix;
  };
}
