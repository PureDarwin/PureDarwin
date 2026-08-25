{ stdenv
, lib
, cmake
, ninja
, pkg-config
, darwinCrossToolchain
, nativeLd
, libSystem
, zlib
, freetype
, nativeMesonTools
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-freetype2";
  version = freetype.version or "0";

  src = freetype.src;

  # freetype's autotools ./configure tries to build a *native* helper tool
  # (apinames) as part of cross-configuring and gets confused by the nix
  # sandbox's compiler naming; its CMakeLists.txt has no such step and cross-
  # compiles cleanly with a plain toolchain file, like the rest of this repo.
  nativeBuildInputs = [ cmake ninja pkg-config ];

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
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_SHARED_LINKER_FLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,11.0,11.5 -lSystem" \
      -DCMAKE_INSTALL_NAME_DIR=/usr/lib \
      -DCMAKE_BUILD_TYPE=Release \
      -DFT_DISABLE_ZLIB=ON \
      -DFT_DISABLE_BZIP2=ON \
      -DFT_DISABLE_PNG=ON \
      -DFT_DISABLE_HARFBUZZ=ON \
      -DFT_DISABLE_BROTLI=ON

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    ninja -C build install
    {
      # CMake stamps @rpath into the install_name; guest binaries need the
      # absolute path they will be loaded from, as the xorg shared libs use.
      INT="${nativeMesonTools}/bin/install_name_tool"
      for dylib in "$out"/lib/*.dylib; do
        [ -L "$dylib" ] && continue
        "$INT" -id "/usr/lib/$(basename "$dylib")" "$dylib"
      done

      mkdir -p "$out/usr/lib"
      for dylib in "$out"/lib/*.dylib; do
        [ -e "$dylib" ] || continue
        ln -sf "../../lib/$(basename "$dylib")" "$out/usr/lib/$(basename "$dylib")"
      done
    }
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}