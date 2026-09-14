# Nix, cross-built for PureDarwin, with every dependency it needs as a static
# archive so the result is one self-contained binary.
{ lib
, pkgs
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, openssl
, zlib
, xz
, curl
, corefoundation
, systemConfiguration
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  cross = import ./cross.nix {
    inherit lib darwinCrossToolchain nativeLd libSystem libcxxDylib libcxxabiDylib targetTriple;
    inherit (pkgs) appleSdk pkg-config cmake;
  };

  inherit (pkgs) stdenv;

  meta = {
    platforms = lib.platforms.linux;
  };

  # A static library built with CMake. Output layout: $out/{lib,include}.
  mkCMakeStatic = { pname, version, src, sourceRoot ? null, cmakeDir ? ".", cmakeFlags ? [ ], deps ? [ ] }:
    stdenv.mkDerivation ({
      pname = "puredarwin-${pname}";
      inherit version src;
      nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.pkg-config ];
      dontUseCmakeConfigure = true;
      configurePhase = ''
        runHook preConfigure
        cat > puredarwin-toolchain.cmake <<'EOF'
        ${cross.cmakeToolchain}
        EOF
        cmake -S ${cmakeDir} -B build -G Ninja \
          -DCMAKE_TOOLCHAIN_FILE=$PWD/puredarwin-toolchain.cmake \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=$out \
          -DCMAKE_INSTALL_LIBDIR=lib \
          -DCMAKE_PREFIX_PATH="${lib.concatStringsSep ";" deps}" \
          -DBUILD_SHARED_LIBS=OFF \
          -DBUILD_TESTING=OFF \
          ${lib.escapeShellArgs cmakeFlags}
        runHook postConfigure
      '';
      buildPhase = "runHook preBuild; ninja -C build; runHook postBuild";
      installPhase = "runHook preInstall; ninja -C build install; runHook postInstall";
      dontFixup = true;
      inherit meta;
    } // lib.optionalAttrs (sourceRoot != null) { inherit sourceRoot; });

  # A static library built with autotools, configured for the cross host.
  mkAutotoolsStatic = { pname, version, src, configureFlags ? [ ], autoreconf ? false }:
    stdenv.mkDerivation {
      pname = "puredarwin-${pname}";
      inherit version src;
      nativeBuildInputs = [ pkgs.pkg-config ]
        ++ lib.optionals autoreconf [ pkgs.autoconf pkgs.automake pkgs.libtool ];
      configurePhase = ''
        runHook preConfigure
        ${lib.optionalString autoreconf "autoreconf -fi"}
        ${cross.autotoolsEnv}
        ./configure --host=${targetTriple} --build=$(cc -dumpmachine) --prefix=$out \
          --disable-shared --enable-static \
          ${lib.escapeShellArgs configureFlags}
        runHook postConfigure
      '';
      enableParallelBuilding = true;
      dontFixup = true;
      inherit meta;
    };
in
rec {
  nix-cross = cross;

  nix-bzip2 = stdenv.mkDerivation {
    pname = "puredarwin-bzip2";
    inherit (pkgs.bzip2) version src;
    dontConfigure = true;
    buildPhase = ''
      runHook preBuild
      for f in blocksort huffman crctable randtable compress decompress bzlib; do
        ${cross.cc} ${cross.cflags} -O2 -D_FILE_OFFSET_BITS=64 -c $f.c -o $f.o
      done
      ${cross.ar} rcs libbz2.a blocksort.o huffman.o crctable.o randtable.o compress.o decompress.o bzlib.o
      runHook postBuild
    '';
    installPhase = ''
      runHook preInstall
      mkdir -p $out/lib/pkgconfig $out/include
      cp libbz2.a $out/lib/
      cp bzlib.h $out/include/
      cat > $out/lib/pkgconfig/bzip2.pc <<EOF
      Name: bzip2
      Description: bzip2 compression library
      Version: ${pkgs.bzip2.version}
      Libs: -L$out/lib -lbz2
      Cflags: -I$out/include
      EOF
      runHook postInstall
    '';
    dontFixup = true;
    inherit meta;
  };

  # The amalgamation is plain C; the full sqlite-src tree needs a host tclsh.
  nix-sqlite = stdenv.mkDerivation {
    pname = "puredarwin-sqlite";
    version = "3.51.2";
    src = pkgs.fetchurl {
      url = "https://sqlite.org/2026/sqlite-amalgamation-3510200.zip";
      sha256 = "1v135y8jsnczxn048f27mgx891gl1id2p2v1s2xbs9ih95d88akf";
    };
    nativeBuildInputs = [ pkgs.unzip ];
    dontConfigure = true;
    buildPhase = ''
      runHook preBuild
      ${cross.cc} ${cross.cflags} -O2 -DSQLITE_THREADSAFE=1 -DHAVE_USLEEP=1 \
        -DSQLITE_ENABLE_COLUMN_METADATA=1 -c sqlite3.c -o sqlite3.o
      ${cross.ar} rcs libsqlite3.a sqlite3.o
      runHook postBuild
    '';
    installPhase = ''
      runHook preInstall
      mkdir -p $out/lib/pkgconfig $out/include
      cp libsqlite3.a $out/lib/
      cp sqlite3.h sqlite3ext.h $out/include/
      cat > $out/lib/pkgconfig/sqlite3.pc <<EOF
      Name: SQLite
      Description: SQL database engine
      Version: 3.51.2
      Libs: -L$out/lib -lsqlite3
      Cflags: -I$out/include
      EOF
      runHook postInstall
    '';
    dontFixup = true;
    inherit meta;
  };

  nix-libsodium = mkAutotoolsStatic {
    pname = "libsodium";
    inherit (pkgs.libsodium) version src;
    configureFlags = [ "--disable-pie" "--without-pthreads" ];
  };

  nix-libblake3 = mkCMakeStatic {
    pname = "libblake3";
    inherit (pkgs.libblake3) version src;
    cmakeDir = "c";
    cmakeFlags = [ "-DBLAKE3_USE_TBB=OFF" ];
  };

  nix-brotli = mkCMakeStatic {
    pname = "brotli";
    inherit (pkgs.brotli) version src;
    cmakeFlags = [ "-DBROTLI_BUILD_TOOLS=OFF" "-DBROTLI_DISABLE_TESTS=ON" ];
  };

  nix-editline = mkAutotoolsStatic {
    pname = "editline";
    inherit (pkgs.editline) version src;
    autoreconf = true;
    configureFlags = [ "--disable-examples" ];
  };

  # Only what Nix unpacks: gzip, bzip2 and xz tarballs.
  nix-libarchive = mkCMakeStatic {
    pname = "libarchive";
    inherit (pkgs.libarchive) version src;
    deps = [ zlib nix-bzip2 xz ];
    cmakeFlags = [
      "-DENABLE_OPENSSL=OFF" "-DENABLE_MBEDTLS=OFF" "-DENABLE_NETTLE=OFF"
      "-DENABLE_LIBB2=OFF" "-DENABLE_LZ4=OFF" "-DENABLE_LZO=OFF" "-DENABLE_ZSTD=OFF"
      "-DENABLE_LIBXML2=OFF" "-DENABLE_EXPAT=OFF"
      "-DENABLE_PCREPOSIX=OFF" "-DENABLE_PCRE2POSIX=OFF"
      "-DENABLE_TAR=OFF" "-DENABLE_CPIO=OFF" "-DENABLE_CAT=OFF"
      "-DENABLE_ACL=OFF" "-DENABLE_XATTR=OFF" "-DENABLE_ICONV=OFF"
      "-DENABLE_TEST=OFF"
    ];
  };

  nix-libgit2 = mkCMakeStatic {
    pname = "libgit2";
    inherit (pkgs.libgit2) version src;
    deps = [ openssl zlib ];
    cmakeFlags = [
      "-DBUILD_CLI=OFF" "-DBUILD_TESTS=OFF" "-DBUILD_EXAMPLES=OFF"
      "-DUSE_SSH=OFF" "-DUSE_HTTPS=OpenSSL"
      # SHA-256 from OpenSSL: the builtin rfc6234 one declares an enum whose
      # SHA1/SHA256 values collide with OpenSSL's functions of the same names.
      "-DUSE_SHA1=CollisionDetection" "-DUSE_SHA256=HTTPS"
      "-DREGEX_BACKEND=builtin" "-DUSE_HTTP_PARSER=builtin"
      "-DUSE_NTLMCLIENT=OFF" "-DUSE_GSSAPI=OFF" "-DUSE_ICONV=OFF"
      "-DUSE_BUNDLED_ZLIB=OFF"
    ];
  };

  # Nix compiles against five Boost libraries and many header-only ones, so
  # build the five and install the complete header tree, as upstream does.
  nix-boost = (mkCMakeStatic {
    pname = "boost";
    version = "1.89.0";
    src = pkgs.fetchurl {
      url = "https://github.com/boostorg/boost/releases/download/boost-1.89.0/boost-1.89.0-cmake.tar.xz";
      hash = "sha256-Z6zsAtDRGLXenrRB9ftwezoc3YhL4AyiS5pzyZVRH3Q=";
    };
    cmakeFlags = [
      "-DBOOST_INCLUDE_LIBRARIES=container;context;coroutine;iostreams;url"
      "-DBOOST_INSTALL_LAYOUT=system"
      "-DBOOST_IOSTREAMS_ENABLE_ZLIB=OFF" "-DBOOST_IOSTREAMS_ENABLE_BZIP2=OFF"
      "-DBOOST_IOSTREAMS_ENABLE_LZMA=OFF" "-DBOOST_IOSTREAMS_ENABLE_ZSTD=OFF"
      "-DBOOST_CONTEXT_BINARY_FORMAT=mach-o"
      "-DBOOST_CONTEXT_ARCHITECTURE=${if cross.info.arch == "arm64" then "arm64" else "x86_64"}"
      "-DBOOST_CONTEXT_ABI=${if cross.info.arch == "arm64" then "aapcs" else "sysv"}"
      "-DBOOST_CONTEXT_ASSEMBLER=gas"
      "-DBOOST_CONTEXT_IMPLEMENTATION=fcontext"
    ];
  }).overrideAttrs (old: {
    postInstall = ''
      for inc in libs/*/include/boost libs/numeric/*/include/boost; do
        cp -rn "$inc" $out/include/ 2>/dev/null || cp -rn "$inc"/. $out/include/boost/
      done
    '';
  });

  nix = stdenv.mkDerivation (finalAttrs: {
    pname = "puredarwin-nix";
    version = "2.34.8";
    src = pkgs.fetchFromGitHub {
      owner = "NixOS";
      repo = "nix";
      rev = finalAttrs.version;
      hash = "sha256-Rvy1PmIUMGI0IS/kwDwmf/VrorU8v1iZYejssSVu1rY=";
    };

    nativeBuildInputs = [ pkgs.meson pkgs.ninja pkgs.pkg-config pkgs.cmake pkgs.bison pkgs.flex ];

    # Meson's CMake dependency method cannot probe a cross toolchain; toml11 is
    # header-only, so let it fall back to the pkg-config file written below.
    postPatch = ''
      substituteInPlace src/libexpr/meson.build --replace-fail "method : 'cmake'," ""
    '';

    configurePhase =
      let
        deps = [
          nix-sqlite nix-libsodium nix-libblake3 nix-brotli nix-editline nix-libarchive
          nix-libgit2 nix-bzip2 curl openssl zlib xz pkgs.nlohmann_json
        ];
        pcPath = lib.concatStringsSep ":" (lib.concatMap (d: [ "${d}/lib/pkgconfig" "${d}/share/pkgconfig" ]) deps);
        crossFile = cross.mesonCrossFile {
          extraCppArgs = [
            # Nix's public headers include Boost and <cxxabi.h> even in
            # subprojects that do not declare either dependency.
            "-isystem" "${nix-boost}/include"
            "-I${libcxxabiDylib}/usr/include"
            "-I${openssl}/include"
          ];
          # Static archives whose own private dependencies meson does not add.
          extraLinkArgs = [
            "-L${openssl}/lib" "-lssl" "-lcrypto"
            "-L${zlib}/lib" "-lz"
            "-L${nix-bzip2}/lib" "-lbz2"
            "-L${xz}/lib" "-llzma"
            # libcurl asks SystemConfiguration for proxy settings.
            "-F${corefoundation}/System/Library/Frameworks" "-framework" "CoreFoundation"
            "-F${systemConfiguration}/System/Library/Frameworks" "-framework" "SystemConfiguration"
          ];
          properties.boost_root = "${nix-boost}";
        };
      in
      ''
        runHook preConfigure
        cat > puredarwin-cross.ini <<'EOF'
        ${crossFile}
        EOF
        mkdir -p pc
        cat > pc/toml11.pc <<EOF
        Name: toml11
        Description: TOML for Modern C++ (header-only)
        Version: ${pkgs.toml11.version}
        Cflags: -I${pkgs.toml11}/include
        EOF
        export PKG_CONFIG_LIBDIR="$PWD/pc:${pcPath}"
        export PKG_CONFIG_PATH="$PKG_CONFIG_LIBDIR"
        meson setup build \
          --cross-file puredarwin-cross.ini \
          --prefix=/usr --sysconfdir=/etc --localstatedir=/nix/var --libdir=lib \
          --buildtype=release \
          -Ddefault_library=static \
          -Dcmake_prefix_path=${pkgs.toml11} \
          -Dunit-tests=false -Ddoc-gen=false -Dbindings=false \
          -Dbenchmarks=false -Djson-schema-checks=false \
          -Dlibutil:cpuid=disabled \
          -Dlibstore:seccomp-sandboxing=disabled \
          -Dlibstore:s3-aws-auth=disabled \
          -Dlibexpr:gc=disabled \
          -Dlibcmd:markdown=disabled \
          -Dlibcmd:readline-flavor=editline
        runHook postConfigure
      '';

    buildPhase = "runHook preBuild; ninja -C build; runHook postBuild";

    installPhase = ''
      runHook preInstall
      DESTDIR=$out meson install -C build --no-rebuild
      # Everything is linked into bin/nix; the static libraries and headers are
      # build-time only.
      rm -rf $out/usr/include $out/usr/lib
      # Nix's own plist lands in usr/Library, which launchd never reads, and
      # points at a profile path; ship one for /usr/bin/nix-daemon instead.
      rm -rf $out/usr/Library
      install -Dm644 ${./org.nixos.nix-daemon.plist} \
        $out/System/Library/LaunchDaemons/org.nixos.nix-daemon.plist
      install -Dm644 ${./nix.conf} $out/etc/nix/nix.conf
      runHook postInstall
    '';

    dontFixup = true;
    inherit meta;
  });
}
