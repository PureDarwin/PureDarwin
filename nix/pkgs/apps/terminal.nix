{ stdenv, lib, librsvg, fetchurl, darwinCrossToolchain, nativeLd, appleSdk, libSystem
, corefoundation, foundation, libobjc, appkit, onyx2d, coretext, quartzcore
, coregraphics, applicationservices, coreservices, windowserver, openglFramework, mesa
, src ? ../../../src/Userspace/Terminal
, targetTriple ? "x86_64-apple-darwin20.4" }:

let
  # VT100/xterm parsing and screen state, MIT
  libvterm = fetchurl {
    url = "https://launchpad.net/libvterm/trunk/v0.3/+download/libvterm-0.3.3.tar.gz";
    hash = "sha256-CRVvQ90hKL00fL7r5Q2aVx0yxk4M8Y0hEZeUav9yJuA=";
  };
in
stdenv.mkDerivation {
  pname = "puredarwin-terminal";
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

    mkdir -p vterm objs
    tar xzf ${libvterm} -C vterm --strip-components=1
    for f in vterm/src/*.c; do
      $cc $common -std=c99 -Ivterm/include -c "$f" -o "objs/vterm_$(basename "$f" .c).o"
    done

    for f in main.m TerminalView.m TerminalTabs.m; do
      $cc $common -x objective-c -fno-objc-arc -fblocks -Wall -Wno-deprecated-declarations \
        -Ivterm/include \
        -I${libSystem}/usr/include -I${libobjc}/usr/include -I${corefoundation}/include \
        -I${foundation}/usr/include -I${onyx2d}/usr/include -I${coretext}/usr/include \
        -I${quartzcore}/usr/include -I${applicationservices}/usr/include \
        -I${coregraphics}/usr/include -I${coreservices}/usr/include \
        -I${windowserver}/usr/include -I${mesa}/usr/include \
        -F${appkit}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks \
        -c "$f" -o "objs/$(basename "$f" .m).o"
    done

    $cc -isysroot "$SDK" -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib -L${appkit}/usr/lib -L${onyx2d}/usr/lib \
      -L${coretext}/usr/lib -L${quartzcore}/usr/lib -L${coregraphics}/usr/lib \
      -L${windowserver}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      objs/*.o \
      -lAppKit -lQuartzCore -lCoreText -lCoreGraphics -lOnyx2D -lWindowServer \
      -lFoundation -lCoreFoundation -lobjc -lSystem \
      -o Terminal
    # the bundle icon Lookout, Finder and the About panel show, from the Dock's artwork
    ${librsvg}/bin/rsvg-convert -w 256 -h 256 Resources/terminal.svg -o AppIcon.png
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    app="$out/Applications/Utilities/Terminal.app/Contents"
    install -Dm755 Terminal "$app/MacOS/Terminal"
    install -Dm644 Info.plist "$app/Info.plist"
    install -Dm644 AppIcon.png "$app/Resources/AppIcon.png"
    install -Dm644 vterm/LICENSE "$app/Resources/libvterm-LICENSE"
    runHook postInstall
  '';

  meta = with lib; {
    description = "AppKit terminal emulator for PureDarwin";
    license = with licenses; [ mpl20 bsd2 ];
    platforms = platforms.unix;
  };
}
