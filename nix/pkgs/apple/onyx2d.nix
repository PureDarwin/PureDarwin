{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, libobjc
, corefoundation
, foundation
, freetype2
, libpng
, libjpeg
, zlib
, src
, appleSdk
}:

let

  installName = "/System/Library/Frameworks/Onyx2D.framework/Versions/A/Onyx2D";

  # libtiff is not packaged for PureDarwin yet. The O2Defines_* headers are
  # Onyx2D's own switches for optional decoders; with LIBTIFF_PRESENT off,
  # O2TIFFImageDirectory falls back to its built-in reader and every TIFF
  # source still builds.
  excludedSrcs = [ ];
in
stdenv.mkDerivation {
  pname = "puredarwin-onyx2d";
  version = "0.1";

  inherit src;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    # Sources #import <Onyx2D/X.h>, so stage the headers under that name.
    mkdir -p staged/Onyx2D
    cp *.h staged/Onyx2D/
    echo "#define LIBTIFF_PRESENT 0" > staged/Onyx2D/O2Defines_libtiff.h
    cp staged/Onyx2D/O2Defines_libtiff.h O2Defines_libtiff.h

    # corefoundation.nix installs its headers flattened into $out/include, so
    # stage the <CoreFoundation/Foo.h> layout Onyx2D's imports expect.
    mkdir -p cf-headers
    ln -s ${corefoundation}/include cf-headers/CoreFoundation

    cc="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    cflags="
      -isysroot $DARWIN_SDK_ROOT
      -mmacosx-version-min=26.5
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
      -fno-stack-protector
      -fobjc-arc-exceptions -fblocks
      -Wno-deprecated-objc-isa-usage -Wno-objc-root-class
      -I$PWD/staged
      -I${libSystem}/usr/include
      -I${libobjc}/usr/include
      -I$PWD/cf-headers
      -I${corefoundation}/include
      -I${foundation}/usr/include
      -I${freetype2}/include/freetype2
      -I${libpng}/include
      -I${libjpeg}/include
      -I${zlib}/include
    "

    objs=""
    for s in *.m; do
      case " ${lib.concatStringsSep " " excludedSrcs} " in
        *" $s "*) continue ;;
      esac
      echo "  CC $s"
      $cc $cflags -c "$s" -o "''${s%.m}.o"
      objs="$objs ''${s%.m}.o"
    done

    $cc \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -L${libobjc}/usr/lib \
      -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib \
      -L${freetype2}/lib -L${libpng}/lib -L${libjpeg}/lib -L${zlib}/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,${installName} \
      $objs \
      -lFoundation -lCoreFoundation -lobjc \
      -lfreetype -lpng -ljpeg -lz \
      -lSystem \
      -o Onyx2D

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    frameworkDir="$out/System/Library/Frameworks/Onyx2D.framework"
    mkdir -p "$frameworkDir/Versions/A/Headers" "$frameworkDir/Versions/A/Resources"
    cp Onyx2D "$frameworkDir/Versions/A/Onyx2D"
    cp *.h "$frameworkDir/Versions/A/Headers/"
    cp Info.plist "$frameworkDir/Versions/A/Resources/"

    ln -s A "$frameworkDir/Versions/Current"
    ln -s Versions/Current/Onyx2D "$frameworkDir/Onyx2D"
    ln -s Versions/Current/Headers "$frameworkDir/Headers"
    ln -s Versions/Current/Resources "$frameworkDir/Resources"

    # Flat alias for callers that link -lOnyx2D rather than -framework Onyx2D.
    mkdir -p "$out/usr/lib" "$out/usr/include"
    ln -s "../../System/Library/Frameworks/Onyx2D.framework/Versions/A/Onyx2D" \
      "$out/usr/lib/libOnyx2D.dylib"
    ln -s "../../System/Library/Frameworks/Onyx2D.framework/Versions/A/Headers" \
      "$out/usr/include/Onyx2D"

    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Onyx2D: the software 2D rasteriser backing CoreGraphics.framework";
    platforms = platforms.unix;
  };
}
