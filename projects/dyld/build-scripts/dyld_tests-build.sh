#!/bin/sh

set -e

source $SRCROOT/build-scripts/include.sh

# Exit on failure

OBJROOT_DYLD_APP_CACHE_UTIL="${TARGET_TEMP_DIR}/Objects_Dyld_App_Cache_Util"
OBJROOT_RUN_STATIC="${TARGET_TEMP_DIR}/Objects_Run_Static"

SYMROOT=${BUILD_DIR}/${CONFIGURATION}${EFFECTIVE_PLATFORM_NAME}/dyld_tests
OBJROOT=${PROJECT_TEMP_DIR}/${CONFIGURATION}${EFFECTIVE_PLATFORM_NAME}
SDKROOT=${SDKROOT:-$(xcrun -sdk macosx.internal --show-sdk-path)}
DEPLOYMENT_TARGET_CLANG_FLAG_NAME=${DEPLOYMENT_TARGET_CLANG_FLAG_NAME:-"mmacosx-version-min"}
DERIVED_FILES_DIR=${DERIVED_FILES_DIR}
LDFLAGS="-L$BUILT_PRODUCTS_DIR"
#LLBUILD=$(xcrun --sdk $SDKROOT --find llbuild 2> /dev/null)
NINJA=${LLBUILD:-`xcrun  --sdk $SDKROOT --find ninja  2> /dev/null`}
BUILD_TARGET=${ONLY_BUILD_TEST:-all}

if [ ! -z "$LLBUILD" ]; then
  NINJA="$LLBUILD ninja build"
fi

OSVERSION="10.14"
if [ ! -z "$DEPLOYMENT_TARGET_CLANG_ENV_NAME" ]; then
    OSVERSION=${!DEPLOYMENT_TARGET_CLANG_ENV_NAME}
fi

if [ -z "$SRCROOT" ]; then
    echo "Error $$SRCROOT must be set"
fi

if [ -z "$ARCHS" ]; then
    PLATFORM_NAME=${PLATFORM_NAME:macosx}
    case "$PLATFORM_NAME" in
       "watchos")   ARCHS="armv7k arm64_32"
       ;;
       "appletvos") ARCHS="arm64"
       ;;
       *)    ARCHS=${ARCHS_STANDARD}
       ;;
    esac
fi

if [ -z "$ARCHS" ]; then
    ARCHS="x86_64"
fi

/bin/mkdir -p ${DERIVED_FILES_DIR}
TMPFILE=$(mktemp ${DERIVED_FILES_DIR}/config.ninja.XXXXXX)

echo "OBJROOT = $OBJROOT" >> $TMPFILE
echo "OSFLAG = $DEPLOYMENT_TARGET_CLANG_FLAG_NAME" >> $TMPFILE
echo "OSVERSION = $OSVERSION" >> $TMPFILE
echo "SDKROOT = $SDKROOT" >> $TMPFILE
echo "SRCROOT = $SRCROOT" >> $TMPFILE
echo "SYMROOT = $SYMROOT" >> $TMPFILE
echo "BUILT_PRODUCTS_DIR = $BUILT_PRODUCTS_DIR" >> $TMPFILE
echo "INSTALL_GROUP = $INSTALL_GROUP" >> $TMPFILE
echo "INSTALL_MODE_FLAG = $INSTALL_MODE_FLAG" >> $TMPFILE
echo "INSTALL_OWNER = $INSTALL_OWNER" >> $TMPFILE
echo "INSTALL_DIR = $INSTALL_DIR" >> $TMPFILE
echo "USER_HEADER_SEARCH_PATHS = $USER_HEADER_SEARCH_PATHS" >> $TMPFILE
echo "SYSTEM_HEADER_SEARCH_PATHS = $SYSTEM_HEADER_SEARCH_PATHS" >> $TMPFILE
echo "ARCHS = $ARCHS" >> $TMPFILE
echo "DERIVED_FILES_DIR = $DERIVED_FILES_DIR" >> $TMPFILE
echo "LDFLAGS = $LDFLAGS" >> $TMPFILE

/usr/bin/rsync -vc $TMPFILE ${DERIVED_FILES_DIR}/config.ninja
/bin/rm -f $TMPFILE

xcodebuild install -target run-static SDKROOT="${SDKROOT}" MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} OBJROOT="${OBJROOT_RUN_STATIC}" SRCROOT="${SRCROOT}" DSTROOT="${DSTROOT}" SYMROOT="${SYMROOT}" RC_ProjectSourceVersion="${RC_ProjectSourceVersion}" DISABLE_SDK_METADATA_PARSING=YES

xcodebuild install -target dyld_app_cache_util -sdk macosx.internal -configuration ${CONFIGURATION} MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} OBJROOT="${OBJROOT_DYLD_APP_CACHE_UTIL}" SRCROOT="${SRCROOT}" DSTROOT="${BUILT_PRODUCTS_DIR}" SYMROOT="${SYMROOT}" RC_ProjectSourceVersion="${RC_ProjectSourceVersion}" INSTALL_PATH="/host_tools" RC_ARCHS="${NATIVE_ARCH_ACTUAL}" DISABLE_SDK_METADATA_PARSING=YES

${SRCROOT}/testing/build_ninja.py ${DERIVED_FILES_DIR}/config.ninja || exit_if_error $? "Generating build.ninja failed"
${NINJA} -C ${DERIVED_FILES_DIR} ${BUILD_TARGET} || exit_if_error $? "Ninja build failed"
