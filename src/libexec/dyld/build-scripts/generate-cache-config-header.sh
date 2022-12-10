#!/bin/sh

/bin/echo "" > ${DERIVED_FILE_DIR}/dyld_cache_config.h

if [ -z "${ARM_SDK}" ]; then
    # if iOS SDK not available, use MacOSX SDK
    ARM_SDK=`xcrun -sdk macosx.internal --show-sdk-path`
fi

SHARED_REGION_FILE="${ARM_SDK}/usr/include/mach/shared_region.h"


if [ -r "${SHARED_REGION_FILE}" ]; then
    /bin/echo -n "#define ARM_SHARED_REGION_START "  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
    awk '/define SHARED_REGION_BASE_ARM[ \t]/ { print $3;}' "${SHARED_REGION_FILE}" >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
    /bin/echo ""  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h

    /bin/echo -n "#define ARM_SHARED_REGION_SIZE "  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
    awk '/define SHARED_REGION_SIZE_ARM[ \t]/ { print $3;}' "${SHARED_REGION_FILE}" >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
    /bin/echo ""  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h

    /bin/echo -n "#define ARM64_SHARED_REGION_START "  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
    awk '/define SHARED_REGION_BASE_ARM64[ \t]/ { print $3;}' "${SHARED_REGION_FILE}" >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
    /bin/echo ""  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h

    /bin/echo -n "#define ARM64_SHARED_REGION_SIZE "  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
    awk '/define SHARED_REGION_SIZE_ARM64[ \t]/ { print $3;}' "${SHARED_REGION_FILE}" >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
    /bin/echo ""  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h

    grep SHARED_REGION_BASE_ARM64_32 "${SHARED_REGION_FILE}" > /dev/null 2>&1
    if [ "$?" -eq "0" ]; then
        /bin/echo -n "#define ARM64_32_SHARED_REGION_START "  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
        awk '/define SHARED_REGION_BASE_ARM64_32/ { print $3;}' "${SHARED_REGION_FILE}" >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
        /bin/echo ""  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h

        /bin/echo -n "#define ARM64_32_SHARED_REGION_SIZE "  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
        awk '/define SHARED_REGION_SIZE_ARM64_32/ { print $3;}' "${SHARED_REGION_FILE}" >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
        /bin/echo ""  >> ${DERIVED_FILE_DIR}/dyld_cache_config.h
    fi
else
    /bin/echo "ERROR: File needed to configure update_dyld_shared_cache does not exist '${SHARED_REGION_FILE}'"
    exit 1
fi

if [ -r "${ARM_SDK}/AppleInternal/DirtyDataFiles/dirty-data-segments-order.txt" ]; then
    mkdir -p "${DSTROOT}/${INSTALL_LOCATION}/usr/local/bin"
    cp "${ARM_SDK}/AppleInternal/DirtyDataFiles/dirty-data-segments-order.txt"  "${DSTROOT}/${INSTALL_LOCATION}/usr/local/bin"
fi
if [ -r "${ARM_SDK}/AppleInternal/OrderFiles/dylib-order.txt" ]; then
    mkdir -p "${DSTROOT}/${INSTALL_LOCATION}/usr/local/bin"
    cp "${ARM_SDK}/AppleInternal/OrderFiles/dylib-order.txt"  "${DSTROOT}/${INSTALL_LOCATION}/usr/local/bin"
fi

