#!/bin/sh
# mslc IN.metal -o OUT.metallib [-e ENTRY] [-S OUT.ll] [-D NAME[=V]]...: MSL source to AIR for air2spv, through
# clang (C++ for OpenCL, the mslc plugin, PD's metal_stdlib) and airify. CLANG, PLUGIN, AIRIFY from the environment
set -e
here=$(dirname "$0")
in= out= entry= ll= defs=
while [ $# -gt 0 ]; do
  case $1 in
    -o) out=$2; shift ;;
    -e) entry=$2; shift ;;
    -S) ll=$2; shift ;;
    -D) defs="$defs -D$2"; shift ;;
    -D*) defs="$defs $1" ;;
    *) in=$1 ;;
  esac
  shift
done
[ -n "$in" ] && [ -n "$out" ] || { echo "usage: mslc IN.metal -o OUT.metallib [-e ENTRY] [-S OUT.ll] [-D N=V]" >&2; exit 2; }
inc=${MSLC_INCLUDE:-$here/include}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# [[threadgroup(n)]] would expand the threadgroup address space macro
sed -E 's/\[\[[[:space:]]*threadgroup[[:space:]]*\(/[[threadgroup_buffer(/g' "$in" > "$tmp/in.metal"
"$CLANG" -x clcpp -cl-std=clc++2021 -cl-no-stdinc -target spirv64 -Xclang -cl-ext=-cl_khr_fp64,-__opencl_c_fp64 \
  -cl-fast-relaxed-math -ffp-contract=off -fplugin="$PLUGIN" -I"$inc" $defs -O0 -Xclang -disable-O0-optnone \
  -S -emit-llvm -Wno-ignored-attributes -Wno-unknown-attributes -Wno-unused-command-line-argument \
  -Wno-duplicate-decl-specifier "$tmp/in.metal" -o "$tmp/front.ll"
"$AIRIFY" "$tmp/front.ll" -m "$out" ${entry:+-e "$entry"} ${ll:+-S "$ll"}
