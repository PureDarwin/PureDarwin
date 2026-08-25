{ stdenv
, lib
, darwinCrossToolchain
, targetTriple ? "x86_64-apple-darwin20.4"
, nativeLd
, libSystem
, libcxxabiDylib
, src
, appleSdk
}:

let

  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";

  # ryu/ are subdirectories; the object names flatten the slash.
  cxxSrcs = [
    "algorithm" "any" "atomic" "barrier" "bind" "call_once" "charconv"
    "chrono" "condition_variable" "condition_variable_destructor"
    "error_category" "exception" "expected" "filesystem/directory_entry"
    "filesystem/directory_iterator" "filesystem/filesystem_clock"
    "filesystem/filesystem_error" "filesystem/operations"
    "filesystem/path" "fstream" "functional" "future" "hash" "ios"
    "ios.instantiations" "iostream" "locale" "memory"
    "memory_resource" "mutex" "mutex_destructor" "new" "new_handler"
    "new_helpers" "optional" "ostream" "print" "random"
    "random_shuffle" "regex" "shared_mutex" "stdexcept" "string"
    "strstream" "system_error" "thread" "typeinfo" "valarray"
    "variant" "vector" "verbose_abort"
    "ryu/d2fixed" "ryu/d2s" "ryu/f2s"
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
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    L=src/Libraries
    ABI=$L/libcxxabi
    CXX=$L/libcxx
    CONFIG=$ABI/config

    CXX_FLAGS="-isysroot $DARWIN_SDK_ROOT -I${libSystem}/usr/include \
      -std=c++23 -nostdinc++ -funwind-tables -fexceptions -fPIC -Os -DNDEBUG \
      -I $CONFIG -I $ABI/include -I $CXX/src -I $CXX/include \
      -D_LIBCPP_BUILDING_LIBRARY -D_LIBCXXABI_BUILDING_LIBRARY \
      -I src/Libraries/llvm-libc -I src/Libraries/llvm-libc/include"

    objs=""

    for s in ${lib.concatStringsSep " " cxxSrcs}; do
      extra=""
      # operator new/delete already come from libc++abi's stdlib_new_delete.cpp.
      [ "$s" = "new" ] && extra="-D_LIBCPP_DISABLE_NEW_DELETE_DEFINITIONS"
      o="cxx_$(echo "$s" | tr / _).o"
      ${cc} $CXX_FLAGS $extra -c "$CXX/src/$s.cpp" -o "$o"
      objs="$objs $o"
    done

    # -reexport-lc++abi, not plain -lc++abi: on Apple platforms libc++.1.dylib
    # RE-EXPORTS libc++abi, so a two-level-namespace binary that records
    # "___cxa_pure_virtual, expected in /usr/lib/libc++.1.dylib" resolves. Linking
    # it as an ordinary dependency makes the symbol reachable transitively but
    # NOT from libc++'s own namespace, and dyld refuses to launch (seen with
    # rustc: "Symbol not found: ___cxa_pure_virtual, Expected in libc++.1.dylib"
    # while nm showed it present and global in libc++abi.dylib).
    ${cc} -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libcxxabiDylib}/usr/lib \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-install_name,/usr/lib/libc++.1.dylib \
      -Wl,-fixup_chains \
      -Wl,-reexport-lc++abi -lSystem \
      -o libc++.1.dylib $objs

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    CONFIG=src/Libraries/libcxxabi/config
    mkdir -p $out/usr/lib $out/usr/include/c++/v1
    # install_name baked in at link time; no install_name_tool pass (llvm's
    # chokes on LC_DYLD_CHAINED_FIXUPS).
    cp libc++.1.dylib $out/usr/lib/
    ln -s libc++.1.dylib $out/usr/lib/libc++.dylib
    # Ship the real C++ headers so downstream ports get <vector> etc.
    cp -a src/Libraries/libcxx/include/. $out/usr/include/c++/v1/
    # __config_site and __assertion_handler are normally CMake-generated, so
    # they are not in include/ - without them every #include <version> fails
    # with "'__config_site' file not found" in consumers.
    for h in __config_site __assertion_handler; do
      [ -f "$CONFIG/$h" ] && cp "$CONFIG/$h" $out/usr/include/c++/v1/
    done
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "PureDarwin libc++.1.dylib (full C++ standard library, cross-built, layered on libc++abi.dylib)";
    platforms = platforms.unix;
  };
}
