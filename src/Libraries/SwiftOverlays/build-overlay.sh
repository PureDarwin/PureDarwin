#!/bin/sh
# build-overlay.sh NAME "SRCS" [swiftc args...]: builds OUT/libswiftNAME.dylib and its module.
# env: ARCH SWIFTC CLANG SYSROOT RESDIR OUT LD WORK SWIFTSRC OVSHIMS OBJCFLAGS, UNDERLYING=1 for overlays
set -e
N=$1
SRCS=$2
shift 2
G=$WORK/gyb-$ARCH/$N
mkdir -p "$G" "$OUT/$N.swiftmodule"

srcs=""
objs=""
for s in $SRCS; do
  case $s in
    *.gyb)
      o=$G/$(basename "${s%.gyb}")
      python3 "$SWIFTSRC/utils/gyb" -DCMAKE_SIZEOF_VOID_P=8 -o "$o" "$s"
      srcs="$srcs $o" ;;
    *.m|*.mm|*.c)
      o=$G/$(basename "$s").o
      $CLANG -c -target "$ARCH-apple-macosx13.0" -isysroot "$SYSROOT" -O2 -fPIC $OBJCFLAGS \
        -I "$OVSHIMS" -I "$SWIFTSRC/include" -I "$WORK/core/include" -I "$SWIFTSRC/stdlib/public/SwiftShims" \
        -DSWIFT_OBJC_INTEROP=1 -o "$o" "$s"
      objs="$objs $o" ;;
    *) srcs="$srcs $s" ;;
  esac
done

# -U__OPTIMIZE__ keeps the libc fortify headers out of the clang modules
$SWIFTC -target "$ARCH-apple-macosx13.0" -sdk "$SYSROOT" -resource-dir "$RESDIR" \
  -module-name "$N" -module-link-name "swift$N" -Xfrontend -autolink-force-load \
  -parse-as-library -enable-library-evolution ${UNDERLYING:+-import-underlying-module} \
  -O -Xcc -U__OPTIMIZE__ -swift-version 5 \
  -Xfrontend -disable-objc-attr-requires-foundation-module \
  -D SWIFT_ENABLE_REFLECTION -I "$OUT" -I "$OVSHIMS" -module-cache-path "$WORK/mcache-$ARCH" \
  -emit-module -emit-module-path "$OUT/$N.swiftmodule/$ARCH-apple-macos.swiftmodule" \
  -emit-module-interface-path "$OUT/$N.swiftmodule/$ARCH-apple-macos.swiftinterface" \
  -emit-library -o "$OUT/libswift$N.dylib" -L "$OUT" -ld-path="$LD" \
  -Xlinker -install_name -Xlinker "/usr/lib/swift/libswift$N.dylib" \
  -Xlinker -compatibility_version -Xlinker 1.0.0 \
  "$@" $srcs $objs
