{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, fastfetch
, cmake
, ninja
, pkg-config
, corefoundation
, iokit
, openglFramework
, mesa
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
in
stdenv.mkDerivation {
  pname = "puredarwin-fastfetch";
  inherit (fastfetch) version;
  src = fastfetch.src;

  nativeBuildInputs = [ cmake ninja pkg-config ];

  cmakeFlags = [
    "-DCMAKE_TOOLCHAIN_FILE=${../../cmake/nix-toolchain.cmake}"
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
    for base in camera wifi bluetooth bluetoothradio cursor font media wallpaper wm wmtheme physicalmemory brightness poweradapter; do
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

    # Upstream's opengl_apple.c is used as-is: PureDarwin has a real
    # OpenGL.framework (CGL over Mesa's OSMesa), so CGLChoosePixelFormat/
    # CGLCreateContext/glGetString all resolve and fastfetch reports Mesa's
    # actual GL version and renderer. The -framework OpenGL link line upstream
    # already emits sits inside the Apple framework block that the awk pass
    # below strips wholesale, so it is re-added to LDFLAGS instead.

    sed -i 's#src/detection/dns/dns_apple\.c#src/detection/dns/dns_linux.c#' CMakeLists.txt

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

    # os_apple.m uses NSProcessInfo/Foundation for OS name+version and has no
    # upstream _nosupport.c - unlike the peripheral modules above, OS
    # identification is core to what fastfetch is for, so don't just stub it
    # to nothing. FFPlatform_unix.c (already compiled unconditionally here)
    # populates instance.state.platform.sysinfo via uname() for every
    # platform - reuse that instead of Foundation, same pattern os_obsd.c
    # already uses for OpenBSD.
    cat > src/detection/os/os_nosupport.c <<'OSEOF'
#include "os.h"

void ffDetectOSImpl(FFOSResult* os)
{
    ffStrbufSetStatic(&os->name, "PureDarwin");
    ffStrbufSetStatic(&os->prettyName, "PureDarwin");
    ffStrbufSet(&os->version, &instance.state.platform.sysinfo.release);
    ffStrbufSet(&os->versionID, &instance.state.platform.sysinfo.release);
    ffStrbufSetStatic(&os->id, "puredarwin");
}
OSEOF
    sed -i 's#src/detection/os/os_apple\.m#src/detection/os/os_nosupport.c#' CMakeLists.txt

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
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export NIX_DARWIN_TOOLCHAIN_DIR="${darwinCrossToolchain}/bin"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -F$DARWIN_SDK_ROOT/System/Library/Frameworks -F${openglFramework}/System/Library/Frameworks -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${corefoundation}/usr/lib -L${iokit}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -Wl,-dylib_file,/usr/lib/libOSMesa.8.dylib:${mesa}/usr/lib/libOSMesa.8.dylib -framework OpenGL -lIOKitCF -lCoreFoundation -lSystem"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -F${openglFramework}/System/Library/Frameworks -I${mesa}/usr/include -I${libSystem}/usr/include -I${corefoundation}/include -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
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