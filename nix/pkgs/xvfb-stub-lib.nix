{ stdenv
, lib
, darwinCrossToolchain
, name
, version
, pcName
, pcDescription
, headers ? {}
, includeFrom ? []
, source
, targetTriple ? "x86_64-apple-darwin20.4"
}:

let
  targetInfo = import ../lib/target-info.nix targetTriple;
in
stdenv.mkDerivation {
  pname = "puredarwin-${name}";
  inherit version;

  dontUnpack = true;

  buildPhase = ''
    runHook preBuild
    mkdir -p build
    cat > build/${name}.c <<'EOF'
${source}
EOF
    ${darwinCrossToolchain}/bin/${targetTriple}-clang -target ${targetInfo.clangTarget} -c build/${name}.c -o build/${name}.o
    ${darwinCrossToolchain}/bin/${targetTriple}-ar rcs build/lib${name}.a build/${name}.o
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/lib/pkgconfig $out/include
    cp build/lib${name}.a $out/lib/
    for include_dir in ${lib.escapeShellArgs (map (dep: "${lib.getDev dep}/include") includeFrom)}; do
      if [ -d "$include_dir" ]; then
        cp -a "$include_dir"/. $out/include/
        chmod -R u+w $out/include
      fi
    done
  '' + lib.concatStringsSep "\n" (lib.mapAttrsToList (path: text: ''
    mkdir -p "$out/include/$(dirname "${path}")"
    cat > "$out/include/${path}" <<'EOF'
${text}
EOF
  '') headers) + ''
    cat > $out/lib/pkgconfig/${pcName}.pc <<EOF
prefix=$out
exec_prefix=$out
libdir=$out/lib
includedir=$out/include

Name: ${pcName}
Description: ${pcDescription}
Version: ${version}
Cflags: -I$out/include
Libs: -L$out/lib -l${name}
EOF
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.linux;
  };
}