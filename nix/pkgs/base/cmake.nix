{ stdenv
, lib
, cmake
, ninja
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, libcurlDylib
, corefoundation
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  targetInfo = import ../../lib/target-info.nix targetTriple;
in
stdenv.mkDerivation {
  pname = "puredarwin-cmake";
  inherit (cmake) version src;

  nativeBuildInputs = [ cmake ninja ];

  # There is no ApplicationServices here; it is only used to locate an Xcode
  # install, and every use of it is behind HAVE_APPLICATION_SERVICES.
  postPatch = ''
    substituteInPlace Source/cmGlobalXCodeGenerator.cxx \
      --replace-fail '#  if !TARGET_OS_IPHONE' '#  if 0'
    # CoreServices went with it: LaunchServices was the only thing wanted from
    # it, and CPackLib already gates its own use on a header check.
    substituteInPlace Source/CMakeLists.txt \
      --replace-fail 'target_link_libraries(CMakeLib PUBLIC "-framework CoreServices")' ""
    # The SDK now publishes CoreServices, so this check passes - but PureDarwin's
    # CoreServices is only the Multiprocessing entry points, not CarbonCore's
    # LocaleStringToLangAndRegionCodes. cmake has a supported fallback for the
    # header being absent, so take it.
    substituteInPlace Source/CMakeLists.txt \
      --replace-fail 'check_include_file("CoreServices/CoreServices.h" HAVE_CoreServices)' \
                     'set(HAVE_CoreServices 0)'
    # libarchive links CoreServices on every APPLE build but references no
    # symbol from it, so ctest picked it up transitively for nothing.
    substituteInPlace Utilities/cmlibarchive/CMakeLists.txt \
      --replace-fail 'LIST(APPEND ADDITIONAL_LIBS "-framework CoreServices")' ""
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    cat > puredarwin-toolchain.cmake <<EOF
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR ${targetInfo.mesonCpu})
set(CMAKE_OSX_SYSROOT "$DARWIN_SDK_ROOT")
set(CMAKE_OSX_DEPLOYMENT_TARGET "26.5")

set(CMAKE_C_COMPILER "${darwinCrossToolchain}/bin/${targetTriple}-clang")
set(CMAKE_CXX_COMPILER "${darwinCrossToolchain}/bin/${targetTriple}-clang++")
set(CMAKE_AR "${darwinCrossToolchain}/bin/${targetTriple}-ar")
set(CMAKE_RANLIB "${darwinCrossToolchain}/bin/${targetTriple}-ranlib")
set(CMAKE_STRIP "${darwinCrossToolchain}/bin/${targetTriple}-strip")
set(CMAKE_INSTALL_NAME_TOOL "${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool")

set(_pd_common "-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -I${libSystem}/usr/include")
set(CMAKE_C_FLAGS_INIT "\''${_pd_common}")
set(CMAKE_CXX_FLAGS_INIT "\''${_pd_common} -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 -F${corefoundation}/System/Library/Frameworks -lc++ -lc++abi -lSystem")

# CMake defaults to the system curl on APPLE, and PureDarwin does ship a real
# /usr/lib/libcurl.4.dylib, so point find_package(CURL) at it.
set(CMAKE_FIND_ROOT_PATH "$DARWIN_SDK_ROOT" "${libSystem}" "${libcurlDylib}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF

    # KWSys probes several libc features with try_run, which cannot run a
    # Darwin binary here. These are the answers for this target.
    cmake -S . -B build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/puredarwin-toolchain.cmake" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DBUILD_TESTING=OFF \
      -DBUILD_CursesDialog=OFF \
      -DBUILD_QtDialog=OFF \
      -DCMAKE_USE_OPENSSL=OFF \
      -DCMake_BUILD_LTO=OFF \
      -DKWSYS_LFS_WORKS=0 \
      -DKWSYS_CXX_HAS_SETENV=1 \
      -DKWSYS_CXX_HAS_UNSETENV=1 \
      -DKWSYS_CXX_HAS_ENVIRON_IN_STDLIB_H=1 \
      -DKWSYS_CXX_HAS_UTIMENSAT=1 \
      -DKWSYS_CXX_HAS_UTIMES=1 \
      -DKWSYS_STL_HAS_WSTRING=1 \
      -DHAVE_POLL_FINE_EXITCODE=0 \
      -DENABLE_ACL=OFF \
      -DHAVE_SYS_ACL_H=0 \
      -DHAVE_COPYFILE_H=0

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    DESTDIR="$out" ninja -C build install
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "CMake, cross-built to run on PureDarwin";
    homepage = "https://cmake.org/";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
