{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, appleSdk
, libSystem
, libobjc
, corefoundation
, foundation
, coreservices
, iokit
, coregraphics
, coretext
, onyx2d
, quartzcore
, corevideo
, iosurface
, audiotoolbox
, appFrameworks
, src
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# the Firefox framework tests: one program per framework area, run once at boot by a launch daemon
stdenv.mkDerivation {
  pname = "puredarwin-frameworks-ff-tests";
  version = "0.1";

  inherit src;
  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    CC=${darwinCrossToolchain}/bin/${targetTriple}-clang
    SDK=${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    FW=System/Library/Frameworks
    mkdir -p inc cf-headers
    ln -s ${corefoundation}/include cf-headers/CoreFoundation
    ln -s ${appFrameworks}/$FW/CoreMedia.framework/Headers inc/CoreMedia

    CFLAGS="-isysroot $SDK -mmacosx-version-min=26.5 -O1 -Wall -Wno-deprecated-declarations -fblocks -fobjc-exceptions -fno-objc-arc \
      -Iinc -Icf-headers -I${libSystem}/usr/include -I${libobjc}/usr/include -I${foundation}/usr/include \
      -I${coreservices}/usr/include -I${coregraphics}/usr/include -I${onyx2d}/usr/include -I${quartzcore}/usr/include \
      -I${corevideo}/usr/include -I${audiotoolbox}/usr/include -I$src"
    LDFLAGS="-isysroot $SDK -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${libobjc}/usr/lib \
      -L${foundation}/usr/lib -L${iokit}/usr/lib"
    # re-exported and linked frameworks, found by install name
    for m in ${coregraphics}:CoreGraphics ${coretext}:CoreText ${appFrameworks}:ImageIO ${corevideo}:CoreVideo ${appFrameworks}:CoreMedia \
        ${audiotoolbox}:CoreAudio ${audiotoolbox}:AudioToolbox ${iosurface}:IOSurface ${coreservices}:CoreServices; do
      n=''${m##*:}; LDFLAGS="$LDFLAGS -Wl,-dylib_file,/System/Library/Frameworks/$n.framework/Versions/A/$n:''${m%%:*}/System/Library/Frameworks/$n.framework/Versions/A/$n"
    done
    fw() { echo "$1/$FW/$2.framework/Versions/A/$2"; }

    $CC $CFLAGS -c $src/cs_test.c -o cs_test.o
    $CC $LDFLAGS cs_test.o $(fw ${coreservices} CoreServices) -lCoreFoundation -lSystem -o cs_test
    $CC $CFLAGS -x objective-c -c $src/as_test.c -o as_test.o
    $CC $LDFLAGS as_test.o $(fw ${appFrameworks} ApplicationServices) -lCoreFoundation -lobjc -lSystem -o as_test
    $CC $CFLAGS -c $src/iokit_test.c -o iokit_test.o
    $CC $LDFLAGS iokit_test.o -lIOKitCF -lCoreFoundation -lSystem -o iokit_test
    $CC $CFLAGS -c $src/media_test.c -o media_test.o
    $CC $LDFLAGS media_test.o $(fw ${appFrameworks} CoreMedia) $(fw ${appFrameworks} VideoToolbox) $(fw ${corevideo} CoreVideo) \
      -lCoreFoundation -lSystem -o media_test
    $CC $CFLAGS -x objective-c -c $src/objc_test.m -o objc_test.o
    $CC $LDFLAGS objc_test.o -lFoundation -lCoreFoundation -lobjc -lSystem -o objc_test
    # CoreFoundation first, so the literals' isa binds from it the way Firefox's does
    $CC $CFLAGS -x objective-c -c $src/ffsyms_test.m -o ffsyms_test.o
    $CC $LDFLAGS ffsyms_test.o -lCoreFoundation -lFoundation -lobjc -lSystem -o ffsyms_test
    $CC $CFLAGS -c $src/misc_test.c -o misc_test.o
    $CC $LDFLAGS misc_test.o -lCoreFoundation -lSystem -o misc_test
    $CC $CFLAGS -c $src/audio_test.c -o audio_test.o
    $CC $LDFLAGS audio_test.o $(fw ${audiotoolbox} AudioToolbox) $(fw ${audiotoolbox} CoreAudio) -lCoreFoundation -lSystem -o audio_test
    $CC $CFLAGS -c $src/dsp_tone.c -o dsp_tone.o
    $CC $LDFLAGS dsp_tone.o -lSystem -o dsp_tone
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/local/libexec/fwtests $out/System/Library/LaunchDaemons
    install -m755 *_test dsp_tone $src/run.sh $out/usr/local/libexec/fwtests/
    install -m644 $src/org.puredarwin.fwtests.plist $out/System/Library/LaunchDaemons/
    runHook postInstall
  '';

  dontFixup = true;

  meta = {
    description = "Tests for the frameworks the official Firefox build links";
    platforms = lib.platforms.unix;
  };
}
