{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, fastfetch
, cmake
, ninja
, pkg-config
, corefoundation
, foundation
, libobjc
, iokit
, openglFramework
, mesa
, glu
, libX11 ? null
, libXext ? null
, libxcb ? null
, libXau ? null
, libXdmcp ? null
  # The Pi has no GL stack worth shipping: linking OpenGL.framework drags Mesa
  # and llvmpipe into a 1GB root filesystem for one report line.
, withOpenGL ? true
  # Wayland-only image: no X11 libraries exist to name here.
, withX11 ? true
, appleSdk
}:

let

  glFrameworkFlag  = lib.optionalString withOpenGL "-F${openglFramework}/System/Library/Frameworks ";
  glIncludeFlag    = lib.optionalString withOpenGL "-I${mesa}/usr/include ";
  xDylibFileFlags  = lib.optionalString withX11
    "-Wl,-dylib_file,/usr/lib/libX11.6.dylib:${libX11}/lib/libX11.6.dylib -Wl,-dylib_file,/usr/lib/libXext.6.dylib:${libXext}/lib/libXext.6.dylib -Wl,-dylib_file,/usr/lib/libxcb.1.1.0.dylib:${libxcb}/lib/libxcb.1.1.0.dylib -Wl,-dylib_file,/usr/lib/libXau.6.dylib:${libXau}/lib/libXau.6.dylib -Wl,-dylib_file,/usr/lib/libXdmcp.6.dylib:${libXdmcp}/lib/libXdmcp.6.dylib ";
  # OpenGL.framework re-exports both, so the linker has to be able to find
  # each one behind its runtime install name.
  glDylibFileFlag  = lib.optionalString withOpenGL "-Wl,-dylib_file,/usr/lib/libGL.1.dylib:${mesa}/usr/lib/libGL.1.dylib -Wl,-dylib_file,/usr/lib/libGLU.1.dylib:${glu}/usr/lib/libGLU.1.dylib ";
  glLinkFlag       = lib.optionalString withOpenGL "-framework OpenGL ";
in
stdenv.mkDerivation {
  pname = if withOpenGL then "puredarwin-fastfetch" else "puredarwin-fastfetch-nogl";
  inherit (fastfetch) version;
  src = fastfetch.src;

  nativeBuildInputs = [ cmake ninja pkg-config ];

  cmakeFlags = [
    "-DCMAKE_TOOLCHAIN_FILE=${../../../cmake/nix-toolchain.cmake}"
    "-DNIX_DARWIN_TOOLCHAIN_DIR=${darwinCrossToolchain}/bin"
    # Without this the toolchain file falls back to the x86 triple and looks for
    # an x86_64-prefixed clang inside the arm64 toolchain.
    "-DNIX_DARWIN_HOST=${targetTriple}"
    "-DHAVE_MALLOC_USABLE_SIZE=OFF"
    "-DHAVE_PIPE2=OFF"
    # memrchr doesn't exist on real macOS either - same false-positive risk.
    "-DHAVE_MEMRCHR=OFF"
    "-DBUILD_FLASHFETCH=OFF"
    "-DBUILD_TESTS=OFF"
    "-DENABLE_SYSTEM_YYJSON=OFF"
    "-DENABLE_SYSTEM_JSONC=OFF"
    "-DENABLE_DIRECTX_HEADERS=OFF"
    "-DENABLE_IMAGEMAGICK6=OFF"
    "-DENABLE_IMAGEMAGICK7=OFF"
    "-DENABLE_CHAFA=OFF"
    "-DENABLE_SQLITE3=OFF"
    "-DENABLE_LIBZFS=OFF"
    "-DENABLE_PULSE=OFF"
    "-DENABLE_VA=OFF"
    "-DENABLE_VDPAU=OFF"
    "-DENABLE_DDCUTIL=OFF"
    "-DENABLE_DBUS=OFF"
    "-DENABLE_EET=OFF"
    "-DENABLE_ELF=OFF"
    "-DENABLE_GIO=OFF"
    "-DENABLE_DCONF=OFF"
    "-DENABLE_ZLIB=OFF"
    "-DENABLE_OPENCL=OFF"
    "-DENABLE_EGL=OFF"
    "-DENABLE_GLX=OFF"
    "-DENABLE_RPM=OFF"
    "-DENABLE_DRM=OFF"
    "-DENABLE_DRM_AMDGPU=OFF"
    "-DENABLE_VULKAN=OFF"
    "-DENABLE_WAYLAND=OFF"
    "-DENABLE_XCB_RANDR=OFF"
    "-DENABLE_XRANDR=OFF"
    "-DENABLE_XFCONF=OFF"
    "-DENABLE_X11=OFF"
    "-DENABLE_LIBCJSON=OFF"
    "-DENABLE_THREADS=OFF"
  ];

  preConfigure = ''
    # keyboard, mouse and gamepad are the IOKit/hid users; there is no
    # user-space IOHIDLib here, so they go the same way as the rest.
    for base in camera wifi bluetooth bluetoothradio cursor font media wallpaper wm wmtheme physicalmemory brightness poweradapter keyboard mouse gamepad; do
      sed -i "s#src/detection/$base/''${base}_apple\.[mc]#src/detection/$base/''${base}_nosupport.c#" CMakeLists.txt
    done

    # sound.h declares ffDetectSound(FFSoundOptions*, FFlist*) in this
    # fastfetch version; upstream's sound_nosupport.c is stale/mismatched
    # against that signature, so write a matching stub instead of using it.
    cat > src/detection/sound/sound_nosupport.c <<'SNDEOF'
#include "sound.h"

const char* ffDetectSound(FF_A_UNUSED FFSoundOptions* options, FF_A_UNUSED FFlist* devices)
{
    return "Not supported on this platform";
}
SNDEOF
    sed -i 's#src/detection/sound/sound_apple\.[mc]#src/detection/sound/sound_nosupport.c#' CMakeLists.txt

    sed -i 's#src/detection/dns/dns_apple\.c#src/detection/dns/dns_linux.c#' CMakeLists.txt

    # KextManager is part of IOKitUser's kext.subproj, which PureDarwin does
    # not have; kextstat's job has no user-space API here yet.
    sed -i 's#src/common/impl/kmod_apple\.c#src/common/impl/kmod_nosupport.c#' CMakeLists.txt

    # gpu_apple.c wants IOGraphicsLib.h and physicaldisk_apple.c the NVMe SMART
    # library; neither is in IOKitUser yet. The gpu_apple.m stub below stays -
    # gpu_nosupport.c only defines ffDetectGPUImpl, not ffGpuDetectDriverVersion.
    sed -i \
      -e 's#src/detection/gpu/gpu_apple\.c#src/detection/gpu/gpu_nosupport.c#' \
      -e 's#src/detection/physicaldisk/physicaldisk_apple\.c#src/detection/physicaldisk/physicaldisk_nosupport.c#' \
      CMakeLists.txt

    cat > src/detection/displayserver/displayserver_nosupport.c <<'DSEOF'
#include "displayserver.h"

void ffConnectDisplayServerImpl(FFDisplayServerResult* ds)
{
    (void) ds;
}
DSEOF
    sed -i 's#src/detection/displayserver/displayserver_apple\.c#src/detection/displayserver/displayserver_nosupport.c#' CMakeLists.txt

    # opencl.c auto-enables real OpenCL.framework usage on its own, hardcoded
    # "#if !defined(FF_HAVE_OPENCL) && defined(__APPLE__) &&
    # defined(MAC_OS_X_VERSION_10_15)" at the top of the file - this
    # completely bypasses the ENABLE_OPENCL=OFF cmake flag on any Apple
    # target with a deployment min >= 10.15 (ours is 11.0)
    cat > src/detection/opencl/opencl.c <<'CLEOF'
#include "opencl.h"

FFOpenCLResult* ffDetectOpenCL(void) {
    static FFOpenCLResult result;
    static bool initialized;

    if (!initialized) {
        initialized = true;
        ffStrbufInit(&result.version);
        ffStrbufInit(&result.name);
        ffStrbufInit(&result.vendor);
        ffListInit(&result.gpus);
        result.error = "fastfetch was compiled without OpenCL support";
    }

    return &result;
}
CLEOF

    cat > src/detection/terminalfont/terminalfont_nosupport.c <<'TFEOF'
#include "terminalfont.h"
#include "detection/terminalshell/terminalshell.h"

bool ffDetectTerminalFontPlatform(const FFTerminalResult* terminal, FFTerminalFontResult* terminalFont)
{
    (void) terminal;
    (void) terminalFont;
    return false;
}
TFEOF
    sed -i 's#src/detection/terminalfont/terminalfont_apple\.m#src/detection/terminalfont/terminalfont_nosupport.c#' CMakeLists.txt
    sed -i '/src\/common\/apple\/osascript\.m/d' CMakeLists.txt

    cat > src/common/apple/version.m <<'VEREOF'
#include "common/apple/version.h"

bool ffGetAppNameAndVersion(const char* exePath, FFstrbuf* retName, FFstrbuf* retVersion)
{
    (void) exePath;
    (void) retName;
    (void) retVersion;
    return false;
}
VEREOF

    # os_apple.m reads /System/Library/CoreServices/SystemVersion.plist through
    # NSDictionary - which we now have, bridged over CFPropertyList - and we ship
    # that plist, so use the real implementation instead of a hardcoded name. It
    # assumes the product is always macOS in two places; the plist supplies the
    # real name, and the id should match it.
    sed -i \
      -e 's/ffStrbufSetStatic(&os->id, "macos");/ffStrbufSetStatic(\&os->id, "puredarwin");/' \
      -e 's/ffStrbufSetStatic(&os->name, "macOS");/ffStrbufSetStatic(\&os->name, "PureDarwin");/' \
      src/detection/os/os_apple.m

    # With os->id reported as "puredarwin", fastfetch's own logo autodetection
    # picks a builtin of that name - so ship Hexley as one rather than making
    # every invocation pass --logo-type/--logo-color flags. Files in
    # src/logo/ascii are globbed into FASTFETCH_DATATEXT_LOGO_<NAME> by CMake.
    cp ${./puredarwin-logo.txt} src/logo/ascii/puredarwin.txt
    cat > pd-logo-entry.c <<'LOGOEOF'
    // PureDarwin
    {
        .names = { "puredarwin" },
        .lines = FASTFETCH_DATATEXT_LOGO_PUREDARWIN,
        .colors = {
            FF_COLOR_FG_RED,
            FF_COLOR_FG_DEFAULT,
        },
        .colorKeys = FF_COLOR_FG_RED,
        .colorTitle = FF_COLOR_FG_RED,
    },
LOGOEOF
    # Builtins are bucketed by first letter and looked up as
    # ffLogoBuiltins[toupper(name[0]) - 'A'], so this has to land in P[].
    sed -i -e '/^    \/\/ PacBSD$/e cat pd-logo-entry.c' src/logo/builtin.c
    rm pd-logo-entry.c

    ${lib.optionalString (!withOpenGL) ''
    # opengl_apple.c is in the Apple source list unconditionally - there is no
    # ENABLE_OPENGL flag - so replace it the same way the other _apple files
    # above are replaced.
    cat > src/detection/opengl/opengl_nosupport.c <<'GLEOF'
#include "opengl.h"

const char* ffDetectOpenGL(FF_A_UNUSED FFOpenGLOptions* options, FF_A_UNUSED FFOpenGLResult* result)
{
    return "fastfetch was compiled without OpenGL support";
}
GLEOF
    sed -i 's#src/detection/opengl/opengl_apple\.c#src/detection/opengl/opengl_nosupport.c#' CMakeLists.txt
    ''}

    # sysinfo.pageSize is only ever multiplied by vm_statistics64 counters,
    # which are in *kernel* pages. hw.pagesize is the task map's allocation
    # granularity - mach_loader.c pins every 64-bit arm64 map to 16K - so on a
    # kernel whose PAGE_SIZE is not 16K the two disagree and the memory line
    # underflows to 16 EiB. host_page_size() is what vm_stat(1) uses.
    sed -i \
      -e 's|^#include "FFPlatform_private.h"$|#include "FFPlatform_private.h"\n#ifdef __APPLE__\n#include <mach/mach.h>\n#endif|' \
      -e 's|^    sysctl((int\[\]) { CTL_HW, HW_PAGESIZE }, 2, \&info->pageSize, \&length, NULL, 0);$|&\n#ifdef __APPLE__\n    { vm_size_t hps = 0; if (host_page_size(mach_host_self(), \&hps) == KERN_SUCCESS \&\& hps) info->pageSize = (uint32_t) hps; }\n#endif|' \
      src/common/impl/FFPlatform_unix.c

    cat > src/detection/gpu/gpu_apple.m <<'GPUEOF'
#include "gpu.h"

const char* ffGpuDetectDriverVersion(FFlist* gpus)
{
    (void) gpus;
    return "Driver version detection not supported";
}

const char* ffGpuDetectMetal(FFlist* gpus)
{
    (void) gpus;
    return "Metal API is not supported here";
}
GPUEOF

    awk '
      /-framework AVFoundation/ { start=NR-2 }
      { lines[NR]=$0 }
      /^    \)$/ { if (start && !stop) stop=NR }
      END {
        for (i=1; i<=NR; i++) {
          if (start && i>=start && i<=stop) continue
          print lines[i]
        }
      }
    ' CMakeLists.txt > CMakeLists.txt.new
    mv CMakeLists.txt.new CMakeLists.txt

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export NIX_DARWIN_TOOLCHAIN_DIR="${darwinCrossToolchain}/bin"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -F$DARWIN_SDK_ROOT/System/Library/Frameworks ${glFrameworkFlag}-fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${foundation}/usr/lib -L${libobjc}/usr/lib -L${iokit}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup ${glDylibFileFlag}${xDylibFileFlags}${glLinkFlag}-lIOKitCF -lCoreFoundation -lFoundation -lobjc -lSystem"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT ${glFrameworkFlag}${glIncludeFlag}-I${libSystem}/usr/include -I${corefoundation}/include -I${foundation}/usr/include -I${libobjc}/usr/include -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
  '';

  dontFixup = true;
  dontStrip = true;

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    cp fastfetch $out/bin/fastfetch 2>/dev/null || find . -maxdepth 2 -name fastfetch -type f -exec cp {} $out/bin/fastfetch \;
    runHook postInstall
  '';

  meta = with lib; {
    platforms = platforms.linux;
  };
}