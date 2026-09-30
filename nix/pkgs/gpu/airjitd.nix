{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, llvm
, libcxxDylib
, libcxxabiDylib
, targetTriple ? "arm64-apple-darwin20.4"
}:

stdenv.mkDerivation {
  pname = "puredarwin-airjitd";
  version = "0.1";
  src = ../../../src/Tools/airjitd;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    CFLAGS="-target arm64-apple-macos14 -O1 -isysroot $SDK -I${llvm}/usr/include -Wall"
    objs=""
    for src in *.c llvm/*.c; do
      obj="''${src//\//_}"
      obj="''${obj%.c}.o"
      $CC $CFLAGS -c "$src" -o "$obj"
      objs="$objs $obj"
    done
    ${nativeLd}/bin/ld -arch arm64 -execute -platform_version macos 10.15 10.15 -e _main \
      -adhoc_codesign -syslibroot ${libSystem} -o airjitd $objs \
      ${llvm}/usr/lib/libLLVM*.a \
      ${libcxxDylib}/usr/lib/libc++.1.dylib ${libcxxabiDylib}/usr/lib/libc++abi.dylib \
      IOKit.tbd libdispatch.tbd ${libSystem}/usr/lib/libSystem.B.dylib
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 airjitd $out/usr/libexec/ifcstart
    runHook postInstall
  '';

  meta = with lib; {
    description = "PureDarwin AIR shader JIT daemon for the paravirtual GPU";
    platforms = platforms.unix;
  };
}
