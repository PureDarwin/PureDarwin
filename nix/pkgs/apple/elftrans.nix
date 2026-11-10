{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, appleSdk
, nativeMigcom
, binutils
, elfaot
, elfaotDarwin
, pkgsMusl
}:

let
  cc = "${darwinCrossToolchain}/bin/x86_64-apple-darwin20.4-clang";
in
stdenv.mkDerivation {
  pname = "puredarwin-elftrans";
  version = "0.1";
  dontUnpack = true;
  nativeBuildInputs = [ binutils elfaot ];

  buildPhase = ''
    runHook preBuild
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    # the EXC_SYSCALL fallback's server, for syscalls no converted site covers
    mkdir -p mig
    MIGCC="${cc}" MIGCOM="${nativeMigcom}/bin/migcom" SDKROOT="$DARWIN_SDK_ROOT" \
      bash ${../../../tools/mig/mig.sh} -novouchers -arch x86_64 -isysroot "$DARWIN_SDK_ROOT" \
      -I$DARWIN_SDK_ROOT/usr/include -user /dev/null -server mig/mach_excServer.c -sheader mig/mach_exc_server.h \
      -header mig/mach_exc.h "$DARWIN_SDK_ROOT/usr/include/mach/mach_exc.defs"

    for f in elftrans syscalls signals procfs events net; do
      ${cc} -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
        -I${libSystem}/usr/include -I${../../../src/Tools/elftrans} -Imig \
        -O2 -Wall -Wextra \
        -c ${../../../src/Tools/elftrans}/$f.c -o $f.o
    done
    ${cc} -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
      -I${libSystem}/usr/include -Imig -O2 -w -c mig/mach_excServer.c -o mach_excServer.o

    # linked high and not PIE, the page zero's end (slide included) is the lowest address a task can use,
    # and __GUEST holds the low 4 GB for Linux programs
    ${cc} -isysroot "$DARWIN_SDK_ROOT" -mmacosx-version-min=26.5 \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-pagezero_size,0x1000 \
      -Wl,-image_base,0x100000000000 -Wl,-no_pie -Wl,-segaddr,__GUEST,0x10000 \
      -o elftrans elftrans.o syscalls.o signals.o procfs.o events.o net.o mach_excServer.o -lSystem

    as ${../../../src/Tools/elftrans/hello-linux.S} -o hello-linux.o
    ld -static -nostdlib -e _start -o hello-static hello-linux.o
    elfaot hello-static hello-linux

    # a static non-PIE musl program, loaded at the address it was linked for
    ${pkgsMusl.stdenv.cc}/bin/cc -O1 -static -fno-pie -no-pie \
      ${../../../src/Tools/elftrans/hello-musl-threads.c} -o musl-threads-static
    elfaot musl-threads-static hello-musl-threads
    for t in signals abort syscalls process proc net; do
      ${pkgsMusl.stdenv.cc}/bin/cc -O1 -static -fno-pie -no-pie \
        ${../../../src/Tools/elftrans}/hello-$t.c -o $t-static
      elfaot $t-static hello-$t
    done

    as ${../../../src/Tools/elftrans/hello-jit.S} -o hello-jit.o
    ld -static -nostdlib -e _start -o jit-static hello-jit.o
    elfaot jit-static hello-jit

    as ${../../../src/Tools/elftrans/hello-absolute.S} -o hello-absolute.o
    ld -static -nostdlib -e _start -o absolute-static hello-absolute.o
    elfaot absolute-static hello-absolute

    cc -O0 -fno-pic -no-pie -nostdlib -static \
      -Wl,-e,_start -Wl,--emit-relocs \
      ${../../../src/Tools/elftrans/hello-data-pointer.c} \
      -o data-pointer-static
    elfaot data-pointer-static hello-data-pointer

    as ${../../../src/Tools/elftrans/libtiny.S} -o libtiny.o
    ld -shared -soname libtiny.so -o libtiny-raw.so libtiny.o
    elfaot libtiny-raw.so libtiny.so

    as ${../../../src/Tools/elftrans/libtiny-ro.S} -o libtiny-ro.o
    ld -shared -z notext -soname libtiny.so \
      -o libtiny-ro-raw.so libtiny-ro.o
    elfaot libtiny-ro-raw.so libtiny-ro.so

    as ${../../../src/Tools/elftrans/hello-dynamic.S} -o hello-dynamic.o
    ld -pie --no-dynamic-linker -e _start -o dynamic-raw \
      hello-dynamic.o -L. -l:libtiny-raw.so
    elfaot dynamic-raw hello-dynamic
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 elftrans $out/usr/bin/elftrans
    install -Dm755 ${elfaotDarwin}/usr/bin/elfaot $out/usr/bin/elfaot
    install -Dm755 hello-linux $out/usr/share/elftrans/hello-linux
    install -Dm755 hello-absolute $out/usr/share/elftrans/hello-absolute
    install -Dm755 hello-jit $out/usr/share/elftrans/hello-jit
    install -Dm755 hello-musl-threads $out/usr/share/elftrans/hello-musl-threads
    install -Dm755 hello-signals $out/usr/share/elftrans/hello-signals
    install -Dm755 hello-abort $out/usr/share/elftrans/hello-abort
    install -Dm755 hello-syscalls $out/usr/share/elftrans/hello-syscalls
    install -Dm755 hello-process $out/usr/share/elftrans/hello-process
    install -Dm755 hello-proc $out/usr/share/elftrans/hello-proc
    install -Dm755 hello-net $out/usr/share/elftrans/hello-net
    install -Dm755 hello-data-pointer $out/usr/share/elftrans/hello-data-pointer
    install -Dm755 hello-dynamic $out/usr/share/elftrans/hello-dynamic
    install -Dm755 libtiny.so $out/usr/share/elftrans/libtiny.so
    install -Dm755 libtiny-ro.so $out/usr/share/elftrans/libtiny-ro.so
    runHook postInstall
  '';

  dontFixup = true;
  meta = with lib; {
    description = "Run a freestanding Linux x86-64 ELF under PureDarwin";
    platforms = platforms.unix;
  };
}
