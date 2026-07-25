{ stdenv
, lib
, requireFile
, darwinCrossToolchain
, nativeLd
, libSystem
, icuSrc
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
  pname = "puredarwin-icucore";
  version = "76.1";

  src = icuSrc;

  nativeBuildInputs = [ stdenv.cc ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"

    mkdir -p native-build
    ( cd native-build
      ../source/configure \
        --disable-tests --disable-samples --disable-extras \
        --disable-icuio
      make -j"$NIX_BUILD_CORES"
    )

    # -Wl,-fixup_chains everywhere below: same eager-bind fix as
    # corefoundation.nix/iokit.nix - PD's dyld lazy-binding path is fragile,
    # these dylibs' own internal calls need to not go through it.
    mkdir -p build
    ( cd build
      CC=${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang \
      CXX=${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang++ \
      AR=${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ar \
      RANLIB=${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-ranlib \
      CFLAGS="-isysroot $DARWIN_SDK_ROOT -I${libSystem}/usr/include -mmacosx-version-min=11.0" \
      CXXFLAGS="-isysroot $DARWIN_SDK_ROOT -I${libSystem}/usr/include -mmacosx-version-min=11.0" \
      LDFLAGS="-fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -Wl,-platform_version,macos,11.0,11.5 -Wl,-fixup_chains -lSystem" \
      ../source/configure \
        --host=x86_64-apple-darwin20.4 \
        --with-cross-build=$PWD/../native-build \
        --disable-renaming \
        --disable-tests --disable-samples --disable-extras --disable-icuio \
        --disable-tools \
        --with-data-packaging=library
    )

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -C build -j"$NIX_BUILD_CORES"
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib

    for f in build/lib/*.dylib; do
      [ -L "$f" ] && continue
      cp "$f" "$out/usr/lib/$(basename "$f")"
    done
    for f in "$out"/usr/lib/*.dylib; do
      name=$(basename "$f")
      ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-install_name_tool \
        -id "/usr/lib/$name" "$f"
      for dep in libicuuc.76.dylib libicudata.76.dylib libicui18n.76.dylib; do
        ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-install_name_tool \
          -change "$dep" "/usr/lib/$dep" "$f" 2>/dev/null || true
      done
    done
    ln -s libicuuc.76.1.dylib $out/usr/lib/libicuuc.76.dylib
    ln -s libicudata.76.1.dylib $out/usr/lib/libicudata.76.dylib
    ln -s libicui18n.76.1.dylib $out/usr/lib/libicui18n.76.dylib

    echo 'static int puredarwin_icucore_placeholder;' > placeholder.c
    ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang \
      -isysroot "$DARWIN_SDK_ROOT" -c placeholder.c -o placeholder.o
    ${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang \
      -isysroot "$DARWIN_SDK_ROOT" -dynamiclib -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib -L${libSystem}/usr/lib -L"$out/usr/lib" \
      -Wl,-platform_version,macos,11.0,11.5 \
      -Wl,-install_name,/usr/lib/libicucore.A.dylib \
      -Wl,-reexport_library,"$out/usr/lib/libicuuc.76.1.dylib" \
      -Wl,-reexport_library,"$out/usr/lib/libicudata.76.1.dylib" \
      -Wl,-reexport_library,"$out/usr/lib/libicui18n.76.1.dylib" \
      -Wl,-fixup_chains \
      -lSystem \
      -o "$out/usr/lib/libicucore.A.dylib" placeholder.o
    ln -s libicucore.A.dylib $out/usr/lib/libicucore.dylib

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Real ICU, cross-built for x86_64-apple-darwin (libicuuc/libicudata/libicui18n) to back libicucore.A.dylib";
    platforms = platforms.linux;
  };
}
