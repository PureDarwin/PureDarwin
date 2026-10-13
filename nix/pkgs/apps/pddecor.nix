{ stdenv, lib, darwinCrossToolchain, nativeLd, appleSdk, libSystem
, corefoundation, foundation, libobjc, appkit, onyx2d, coretext, quartzcore
, coregraphics, applicationservices, coreservices, windowserver, openglFramework, mesa
, iokit, iokitHeaders, wayland, waylandScanner
, src ? ../../../src/Userspace/pddecor
, protocol ? ../../../src/ThirdParty/neuswc/protocol/pd-decoration-v1.xml
, targetTriple ? "x86_64-apple-darwin20.4" }:

# title bars for Wayland windows that are not AppKit's, drawn by AppKit's own frame code
stdenv.mkDerivation {
  pname = "puredarwin-pddecor";
  version = "0.1";
  inherit src;
  dontConfigure = true;
  dontFixup = true;
  dontStrip = true;
  nativeBuildInputs = [ waylandScanner ];

  buildPhase = ''
    runHook preBuild
    export SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    cc=${darwinCrossToolchain}/bin/${targetTriple}-clang
    common="-isysroot $SDK -mmacosx-version-min=26.5 -D_DARWIN_C_SOURCE -fno-stack-protector -O2"

    wayland-scanner client-header ${protocol} pd-decoration-v1-client-protocol.h
    wayland-scanner private-code ${protocol} pd-decoration-v1-protocol.c
    mkdir -p objs
    for f in main.m pd-decoration-v1-protocol.c; do
      $cc $common -x objective-c -fno-objc-arc -fblocks -Wall -Wno-deprecated-declarations \
        -I. -I${wayland}/include \
        -I${libSystem}/usr/include -I${libobjc}/usr/include -I${corefoundation}/include \
        -I${foundation}/usr/include -I${onyx2d}/usr/include -I${coretext}/usr/include \
        -I${quartzcore}/usr/include -I${applicationservices}/usr/include \
        -I${coregraphics}/usr/include -I${coreservices}/usr/include \
        -I${windowserver}/usr/include -I${iokitHeaders}/include -I${mesa}/usr/include \
        -F${appkit}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks \
        -c "$f" -o "objs/$(basename "$f" | sed 's/\.[mc]$//').o"
    done

    $cc -isysroot "$SDK" -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib -L${appkit}/usr/lib -L${onyx2d}/usr/lib \
      -L${coretext}/usr/lib -L${quartzcore}/usr/lib -L${coregraphics}/usr/lib \
      -L${windowserver}/usr/lib -L${iokit}/usr/lib -L${wayland}/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      objs/*.o \
      -lAppKit -lQuartzCore -lCoreText -lCoreGraphics -lOnyx2D -lWindowServer \
      -lFoundation -lIOKitCF -lCoreFoundation -lwayland-client -lobjc -lSystem \
      -o pddecor
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    app="$out/System/Library/CoreServices/pddecor.app/Contents"
    install -Dm755 pddecor "$app/MacOS/pddecor"
    install -Dm644 Info.plist "$app/Info.plist"
    runHook postInstall
  '';
}
