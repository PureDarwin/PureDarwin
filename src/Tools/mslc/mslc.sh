#!/bin/sh
# mslc IN.metal -o OUT.metallib [-e ENTRY] [-S OUT.ll] [-D NAME[=V]]... [-include F]: MSL source to AIR for air2spv, through
# clang (C++ for OpenCL, the mslc plugin, PD's metal_stdlib) and airify. CLANG, PLUGIN, AIRIFY from the environment
# without -e the output is an MTLB library of every entry, with -e the one function blob air2spv reads
set -e
here=${0%/*}
in= out= entry= ll= defs=
while [ $# -gt 0 ]; do
  case $1 in
    -o) out=$2; shift ;;
    -e) entry=$2; shift ;;
    -S) ll=$2; shift ;;
    -D) defs="$defs -D$2"; shift ;;
    -D*) defs="$defs $1" ;;
    -include) defs="$defs -include $2"; shift ;;
    *) in=$1 ;;
  esac
  shift
done
[ -n "$in" ] && [ -n "$out" ] || { echo "usage: mslc IN.metal -o OUT.metallib [-e ENTRY] [-S OUT.ll] [-D N=V]" >&2; exit 2; }
inc=${MSLC_INCLUDE:-$here/include}
[ -d "$inc" ] || inc=/usr/share/mslc/include
# the PureDarwin install, when the environment names nothing
: "${CLANG:=/usr/libexec/mslc-clang}" "${PLUGIN:=/usr/lib/mslc-plugin.dylib}" "${AIRIFY:=/usr/libexec/mslc-airify}"
tmp=${TMPDIR:-/tmp}/mslc.$$
mkdir -m 700 "$tmp" || exit 1
trap 'rm -rf "$tmp"' EXIT

# [[threadgroup(n)]] would expand the threadgroup address space macro,
# constexpr sampler takes the literal sampler type, which converts to sampler where one is used
sed -E -e 's/\[\[[[:space:]]*threadgroup[[:space:]]*\(/[[threadgroup_buffer(/g' \
  -e 's/constexpr([[:space:]]+)(metal::)?sampler([[:space:]])/constexpr\1::metal::__pd_csampler\3/g' "$in" > "$tmp/in.metal"
"$CLANG" -x clcpp -cl-std=clc++2021 -cl-no-stdinc -target spirv64 -Xclang -cl-ext=-cl_khr_fp64,-__opencl_c_fp64 \
  -cl-fast-relaxed-math -ffp-contract=off -fplugin="$PLUGIN" -I"$inc" $defs -O0 -Xclang -disable-O0-optnone \
  -S -emit-llvm -Wno-ignored-attributes -Wno-unknown-attributes -Wno-unused-command-line-argument \
  -Wno-duplicate-decl-specifier "$tmp/in.metal" -o "$tmp/front.ll"
if [ -n "$entry" ]; then
  "$AIRIFY" "$tmp/front.ll" -m "$out" -e "$entry" ${ll:+-S "$ll"}
else
  "$AIRIFY" "$tmp/front.ll" -L "$out" ${ll:+-S "$ll"}
fi
