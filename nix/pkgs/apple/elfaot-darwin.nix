{ stdenv
, lib
, capstone
, glibc
, gnumake
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
}:

let
  cc = "${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang";
  ar = "${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar";
  ranlib = "${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ranlib";
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
in
stdenv.mkDerivation {
  pname = "puredarwin-elfaot-darwin";
  version = "0.1";
  dontUnpack = true;
  nativeBuildInputs = [ gnumake ];

  buildPhase = ''
    runHook preBuild
    ln -s ${glibc.dev}/include/elf.h elf.h
    CFLAGS="-O2 -isysroot ${sdk} -mmacosx-version-min=26.5" \
      make -C ${capstone.src} CAPSTONE_ARCHS=x86 \
        CAPSTONE_STATIC=yes CAPSTONE_SHARED=no CAPSTONE_BUILD_CORE_ONLY=yes \
        CC=${cc} AR=${ar} RANLIB=${ranlib} BUILDDIR=$PWD/capstone

    ${cc} -isysroot ${sdk} -mmacosx-version-min=26.5 \
      -I${libSystem}/usr/include -I. -I${capstone.src}/include \
      -I${../../../src/Tools/elftrans} -O2 -Wall -Wextra \
      -c ${../../../src/Tools/elftrans/elfaot.c} -o elfaot.o

    ${cc} -isysroot ${sdk} -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-pagezero_size,0x1000 \
      -o elfaot elfaot.o capstone/libcapstone.a -lSystem
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 elfaot $out/usr/bin/elfaot
    runHook postInstall
  '';

  dontFixup = true;
  meta = with lib; {
    description = "Prepare Linux x86-64 ELF for relocated PureDarwin execution";
    platforms = platforms.unix;
  };
}
