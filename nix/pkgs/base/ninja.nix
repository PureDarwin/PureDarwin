{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, ninja
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let

  # configure.py's own list, minus the Windows and browse sources. Compiling
  # these directly avoids the bootstrap, which builds ninja with the host
  # compiler and then runs it - impossible when the target is not the host.
  sources = [
    "build" "build_log" "clean" "clparser" "debug_flags" "depfile_parser"
    "deps_log" "disk_interface" "dyndep" "dyndep_parser" "edit_distance"
    "elide_middle" "eval_env" "graph" "graphviz" "jobserver" "json"
    "lexer" "line_printer" "manifest_parser" "metrics" "missing_deps"
    "parser" "real_command_runner" "state" "status_printer"
    "string_piece_util" "util" "version"
    "jobserver-posix" "subprocess-posix"
    "ninja"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-ninja";
  inherit (ninja) version src;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    CXX="${darwinCrossToolchain}/bin/${targetTriple}-clang++"
    CXXFLAGS="-isysroot $DARWIN_SDK_ROOT -std=c++11 -O2 -Qunused-arguments \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1 -I${libSystem}/usr/include \
      -DNDEBUG -DNINJA_PYTHON=\"python3\" -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-dylinker_install_name,/usr/lib/dyld \
      -Wl,-platform_version,macos,26.5,26.5 -lc++ -lc++abi -lSystem"

    mkdir -p obj
    for name in ${lib.concatStringsSep " " sources}; do
      $CXX $CXXFLAGS -c "src/$name.cc" -o "obj/$name.o"
    done
    $CXX $LDFLAGS obj/*.o -o ninja

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 ninja "$out/usr/bin/ninja"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Ninja build tool, cross-built to run on PureDarwin";
    homepage = "https://ninja-build.org/";
    license = licenses.asl20;
    platforms = platforms.linux;
  };
}
