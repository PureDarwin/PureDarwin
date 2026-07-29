{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
, nativeLd
, libSystem
, libcxxabiDylib
, src
}:

let
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };

  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";

  cxxSrcs = [
    "algorithm" "any" "bind" "charconv" "chrono" "condition_variable"
    "condition_variable_destructor" "debug" "exception" "functional" "future"
    "hash" "ios" "iostream" "locale" "memory" "mutex" "mutex_destructor" "new"
    "optional" "random" "regex" "shared_mutex" "stdexcept" "string" "strstream"
    "system_error" "thread" "typeinfo" "utility" "valarray" "variant" "vector"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-libcxx-dylib";
  version = "1";

  inherit src;

  nativeBuildInputs = [ stdenv.cc ];

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    L=src/Libraries
    ABI=$L/libcxxabi
    CXX=$L/libcxx
    CONFIG=$ABI/config

    CXX_FLAGS="-isysroot $DARWIN_SDK_ROOT -I${libSystem}/usr/include \
      -std=c++17 -nostdinc++ -funwind-tables -fexceptions -fPIC -Os -DNDEBUG \
      -I $CONFIG -I $ABI/include -I $CXX/src -I $CXX/include \
      -D_LIBCPP_BUILDING_LIBRARY -D_LIBCXXABI_BUILDING_LIBRARY"

    objs=""

    for s in ${lib.concatStringsSep " " cxxSrcs}; do
      extra=""
      # operator new/delete already come from libc++abi's stdlib_new_delete.cpp.
      [ "$s" = "new" ] && extra="-D_LIBCPP_DISABLE_NEW_DELETE_DEFINITIONS"
      ${cc} $CXX_FLAGS $extra -c "$CXX/src/$s.cpp" -o "cxx_$s.o"
      objs="$objs cxx_$s.o"
    done

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libcxxabiDylib}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-install_name,/usr/lib/libc++.1.dylib \
      -Wl,-fixup_chains \
      -lc++abi -lSystem \
      -o libc++.1.dylib $objs

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $out/usr/include/c++/v1
    # install_name baked in at link time; no install_name_tool pass (llvm's
    # chokes on LC_DYLD_CHAINED_FIXUPS).
    cp libc++.1.dylib $out/usr/lib/
    ln -s libc++.1.dylib $out/usr/lib/libc++.dylib
    # Ship the real C++ headers so downstream ports get <vector> etc.
    cp -a src/Libraries/libcxx/include/. $out/usr/include/c++/v1/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "PureDarwin libc++.1.dylib (full C++ standard library, cross-built, layered on libc++abi.dylib)";
    platforms = platforms.linux;
  };
}
