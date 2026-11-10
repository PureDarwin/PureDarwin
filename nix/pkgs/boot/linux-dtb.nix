# SPDX-License-Identifier: MIT OR BSD-2-Clause OR BSD-3-Clause OR GPL-2.0-only
# builds one board's device tree from a linux source tarball, so no dts enters the pd tree
{ lib, runCommand, xz, dtc, stdenv
# a fetchurl of a kernel.org tarball, its url and hash go into the source note
, linuxSrc
# path below arch/arm64/boot/dts, e.g. "nvidia/tegra210-p2371-2180.dts"
, dts
, name ? "linux-dtb-${lib.removeSuffix ".dts" (baseNameOf dts)}"
# more -I directories inside the source, for boards that need them
, extraIncludes ? [ ]
# property changes fdtput applies after dtc, each listed in SOURCE,
# e.g. { node = "/gpu@57000000"... }
, fixups ? [ ]
}:

runCommand name {
  nativeBuildInputs = [ xz dtc stdenv.cc ];
} ''
  mkdir src
  tar -xf ${linuxSrc} --strip-components=1 --no-wildcards-match-slash -C src \
    --wildcards '*/Makefile' '*/COPYING' '*/LICENSES' '*/include/dt-bindings' '*/include/uapi' \
    '*/scripts/dtc/include-prefixes' '*/arch/arm64/boot/dts'
  version=$(awk '/^VERSION/{v=$3} /^PATCHLEVEL/{p=$3} /^SUBLEVEL/{s=$3} END{print v"."p"."s}' src/Makefile)

  # the same flags as the kernel's dtc_cpp_flags, plus a list of every file read
  cd src
  cpp -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
    -I arch/arm64/boot/dts -I scripts/dtc/include-prefixes -I include \
    ${lib.concatMapStringsSep " " (i: "-I ${i}") extraIncludes} \
    -MD -MF ../deps -MT board \
    -o ../board.dts.tmp arch/arm64/boot/dts/${dts}
  cd ..

  mkdir -p $out/licenses
  dtc -I dts -O dtb -q -o $out/board.dtb board.dts.tmp
${lib.concatMapStrings (f: ''
  fdtput -t ${f.type or "s"} $out/board.dtb ${f.node} ${f.prop} ${lib.escapeShellArg f.value}
'') fixups}

  # every input file with its spdx line, resolved through the include-prefixes symlinks
  sed -e 's/^board://' -e 's/\\$//' deps | tr ' ' '\n' | grep . | sort -u > files
  : > spdx
  while read -r f; do
    rel=$(realpath --relative-to=src "src/$f")
    line=$(grep -m1 -o 'SPDX-License-Identifier:.*' "src/$f" | sed 's/ *\*\/$//')
    printf '%s: %s\n' "$rel" "''${line:-no SPDX line}" >> spdx
  done < files

  # ship the text of every licence named, gpl-2.0 always
  cp src/COPYING $out/licenses/COPYING
  for id in GPL-2.0 $(sed 's/.*SPDX-License-Identifier://' spdx | tr -c 'A-Za-z0-9.+-' '\n' \
      | grep -vxE 'OR|AND|WITH|' | sed 's/+$//' | sort -u); do
    for t in src/LICENSES/*/"$id"; do
      [ -f "$t" ] && cp "$t" $out/licenses/
    done
  done

  cat > $out/SOURCE <<NOTE
board.dtb was built from Linux $version, arch/arm64/boot/dts/${dts}.
Corresponding source: ${linuxSrc.url or "https://www.kernel.org/"}
Tarball hash: ${linuxSrc.outputHash or "unknown"}
The licence texts are in licenses/. The files it was built from:

NOTE
  cat spdx >> $out/SOURCE
${lib.optionalString (fixups != [ ]) ''
  cat >> $out/SOURCE <<NOTE

PureDarwin changed these properties after building, with fdtput:
${lib.concatMapStrings (f: "  ${f.node} ${f.prop} = ${f.value}
") fixups}NOTE
''}

  # last, after any fixups, so it names the blob that ships
  printf '\nboard.dtb sha256: %s\n' "$(sha256sum $out/board.dtb | cut -d' ' -f1)" >> $out/SOURCE
''
