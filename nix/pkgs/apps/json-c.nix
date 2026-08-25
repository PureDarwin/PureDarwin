{ stdenv
, lib
, cmake
, ninja
, pkg-config
, darwinCrossToolchain
, nativeLd
, libSystem
, src
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;
in
stdenv.mkDerivation {
  pname = "puredarwin-json-c";
  version = "0.18";
  inherit src;

  nativeBuildInputs = [ cmake ninja pkg-config ];

  configurePhase = ''
    runHook preConfigure
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    cat > puredarwin-toolchain.cmake <<EOF
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR ${targetInfo.mesonCpu})
set(CMAKE_C_COMPILER ${darwinCrossToolchain}/bin/${targetTriple}-clang)
set(CMAKE_AR ${darwinCrossToolchain}/bin/${targetTriple}-ar)
set(CMAKE_RANLIB ${darwinCrossToolchain}/bin/${targetTriple}-ranlib)
set(CMAKE_STRIP ${darwinCrossToolchain}/bin/${targetTriple}-strip)
set(CMAKE_SYSROOT $DARWIN_SDK_ROOT)
set(CMAKE_C_FLAGS "-mmacosx-version-min=11.0 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector")
set(CMAKE_EXE_LINKER_FLAGS "--ld-path=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,11.0,11.5 -lSystem")
set(CMAKE_SHARED_LINKER_FLAGS "--ld-path=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,11.0,11.5 -lSystem")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF
    cmake -S . -B build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/puredarwin-toolchain.cmake \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DCMAKE_INSTALL_PREFIX=$out \
      -DBUILD_SHARED_LIBS=ON \
      -DDISABLE_BIGNUM=ON \
      -DBUILD_TESTING=OFF \
      -DDOC_INSTALL_DIR=share/doc/json-c
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
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "JSON-C cross-built for PureDarwin";
    homepage = "https://github.com/json-c/json-c";
    license = licenses.mit;
    platforms = platforms.linux;
  };
}
