{ stdenv
, lib
, cmake
, ninja
, pkg-config
, python3
, perl
, ruby
, gperf
, unifdef
, glibNative
, libxml2Native
, waylandScanner
, nativeMesonTools
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, webkitgtk
, deps
, icu
, mesa
, targetTriple ? "x86_64-apple-darwin20.4"
# Requires C++23, needs a port and a half
, configureOnly ? false
, appleSdk
}:

let
  depPcPaths = map lib.getDev deps;
in
stdenv.mkDerivation {
  pname = "puredarwin-webkitgtk";
  inherit (webkitgtk) version src;

  nativeBuildInputs = [
    cmake ninja pkg-config python3 perl ruby gperf unifdef glibNative
    waylandScanner
  ];
  buildInputs = deps;

  postPatch = ''
    patchShebangs .

    patch -p1 < ${./webkitgtk-gtk-port-on-darwin.patch}
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeMesonTools}/bin:${glibNative}/bin:${libxml2Native}/bin:$PATH"
    export XMLLINT="${libxml2Native}/bin/xmllint"

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" depPcPaths}:${lib.makeSearchPath "share/pkgconfig" depPcPaths}:${lib.makeSearchPath "usr/lib/pkgconfig" deps}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    # ENABLE_JIT=OFF / ENABLE_C_LOOP=ON: JSC's x86_64 JIT is Apple's own
    # OS(DARWIN) code using HAVE(REMAP_JIT)/vm_remap, unverified against this
    # XNU - and ExecutableAllocator.cpp's OS(DARWIN) path includes
    # <wtf/spi/cocoa/MachVMSPI.h>, which the GTK release tarball does not ship
    # at all (it strips spi/cocoa/ but keeps the include). CLoop is JSC's
    # portable bytecode interpreter: slower, but it takes the whole JIT
    # question off the critical path. WebKitFeatures.cmake:266-269 declares
    # exactly three things as conflicting with C_LOOP - JIT, SAMPLING_PROFILER
    # and WEBASSEMBLY - and all three are off below.
    commonFlags="-DKERN_NOT_FOUND=56 -DUSE_TZONE_MALLOC=0 -DWL_EGL_PLATFORM=1 -I${mesa}/usr/include -isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -Qunused-arguments -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -I${libSystem}/usr/include ${lib.concatMapStringsSep " " (dep: "-I${lib.getDev dep}/include") deps}"
    linkFlags="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") deps} -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,11.0,11.5 -lc++ -lc++abi -lsharpyuv -lSystem"

    cmake -B build -G Ninja \
      -DCMAKE_SYSTEM_NAME=Darwin \
      -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
      -DCMAKE_C_COMPILER=${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -DCMAKE_CXX_COMPILER=${darwinCrossToolchain}/bin/${targetTriple}-clang++ \
      -DCMAKE_AR=${darwinCrossToolchain}/bin/${targetTriple}-ar \
      -DCMAKE_RANLIB=${darwinCrossToolchain}/bin/${targetTriple}-ranlib \
      -DCMAKE_INSTALL_NAME_TOOL=${nativeMesonTools}/bin/install_name_tool \
      -DCMAKE_C_FLAGS="$commonFlags" \
      -DCMAKE_CXX_FLAGS="$commonFlags -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1" \
      -DCMAKE_EXE_LINKER_FLAGS="$linkFlags" \
      -DCMAKE_SHARED_LINKER_FLAGS="$linkFlags" \
      -DCMAKE_INSTALL_PREFIX=$out \
      -DCMAKE_INSTALL_NAME_DIR=/lib \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CROSSCOMPILING=ON \
      -DCMAKE_FIND_ROOT_PATH="${lib.concatStringsSep ";" (lib.concatMap (d: [ "${d}" "${d}/usr" ]) deps)}" \
      -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
      -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
      -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
      -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
      -DPORT=GTK \
      -DWAYLAND_SCANNER=${waylandScanner}/bin/wayland-scanner \
      -DUSE_APPLE_ICU=OFF \
      -DUSE_GTK4=OFF \
      -DENABLE_X11_TARGET=OFF \
      -DENABLE_WAYLAND_TARGET=ON \
      -DENABLE_INTROSPECTION=OFF \
      -DENABLE_JOURNALD_LOG=OFF \
      -DENABLE_DOCUMENTATION=OFF \
      -DENABLE_BUBBLEWRAP_SANDBOX=OFF \
      -DENABLE_JIT=OFF \
      -DENABLE_C_LOOP=ON \
      -DENABLE_SAMPLING_PROFILER=OFF \
      -DENABLE_WEBASSEMBLY=OFF \
      -DUSE_GSTREAMER=OFF \
      -DENABLE_VIDEO=OFF \
      -DENABLE_WEB_AUDIO=OFF \
      -DENABLE_WEB_CODECS=OFF \
      -DENABLE_SPEECH_SYNTHESIS=OFF \
      -DENABLE_MEDIA_STREAM=OFF \
      -DENABLE_WEB_RTC=OFF \
      -DENABLE_GAMEPAD=OFF \
      -DENABLE_SPELLCHECK=OFF \
      -DENABLE_WEBGL=OFF \
      -DENABLE_XSLT=OFF \
      -DENABLE_GPU_PROCESS=OFF \
      -DENABLE_WEBXR=OFF \
      -DENABLE_WK_WEB_EXTENSIONS=OFF \
      -DUSE_AVIF=OFF \
      -DUSE_JPEGXL=OFF \
      -DUSE_LCMS=OFF \
      -DUSE_WOFF2=OFF \
      -DUSE_GBM=OFF \
      -DUSE_LIBDRM=OFF \
      -DUSE_LIBBACKTRACE=OFF \
      -DUSE_LIBSECRET=OFF \
      -DUSE_LIBHYPHEN=OFF \
      -DUSE_LIBRICE=OFF \
      -DUSE_SYSPROF_CAPTURE=OFF \
      -DENABLE_API_TESTS=OFF \
      -DENABLE_LAYOUT_TESTS=OFF \
      -DENABLE_MINIBROWSER=OFF

    runHook postConfigure
  '';

  dontBuild = configureOnly;

  buildPhase = lib.optionalString (!configureOnly) ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = if configureOnly then ''
    runHook preInstall
    mkdir -p $out/share/webkit-configure
    cp configure-output.log $out/share/webkit-configure/
    cp build/CMakeCache.txt $out/share/webkit-configure/ 2>/dev/null || true
    runHook postInstall
  '' else ''
    runHook preInstall
    ninja -C build install
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "WebKitGTK for PureDarwin - configure probe (stage 2 of the port)";
    platforms = platforms.linux;
  };
}
