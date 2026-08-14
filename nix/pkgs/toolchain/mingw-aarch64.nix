{ pkgs
, system
, llvmVersion ? "21"
}:

let
  patchedNixpkgs = pkgs.applyPatches {
    name = "nixpkgs-mingw-aarch64-atomics-fix";
    src = pkgs.path;
    postPatch = ''
      substituteInPlace pkgs/development/compilers/llvm/common/compiler-rt/default.nix \
        --replace-fail \
          'withAtomicsPthread ? lib.versionAtLeast release_version "19" && stdenv.cc.libc != null,' \
          'withAtomicsPthread ? lib.versionAtLeast release_version "19" && stdenv.cc.libc != null && !stdenv.hostPlatform.isWindows,' \
        --replace-fail \
          'withAtomicsLib ? stdenv.hostPlatform.hasSharedLibraries,' \
          'withAtomicsLib ? stdenv.hostPlatform.hasSharedLibraries && !stdenv.hostPlatform.isWindows,'
    '';
  };

  # Applied to both the cross set and the build-host set: the cross stdenv's
  # compiler comes from buildPackages, so pinning only one leaves the other.
  pinLlvm = final: prev: { llvmPackages = prev."llvmPackages_${llvmVersion}"; };

  crossPkgs = import patchedNixpkgs {
    # `system` explicitly: flake evaluation is pure, so the nested import cannot
    # fall back to builtins.currentSystem.
    inherit system;
    crossSystem = {
      config = "aarch64-w64-mingw32";
      libc = "ucrt";
      useLLVM = true;
    };
    crossOverlays = [ pinLlvm ];
    overlays = [ pinLlvm ];
  };
  # winpthreads supplies mingw's pthread.h/sched.h. nixpkgs' aarch64 build fails
  # because windres gets no include path from the wrapper, so its version.rc
  # cannot find <winver.h>. A third upstream bug in this cross set.
  pthreads = crossPkgs.windows.mingw_w64_pthreads.overrideAttrs (old: {
    RCFLAGS = "-I${crossPkgs.windows.mingw_w64_headers}/include";
  });
  arm64ecCc = pkgs.runCommand "arm64ec-w64-mingw32-toolchain" { } ''
    mkdir -p $out/bin
    # Wine's PE linker still asks for libgcc.a. LLVM MinGW provides the
    # equivalent compiler-rt builtins archive, so expose this compatibility
    # name only in the ARM64EC wrapper.
    mkdir -p $out/lib
    ln -s ${crossPkgs.stdenv.cc}/resource-root/lib/windows/libclang_rt.builtins-aarch64.a \
      $out/lib/libgcc.a
    for t in clang clang++ cc c++; do
      cat > $out/bin/arm64ec-w64-mingw32-$t <<EOF
    #!${pkgs.bash}/bin/bash
    export NIX_CC_WRAPPER_SUPPRESS_TARGET_WARNING=1
    args=()
    skip_target_value=0
    response_index=0
    for arg in "\$@"; do
      case "\$arg" in
        -print-file-name=libgcc.a|-print-file-name=libgcc_eh.a)
          printf '%s\\n' "$out/lib/libgcc.a"
          exit 0
          ;;
      esac
      if [ "\$skip_target_value" = 1 ]; then
        skip_target_value=0
        continue
      fi
      # Wine's configure probes pass -target arm64ec-windows.  That spelling
      # is not a complete MinGW target and would override the canonical
      # arm64ec-w64-windows-gnu target below.
      if [ "\$arg" = -target ] || [ "\$arg" = --target ]; then
        skip_target_value=1
        continue
      fi
      case "\$arg" in
        -target=*|--target=*) continue ;;
      esac
      if [ "\$arg" = libgcc.a ]; then
        arg="$out/lib/libgcc.a"
      fi
      case "\$arg" in
        @*)
          response_file="\$arg"
          response_file="\''${response_file#@}"
          if [ -f "\$response_file" ]; then
            response_copy="\$TMPDIR/arm64ec-response-\$\$-\$response_index"
            sed "s#libgcc\\.a#$out/lib/libgcc.a#g" "\$response_file" > "\$response_copy"
            arg="@\$response_copy"
            response_index=\$((response_index + 1))
          fi
          ;;
      esac
      args+=("\$arg")
    done
    exec ${crossPkgs.stdenv.cc}/bin/aarch64-w64-mingw32-$t \\
      --target=arm64ec-w64-windows-gnu \\
      -fuse-ld=${crossPkgs.stdenv.cc.bintools}/bin/ld.lld -L$out/lib "\''${args[@]}"
    EOF
      chmod +x $out/bin/arm64ec-w64-mingw32-$t
    done
    # Binutils are target-agnostic enough for PE here; symlink under the names
    # Wine and CMake look for.
    # Clang looks up the target-prefixed linker when the ARM64EC target is
    # selected. Expose both linker names as well as the ancillary binutils.
    for t in ar ranlib strip objcopy dlltool windres nm objdump ld ld.lld; do
      if [ -e ${crossPkgs.stdenv.cc}/bin/aarch64-w64-mingw32-$t ]; then
        ln -s ${crossPkgs.stdenv.cc}/bin/aarch64-w64-mingw32-$t \
          $out/bin/arm64ec-w64-mingw32-$t
      fi
    done
  '';
in
{
  cc = crossPkgs.stdenv.cc;
  mingw = crossPkgs.windows.mingw_w64;
  inherit pthreads;
  inherit arm64ecCc;
  inherit crossPkgs;
}
