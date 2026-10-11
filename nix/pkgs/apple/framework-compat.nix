{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, corefoundation
, foundation
  # the Swift runtime, whose libswiftFoundation is the Swift half of Foundation
, swiftRuntime
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# the framework paths Xcode-built apps link against, over PD's own libraries: CoreFoundation.framework/Versions/A re-exports libFoundation and libCoreFoundation (Foundation first, so its strong ___CFConstantStringClassReference wins over CF's weak placeholder),
# Foundation.framework/Versions/C also re-exports libswiftFoundation for Swift apps, and AppKit.framework/Versions/C is the same file as PD's (dyld matches by inode). Compatibility versions are unneeded, dyld enforces them only below macOS 10.14
stdenv.mkDerivation {
  pname = "puredarwin-framework-compat";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    echo "// re-exports only" > empty.c
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -c empty.c -o empty.o
    $CC -fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot $SDK -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib \
      -dynamiclib -install_name /System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation \
      -compatibility_version 150 -current_version 1338 \
      -Wl,-reexport_library,${foundation}/usr/lib/libFoundation.dylib \
      -Wl,-reexport_library,${corefoundation}/usr/lib/libCoreFoundation.dylib \
      empty.o -lSystem -o CoreFoundation
    $CC -fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot $SDK -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib \
      -dynamiclib -install_name /System/Library/Frameworks/Foundation.framework/Versions/C/Foundation \
      -compatibility_version 300 -current_version 5026.5.4 \
      -Wl,-reexport_library,${foundation}/usr/lib/libFoundation.dylib \
      -Wl,-reexport_library,${swiftRuntime}/usr/lib/swift/libswiftFoundation.dylib \
      empty.o -lSystem -o Foundation
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    F=$out/System/Library/Frameworks
    install -Dm755 CoreFoundation $F/CoreFoundation.framework/Versions/A/CoreFoundation
    install -Dm755 Foundation $F/Foundation.framework/Versions/C/Foundation
    mkdir -p $F/AppKit.framework/Versions/C
    ln -s ../A/AppKit $F/AppKit.framework/Versions/C/AppKit
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "Apple framework install paths for Xcode-built apps over PureDarwin's Foundation, CoreFoundation and AppKit";
    platforms = lib.platforms.unix;
  };
}
