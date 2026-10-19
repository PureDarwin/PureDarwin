{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, corefoundation
, src
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# IOSurface.framework for PureDarwin applications: surfaces shared between processes over Mach memory entries, and iosurfaced for global IDs
stdenv.mkDerivation {
  pname = "puredarwin-iosurface";
  version = "0.1";

  inherit src;
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    mkdir -p cf-headers
    ln -s ${corefoundation}/include cf-headers/CoreFoundation
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -O2 -Wall -Icf-headers -I${libSystem}/usr/include -I$src \
      -c $src/IOSurface.c -o IOSurface.o
    # the broker for global surface IDs
    $CC -isysroot $SDK -mmacosx-version-min=26.5 -O2 -Wall -I${libSystem}/usr/include -I$src \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib \
      $src/iosurfaced.c -lSystem -o iosurfaced
    $CC -fuse-ld=${nativeLd}/bin/ld -nostdlib -isysroot $SDK -mmacosx-version-min=26.5 \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib -L${corefoundation}/usr/lib \
      -dynamiclib -install_name /System/Library/Frameworks/IOSurface.framework/Versions/A/IOSurface \
      -compatibility_version 1 -current_version 1 \
      IOSurface.o -lCoreFoundation -lSystem -o IOSurface
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    F=$out/System/Library/Frameworks/IOSurface.framework
    install -Dm755 IOSurface $F/Versions/A/IOSurface
    ln -s A $F/Versions/Current
    ln -s Versions/Current/IOSurface $F/IOSurface
    install -Dm755 iosurfaced $out/usr/libexec/iosurfaced
    install -Dm644 $src/org.puredarwin.iosurfaced.plist $out/System/Library/LaunchDaemons/org.puredarwin.iosurfaced.plist
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "IOSurface for PureDarwin applications, with iosurfaced for global surface IDs";
    platforms = lib.platforms.unix;
  };
}
