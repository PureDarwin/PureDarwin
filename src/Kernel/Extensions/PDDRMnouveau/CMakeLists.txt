include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")

set(drmkmod "${PD_DRMKMOD_DIR}")
pd_drm_layout("${drmkmod}" drminc drmdefs drmopts)
pd_drm_core_includes("${drmkmod}" coreinc)
pd_drm_nouveau_includes("${drmkmod}" nvinc)

# the same linuxkpi directories add_fbsdkpi_driver gives its own objects
set(lkpiinc ${drminc}
    "${FBSDKPI_DIR}/pd/lkpi-override"
    "${FBSDKPI_DIR}/freebsd/linuxkpi/common/include"
    "${FBSDKPI_DIR}/freebsd/linuxkpi/dummy/include"
    "${FBSDKPI_DIR}/pd/fbsd-extra"
    "${FBSDKPI_DIR}/freebsd"
    "${FBSDKPI_DIR}/haiku/freebsd_iflib/compat")
set(lkpiopt "SHELL:-include ${FBSDKPI_DIR}/pd/fbsd-extra/pd_lkpi_pre.h"
    ${drmopts} -Werror=implicit-function-declaration)

set(core)
foreach(src IN LISTS PD_DRM_CORE_SOURCES)
    list(APPEND core "${drmkmod}/drivers/gpu/drm/${src}")
endforeach()
foreach(src IN LISTS PD_DRM_DMABUF_SOURCES)
    list(APPEND core "${drmkmod}/drivers/dma-buf/${src}")
endforeach()
foreach(src IN LISTS PD_DRM_VIDEO_SOURCES)
    list(APPEND core "${drmkmod}/drivers/video/${src}")
endforeach()
foreach(src IN LISTS PD_DRM_PD_SOURCES)
    list(APPEND core "${PD_DRM_DIR}/src/${src}")
endforeach()
_fbsdkpi_objects(PDDRMnouveau_drm SOURCES ${core}
    INCLUDES ${lkpiinc} ${coreinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
        KBUILD_MODNAME=\"drm\" LINUXKPI_PARAM_PREFIX=drm_
    OPTIONS ${lkpiopt})

set(nv)
foreach(src IN LISTS PD_DRM_NOUVEAU_SOURCES)
    list(APPEND nv "${drmkmod}/drivers/gpu/drm/nouveau/${src}")
endforeach()
foreach(src IN LISTS PD_DRM_NOUVEAU_PD_SOURCES)
    list(APPEND nv "${PD_DRM_DIR}/src/${src}")
endforeach()
_fbsdkpi_objects(PDDRMnouveau_nouveau SOURCES ${nv}
    INCLUDES ${lkpiinc} ${coreinc} ${nvinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs} CONFIG_DRM_NOUVEAU PD_DRM_FB_DRIVER_BUFFERS
        CONFIG_NOUVEAU_I2C_INTERNAL CONFIG_NOUVEAU_I2C_INTERNAL_DEFAULT
        KBUILD_MODNAME=\"nouveau\" LINUXKPI_PARAM_PREFIX=nouveau_
    OPTIONS ${lkpiopt} "SHELL:-include ${PD_DRM_DIR}/include/pd_nouveau_pre.h"
        -Wno-pointer-arith -Wno-format -Wno-cast-qual)

add_fbsdkpi_driver(PDDRMnouveau
    CLASS PDDRMnouveau
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDDRMnouveau
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI_DRM
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
    LKPI_INCLUDES ${drminc}
    LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
    LKPI_OPTIONS ${drmopts}
    OBJECTS PDDRMnouveau_drm PDDRMnouveau_nouveau
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMDevice.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMFramebuffer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/pstate_sysctl.cpp"
)

# nouveau attaches only when asked to with the pdnouveau=1 boot-arg
# its own framebuffer and presenter class names, so it shares the aux KC with amdgpu and i915
target_compile_definitions(PDDRMnouveau.kext PRIVATE PDDRM_OPT_IN_BOOTARG="pdnouveau"
    PDDRMFramebuffer=PDDRMFramebufferNV PDDRMPresenter=PDDRMPresenterNV)
target_include_directories(PDDRMnouveau.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOACPIFamily/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily"
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/include")
