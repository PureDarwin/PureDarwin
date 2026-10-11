{ stdenv
, lib
, cmake
, ninja
, python3
, darwinCrossToolchain
, nativeLd
, libSystem
, nativeMesonTools
, pkg-config
, libX11 ? null
, libxcb ? null
, libXau ? null
, libXdmcp ? null
, libXrandr ? null
, libXrender ? null
, xorgproto ? null
, corefoundation
, vulkanLoader
, vulkanHeaders
, targetTriple ? "x86_64-apple-darwin20.4"
  # headless build: no x11 wsi and no x11 libraries in the closure
, withX11 ? true
  # wayland wsi passthrough (vkCreateWaylandSurfaceKHR), the loader only needs the client headers
, wayland ? null
, appleSdk
}:

let
  xDeps = lib.optionals withX11 [ libX11 libxcb libXau libXdmcp libXrandr libXrender xorgproto ];
  wsiX11 = if withX11 then "ON" else "OFF";
  wsiWayland = if wayland != null then "ON" else "OFF";
  wlInclude = lib.optionalString (wayland != null) "-I${lib.getDev wayland}/include";
  cmakeCpu = if lib.hasPrefix "arm64-" targetTriple then "aarch64" else "x86_64";
in
stdenv.mkDerivation {
  pname = "puredarwin-vulkan-loader";
  inherit (vulkanLoader) version src;

  nativeBuildInputs = [ cmake ninja python3 pkg-config ];

  # elseif(APPLE) fires before the X11 branch and sets only the Metal platforms, so the X11 and Wayland surface extensions would be compiled out.
  # PureDarwin presents through X11 and Wayland and has Metal and CAMetalLayer, so the Darwin build takes the Unix branch and adds VK_EXT_metal_surface and VK_MVK_macos_surface to it: Mac apps (MoltenVK's vkcube, volk) dlopen libvulkan.dylib and find this one
  postPatch = ''
    sed -i \
      -e 's/^elseif(APPLE)$/elseif(FALSE)/' \
      -e 's/CMAKE_SYSTEM_NAME MATCHES "Linux|BSD|DragonFly|GNU|CYGWIN"/CMAKE_SYSTEM_NAME MATCHES "Linux|BSD|DragonFly|GNU|CYGWIN|Darwin"/' \
      -e '/CMAKE_SYSTEM_NAME MATCHES "Linux|BSD|DragonFly|GNU|CYGWIN|Darwin"/a\    if(APPLE)\n        target_compile_definitions(platform_wsi INTERFACE VK_USE_PLATFORM_METAL_EXT VK_USE_PLATFORM_MACOS_MVK)\n    endif()' \
      CMakeLists.txt
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PKG_CONFIG_PATH="${lib.concatMapStringsSep ":" (d: "${lib.getDev d}/lib/pkgconfig") xDeps}:${lib.concatMapStringsSep ":" (d: "${lib.getDev d}/share/pkgconfig") xDeps}${lib.optionalString (wayland != null) ":${lib.getDev wayland}/lib/pkgconfig"}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    commonFlags="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -I${libSystem}/usr/include ${lib.concatMapStringsSep " " (d: "-I${lib.getDev d}/include") xDeps} ${wlInclude}"
    linkFlags="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib ${lib.concatMapStringsSep " " (d: "-L${d}/lib") xDeps} -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,26.5,26.5 -F${corefoundation}/System/Library/Frameworks -lSystem"

    cmake -B build -G Ninja \
      -DCMAKE_SYSTEM_NAME=Darwin \
      -DCMAKE_SYSTEM_PROCESSOR=${cmakeCpu} \
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
      -DCMAKE_INSTALL_NAME_DIR=/usr/lib \
      -DCMAKE_BUILD_TYPE=Release \
      -DVULKAN_HEADERS_INSTALL_DIR=${vulkanHeaders} \
      -DBUILD_TESTS=OFF \
      -DBUILD_WSI_XCB_SUPPORT=${wsiX11} \
      -DBUILD_WSI_XLIB_SUPPORT=${wsiX11} \
      -DBUILD_WSI_WAYLAND_SUPPORT=${wsiWayland} \
      -DAPPLE_STATIC_LOADER=OFF \
      -DUSE_GAS=OFF \
      -DUSE_MASM=OFF

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

    INT="${nativeMesonTools}/bin/install_name_tool"
    for dylib in "$out"/usr/lib/*.dylib; do
      [ -L "$dylib" ] && continue
      [ -e "$dylib" ] || continue
      "$INT" -id "/usr/lib/$(basename "$dylib")" "$dylib" 2>/dev/null || true
    done

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Vulkan loader, cross-built for PureDarwin (dispatches to the lavapipe ICD)";
    platforms = platforms.unix;
  };
}
