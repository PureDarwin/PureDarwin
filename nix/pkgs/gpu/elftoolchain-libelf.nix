{ stdenv
, lib
, fetchurl
, gnum4
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# elftoolchain's libelf (BSD-2-Clause), which mesa's radeonsi needs to read the
# shader binaries it builds
stdenv.mkDerivation rec {
  pname = "puredarwin-libelf";
  version = "0.7.1";

  src = fetchurl {
    url = "https://downloads.sourceforge.net/project/elftoolchain/Sources/elftoolchain-${version}/elftoolchain-${version}.tar.bz2";
    sha256 = "1dfj5fxvlsqa88rcyxpl88pyqjzvydi7bp8mf8w984pjzj8lbwa4";
  };

  nativeBuildInputs = [ gnum4 ];
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    cd libelf
    for g in convert fsize msize; do
      m4 -D SRCDIR=$PWD libelf_$g.m4 > libelf_$g.c
    done
    # every C file in libelf/ is part of the library
    srcs=$(ls *.c)
    objs=""
    for f in $srcs; do
      ${darwinCrossToolchain}/bin/${targetTriple}-clang -c -O2 -fPIC \
        -isysroot "$SDK" -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
        -fno-stack-protector -I${libSystem}/usr/include -I. -I../common \
        -Wno-error "$f" -o "''${f%.c}.o"
      objs="$objs ''${f%.c}.o"
    done
    ${darwinCrossToolchain}/bin/${targetTriple}-clang -dynamiclib -o libelf.1.dylib $objs \
      -install_name /usr/lib/libelf.1.dylib -compatibility_version 1 -current_version 1 \
      -isysroot "$SDK" -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -lSystem
    cd ..
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib/pkgconfig $out/usr/include
    cp libelf/libelf.1.dylib $out/usr/lib/
    ln -s libelf.1.dylib $out/usr/lib/libelf.dylib
    cp libelf/libelf.h libelf/gelf.h common/elfdefinitions.h $out/usr/include/
    # the standard name for symbol index 0, which this release does not define
    cat >> $out/usr/include/elfdefinitions.h <<'EOF2'
#ifndef STN_UNDEF
#define STN_UNDEF 0
#endif
EOF2
    cat > $out/usr/lib/pkgconfig/libelf.pc <<EOF
    prefix=$out/usr
    libdir=\''${prefix}/lib
    includedir=\''${prefix}/include

    Name: libelf
    Description: elftoolchain ELF object file access library
    Version: ${version}
    Libs: -L\''${libdir} -lelf
    Cflags: -I\''${includedir}
    EOF
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "elftoolchain libelf for PureDarwin";
    license = licenses.bsd2;
    platforms = platforms.unix;
  };
}
