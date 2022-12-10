# dyld should make all of the following:
#
# - /usr/libexec/dyld
#   - ${SRCROOT}/build-scripts/configure-dyld-archives.sh
#   - compile
#   - if [ "${RC_PURPLE}" = "YES" ]
#     then
#         mkdir -p ${DSTROOT}//System/Library/Caches/com.apple.dyld
#         echo "existence of this file enables dyld to have dylibs override shared cache" > ${DSTROOT}//System/Library/Caches/com.apple.dyld/enable-dylibs-to-override-cache
#     fi
#
# - /usr/lib/system/libdyld.dylib
#   - compile
#   - public headers
#   - private headers
#   - $SRCROOT/build-scripts/libdyld-generate-version-headers.sh (${SRCROOT}/include/mach-o/dyld_priv.h)
#   - cp ${SRCROOT}/include/dlfcn.h  ${DSTROOT}/usr/include/
#   - cp ${SRCROOT}/include/dlfcn_private.h  ${DSTROOT}/usr/local/include/
#   - cp ${SRCROOT}/include/mach-o/dyld.modulemap  ${DSTROOT}/usr/include/mach-o
#   - /usr/local/share/misc/dyld.codes
#   - /usr/local/include/objc-shared-cache.h
#
# - libKernelCollectionBuilder ($(DT_TOOLCHAIN_DIR)/usr/lib/libKernelCollectionBuilder.dylib)
#   - compile
#   - link with CoreFoundation.framework
#   - $(INSTALL_LOCATION)/usr/local/include/kernel_collection_builder.h
#
# - /usr/local/lib/libdsc.a
#   - compile
#   - /usr/local/include/mach-o/dsc_{extractor,iterator}.h
#
# - /usr/lib/dsc_extractor.bundle
#   - compile
#
# - libslc_builder.dylib
#   - $SRCROOT/build-scripts/generate-cache-config-header.sh
#   - compile
#   - link with Foundation.framework
#
# - $SRCROOT/build-scripts/update_dyld_shared_cache-build.sh
#

# - /usr/local/bin/dyld_shared_cache_util
# - /usr/local/bin/dyld_closure_util
# - /usr/local/bin/dyld_info
# - /usr/local/bin/dyld_usage

# - /usr/local/bin/dyld_shared_cache_builder
# - /usr/local/bin/dyld_app_cache_util

# - /usr/share/man/man1/dyld.1
# - /usr/share/man/man3/dladdr.3
# - /usr/share/man/man3/dlclose.3
# - /usr/share/man/man3/dlerror.3
# - /usr/share/man/man3/dlopen.3
# - /usr/share/man/man3/dlsym.3
# - /usr/share/man/man3/dyld.3
# - /usr/share/man/man3/dlopen_preflight.3



# set_source_files_properties(foo.cpp PROPERTIES COMPILE_FLAGS -Wno-effc++)


# We have a dependency on....
# - libsystem_kernel
# - libsystem_platform
# - libsystem_c
# - libdispatch
# - libpthread
# - libsystem_sandbox
# - libcorecrypto
# - libCommonCrypto
