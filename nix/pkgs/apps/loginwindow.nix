{ stdenv, lib, darwinCrossToolchain, nativeLd, appleSdk, libSystem
, corefoundation, foundation, libobjc, appkit, onyx2d, coretext, quartzcore
, coregraphics, applicationservices, coreservices, windowserver, openglFramework, mesa
, iokit, python3

, src ? ../../../src/Userspace/loginwindow
, cryptoHeaders ? ../../../src/Libraries/CommonCrypto/include
, targetTriple ? "x86_64-apple-darwin20.4" }:

# the login screen, dsuser that keeps local accounts in dslocal, and pdaccountsd that changes them for people
stdenv.mkDerivation {
  pname = "puredarwin-loginwindow";
  version = "0.1";
  inherit src;
  nativeBuildInputs = [ python3 ];
  dontConfigure = true;
  dontFixup = true;
  dontStrip = true;

  buildPhase = ''
    runHook preBuild
    export SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    cc=${darwinCrossToolchain}/bin/${targetTriple}-clang
    common="-isysroot $SDK -mmacosx-version-min=26.5 -D_DARWIN_C_SOURCE -fno-stack-protector -O2"
    incs="-I. -I${cryptoHeaders} \
      -I${libSystem}/usr/include -I${libobjc}/usr/include -I${corefoundation}/include \
      -I${foundation}/usr/include -I${onyx2d}/usr/include -I${coretext}/usr/include \
      -I${quartzcore}/usr/include -I${applicationservices}/usr/include \
      -I${coregraphics}/usr/include -I${coreservices}/usr/include \
      -I${windowserver}/usr/include -I${mesa}/usr/include \
      -F${appkit}/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks"
    link="-isysroot $SDK -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib -L${appkit}/usr/lib -L${onyx2d}/usr/lib \
      -L${coretext}/usr/lib -L${quartzcore}/usr/lib -L${coregraphics}/usr/lib \
      -L${windowserver}/usr/lib -F${iokit}/System/Library/Frameworks \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5"

    mkdir -p objs
    $cc $common -x objective-c -fno-objc-arc -fblocks -Wall -Wno-deprecated-declarations $incs -c main.m -o objs/main.o
    $cc $common -Wall $incs -c pdaccounts.c -o objs/pdaccounts.o
    $cc $common -Wall $incs -c dsuser.c -o objs/dsuser.o
    $cc $common -Wall $incs -c pdaccounts_client.c -o objs/pdaccounts_client.o
    $cc $common -Wall $incs -c pdaccountsd.c -o objs/pdaccountsd.o

    $cc $link objs/main.o objs/pdaccounts.o \
      -lAppKit -lQuartzCore -lCoreText -lCoreGraphics -lOnyx2D -lWindowServer \
      -lFoundation -framework IOKit -lCoreFoundation -lobjc -lSystem -o loginwindow
    $cc $link objs/dsuser.o objs/pdaccounts.o objs/pdaccounts_client.o -lCoreFoundation -lobjc -lSystem -o dsuser
    $cc $link objs/pdaccountsd.o objs/pdaccounts.o objs/pdaccounts_client.o -lCoreFoundation -lobjc -lSystem -o pdaccountsd
    python3 make-pictures.py pictures
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    app="$out/System/Library/CoreServices/loginwindow.app/Contents"
    install -Dm755 loginwindow "$app/MacOS/loginwindow"
    install -Dm644 Info.plist "$app/Info.plist"
    install -Dm755 dsuser "$out/usr/sbin/dsuser"
    install -Dm755 pdaccountsd "$out/usr/libexec/pdaccountsd"
    install -Dm644 org.puredarwin.pdaccountsd.plist "$out/System/Library/LaunchDaemons/org.puredarwin.pdaccountsd.plist"
    for f in pictures/*.png; do
      install -Dm644 "$f" "$out/Library/User Pictures/PureDarwin/$(basename "$f")"
    done
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin loginwindow and dsuser: the login screen and local accounts in dslocal";
    license = with licenses; [ mpl20 bsd2 ];
    platforms = platforms.unix;
  };
}
