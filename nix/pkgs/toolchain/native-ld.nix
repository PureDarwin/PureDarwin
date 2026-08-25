{ stdenv
, lib
, cmake
, ninja
, darwinCrossToolchain
, openssl
, bison
, flex
, perl
, bash
, zlib
, ed
, unifdef
, tcsh
, gnustep-base
, pax
, coreutils
, findutils
, gawk
, gnused
, clang
, libuuid
, ruby
, iig
, libxml2
, libtapi
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  nativeLdSource = lib.fileset.toSource {
    root = ../../..;
    fileset = lib.fileset.unions [
      ../../../cmake/suppress_warnings.cmake
      ../../../tools/xar
      ../../../tools/cctools/include
      ../../../tools/cctools/libmacho
      ../../../tools/cctools/libstuff
      ../../../tools/cctools/misc
      ../../../tools/cctools/ld64/src
      ../../../src/Libraries/CommonCrypto
      ../../../src/Libraries/libSystem/corecrypto
    ];
  };

  archDefines = lib.optionalString stdenv.hostPlatform.isAarch64 " -D__arm64__=1";
in
stdenv.mkDerivation {
  pname = "puredarwin-native-ld";
  version = "0.1";

  src = nativeLdSource;

  nativeBuildInputs = [
    cmake ninja darwinCrossToolchain bison flex perl bash ed unifdef tcsh
    gnustep-base pax coreutils findutils gawk gnused clang ruby iig
  ];
  buildInputs = [ zlib libuuid openssl libxml2 libtapi ];

  NIX_DARWIN_TOOLCHAIN_DIR = "${darwinCrossToolchain}/bin";

  configurePhase = ''
    runHook preConfigure
    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    mkdir -p .nix-stubs
    cat > .nix-stubs/sw_vers <<'EOF'
#!/bin/sh
echo 11.3
EOF
    chmod +x .nix-stubs/sw_vers
    export PATH="$PWD/.nix-stubs:$PATH"
    patchShebangs src tools

    mkdir -p .nix-native-stubs
    ln -sf ${clang}/bin/clang .nix-native-stubs/${targetTriple}-clang
    ln -sf "$(command -v ar)" .nix-native-stubs/${targetTriple}-ar

    mkdir -p .nix-native-stubs/sdk-shim-include
    echo '#include <machine/endian.h>' > .nix-native-stubs/sdk-shim-include/endian.h

    export NIX_NATIVE_DARWIN_HEADER_FLAGS="-isysroot $DARWIN_SDK_ROOT -D__APPLE__ -D__MACH__ -D__APPLE_CC__=1${archDefines}"
    export NIX_NATIVE_DARWIN_HEADER_DIRS="$PWD/.nix-native-stubs/sdk-shim-include;$DARWIN_SDK_ROOT/usr/include"

    cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN_NATIVE_LD)

include(cmake/suppress_warnings.cmake)
add_compile_options(-Wno-return-type -Wno-error=cpp -Wno-nullability-completeness)

add_subdirectory(src/Libraries/libSystem/corecrypto)
add_subdirectory(src/Libraries/CommonCrypto)
add_subdirectory(tools/xar)
add_subdirectory(tools/cctools/libmacho)
add_subdirectory(tools/cctools/libstuff)
add_subdirectory(tools/cctools/misc)
add_subdirectory(tools/cctools/ld64/src)
EOF

    cat > src/Libraries/libSystem/corecrypto/CMakeLists.txt <<'EOF'
include(host_corecrypto_static.cmake)
EOF

    cat > src/Libraries/CommonCrypto/CMakeLists.txt <<'EOF'
include(host_commoncrypto_static.cmake)
EOF

    cmake -S . -B build-nix-native -G Ninja \
      -DCMAKE_C_COMPILER=${clang}/bin/clang \
      -DCMAKE_CXX_COMPILER=${clang}/bin/clang++ \
      -DCMAKE_AR="$PWD/.nix-native-stubs/${targetTriple}-ar" \
      -DCMAKE_BUILD_TYPE=Release \
      -DOPENSSL_ROOT_DIR=${openssl.dev} \
      -DOPENSSL_CRYPTO_LIBRARY=${openssl.out}/lib/libcrypto.so \
      -DOPENSSL_SSL_LIBRARY=${openssl.out}/lib/libssl.so \
      -DZLIB_INCLUDE_DIR=${zlib.dev}/include \
      -DZLIB_LIBRARY=${zlib.out}/lib/libz.so \
      -DLIBXML2_INCLUDE_DIR=${libxml2.dev}/include/libxml2 \
      -DLIBXML2_LIBRARY=${libxml2.out}/lib/libxml2.so \
      -DPUREDARWIN_MACOSX_SDK="$DARWIN_SDK_ROOT" \
      -DPUREDARWIN_LIBTAPI_INCLUDE_DIR=${lib.getDev libtapi}/include \
      -DPUREDARWIN_LIBTAPI_LIBRARY=${lib.getLib libtapi}/lib/libtapi.so
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    # host_strip alongside: xnu's kernel build strips the linked kernel with
    # $(STRIP), and llvm-strip refuses this Mach-O ("shared library is not
    # yet supported") - only real cctools strip handles it.
    ninja -C build-nix-native host_ld host_strip
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    cp build-nix-native/tools/cctools/ld64/src/ld/ld $out/bin/ld.real
    cat > $out/bin/ld <<'EOF'
#!/bin/sh
ulimit -s unlimited 2>/dev/null || true
exec "$(dirname "$0")/ld.real" "$@"
EOF
    chmod +x $out/bin/ld $out/bin/ld.real
    cp build-nix-native/tools/cctools/misc/strip $out/bin/strip
    runHook postInstall
  '';

  meta = with lib; {
    description = "Apple's cctools ld64, built for linking PureDarwin (host and target)";
    platforms = platforms.linux;
  };
}
