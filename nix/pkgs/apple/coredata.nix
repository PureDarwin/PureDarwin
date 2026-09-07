{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, libobjc
, corefoundation
, foundation
, src
, appleSdk
}:

let
  installName = "/System/Library/Frameworks/CoreData.framework/Versions/A/CoreData";
in
stdenv.mkDerivation {
  pname = "puredarwin-coredata";
  version = "0.1";

  inherit src;

  buildPhase = ''
    runHook preBuild

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    mkdir -p staged/CoreData cf-headers
    cp *.h staged/CoreData/
    ln -s ${corefoundation}/include cf-headers/CoreFoundation

    cc="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    cflags="
      -isysroot $DARWIN_SDK_ROOT
      -mmacosx-version-min=26.5
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
      -fno-stack-protector
      -fobjc-exceptions -fblocks
      -Wno-deprecated-objc-isa-usage -Wno-objc-root-class
      -I$PWD/staged
      -I$PWD/cf-headers
      -I${libSystem}/usr/include
      -I${libobjc}/usr/include
      -I${corefoundation}/include
      -I${foundation}/usr/include
    "

    objs=""
    for source in *.m; do
      object="''${source%.m}.o"
      echo "  CC $source"
      $cc $cflags -c "$source" -o "$object"
      objs="$objs $object"
    done

    $cc \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -L${libobjc}/usr/lib \
      -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,${installName} \
      $objs -lFoundation -lCoreFoundation -lobjc -lSystem \
      -o CoreData

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    frameworkDir="$out/System/Library/Frameworks/CoreData.framework"
    mkdir -p "$frameworkDir/Versions/A/Headers" "$frameworkDir/Versions/A/Resources"
    cp CoreData "$frameworkDir/Versions/A/CoreData"
    cp *.h "$frameworkDir/Versions/A/Headers/"
    cp Info.plist "$frameworkDir/Versions/A/Resources/"
    if [ -d English.lproj ]; then
      cp -R English.lproj "$frameworkDir/Versions/A/Resources/"
    fi

    ln -s A "$frameworkDir/Versions/Current"
    ln -s Versions/Current/CoreData "$frameworkDir/CoreData"
    ln -s Versions/Current/Headers "$frameworkDir/Headers"
    ln -s Versions/Current/Resources "$frameworkDir/Resources"

    mkdir -p "$out/usr/lib" "$out/usr/include"
    ln -s "../../System/Library/Frameworks/CoreData.framework/Versions/A/CoreData" \
      "$out/usr/lib/libCoreData.dylib"
    ln -s "../../System/Library/Frameworks/CoreData.framework/Versions/A/Headers" \
      "$out/usr/include/CoreData"

    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "Cocotron CoreData compatibility framework";
    platforms = platforms.unix;
  };
}
