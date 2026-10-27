{ stdenv
, lib
, pkg-config
, gnumake
, flex
, bison
, darwinCrossToolchain
, nativeLd
, libSystem
, wine
, wineTools
, mingwGcc
, mingwBintools
, mingwGcc32
, mingwBintools32
, python3
, libX11
, libxcb
, libXau
, libXdmcp
, libXext
, libXrender
, libXfixes
, libXi
, libXcursor
, libXrandr
, xorgproto
, freetype
, fontconfig
, expat
, gnutls
, mesa
, coreservices
, security
, diskArbitration
, systemConfiguration
, iokit
, corefoundation
, perl
, wayland
, waylandProtocols
, waylandScanner
, xkbcommon
, libxml2
  # PD's own frameworks, for winemac.drv and winecoreaudio.drv
, libobjc ? null
, foundation ? null
, appkit ? null
, carbon ? null
, corevideo ? null
, quartzcore ? null
, coregraphics ? null
, coretext ? null
, applicationservices ? null
, audiotoolbox ? null
, appFrameworks ? null
, iosurface ? null
, metalObjc ? null
, openglFramework ? null
, onyx2d ? null
, windowserver ? null
, cocoa ? null
, coredata ? null
, glu ? null
, targetTriple ? "x86_64-apple-darwin20.4"
, targetArch ? "x86_64"
  # aarch64-w64-mingw32 clang wrapper. Required when targetArch is "arm64" and
  # unused otherwise: the native half of Wine's PE side must be ARM64 COFF, and
  # GNU mingw-w64's gcc cannot target aarch64 at all.
, mingwAarch64Cc ? null
  # Same clang under arm64ec-w64-mingw32-* names, for the ARM64EC PE half.
, mingwArm64ecCc ? null
  # FEX's ARM64 WoW64 CPU backend, installed beside Wine's ARM64 PE modules.
, fexWow64 ? null
, appleSdk
  # Build Wine the way it is built for stock macOS: no preloader, a 13.0 minimum and Apple's
  # framework install names, so the same binaries run on a Mac and on PureDarwin.
, stockMac ? false
}:

assert targetArch == "arm64" -> mingwAarch64Cc != null;
assert targetArch == "arm64" -> fexWow64 != null;

let
  isArm64 = targetArch == "arm64";
  # wine-tools.nix configures for the build machine's own PE arch.
  toolsPeArch = if stdenv.buildPlatform.isAarch64 then "aarch64" else "x86_64";
  peArchs = if isArm64 then "aarch64,x86_64" else "i386,x86_64";

  # The PE compilers. Both run on the build host and emit Windows binaries, so
  # neither ever touches PureDarwin.
  peCompilers =
    if isArm64
    then [ mingwAarch64Cc mingwGcc ]
    else [ mingwGcc mingwGcc32 ];

  xDeps = [
    xorgproto libX11 libxcb libXau libXdmcp libXext
    libXrender libXfixes libXi libXcursor libXrandr
  ];
  waylandDeps = [ wayland waylandProtocols xkbcommon ];
  # winemac.drv and winecoreaudio.drv build against PD's own frameworks when they are passed in,
  # otherwise the header caches are seeded to no so only the X11 driver is chosen
  enableMac = appkit != null;
  macFrameworkOverrides = lib.concatStringsSep " " ([
    "ac_cv_header_OpenAL_al_h=no"
  ] ++ lib.optionals (!enableMac) [
    "ac_cv_header_AppKit_AppKit_h=no"
    "ac_cv_header_Metal_Metal_h=no"
    "ac_cv_header_CoreGraphics_CoreGraphics_h=no"
    "ac_cv_header_ApplicationServices_ApplicationServices_h=no"
    "ac_cv_header_Carbon_Carbon_h=no"
    "ac_cv_header_QuartzCore_QuartzCore_h=no"
    "ac_cv_header_CoreAudio_CoreAudio_h=no"
    "ac_cv_header_AudioToolbox_AudioToolbox_h=no"
    "ac_cv_header_AudioUnit_AudioUnit_h=no"
    "ac_cv_header_IOKit_IOKit_h=no"
    "ac_cv_header_Security_Security_h=no"
  ]);
  # every framework winemac.drv and winecoreaudio.drv name, from the PD package that builds it
  pdFrameworkPackages = lib.optionals enableMac [
    foundation appkit carbon corevideo quartzcore coregraphics coretext applicationservices
    audiotoolbox appFrameworks iosurface metalObjc openglFramework cocoa coredata
  ];
  pdHeaderDirs = map (p: "-I${p}/usr/include") (lib.optionals enableMac [ libobjc foundation onyx2d coregraphics coretext quartzcore applicationservices corevideo windowserver ]);
in
stdenv.mkDerivation {
  pname = "puredarwin-wine";
  inherit (wine) version;
  src = wine.src;

  postPatch = lib.optionalString enableMac ''
    # window.c asks Carbon's IsWindowVisible about an HWND, it links on macOS only because HIToolbox still
    # exports that name. The unix side means win32u's call
    substituteInPlace dlls/winemac.drv/window.c \
      --replace-fail '|| !IsWindowVisible(hwnd)) return -1;' '|| !NtUserIsWindowVisible(hwnd)) return -1;'
  '' + lib.optionalString isArm64 ''
    # Clang provides the C11 atomic builtins on both AArch64 and ARM64EC, but
    # Wine's old guard only recognizes GCC. ARM64EC also defines __x86_64__, so
    # falling through selects x86 lock/xchg assembly and fails the AArch64
    # assembler. Prefer the compiler builtins for Clang targets.
    substituteInPlace include/winnt.h \
      --replace-fail '#if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 7))' \
        '#if defined(__clang__) || (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 7))'
    substituteInPlace include/winnt.h \
      --replace-fail '#if defined(__x86_64__) || defined(__i386__)
    for (;;) __asm__ __volatile__( "int $0x29" :: "c" ((ULONG_PTR)code) : "memory" );' \
        '#if defined(__arm64ec__)
    {
        register ULONG_PTR val __asm__("x0") = code;
        for (;;) __asm__ __volatile__( "brk #0xf003" :: "r" (val) : "memory" );
    }
#elif defined(__x86_64__) || defined(__i386__)
    for (;;) __asm__ __volatile__( "int $0x29" :: "c" ((ULONG_PTR)code) : "memory" );'
  '';

  patches = [
    ./patches/wine-init-handler-null-gsbase.patch
    ./patches/wine-fault-handler-null-teb.patch
    ./patches/wine-arm64ec-import-lib.patch
    ./patches/wine-arm64ec-darwin-asm-section.patch
    ./patches/wine-arm64ec-vccorlib-rtti-naked.patch
    ./patches/wine-arm64-import-section-align.patch
    ./patches/wine-winemac-remote-gl.patch
    ./patches/wine-winemac-vulkan-no-metal.patch
    ./patches/wine-winemac-remote-layer.patch
  ] ++ lib.optionals (!stockMac) [
    # PureDarwin ships DejaVu only: CEF and other DirectWrite users find Arial, Segoe UI and friends through it
    ./patches/wine-default-font-replacements.patch
  ] ++ lib.optionals isArm64 [
    ./patches/wine-arm64ec-rtti-msvcp90.patch
  ];

  # mingwGcc builds Wine's PE-format modules. Like winebuild it runs on the
  # build host and emits Windows binaries, so it never touches PureDarwin.
  nativeBuildInputs = [ pkg-config gnumake flex bison python3 perl waylandScanner ] ++ peCompilers;
  buildInputs = xDeps ++ waylandDeps ++ [ freetype fontconfig ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:${waylandScanner}/bin:$PATH"
    # expat is here only because fontconfig.pc lists it in Requires.private:
    # PKG_CONFIG_LIBDIR pins the search path, so a missing transitive .pc makes
    # the whole fontconfig query fail and configure decides fontconfig is absent.
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" (map lib.getDev (xDeps ++ waylandDeps ++ [ freetype fontconfig expat gnutls ]))}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export CXX="${darwinCrossToolchain}/bin/${targetTriple}-clang++"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    # one -F directory holding every PD framework, so <AppKit/AppKit.h> and -framework AppKit both resolve
    pdfw="$TMPDIR/pd-frameworks"
    mkdir -p "$pdfw"
    for pkg in ${lib.concatStringsSep " " pdFrameworkPackages}; do
      # a framework can come in parts, ApplicationServices has its headers in one package and its binary in another
      for fw in "$pkg"/System/Library/Frameworks/*.framework; do
        if [ -e "$fw" ]; then cp -rsn "$fw" "$pdfw/"; chmod -R u+w "$pdfw"; fi
      done
    done
    # the SDK's <libkern/OSAtomic.h> is xnu's kernel one, winemac needs the user one libplatform ships.
    # The SDK should publish that one itself, a change that rebuilds everything, so only libkern/ is staged here
    mkdir -p "$TMPDIR/pd-libkern/libkern"
    ${lib.optionalString enableMac ''cp ${../../../src/Libraries/libSystem/libplatform/include/libkern}/*.h "$TMPDIR/pd-libkern/libkern/"''}
    export CPPFLAGS="-I$TMPDIR/pd-libkern -F$pdfw ${lib.concatStringsSep " " pdHeaderDirs} -F${coreservices}/System/Library/Frameworks -F${security}/System/Library/Frameworks -F${diskArbitration}/System/Library/Frameworks -F${systemConfiguration}/System/Library/Frameworks -I${mesa}/usr/include -I${libSystem}/usr/include -I${../wayland/pd-compat-include} ${lib.concatMapStringsSep " " (dep: "-I${lib.getDev dep}/include") (xDeps ++ waylandDeps)}"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector"
    # OpenGL.framework re-exports libGL and libGLU by their /usr/lib install names, which ld has to find,
    # and Carbon re-exports its HIToolbox subframework the same way
    hitoolbox=/System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/HIToolbox
    glReexports="${lib.optionalString enableMac "-Wl,-dylib_file,/usr/lib/libGL.1.dylib:${mesa}/usr/lib/libGL.1.dylib -Wl,-dylib_file,/usr/lib/libGLU.1.dylib:${glu}/usr/lib/libGLU.1.dylib -Wl,-dylib_file,$hitoolbox:${carbon}$hitoolbox"}"
    # clang adds -lobjc for Objective-C links by itself, -nostdlib takes that away
    # and the SDK's tbds don't record that Carbon re-exports ApplicationServices and CoreServices re-exports
    # CoreFoundation, so ld can't follow them the way it does against Apple's SDK
    objcLib="${lib.optionalString enableMac "-L${libobjc}/usr/lib -lobjc -framework ApplicationServices -framework CoreFoundation"}"
    export LDFLAGS="$glReexports $objcLib -F$pdfw -isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${mesa}/usr/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") (xDeps ++ waylandDeps)} -Wl,-platform_version,macos,${if stockMac then "13.0" else "26.5"},26.5${lib.optionalString stockMac " -Wl,-headerpad_max_install_names"} -F${coreservices}/System/Library/Frameworks -framework CoreServices -F${security}/System/Library/Frameworks -framework Security -F${diskArbitration}/System/Library/Frameworks -framework DiskArbitration -F${systemConfiguration}/System/Library/Frameworks -framework SystemConfiguration -F${iokit}/System/Library/Frameworks -F${corefoundation}/System/Library/Frameworks -lSystem"

    # The cross build invokes widl for IDLs that import stdole2.tlb. The
    # native-tools derivation deliberately builds only the host tools, so its
    # tree contains the recipe but not the generated PE typelib. Generate it
    # in a writable copy; the Nix store input remains untouched.
    wine_tools_build="$TMPDIR/wine-tools"
    cp -a "${wineTools}" "$wine_tools_build"
    chmod -R u+w "$wine_tools_build"
    ${lib.optionalString isArm64 ''
    # Import libraries are generated by winebuild from the native-tools copy,
    # not by the PE-side source tree. Rebuild that host tool with the patched
    # ARM64 import-section layout before configure starts using it.
    cp tools/winebuild/import.c "$wine_tools_build/tools/winebuild/import.c"
    ${gnumake}/bin/make -C "$wine_tools_build" -j"$NIX_BUILD_CORES" tools/winebuild/winebuild
    ''}
    # The tools tree names the rule after the build machine's PE arch. The
    # typelib only records pointer size, so either 64-bit one serves both.
    ${gnumake}/bin/make -C "$wine_tools_build" -j"$NIX_BUILD_CORES" \
      dlls/stdole2.tlb/${toolsPeArch}-windows/stdole2.tlb
    mkdir -p dlls/stdole2.tlb/x86_64-windows
    cp "$wine_tools_build/dlls/stdole2.tlb/${toolsPeArch}-windows/stdole2.tlb" \
      dlls/stdole2.tlb/x86_64-windows/stdole2.tlb
    # widl normalizes ARM64EC's architecture directory to aarch64-windows.
    mkdir -p dlls/stdole2.tlb/aarch64-windows
    cp dlls/stdole2.tlb/x86_64-windows/stdole2.tlb \
      dlls/stdole2.tlb/aarch64-windows/stdole2.tlb

    # xkbcommon is built static, so pkg-config's plain --libs omits the
    # transitive libxml2 that libxkbregistry needs and configure's link probe
    # fails. These AC_ARG_VARs override the pkg-config query outright.
    export XKBCOMMON_CFLAGS="-I${lib.getDev xkbcommon}/include"
    export XKBCOMMON_LIBS="-L${xkbcommon}/lib -lxkbcommon"
    export XKBREGISTRY_CFLAGS="-I${lib.getDev xkbcommon}/include"
    export XKBREGISTRY_LIBS="-L${xkbcommon}/lib -lxkbregistry ${libxml2}/lib/libxml2.a"

    # PD has no PC/SC smart card framework, and without the mac frameworks win32u and localspl drop theirs too
    sed -i -e 's|PCSCLITE_LIBS="-framework PCSC"|PCSCLITE_LIBS=""|' configure
    ${lib.optionalString (!enableMac) ''
    sed -i -e 's|^CORETEXT_LIBS=.*|CORETEXT_LIBS=""|' \
           -e 's|^APPKIT_LIBS=.*|APPKIT_LIBS=""|' \
           -e 's|^APPLICATIONSERVICES_LIBS=.*|APPLICATIONSERVICES_LIBS=""|' \
           -e 's|CORETEXT_LIBS="-framework CoreText"|CORETEXT_LIBS=""|' \
           -e 's|APPKIT_LIBS="-framework AppKit"|APPKIT_LIBS=""|' \
           -e 's|APPLICATIONSERVICES_LIBS="-framework ApplicationServices"|APPLICATIONSERVICES_LIBS=""|' configure
    ''}

    # cups.c pulls ApplicationServices for Carbon Print Manager (PMCreateSession,
    # PMPaperGetPPDPaperName). No printing stack here and we configure
    # --without-cups; both __APPLE__ blocks are self-contained and the second has
    # an #else.
    sed -i 's|^#ifdef __APPLE__$|#if 0 /* PureDarwin: no ApplicationServices/Carbon printing */|' \
      dlls/winspool.drv/cups.c

    # freetype.c includes CoreText unconditionally, but its only user
    # (load_mac_fonts) sits behind #ifdef SONAME_LIBFREETYPE/#elif __APPLE__ and
    # we build with freetype, so that branch is dead. Drop the include.
    # The stock Mac build has no fontconfig, so its font list comes from CoreText as on a Mac.
    ${lib.optionalString (!stockMac) ''
    perl -0777 -i -pe "s{\#ifdef __APPLE__\n\#include <CoreText/CoreText\.h>\n\#endif /\* __APPLE__ \*/}{/* PureDarwin: no CoreText; load_mac_fonts is behind !SONAME_LIBFREETYPE */}s" \
      dlls/win32u/freetype.c
    ''}

    # ip.c hand-copies struct ip6stat because "Mac OS doesn't export
    # <netinet6/ip6_var.h> to user-space". Ours does, from the same xnu the
    # kernel is built from, so use the real one rather than a drifting copy.
    perl -0777 -i -pe "s{\#ifdef __APPLE__\n(/\* For reasons unknown, Mac OS doesn't export)}{#if 0 /* PureDarwin: our SDK does export it */\n\$1}s" \
      dlls/nsiproxy.sys/ip.c

    # cred.c's __APPLE__ branch wants Spotlight (MDQuery*) and the Carbon
    # keychain (SecKeychain*); PureDarwin has neither, and the file carries a
    # portable #else. Take that instead of half-compiling the Apple one.
    sed -i '0,/^#ifdef __APPLE__$/! s|^#ifdef __APPLE__$|#if 0 /* PureDarwin: no Spotlight or Carbon keychain */|' dlls/mountmgr.sys/cred.c

    # The preloader reads argc/argv off the raw stack, which only LC_UNIXTHREAD gives it, and ld64
    # emits LC_MAIN from 10.8 on, so this link keeps a 10.7 minimum
    sed -i 's| -ldylib1\.o| -fuse-ld=${nativeLd}/bin/ld -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,10.7,26.5 -lSystem|' configure

${lib.optionalString stockMac ''
    # The loader reserves Wine's low and top-down ranges as zerofill segments, as upstream does
    # when ld knows -no_huge. Our ld64 lays those out as written and emits no chained fixups,
    # so the check is seeded and the two flags it does not know are dropped.
    export ac_cv_cflags__Wl__no_huge=yes
    sed -i 's|-no_huge,-no_fixup_chains,||' configure
    # A Mac keeps the resolver in libresolv.9.dylib, PD links it into libSystem. dnsapi links the Mac way,
    # against a stub with Apple's install name
    mkdir -p "$TMPDIR/pd-resolv"
    cat > "$TMPDIR/pd-resolv/libresolv.tbd" <<'TBD'
--- !tapi-tbd
tbd-version: 4
targets: [ x86_64-macos ]
install-name: '/usr/lib/libresolv.9.dylib'
current-version: 1
compatibility-version: 1
exports:
  - targets: [ x86_64-macos ]
    symbols: [ ___res_9_state, _res_9_dn_expand, _res_9_init, _res_9_ndestroy, _res_9_ninit,
               _res_9_nquery, _res_9_ns_get16, _res_9_ns_get32, _res_9_ns_initparse,
               _res_9_ns_name_ntop, _res_9_ns_name_uncompress, _res_9_ns_parserr, _res_9_query,
               _res_9_search ]
...
TBD
    export LDFLAGS="$LDFLAGS -L$TMPDIR/pd-resolv"
    export ac_cv_have_resolv=-lresolv
    # A Mac has no FreeType, so it ships in the tree next to Wine and win32u loads it from there
    export ac_cv_lib_soname_freetype=@loader_path/../../libfreetype.6.dylib
''}
${lib.optionalString (!stockMac) ''
    # winevulkan loads the image's Vulkan loader at run time, Wine carries its own headers
    export ac_cv_lib_soname_vulkan=/usr/lib/libvulkan.1.dylib
''}
${lib.optionalString isArm64 ''
    export CROSSCFLAGS="-g -O1 -fno-unwind-tables"
    export aarch64_CFLAGS="-g -O1 -fno-unwind-tables -mno-unaligned-access"
    ln -sf ${mingwAarch64Cc}/resource-root/lib/windows/libclang_rt.builtins-aarch64.a libgcc.a
''}
    set +e
    env ${macFrameworkOverrides} ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --with-wine-tools="$wine_tools_build" \
      --enable-win64 \
      --with-x \
      ${lib.optionalString (!enableMac) "--disable-winemac.drv --disable-winecoreaudio.drv"} \
      --with-mingw \
      --enable-archs=${peArchs} \
      ${if stockMac then "--without-fontconfig" else "--with-fontconfig"} \
      --without-alsa \
      --without-capi \
      --without-cups \
      --without-dbus \
      --with-gnutls \
      --without-gssapi \
      --without-gstreamer \
      --without-krb5 \
      --without-netapi \
      --without-oss \
      --without-pcap \
      --without-pulse \
      --without-sane \
      --without-sdl \
      --without-udev \
      --without-unwind \
      --without-usb \
      --without-v4l2 \
      ${if stockMac then "--without-vulkan" else "--with-vulkan"} \
      --with-wayland
    configureStatus=''${PIPESTATUS[0]}
    set -e
    echo "configure exit status: $configureStatus" >> configure-output.log
    if [ "$configureStatus" -ne 0 ]; then
      echo "Wine configure failed" >&2
      exit "$configureStatus"
    fi

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild

    # Do not swallow make's status: a partial tree used to sail through to
    # installPhase and ship a broken wine.
    set +e
    make -j$NIX_BUILD_CORES
    buildStatus=$?
    set -e
    if [ "$buildStatus" -ne 0 ]; then
      exit "$buildStatus"
    fi

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p "$out"

    # DESTDIR install so binaries record guest paths (--prefix=/usr above).
    set +e
    make install DESTDIR="$out" >> install.log 2>&1
    installStatus=$?
    set -e
    if [ "$installStatus" -ne 0 ]; then
      tail -n 200 install.log >&2
      exit "$installStatus"
    fi

    # make install puts the loader only in lib/wine/<arch>-unix, but every app
    # in bin/ is a symlink to a bin/wine that nothing creates. Supply it.
    if [ ! -e "$out/usr/bin/wine" ] && [ -e "$out/usr/lib/wine/x86_64-unix/wine" ]; then
      ln -s ../lib/wine/x86_64-unix/wine "$out/usr/bin/wine"
    fi

    ${lib.optionalString isArm64 ''
    mkdir -p "$out/usr/lib/wine/aarch64-windows"
    cp "${fexWow64}/usr/lib/wine/aarch64-windows/libwow64fex.dll" \
      "$out/usr/lib/wine/aarch64-windows/"
    ''}

    # Everything else Wine needs from the host it dlopens by soname, so the only
    # @rpath references are its own sibling modules, which @loader_path finds.
    # winewayland.drv is the exception: it links libwayland directly, and
    # @loader_path points at lib/wine/<arch>-unix where no libwayland lives.
    for so in "$out"/usr/lib/wine/*-unix/winewayland.so; do
      [ -e "$so" ] || continue
      for base in libwayland-client.0.dylib libwayland-egl.1.dylib; do
        ${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool \
          -change "@rpath/$base" "/lib/$base" "$so" 2>/dev/null || true
      done
    done

    ${lib.optionalString stockMac ''
    # PD installs a few frameworks under /usr/lib names and every framework at Versions/A, a Mac has
    # the framework paths and keeps AppKit at Versions/C. The header padding in LDFLAGS makes room for the longer names
    find "$out/usr" -type f \( -name '*.so' -o -perm -u+x \) | while read -r f; do
      ${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool \
        -change /usr/lib/libFoundation.dylib /System/Library/Frameworks/Foundation.framework/Versions/C/Foundation \
        -change /usr/lib/libCoreFoundation.dylib /System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation \
        -change /usr/lib/libGLU.1.dylib /System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGLU.dylib \
        -change /System/Library/Frameworks/AppKit.framework/Versions/A/AppKit /System/Library/Frameworks/AppKit.framework/Versions/C/AppKit \
        "$f" 2>/dev/null || true
    done
    # the FreeType win32u opens through @loader_path, with a minimum a Mac accepts
    cp ${freetype}/lib/libfreetype.6.dylib "$out/usr/lib/libfreetype.6.dylib"
    chmod u+w "$out/usr/lib/libfreetype.6.dylib"
    ${darwinCrossToolchain}/bin/${targetTriple}-install_name_tool -id @loader_path/libfreetype.6.dylib "$out/usr/lib/libfreetype.6.dylib"
    # there is no vtool here, LC_BUILD_VERSION's minos is one word to rewrite
    python3 - "$out/usr/lib/libfreetype.6.dylib" <<'PY'
import struct, sys
path = sys.argv[1]
data = bytearray(open(path, "rb").read())
ncmds = struct.unpack_from("<I", data, 16)[0]
off = 32
for _ in range(ncmds):
    cmd, size = struct.unpack_from("<II", data, off)
    if cmd == 0x32:
        struct.pack_into("<I", data, off + 12, 13 << 16)
    off += size
open(path, "wb").write(data)
PY
    ''}

    # The PE modules carry mingw DWARF debug info - mshtml.dll alone is 30MB of
    # it, ~765MB across the tree. Each PE architecture needs its own strip; a
    # mismatched one silently does nothing, hence the per-directory pairing.
    strip_pe() {
      local dir="$1" tool="$2"
      [ -d "$dir" ] || return 0
      find "$dir" \( -name '*.dll' -o -name '*.exe' \) -print0 \
        | xargs -0 -r -n1 "$tool" --strip-debug 2>/dev/null || true
    }
    strip_pe "$out/usr/lib/wine/x86_64-windows" ${mingwBintools}/bin/x86_64-w64-mingw32-strip
    ${lib.optionalString isArm64 ''
    strip_pe "$out/usr/lib/wine/aarch64-windows" ${toString mingwAarch64Cc}/bin/aarch64-w64-mingw32-strip
    ''}
    ${lib.optionalString (!isArm64) ''
    strip_pe "$out/usr/lib/wine/i386-windows" ${mingwBintools32}/bin/i686-w64-mingw32-strip
    ''}

    # Build logs live under usr/share, not $out root: image.nix copies each
    # package's tree verbatim into the image root, so anything at the top level
    # here would land in / on the running system.
    logdir="$out/usr/share/wine-build"
    mkdir -p "$logdir"
    for f in configure-output.log build-status.txt build-tail.log config.log install.log; do
      cp "$f" "$logdir/" 2>/dev/null || true
    done
    cp include/config.h "$logdir/config.h" 2>/dev/null || true

    runHook postInstall
  '';

  dontFixup = true;
}
