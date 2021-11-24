#!/bin/bash
set -x
if [ "${ACTION}" != "install" ]; then exit 0; fi
if [ "${DRIVERKIT}" = 1 ]; then exit 0; fi

MKDIR="mkdir -p"
PLUTIL="plutil"

SRC=$1
FILENAME=$(basename $1)

# Installs ErrnoErrors.strings into CoreTypes.bundle

CORETYPESFRAMEWORK=${DSTROOT}/System/Library/CoreServices/CoreTypes.bundle
${MKDIR} ${CORETYPESFRAMEWORK}/Contents/Resources/en.lproj
${PLUTIL} -convert binary1 -o ${CORETYPESFRAMEWORK}/Contents/Resources/en.lproj/${FILENAME} $1
