#!/bin/sh

source $SRCROOT/build-scripts/include.sh

env -i PATH="${PATH}" xcodebuild install -sdk ${SDKROOT} -configuration ${CONFIGURATION} -target dyld -target libdyld -target dyld_tests -target dyld_usage TOOLCHAINS="${TOOLCHAINS}" DSTROOT=${DERIVED_FILES_DIR}/TestRoot OBJROOT=${DERIVED_FILES_DIR}/objroot DISABLE_SDK_METADATA_PARSING=YES XCTestGenPath=${DERIVED_FILES_DIR}/XCTestGenerated.h GCC_PREPROCESSOR_DEFINITIONS='$GCC_PREPROCESSOR_DEFINITIONS BUILD_FOR_TESTING=1' || exit_if_error $? "Build failed"
${BUILT_PRODUCTS_DIR}/chroot_util -chroot ${DERIVED_FILES_DIR}/TestRoot -fallback / -add_file /bin/echo -add_file /bin/sh -add_file /bin/bash -add_file /bin/ls -add_file /usr/sbin/dtrace -add_file /sbin/mount -add_file /sbin/mount_devfs -add_file /usr/lib/libobjc-trampolines.dylib -add_file /usr/bin/leaks -add_file /System/iOSSupport/System/Library/Frameworks/UIKit.framework/UIKit || exit_if_error $? "Chroot build failed"
/bin/mkdir -p ${DERIVED_FILES_DIR}/TestRoot/dev
/bin/mkdir -m 777 -p ${DERIVED_FILES_DIR}/TestRoot/tmp

