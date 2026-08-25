{ stdenv
, lib
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
, nativeLd
, libSystem
, corefoundation
, iokitCFStatic
, isArmv6 ? lib.hasPrefix "armv6-" targetTriple
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-iokit";
  version = "0.1";

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    # -Wl,-fixup_chains: same eager-bind fix as corefoundation.nix - PD's
    # dyld lazy-binding path is fragile and this dylib's own internal calls
    # need to not go through it.
    ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -L${libSystem}/usr/lib -L${corefoundation}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-install_name,/usr/lib/libIOKitCF.dylib \
      -Wl,-force_load,${iokitCFStatic}/usr/lib/system/libIOKitCF.a \
      ${lib.optionalString (!isArmv6) "-Wl,-fixup_chains"} \
      -lCoreFoundation -lSystem \
      -o libIOKitCF.dylib

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib
    cp libIOKitCF.dylib $out/usr/lib/

    fwdir=$out/System/Library/Frameworks/IOKit.framework
    mkdir -p "$fwdir/Versions/A/Resources"
    ln -s ../../../../../../usr/lib/libIOKitCF.dylib "$fwdir/Versions/A/IOKit"
    ln -sf A "$fwdir/Versions/Current"
    ln -sf Versions/Current/IOKit "$fwdir/IOKit"
    ln -sf Versions/Current/Resources "$fwdir/Resources"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Real CF-shaped IOKitLib (IOServiceGetMatchingService, IORegistryEntryCreateCFProperty, etc), linked against real CoreFoundation";
    platforms = platforms.unix;
  };
}
