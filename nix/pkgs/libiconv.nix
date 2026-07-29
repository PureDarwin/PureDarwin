{ stdenv
, lib
, requireFile
, gnumake
, darwinCrossToolchain
, nativeLd
, targetTriple ? "x86_64-apple-darwin20.4"
, libSystem
, libiconvReal
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
  pname = "puredarwin-libiconv";
  inherit (libiconvReal) version src;

  nativeBuildInputs = [
    gnumake
  ];

  postPatch = ''
    # The Darwin SDK's MB_CUR_MAX_L macro references the private
    # ____mb_cur_max_l symbol, which PureDarwin libSystem does not export.
    sed -i 's/strcmp (codeset, "UTF-8") == 0 && MB_CUR_MAX_L (uselocale (NULL)) <= 1/0/' libcharset/lib/localcharset.c

    # GNU libiconv exports libiconv_* on Darwin by default, while some
    # configure paths still link consumers against the POSIX iconv_* names.
    # Provide both spellings from the dylib.
    cat >> lib/iconv.c <<'EOF'

#undef iconv_open
#undef iconv
#undef iconv_close

__attribute__((visibility("default")))
iconv_t
iconv_open(const char *tocode, const char *fromcode)
{
  return libiconv_open(tocode, fromcode);
}

__attribute__((visibility("default")))
size_t
iconv(iconv_t cd, ICONV_CONST char **inbuf, size_t *inbytesleft,
      char **outbuf, size_t *outbytesleft)
{
  return libiconv(cd, inbuf, inbytesleft, outbuf, outbytesleft);
}

__attribute__((visibility("default")))
int
iconv_close(iconv_t cd)
{
  return libiconv_close(cd);
}
EOF
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    tar xf ${sdkTarball} -C sdk
    export DARWIN_SDK_ROOT="$PWD/sdk/MacOSX11.3.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export LD="${nativeLd}/bin/ld"
    export AR="${darwinCrossToolchain}/bin/${targetTriple}-ar"
    export RANLIB="${darwinCrossToolchain}/bin/${targetTriple}-ranlib"
    export STRIP="${darwinCrossToolchain}/bin/${targetTriple}-strip"
    export NM="${darwinCrossToolchain}/bin/${targetTriple}-nm"
    export OBJDUMP="${darwinCrossToolchain}/bin/${targetTriple}-objdump"
    export CPPFLAGS="-I${libSystem}/usr/include"
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,11.0,11.5 -lSystem"

    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --libdir=/usr/lib \
      --enable-shared \
      --enable-static \
      --disable-nls

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    make -C libcharset install \
      prefix="$out/usr" \
      exec_prefix="$out/usr" \
      libdir="$out/usr/lib"
    make -C lib install \
      prefix="$out/usr" \
      exec_prefix="$out/usr" \
      libdir="$out/usr/lib"
    mkdir -p "$out/usr/bin"
    install -m755 src/.libs/iconv_no_i18n "$out/usr/bin/iconv"
    mkdir -p "$out/usr/include"
    install -m644 include/iconv.h.inst "$out/usr/include/iconv.h"

    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
