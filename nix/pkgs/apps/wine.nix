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
}:

let
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

  patches = [
    ./wine-init-handler-null-gsbase.patch
    ./wine-fault-handler-null-teb.patch
  ];

  # mingwGcc builds Wine's PE-format modules. Like winebuild it runs on the
  # build host and emits Windows binaries, so it never touches PureDarwin.
  nativeBuildInputs = [ pkg-config gnumake flex bison mingwGcc mingwGcc32 python3 waylandScanner ];
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

    set +e
    env ${macFrameworkOverrides} ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --with-wine-tools=${wineTools} \
      --enable-win64 \
      --with-x \
      --disable-winemac.drv \
      --disable-winecoreaudio.drv \
      --with-mingw \
      --enable-archs=i386,x86_64 \
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
      --with-wayland \
      2>&1 | tee configure-output.log
    configureStatus=''${PIPESTATUS[0]}
    set -e
    echo "configure exit status: $configureStatus" >> configure-output.log

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    set +e
    make -j$NIX_BUILD_CORES > build-all.log 2>&1
    echo "make exit status: $?" | tee build-status.txt
    tail -n 200 build-all.log > build-tail.log

    set -e
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p "$out"

    # DESTDIR install so binaries record guest paths (--prefix=/usr above).
    make install DESTDIR="$out" >> install.log 2>&1 || true

    # make install puts the loader only in lib/wine/<arch>-unix, but every app
    # in bin/ is a symlink to a bin/wine that nothing creates. Supply it.
    if [ ! -e "$out/usr/bin/wine" ] && [ -e "$out/usr/lib/wine/x86_64-unix/wine" ]; then
      ln -s ../lib/wine/x86_64-unix/wine "$out/usr/bin/wine"
    fi

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
    # it, ~765MB across the tree.
    find "$out/usr/lib/wine/x86_64-windows" \( -name '*.dll' -o -name '*.exe' \) -print0 \
      | xargs -0 -r -n1 ${mingwBintools}/bin/x86_64-w64-mingw32-strip --strip-debug 2>/dev/null || true
    if [ -d "$out/usr/lib/wine/i386-windows" ]; then
      find "$out/usr/lib/wine/i386-windows" \( -name '*.dll' -o -name '*.exe' \) -print0 \
        | xargs -0 -r -n1 ${mingwBintools32}/bin/i686-w64-mingw32-strip --strip-debug 2>/dev/null || true
    fi

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
