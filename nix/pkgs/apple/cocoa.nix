{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, foundation
, appkit
, coredata
, src
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# the Cocoa.framework as Apple ships it: an umbrella with no code that re-exports Foundation, AppKit and CoreData,
# so Xcode-built apps that link -framework Cocoa find all three through it
stdenv.mkDerivation {
  pname = "puredarwin-cocoa";
  version = "0.1";

  inherit src;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    echo "// re-exports only" > empty.c
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -c empty.c -o empty.o
    $CC -fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot $SDK -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib \
      -dynamiclib -install_name /System/Library/Frameworks/Cocoa.framework/Versions/A/Cocoa \
      -compatibility_version 1 -current_version 24 \
      -Wl,-reexport_library,${foundation}/usr/lib/libFoundation.dylib \
      -Wl,-reexport_library,${appkit}/System/Library/Frameworks/AppKit.framework/Versions/A/AppKit \
      -Wl,-reexport_library,${coredata}/System/Library/Frameworks/CoreData.framework/Versions/A/CoreData \
      empty.o -lSystem -o Cocoa
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    F=$out/System/Library/Frameworks/Cocoa.framework
    install -Dm755 Cocoa $F/Versions/A/Cocoa
    install -Dm644 Cocoa.h $F/Versions/A/Headers/Cocoa.h
    install -Dm644 Info.plist $F/Versions/A/Resources/Info.plist
    sed -i 's/''${PRODUCT_NAME}/Cocoa/' $F/Versions/A/Resources/Info.plist
    ln -s A $F/Versions/Current
    ln -s Versions/Current/Cocoa $F/Cocoa
    ln -s Versions/Current/Headers $F/Headers
    ln -s Versions/Current/Resources $F/Resources
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "Cocoa umbrella framework re-exporting PureDarwin's Foundation, AppKit and CoreData";
    platforms = lib.platforms.unix;
  };
}
