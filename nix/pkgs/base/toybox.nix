{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, toybox
, gnumake
, zlib
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

let
in
stdenv.mkDerivation {
  pname = "puredarwin-toybox";
  inherit (toybox) version;
  src = toybox.src;

  nativeBuildInputs = [ gnumake ];

  configurePhase = ''
    runHook preConfigure

    mkdir -p sdk
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    export HOSTCC=cc
    export CFLAGS="-isysroot $DARWIN_SDK_ROOT -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -I${libSystem}/usr/include"
    export LDFLAGS="-isysroot $DARWIN_SDK_ROOT -fuse-ld=${nativeLd}/bin/ld -nostdlib -Wl,-Z -L${libSystem}/usr/lib -L${zlib}/lib -Wl,-force_load,${zlib}/lib/libz.a -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,11.0,11.5 -Wl,-undefined,dynamic_lookup -lSystem"
    export LDOPTIMIZE="-Wl,-dead_strip"

    chmod +x scripts/*.sh
    patchShebangs scripts/

    make allnoconfig
    for opt in \
      AWK BASENAME CAT CHMOD CLEAR CMP CP CUT DATE DD DF DIFF DIRNAME DU \
      ECHO ENV EXPR FALSE FIND GREP GUNZIP GZIP HEAD HOSTNAME ID KILL LN \
      LS MKDIR MKNOD MORE MV NETCAT PATCH PRINTF PWD READLINK REALPATH \
      RESET RM RMDIR SED SEQ SH SLEEP SORT STAT \
      TAIL TAR TEST TOUCH TR TRUE \
      TRUNCATE TTY UNAME UNIQ VI WC WHICH WHOAMI XARGS YES \
      SHA1SUM SHA256SUM SHA512SUM; do
      if LC_ALL=C grep -q "^CONFIG_''${opt}=" .config 2>/dev/null; then
        sed -i "s/^CONFIG_''${opt}=.*/CONFIG_''${opt}=y/" .config
      elif LC_ALL=C grep -q "^# CONFIG_''${opt} is not set" .config 2>/dev/null; then
        sed -i "s/^# CONFIG_''${opt} is not set/CONFIG_''${opt}=y/" .config
      else
        echo "CONFIG_''${opt}=y" >> .config
      fi
    done

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES toybox
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    cp toybox $out/bin/toybox
    LC_ALL=C grep -oE '^CONFIG_[A-Z0-9_]+=y$' .config \
      | sed -e 's/^CONFIG_//' -e 's/=y$//' \
      | tr 'A-Z' 'a-z' > $out/applets.txt
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}
