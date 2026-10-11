{ stdenv, lib, fetchurl, darwinCrossToolchain, nativeLd, appleSdk, libSystem
, corefoundation, foundation, libobjc, appkit, onyx2d, coretext, quartzcore
, coregraphics, applicationservices, coreservices, windowserver, openglFramework, mesa
, librsvg, iokit, iokitHeaders
, src ? ../../../src/Userspace/MenuBar
, targetTriple ? "x86_64-apple-darwin20.4" }:

stdenv.mkDerivation {
  pname = "puredarwin-menubar";
  version = "0.1";
  inherit src;
  dontConfigure = true;
  dontFixup = true;
  dontStrip = true;

  buildPhase = ''
    runHook preBuild
    export SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    cc=${darwinCrossToolchain}/bin/${targetTriple}-clang
    common="-isysroot $SDK -mmacosx-version-min=26.5 -D_DARWIN_C_SOURCE -fno-stack-protector -O2"

    mkdir -p objs png
    # the protocol header is shared with AppKit
    cp ${../../../src/Frameworks/AppKit/PDMenuServer.h} PDMenuServer.h
    for f in main.m MenuBarView.m AboutPanel.m; do
      $cc $common -x objective-c -fno-objc-arc -fblocks -Wall -Wno-deprecated-declarations \
        -I. \
        -I${libSystem}/usr/include -I${libobjc}/usr/include -I${corefoundation}/include \
        -I${foundation}/usr/include -I${onyx2d}/usr/include -I${coretext}/usr/include \
        -I${quartzcore}/usr/include -I${applicationservices}/usr/include \
        -I${coregraphics}/usr/include -I${coreservices}/usr/include \
        -I${windowserver}/usr/include -I${iokitHeaders}/include -I${mesa}/usr/include \
        -F${appkit}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks \
        -c "$f" -o "objs/$(basename "$f" .m).o"
    done

    $cc -isysroot "$SDK" -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib -L${appkit}/usr/lib -L${onyx2d}/usr/lib \
      -L${coretext}/usr/lib -L${quartzcore}/usr/lib -L${coregraphics}/usr/lib \
      -L${windowserver}/usr/lib -L${iokit}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      objs/*.o \
      -lAppKit -lQuartzCore -lCoreText -lCoreGraphics -lOnyx2D -lWindowServer \
      -lFoundation -lIOKitCF -lCoreFoundation -lobjc -lSystem \
      -o MenuBar

    # icons render at 1x and 2x, symbolic ones stay dark on the light bar
    for name in beak wifi volume battery search; do
      ${librsvg}/bin/rsvg-convert -w 16 -h 16 Resources/$name.svg -o png/$name.png
      ${librsvg}/bin/rsvg-convert -w 32 -h 32 Resources/$name.svg -o png/$name@2x.png
    done
    # the About window's logo
    ${librsvg}/bin/rsvg-convert -w 192 -h 192 --keep-aspect-ratio Resources/beak.svg -o png/beak-large.png
    ${librsvg}/bin/rsvg-convert -w 44 -h 44 Resources/search.svg -o png/search-large.png
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    app="$out/System/Library/CoreServices/MenuBar.app/Contents"
    install -Dm755 MenuBar "$app/MacOS/MenuBar"
    install -Dm644 Info.plist "$app/Info.plist"
    install -Dm644 -t "$app/Resources" png/*.png
    install -Dm644 Resources/TRENTA-LICENSE "$app/Resources/TRENTA-LICENSE"
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin system menu bar, the server AppKit apps publish their menus to";
    license = with licenses; [ mpl20 bsd2 cc-by-sa-40 ];
    platforms = platforms.unix;
  };
}
