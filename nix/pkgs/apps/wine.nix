{ stdenv
, lib
, requireFile
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
, wayland
, waylandProtocols
, waylandScanner
, xkbcommon
, libxml2
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
}:

assert targetArch == "arm64" -> mingwAarch64Cc != null;
assert targetArch == "arm64" -> fexWow64 != null;

let
  isArm64 = targetArch == "arm64";
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
  sdkTarball = requireFile {
    name = "MacOSX11.3.sdk.tar.xz";
    sha256 = "9adc1373d3879e1973d28ad9f17c9051b02931674a3ec2a2498128989ece2cb1";
    message = ''
      MacOSX11.3.sdk.tar.xz (Apple SDK, proprietary - not fetchable/redistributable)
      is not yet in your Nix store. Register your local copy with:
        nix-store --add-fixed sha256 /path/to/MacOSX11.3.sdk.tar.xz
    '';
  };
  # The 11.3 SDK carries AppKit/Metal headers, so Wine's configure would find
  # them and enable winemac.drv against frameworks PureDarwin does not have.
  # Pre-seed the header caches to no so the X11 driver is chosen instead.
  macFrameworkOverrides = lib.concatStringsSep " " [
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
    "ac_cv_header_OpenAL_al_h=no"
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-wine";
  inherit (wine) version;
  src = wine.src;

  postPatch = lib.optionalString isArm64 ''
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
  ] ++ lib.optionals isArm64 [
    ./patches/wine-arm64ec-rtti-msvcp90.patch
  ];

  # mingwGcc builds Wine's PE-format modules. Like winebuild it runs on the
  # build host and emits Windows binaries, so it never touches PureDarwin.
  nativeBuildInputs = [ pkg-config gnumake flex bison python3 waylandScanner ] ++ peCompilers;
  buildInputs = xDeps ++ waylandDeps ++ [ freetype fontconfig ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
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
    export CPPFLAGS="-I${mesa}/usr/include -I${libSystem}/usr/include -I${../wayland/pd-compat-include} ${lib.concatMapStringsSep " " (dep: "-I${lib.getDev dep}/include") (xDeps ++ waylandDeps)}"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -L${mesa}/usr/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") (xDeps ++ waylandDeps)} -Wl,-platform_version,macos,11.0,11.5 -lSystem"

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
    ${gnumake}/bin/make -C "$wine_tools_build" -j"$NIX_BUILD_CORES" \
      dlls/stdole2.tlb/x86_64-windows/stdole2.tlb
    mkdir -p dlls/stdole2.tlb/x86_64-windows
    cp "$wine_tools_build/dlls/stdole2.tlb/x86_64-windows/stdole2.tlb" \
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

    # dlls/win32u/Makefile.in has an unconditional
    # UNIX_LIBS = $(CORETEXT_LIBS) $(APPKIT_LIBS)
    # Blank them.
    sed -i -e 's|^CORETEXT_LIBS=.*|CORETEXT_LIBS=""|' \
           -e 's|^APPKIT_LIBS=.*|APPKIT_LIBS=""|' \
           -e 's|CORETEXT_LIBS="-framework CoreText"|CORETEXT_LIBS=""|' \
           -e 's|APPKIT_LIBS="-framework AppKit"|APPKIT_LIBS=""|' configure

    sed -i 's| -ldylib1\.o| -fuse-ld=${nativeLd}/bin/ld -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,10.7,11.5 -lSystem|' configure

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
      --disable-winemac.drv \
      --disable-winecoreaudio.drv \
      --with-mingw \
      --enable-archs=${peArchs} \
      --with-fontconfig \
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
      --without-vulkan \
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

    set +e
    make -j$NIX_BUILD_CORES
    set -e

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
