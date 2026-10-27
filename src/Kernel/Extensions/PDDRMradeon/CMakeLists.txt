include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")

set(drmkmod "${PD_DRMKMOD_DIR}")
pd_drm_layout("${drmkmod}" drminc drmdefs drmopts)
pd_drm_core_includes("${drmkmod}" coreinc)
pd_drm_radeon_includes("${drmkmod}" radinc)

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
_fbsdkpi_objects(PDDRMradeon_drm SOURCES ${core}
    INCLUDES ${lkpiinc} ${coreinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
        KBUILD_MODNAME=\"drm\" LINUXKPI_PARAM_PREFIX=drm_
    OPTIONS ${lkpiopt})

set(rad)
foreach(src IN LISTS PD_DRM_RADEON_SOURCES)
    list(APPEND rad "${drmkmod}/drivers/gpu/drm/radeon/${src}")
endforeach()
foreach(src IN LISTS PD_DRM_RADEON_PD_SOURCES)
    list(APPEND rad "${PD_DRM_DIR}/src/${src}")
endforeach()
# drm-kmod's radeon/Makefile names the module radeonkms
_fbsdkpi_objects(PDDRMradeon_radeon SOURCES ${rad}
    INCLUDES ${lkpiinc} ${coreinc} ${radinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs} CONFIG_DRM_RADEON PD_DRM_FB_DRIVER_BUFFERS
        KBUILD_MODNAME=\"radeonkms\" LINUXKPI_PARAM_PREFIX=radeon_ DRM_SYSCTL_PARAM_PREFIX=_radeonkms
    OPTIONS ${lkpiopt} -Wno-pointer-arith -Wno-format -Wno-cast-qual)

add_fbsdkpi_driver(PDDRMradeon
    CLASS PDDRMradeon
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDDRMradeon
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI_DRM
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
    LKPI_INCLUDES ${drminc}
    LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
    LKPI_OPTIONS ${drmopts}
    OBJECTS PDDRMradeon_drm PDDRMradeon_radeon
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMDevice.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMFramebuffer.cpp"
)

# radeon attaches by default, pdradeon=0 leaves the gpu to the GOP framebuffer
# its own framebuffer and presenter class names, so it shares the aux KC with amdgpu
target_compile_definitions(PDDRMradeon.kext PRIVATE PDDRM_OPT_OUT_BOOTARG="pdradeon"
    PDDRMFramebuffer=PDDRMFramebufferRadeon PDDRMPresenter=PDDRMPresenterRadeon)
target_include_directories(PDDRMradeon.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOACPIFamily/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily"
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/include")
