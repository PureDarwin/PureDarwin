{ stdenv
, lib
, gnumake
, pkg-config
, darwinCrossToolchain
, nativeLd
, libSystem
, dmenu
, xorgproto
, libX11
, libxcb
, libXft
, libXrender
, libXau
, libXdmcp
, freetype2
, fontconfig
, expat
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
  deps = [
    xorgproto
    libX11
    libxcb
    libXft
    libXrender
    libXau
    libXdmcp
    freetype2
    fontconfig
    expat
  ];
in
stdenv.mkDerivation {
  pname = "puredarwin-dmenu";
  version = dmenu.version;

  src = dmenu.src;

  nativeBuildInputs = [ gnumake pkg-config ];
  buildInputs = deps;

  postPatch = ''
    # No Xinerama on PureDarwin yet.
    sed -i '/#define XINERAMA/d' config.mk 2>/dev/null || true
  '';

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    CFLAGS="-isysroot $DARWIN_SDK_ROOT -mmacosx-version-min=11.0 -Qunused-arguments -std=c99 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -I${libSystem}/usr/include ${lib.concatMapStringsSep " " (dep: "-I${lib.getDev dep}/include") deps}"
    LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib ${lib.concatMapStringsSep " " (dep: "-L${dep}/lib") deps} -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -lSystem"

    cat > config.mk <<EOF
VERSION = ${dmenu.version}
PREFIX = \$(out)
MANPREFIX = \$(PREFIX)/share/man
X11INC = ${lib.getDev libX11}/include
X11LIB = ${libX11}/lib
FREETYPEINC = ${lib.getDev freetype2}/include/freetype2
FREETYPELIBS = -lXft
INCS = -I\$(X11INC) -I\$(FREETYPEINC)
LIBS = \$(FREETYPELIBS) -lX11 -lxcb -lXrender -lXau -lXdmcp -lfontconfig -lfreetype -lexpat
CPPFLAGS = -DVERSION=\"${dmenu.version}\" -D_DEFAULT_SOURCE -D_BSD_SOURCE
CFLAGS = $CFLAGS \$(INCS) \$(CPPFLAGS)
LDFLAGS = $LDFLAGS \$(LIBS)
CC = $CC
EOF

    export out
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    make PREFIX=$out install
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
