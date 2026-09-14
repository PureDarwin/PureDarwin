{ stdenv
, lib
, src
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, gnumake
, targetTriple ? "x86_64-apple-darwin20.4"
}:

# os-test measures the POSIX surface an OS provides. The ~5650 tests are
# cross-compiled here with PureDarwin's toolchain and libSystem (mirroring
# os-test's cross-ssh flow); the binaries are staged into the image and run
# in the guest for the runtime outcomes. The patch adds misc/os.sh reporting
# "puredarwin" so results never collide with a macOS run.

stdenv.mkDerivation {
  pname = "puredarwin-os-test";
  version = "unstable";
  inherit src;

  patches = [ ./os-test-puredarwin.patch ];

  nativeBuildInputs = [ gnumake ];

  # os-test's helper scripts use /usr/bin/env, which exists neither in the Nix
  # sandbox nor on PureDarwin. /bin/sh exists in both.
  postPatch = ''
    find . -name '*.sh' -exec sed -i '1s|^#!/usr/bin/env sh$|#!/bin/sh|' {} +
  '';

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    # The image symlinks these compatibility names at /usr/lib onto libSystem
    # (see image.nix); mirror that here so -lm and friends resolve the same way
    # they do in the guest.
    mkdir -p pd-libs
    cp ${libSystem}/usr/lib/libSystem.B.dylib pd-libs/
    for compat in libc libm libpthread libdl libinfo; do
      ln -sf libSystem.B.dylib pd-libs/$compat.dylib
    done

    cat > pd-clang <<EOF
    #!${stdenv.shell}
    exec ${darwinCrossToolchain}/bin/${targetTriple}-clang \
      -isysroot "$DARWIN_SDK_ROOT" -I${libSystem}/usr/include -Qunused-arguments "\$@"
    EOF
    sed -i 's/^    //' pd-clang
    chmod +x pd-clang

    # No -undefined,dynamic_lookup here on purpose: it would make every link
    # succeed and os-test could never report the `undefined` outcome, silently
    # inflating the score.
    # --ld-path, not -fuse-ld=<path>: the latter warns (-Wfuse-ld-path), and
    # os-test's try-compile.sh builds with -Werror, which would turn every link
    # into a failure and report the whole include suite as `undefined`.
    pdld="-isysroot $DARWIN_SDK_ROOT --ld-path=${nativeLd}/bin/ld -nostdlib -Wl,-Z"
    pdld="$pdld -L$PWD/pd-libs -L${libSystem}/usr/lib"
    pdld="$pdld -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib"
    pdld="$pdld -Wl,-dylinker_install_name,/usr/lib/dyld"
    pdld="$pdld -Wl,-platform_version,macos,26.5,26.5 -lSystem"

    # A failed compile is a *result*, not a build failure: compile.sh records the
    # outcome and exits 0. -k keeps going past the suites whose helpers have to
    # run on the build host (namespace, and the dlfcn shared objects).
    # The namespace suite preprocesses the tests with the cross compiler (it is
    # probing PureDarwin's headers) but builds misc/namespace, which analyses
    # that output, to run on the build machine.
    make -k OS=PureDarwin \
      CC="$PWD/pd-clang" \
      CFLAGS="-std=c17 -pthread" \
      LDFLAGS="$pdld -lm" \
      EXTRA_LDFLAGS="" \
      CC_FOR_BUILD="$CC" \
      CFLAGS_FOR_BUILD="" \
      CPPFLAGS_FOR_BUILD="" \
      LDFLAGS_FOR_BUILD="" \
      -j''${NIX_BUILD_CORES:-1} all || true

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/usr/share/os-test
    cp -R ./. $out/usr/share/os-test/
    rm -rf $out/usr/share/os-test/pd-libs $out/usr/share/os-test/pd-clang
    find $out/usr/share/os-test -name '*.o' -delete
    chmod -R u+w $out/usr/share/os-test

    runHook postInstall
  '';

  # The tests are PureDarwin Mach-O binaries; nothing here is for the host.
  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "POSIX conformance test suites, cross-built for PureDarwin";
    homepage = "https://gitlab.com/sortix/os-test";
    license = licenses.isc;
    platforms = platforms.linux;
  };
}
