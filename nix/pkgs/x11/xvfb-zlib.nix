{ stdenv
, lib
, cmake
, ninja
, darwinCrossToolchain
, nativeLd
, libSystem
, zlib
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-zlib";
  version = zlib.version or "0";

  src = zlib.src;

  # nixpkgs' own zlib is built for the Linux host (ELF); libfontenc/libXfont2
  # need a real Mach-O zlib for the Darwin target, so cross-compile zlib's own
  # CMakeLists.txt the same way as freetype2 (see xvfb-freetype.nix).
  nativeBuildInputs = [ cmake ninja ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    cmake -B build -G Ninja \
      -DCMAKE_SYSTEM_NAME=Darwin \
      -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
      -DCMAKE_C_COMPILER=${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -DCMAKE_AR=${darwinCrossToolchain}/bin/${targetTriple}-ar \
      -DCMAKE_RANLIB=${darwinCrossToolchain}/bin/${targetTriple}-ranlib \
      -DCMAKE_C_FLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -I${libSystem}/usr/include" \
      -DCMAKE_EXE_LINKER_FLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -lSystem" \
      -DCMAKE_INSTALL_PREFIX=$out \
      -DBUILD_SHARED_LIBS=OFF

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build zlibstatic
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/lib/pkgconfig $out/include
    install -m644 build/libz.a $out/lib/libz.a
    install -m644 zlib.h zconf.h $out/include/
    sed -e "s|@prefix@|$out|" -e "s|@exec_prefix@|$out|" -e "s|@libdir@|$out/lib|" -e "s|@sharedlibdir@|$out/lib|" -e "s|@includedir@|$out/include|" -e "s|@VERSION@|${zlib.version or "1.3.1"}|" build/zlib.pc > $out/lib/pkgconfig/zlib.pc
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}