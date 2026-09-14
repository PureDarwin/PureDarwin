# Cross-compilation settings shared by Nix and its dependencies: one place for
# the compiler, SDK and link flags, rendered for meson, CMake and autotools.
{ lib
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, appleSdk
, pkg-config
, cmake
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  info = import ../../lib/target-info.nix targetTriple;
  bin = "${darwinCrossToolchain}/bin/${targetTriple}";
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";

  cflagsList = [
    "-isysroot" sdk "-mmacosx-version-min=26.5" "-Qunused-arguments"
    "-U_FORTIFY_SOURCE" "-D_FORTIFY_SOURCE=0" "-fno-stack-protector"
    "-I${libSystem}/usr/include"
  ] ++ lib.optional (info.arch == "arm64") "-DXNU_PLATFORM_MacOSX=1";
  cxxflagsList = cflagsList ++ [ "-nostdinc++" "-I${libcxxDylib}/usr/include/c++/v1" ];

  # No -undefined dynamic_lookup: it makes every configure-time link test pass.
  ldflagsList = [
    "-isysroot" sdk "-mmacosx-version-min=26.5" "-fuse-ld=${nativeLd}/bin/ld" "-nostdlib"
    "-L${libSystem}/usr/lib"
    "-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib"
    "-Wl,-dylinker_install_name,/usr/lib/dyld"
    "-Wl,-platform_version,macos,26.5,26.5"
  ];
  cxxLibsList = [ "-L${libcxxDylib}/usr/lib" "-L${libcxxabiDylib}/usr/lib" "-lc++" "-lc++abi" ];

  str = lib.concatStringsSep " ";
  quote = lib.concatMapStringsSep ", " (x: "'${x}'");
in
rec {
  inherit info sdk targetTriple;

  cc = "${bin}-clang";
  cxx = "${bin}-clang++";
  ar = "${bin}-ar";
  ranlib = "${bin}-ranlib";
  strip = "${bin}-strip";

  cflags = str cflagsList;
  cxxflags = str cxxflagsList;
  ldflags = str ldflagsList;
  cxxlibs = str cxxLibsList;

  mesonCrossFile = { extraCppArgs ? [ ], extraLinkArgs ? [ ], properties ? { } }: ''
    [binaries]
    c = '${cc}'
    # Meson's prelink step runs a bare `c++ -r`, without any link args; left to
    # itself the driver adds -lc++ and a default sysroot neither of which exist.
    cpp = ['${cxx}', '-nostdlib']
    ar = '${ar}'
    ranlib = '${ranlib}'
    strip = '${strip}'
    pkg-config = '${pkg-config}/bin/pkg-config'
    # Only reads CMake package configs (toml11); nothing is built with it.
    cmake = '${cmake}/bin/cmake'

    [built-in options]
    c_args = [${quote cflagsList}]
    cpp_args = [${quote (cxxflagsList ++ extraCppArgs)}]
    c_link_args = [${quote (ldflagsList ++ extraLinkArgs ++ [ "-lSystem" ])}]
    cpp_link_args = [${quote (ldflagsList ++ extraLinkArgs ++ cxxLibsList ++ [ "-lSystem" ])}]

    [host_machine]
    system = 'darwin'
    subsystem = 'macos'
    cpu_family = '${info.mesonCpuFamily}'
    cpu = '${info.mesonCpu}'
    endian = '${info.mesonEndian}'

    [properties]
    needs_exe_wrapper = true
    ${lib.concatStrings (lib.mapAttrsToList (k: v: "${k} = '${v}'\n") properties)}
  '';

  cmakeToolchain = ''
    set(CMAKE_SYSTEM_NAME Darwin)
    set(CMAKE_SYSTEM_PROCESSOR ${info.mesonCpu})
    set(CMAKE_OSX_SYSROOT "${sdk}")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "26.5")
    set(CMAKE_C_COMPILER "${cc}")
    set(CMAKE_CXX_COMPILER "${cxx}")
    set(CMAKE_AR "${ar}")
    set(CMAKE_RANLIB "${ranlib}")
    set(CMAKE_STRIP "${strip}")
    set(CMAKE_C_FLAGS_INIT "${cflags}")
    set(CMAKE_CXX_FLAGS_INIT "${cxxflags}")
    set(CMAKE_ASM_FLAGS_INIT "${cflags}")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "${ldflags} ${cxxlibs} -lSystem")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "${ldflags} ${cxxlibs} -lSystem")
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  '';

  # Sourced by autotools builds; LIBS comes last on every link line.
  autotoolsEnv = ''
    export CC="${cc}" CXX="${cxx}" AR="${ar}" RANLIB="${ranlib}" STRIP="${strip}"
    export CFLAGS="${cflags} -O2" CXXFLAGS="${cxxflags} -O2"
    export LDFLAGS="${ldflags}" LIBS="-lSystem"
  '';
}
