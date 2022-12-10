#!/bin/sh

if [ "${DRIVERKIT}" = 1 ]; then
    RUNTIME_PREFIX="/System/DriverKit/Runtime"
else
    RUNTIME_PREFIX=""
fi

/bin/mkdir -p ${DERIVED_FILES_DIR}
/bin/mkdir -p ${DSTROOT}${RUNTIME_PREFIX}/usr/local/include/mach-o/

VERSIONS=${SDKROOT}${RUNTIME_PREFIX}/usr/local/include/dyld/for_dyld_priv.inc
DYLD_PRIV_IN=${SRCROOT}/include/mach-o/dyld_priv.h
DYLD_PRIV_OUT=${DSTROOT}${RUNTIME_PREFIX}/usr/local/include/mach-o/dyld_priv.h
TMPFILE=$(mktemp ${DERIVED_FILES_DIR}/dyld_priv.h.XXXXXX)

/bin/chmod 0644 $TMPFILE

while IFS="" read -r p || [ -n "$p" ]
do
  case "$p" in
    *@VERSION_DEFS* ) cat "$VERSIONS" >> $TMPFILE ;;
    *               ) echo "$p" >> $TMPFILE ;;
  esac
done < $DYLD_PRIV_IN

/usr/bin/rsync -vc $TMPFILE $DYLD_PRIV_OUT
/bin/rm -f $TMPFILE
