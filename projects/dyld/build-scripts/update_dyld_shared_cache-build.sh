
if [ "${ACTION}" = "install" ]
then
    OBJROOT_LOCAL="${TARGET_TEMP_DIR}/Objects_Local"
    xcodebuild install -target dyld_shared_cache_builder SDKROOT="${SDKROOT}" MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} OBJROOT="${OBJROOT_LOCAL}" SRCROOT="${SRCROOT}" DSTROOT="${DSTROOT}" SYMROOT="${SYMROOT}" RC_ProjectSourceVersion="${RC_ProjectSourceVersion}" DISABLE_SDK_METADATA_PARSING=YES

    # On macOS, also install dyld_shared_cache_builder to the platform so that root_util can find it.
    if [ "${RC_PURPLE}" = "" ]
    then
      if [ "${PLATFORM_DIR}" != "" ]
      then
        # Note this is set to something like DEVELOPER_INSTALL_DIR=/Applications/Xcode.app/Contents/Developer
        mkdir -p ${DSTROOT}/${DEVELOPER_INSTALL_DIR}/Platforms/MacOSX.platform/usr/local/bin/
        cp ${DSTROOT}/usr/local/bin/dyld_shared_cache_builder ${DSTROOT}/${DEVELOPER_INSTALL_DIR}/Platforms/MacOSX.platform/usr/local/bin/dyld_shared_cache_builder
      fi
    fi

    if [ "${RC_PURPLE}" = "YES" ]
    then
        OBJROOT_UTILS="${TARGET_TEMP_DIR}/Objects_Utils"
        xcodebuild install -target dyld_closure_util -target dyld_shared_cache_util SDKROOT="${SDKROOT}" MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} OBJROOT="${OBJROOT_UTILS}" SRCROOT="${SRCROOT}" DSTROOT="${DSTROOT}" SYMROOT="${SYMROOT}" RC_ProjectSourceVersion="${RC_ProjectSourceVersion}" DISABLE_SDK_METADATA_PARSING=YES
        if [ "${RC_BRIDGE}" != "YES" ]
        then
            OBJROOT_SIM="${TARGET_TEMP_DIR}/Objects_Sim"
            xcodebuild install -target update_dyld_sim_shared_cache SDKROOT="${SDKROOT}" MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} OBJROOT="${OBJROOT_SIM}" SRCROOT="${SRCROOT}" DSTROOT="${DSTROOT}" SYMROOT="${SYMROOT}" RC_ProjectSourceVersion="${RC_ProjectSourceVersion}" DISABLE_SDK_METADATA_PARSING=YES
        fi
    else
        OBJROOT_MAC="${TARGET_TEMP_DIR}/Objects_Mac"
        xcodebuild install -target update_dyld_shared_cache_tool SDKROOT="${SDKROOT}" MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} OBJROOT="${OBJROOT_MAC}" SRCROOT="${SRCROOT}" DSTROOT="${DSTROOT}" SYMROOT="${SYMROOT}" RC_ProjectSourceVersion="${RC_ProjectSourceVersion}" DISABLE_SDK_METADATA_PARSING=YES
        OBJROOT_MAC="${TARGET_TEMP_DIR}/Objects2_Mac"
        xcodebuild install -target update_dyld_shared_cache_root_mode_tool SDKROOT="${SDKROOT}" MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} OBJROOT="${OBJROOT_MAC}" SRCROOT="${SRCROOT}" DSTROOT="${DSTROOT}" SYMROOT="${SYMROOT}" RC_ProjectSourceVersion="${RC_ProjectSourceVersion}" DISABLE_SDK_METADATA_PARSING=YES
    fi
fi

# On macOS build the kernel linker in to /usr/lib too.  It defaults to the toolchain
if [ "${ACTION}" != "installhdrs" ]
then
  if [ "${RC_PURPLE}" = "" ]
  then
    OBJROOT_MAC="${TARGET_TEMP_DIR}/Objects_Linker_Mac"
    xcodebuild ${ACTION} -target libKernelCollectionBuilder SDKROOT="${SDKROOT}" MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} OBJROOT="${OBJROOT_MAC}" SRCROOT="${SRCROOT}" DSTROOT="${DSTROOT}" SYMROOT="${SYMROOT}/usr/lib" RC_ProjectSourceVersion="${RC_ProjectSourceVersion}" LD_DYLIB_INSTALL_NAME="/usr/lib/libKernelCollectionBuilder.dylib" INSTALL_PATH="/usr/lib"
  fi
fi
