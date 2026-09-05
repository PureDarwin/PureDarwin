{ stdenv
, lib
, pkg-config
, gnumake
, perl
, darwinCrossToolchain
, nativeLd
, libSystem
, src
, pname
, version
, deps ? []
, nativeDeps ? []
, configureFlags ? []
, preConfigureExtra ? ""
, postPatchExtra ? ""
, postInstallExtra ? ""
, patches ? []
, targetTriple ? "x86_64-apple-darwin20.4"
, guestPrefix ? false
, shared ? false
, extraLinkFlags ? [ ]
, nativeMesonTools ? null
, appleSdk
}:

assert shared -> nativeMesonTools != null;

let
  depPcPaths = map lib.getDev (deps ++ nativeDeps);
in
stdenv.mkDerivation {
  inherit pname version src patches;

  postPatch = postPatchExtra;

  nativeBuildInputs = [
    pkg-config
    gnumake
    perl
  ] ++ nativeDeps;

  buildInputs = deps;

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" depPcPaths}:${lib.makeSearchPath "share/pkgconfig" depPcPaths}"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export NM="${darwinCrossToolchain}/bin/${targetTriple}-nm"
    export LD="${nativeLd}/bin/ld"
    # -dylinker_install_name is omitted when building dylibs
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    # libtool filters flags it does not recognise out of both CFLAGS and LDFLAGS
    # when it builds its CCLD command, so -fuse-ld cannot be passed as a flag
    mkdir -p .pd-cc
    cat > .pd-cc/cc <<PDCCEOF
#!/bin/sh
exec ${darwinCrossToolchain}/bin/${targetTriple}-clang \
  -fuse-ld=${nativeLd}/bin/ld -Qunused-arguments "\$@"
PDCCEOF
    chmod +x .pd-cc/cc
    export CC="$PWD/.pd-cc/cc"

    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -I${libSystem}/usr/include ${lib.concatMapStringsSep " " (dep: "-I${lib.getDev dep}/include") deps}"
    pd_dylib_maps=""
    rm -f .pd-dylib-maps
    for pd_dep_lib in ${lib.concatMapStringsSep " " (dep: "${dep}/lib") deps}; do
      [ -d "$pd_dep_lib" ] || continue
      for pd_lib in "$pd_dep_lib"/*.dylib; do
        [ -e "$pd_lib" ] || continue
        [ -L "$pd_lib" ] && continue
        pd_id=$(${darwinCrossToolchain}/bin/${targetTriple}-otool -D "$pd_lib" 2>/dev/null | tail -1)
        case "$pd_id" in
          /*) pd_dylib_maps="$pd_dylib_maps -Wl,-dylib_file,$pd_id:$pd_lib" ;;
        esac
        # ...and the guest paths that dylib itself records. These are transitive
        # (librsvg -> fontconfig -> freetype): ld64 loads them to complete the
        # link, so they need mapping too even though they are not direct deps.
        ${darwinCrossToolchain}/bin/${targetTriple}-otool -L "$pd_lib" 2>/dev/null \
          | tail -n +2 | awk '{print $1}' | while read -r pd_need; do
            case "$pd_need" in
              /lib/*|/usr/lib/*) ;;
              *) continue ;;
            esac
            pd_base=$(basename "$pd_need")
            for pd_search in ${lib.concatMapStringsSep " " (dep: "${dep}/lib") deps}; do
              if [ -e "$pd_search/$pd_base" ]; then
                echo " -Wl,-dylib_file,$pd_need:$pd_search/$pd_base"
                break
              fi
            done
          done >> .pd-dylib-maps
      done
    done
    [ -e .pd-dylib-maps ] && pd_dylib_maps="$pd_dylib_maps $(tr -d '\n' < .pd-dylib-maps)"

    export LDFLAGS="$pd_dylib_maps -isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") deps} -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib${lib.optionalString (!shared) " -Wl,-dylinker_install_name,/usr/lib/dyld"} -Wl,-platform_version,macos,26.5,26.5 -lSystem"

    ${preConfigureExtra}

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=${if guestPrefix then "/" else "$out"} \
      ${if shared then "--enable-shared --disable-static" else "--disable-shared --enable-static"} \
      ${lib.escapeShellArgs configureFlags}

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    # extraLinkFlags is applied here rather than in LDFLAGS at configure time:
    # -force_load pulls a whole archive in, and configure's "can the compiler
    # create executables" test links a trivial program with no other libraries,
    # so the archive's own undefined symbols make that probe fail.
    make -j$NIX_BUILD_CORES${lib.optionalString (extraLinkFlags != [ ])
      " LDFLAGS=\"$LDFLAGS ${lib.concatStringsSep " " extraLinkFlags}\""}
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install${lib.optionalString guestPrefix " DESTDIR=$out"}
    ${lib.optionalString guestPrefix ''
      # .pc files inherit prefix=/ from configure, which is correct for the guest
      # but unusable by anything cross-compiling against this package here. Point
      # them back into the store; only build-time consumers ever read them.
      for pc in "$out"/lib/pkgconfig/*.pc "$out"/share/pkgconfig/*.pc; do
        [ -f "$pc" ] || continue
        substituteInPlace "$pc" --replace-quiet "prefix=/" "prefix=$out"
      done

      # Drop the libtool archives. Under this prefix they record libdir='//lib'
      # and cross-reference other packages' '//lib/*.la', which resolve to nothing
      # at build time and cannot be rewritten - each path belongs to a different
      # store output. Everything here is a static library shipping a .pc, so
      # libtool consumers fall back to -L/-l from pkg-config, which is correct.
      rm -f "$out"/lib/*.la
    ''}
    ${lib.optionalString shared ''
      INSTALL_NAME_TOOL="${nativeMesonTools}/bin/install_name_tool"
      dylibs=$(find "$out/lib" -maxdepth 1 -name "*.dylib" -not -type l)
      for dylib in $dylibs; do
        base=$(basename "$dylib")
        "$INSTALL_NAME_TOOL" -id "/usr/lib/$base" "$dylib"
      done

      mkdir -p "$out/usr/lib"
      for dylib in "$out"/lib/*.dylib; do
        [ -e "$dylib" ] || continue
        ln -sf "../../lib/$(basename "$dylib")" "$out/usr/lib/$(basename "$dylib")"
      done
      allfiles=$(
        [ ! -d "$out/bin" ] || find "$out/bin" -type f
        [ ! -d "$out/lib" ] || find "$out/lib" -type f
      )
      for f in $allfiles; do
        for dylib in $dylibs; do
          base=$(basename "$dylib")
          "$INSTALL_NAME_TOOL" -change "@rpath/$base" "/usr/lib/$base" "$f" 2>/dev/null || true
          # a sibling inside the same project can be recorded by absolute install
          # path rather than @rpath (libxfce4windowingui -> libxfce4windowing), which
          # the @rpath rewrite above never matches
          "$INSTALL_NAME_TOOL" -change "$out/lib/$base" "/usr/lib/$base" "$f" 2>/dev/null || true
          # siblings inside one project can be recorded by absolute install path
          # rather than @rpath (libxfce4windowingui -> libxfce4windowing), which the
          # @rpath rewrite above never matches
          "$INSTALL_NAME_TOOL" -change "$out/lib/$base" "/usr/lib/$base" "$f" 2>/dev/null || true
        done
      done
    ''}
    ${postInstallExtra}
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
