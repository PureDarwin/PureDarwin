#!/bin/sh


#LLBUILD=$(xcrun --sdk $SDKROOT --find llbuild 2> /dev/null)
NINJA=${LLBUILD:-`xcrun  --sdk $SDKROOT --find ninja  2> /dev/null`}
INSTALL_TARGET="install"

if [ ! -z "$LLBUILD" ]; then
  NINJA="$LLBUILD ninja build"
fi

if [ ! -z "$ONLY_BUILD_TEST" ]; then
    INSTALL_TARGET="install-$BUILD_ONLY"
fi

${NINJA} -C ${DERIVED_FILES_DIR} ${INSTALL_TARGET} || exit_if_error $? "Ninja install failed"
