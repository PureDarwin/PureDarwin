{ stdenv
, lib
, python3
, darwinCrossToolchain
, nativeLd
, nativeMigcom
, libSystem
, libcxxDylib
, libcxxabiDylib
, corefoundation
, llvmSrc
, llvmVersion
, clang
, targetTriple ? "x86_64-apple-darwin20.4"
, appleSdk
}:

# lldb's debugserver (gdb-remote stub over Mach), x86_64 only, without Foundation
stdenv.mkDerivation {
  pname = "puredarwin-debugserver";
  version = llvmVersion;

  dontUnpack = true;
  nativeBuildInputs = [ python3 ];

  buildPhase = ''
    runHook preBuild
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    SDK=$DARWIN_SDK_ROOT

    cp -r ${llvmSrc}/lldb/tools/debugserver/source src
    chmod -R u+w src
    mkdir -p shim/Security gen
    cp ${llvmSrc}/lldb/source/Host/common/SocketAddress.cpp src/

    python3 ${./debugserver-pd.py} src

    # mach_exc stubs from the sdk's mach_exc.defs
    cat > migcc <<EOF
#!/bin/sh
exec ${clang}/bin/clang -target x86_64-apple-darwin25.5 "\$@"
EOF
    chmod +x migcc
    export MIGCC=$PWD/migcc MIGCOM=${nativeMigcom}/bin/migcom
    (cd gen && bash ${../../../tools/mig/mig.sh} -I"$SDK/usr/include" -isysroot "$SDK" ../src/MacOSX/dbgnub-mig.defs)
    # libSystem does not export these two mig routines yet, so carry the user stubs
    mkdir -p mig
    (cd mig && bash ${../../../tools/mig/mig.sh} -I"$SDK/usr/include" -isysroot "$SDK" \
      -server /dev/null -header ../gen/task.h -user taskUser.c "$SDK/usr/include/mach/task.defs")
    (cd mig && bash ${../../../tools/mig/mig.sh} -I"$SDK/usr/include" -isysroot "$SDK" \
      -server /dev/null -header ../gen/mach_vm.h -user mach_vmUser.c "$SDK/usr/include/mach/mach_vm.defs")
    python3 ${./debugserver-pd.py} extract mig/taskUser.c gen/pd_task_extra.c task_set_state
    python3 ${./debugserver-pd.py} extract mig/mach_vmUser.c gen/pd_mach_vm_extra.c mach_vm_page_range_query
    cp shim/pd_fallbacks.c gen/

    v="${llvmVersion}"
    maj=''${v%%.*}; rest=''${v#*.}; min=''${rest%%.*}; pat=''${rest#*.}
    sed -e "s/@LLDB_VERSION_MAJOR@/$maj/g" -e "s/@LLDB_VERSION_MINOR@/$min/g" \
        -e "s/@LLDB_VERSION_PATCH@/$pat/g" -e "s/@LLDB_VERSION_SUFFIX@//g" \
        src/debugserver_vers.c.in > gen/debugserver_vers.c

    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang"
    CXX="${darwinCrossToolchain}/bin/${targetTriple}-clang++"
    common="-isysroot $SDK -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -O1 -g -Ishim -Igen -Isrc -Isrc/MacOSX -Isrc/MacOSX/x86_64 -I${llvmSrc}/lldb/include -I${llvmSrc}/llvm/include -I${libSystem}/usr/include -I${corefoundation}/include -Wno-everything"
    cxxflags="$common -std=c++17 -nostdinc++ -I${libcxxDylib}/usr/include/c++/v1"

    objs=""
    for f in src/*.cpp src/MacOSX/*.cpp src/MacOSX/x86_64/DNBArchImplX86_64.cpp src/MacOSX/MachProcess.mm src/MacOSX/MachTask.mm; do
      case "$f" in */libdebugserver.cpp) continue ;; esac
      o=obj/$(echo "$f" | tr / _).o
      mkdir -p obj
      $CXX $cxxflags -x c++ -c "$f" -o "$o"
      objs="$objs $o"
    done
    for f in gen/*.c; do
      o=obj/$(basename "$f").o
      $CC $common -c "$f" -o "$o"
      objs="$objs $o"
    done

    $CXX -isysroot $SDK -mmacosx-version-min=26.5 -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libcxxDylib}/usr/lib -L${libcxxabiDylib}/usr/lib -L${corefoundation}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -o debugserver $objs -lCoreFoundation -lc++ -lc++abi -lSystem
    runHook postBuild
  '';

  installPhase = ''
    install -Dm755 debugserver $out/usr/bin/debugserver
  '';

  dontFixup = true;
  dontStrip = true;

  meta = with lib; {
    description = "lldb debugserver cross-built for PureDarwin";
    platforms = platforms.unix;
  };
}
