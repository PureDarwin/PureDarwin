{ stdenv
, lib
, cmake
, ninja
, pkg-config
, python3
, glslang
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, nativeMesonTools
, vulkanTools
, vulkanHeaders
, vulkanLoader
, libX11
, libXext
, libxcb
, libXau
, libXdmcp
, libXrandr
, xorgproto
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  xDeps = [ libX11 libXext libxcb libXau libXdmcp libXrandr xorgproto ];
in
stdenv.mkDerivation {
  pname = "puredarwin-vulkan-tools";
  inherit (vulkanTools) version src;

  nativeBuildInputs = [ cmake ninja python3 glslang pkg-config ];

  postPatch = ''
    for f in cube/CMakeLists.txt vulkaninfo/CMakeLists.txt CMakeLists.txt; do
      [ -e "$f" ] || continue
      sed -i \
        -e 's/CMAKE_SYSTEM_NAME MATCHES "Linux|BSD|GNU"/CMAKE_SYSTEM_NAME MATCHES "Linux|BSD|GNU|Darwin"/g' \
        -e 's/^if(APPLE)$/if(FALSE)/' \
        -e 's/^elseif(APPLE)$/elseif(FALSE)/' \
        -e 's/if(ANDROID OR APPLE)/if(ANDROID)/' \
        "$f"
    done
    # cube's xcb/xlib loaders dlopen by Linux soname; Darwin names them
    # differently, so the library is present but never found.
    sed -i -e 's/"libxcb\.so\.1"/"libxcb.1.dylib"/' -e 's/"libxcb\.so"/"libxcb.dylib"/' cube/xcb_loader.h
    sed -i -e 's/"libX11\.so\.6"/"libX11.6.dylib"/' -e 's/"libX11\.so"/"libX11.dylib"/' cube/xlib_loader.h

    for f in cube/cube.c cube/cube.cpp; do
      [ -e "$f" ] || continue
      sed -i 's/defined(__linux__) || defined(__FreeBSD__)/defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)/' "$f"
    done
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    # xorgproto ships its .pc files under share/pkgconfig, and x11.pc Requires
    # xproto - with PKG_CONFIG_LIBDIR pinned, a missing transitive .pc fails the
    # whole query rather than just that one lookup.
    export PKG_CONFIG_PATH="${lib.concatMapStringsSep ":" (d: "${lib.getDev d}/lib/pkgconfig") xDeps}:${lib.concatMapStringsSep ":" (d: "${lib.getDev d}/share/pkgconfig") xDeps}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    commonFlags="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -I${libSystem}/usr/include -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1 ${lib.concatMapStringsSep " " (d: "-I${lib.getDev d}/include") xDeps}"
    linkFlags="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${vulkanLoader}/usr/lib ${lib.concatMapStringsSep " " (d: "-L${d}/lib") xDeps} -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,26.5,26.5 -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -lc++ -lc++abi -lSystem"

    cmake -B build -G Ninja \
      -DCMAKE_SYSTEM_NAME=Darwin \
      -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
      -DCMAKE_C_COMPILER=${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -DCMAKE_CXX_COMPILER=${darwinCrossToolchain}/bin/${targetTriple}-clang++ \
      -DCMAKE_AR=${darwinCrossToolchain}/bin/${targetTriple}-ar \
      -DCMAKE_RANLIB=${darwinCrossToolchain}/bin/${targetTriple}-ranlib \
      -DCMAKE_INSTALL_NAME_TOOL=${nativeMesonTools}/bin/install_name_tool \
      -DCMAKE_C_FLAGS="$commonFlags" \
      -DCMAKE_CXX_FLAGS="$commonFlags" \
      -DCMAKE_EXE_LINKER_FLAGS="$linkFlags" \
      -DCMAKE_SHARED_LINKER_FLAGS="$linkFlags" \
      -DCMAKE_INSTALL_PREFIX=$out/usr \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="${vulkanHeaders};${vulkanLoader}/usr;${glslang}" \
      -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
      -DBUILD_CUBE=ON \
      -DBUILD_VULKANINFO=ON \
      -DBUILD_ICD=OFF \
      -DCUBE_WSI_SELECTION=XCB \
      -DBUILD_WSI_XCB_SUPPORT=ON \
      -DBUILD_WSI_XLIB_SUPPORT=ON \
      -DBUILD_WSI_WAYLAND_SUPPORT=OFF

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

  meta = with lib; {
    description = "vulkaninfo and vkcube, cross-built for PureDarwin";
    platforms = platforms.linux;
  };
}
