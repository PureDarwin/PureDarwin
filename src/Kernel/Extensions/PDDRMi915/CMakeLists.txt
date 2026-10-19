include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")

set(drmkmod "${PD_DRMKMOD_DIR}")
pd_drm_layout("${drmkmod}" drminc drmdefs drmopts)
pd_drm_core_includes("${drmkmod}" coreinc)
pd_drm_i915_includes("${drmkmod}" i915inc)

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
_fbsdkpi_objects(PDDRMi915_drm SOURCES ${core}
    INCLUDES ${lkpiinc} ${coreinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
        KBUILD_MODNAME=\"drm\" LINUXKPI_PARAM_PREFIX=drm_
    OPTIONS ${lkpiopt})

set(i915)
foreach(src IN LISTS PD_DRM_I915_SOURCES)
    list(APPEND i915 "${drmkmod}/drivers/gpu/drm/${src}")
endforeach()
foreach(src IN LISTS PD_DRM_I915_PD_SOURCES)
    list(APPEND i915 "${PD_DRM_DIR}/src/${src}")
endforeach()
set(i915defs FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs} I915 PD_DRM_FB_DRIVER_BUFFERS
    KBUILD_MODNAME=\"i915kms\" LINUXKPI_PARAM_PREFIX=i915_)
foreach(c IN LISTS PD_DRM_I915_KCONFIG)
    list(APPEND i915defs CONFIG_${c})
endforeach()
_fbsdkpi_objects(PDDRMi915_i915 SOURCES ${i915}
    INCLUDES ${lkpiinc} ${coreinc} ${i915inc}
    DEFINITIONS ${i915defs}
    OPTIONS ${lkpiopt} -Wno-pointer-arith -Wno-format -Wno-cast-qual)

add_fbsdkpi_driver(PDDRMi915
    CLASS PDDRMi915
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDDRMi915
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI_DRM
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
    LKPI_INCLUDES ${drminc}
    LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
    LKPI_OPTIONS ${drmopts}
    OBJECTS PDDRMi915_drm PDDRMi915_i915
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMDevice.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMFramebuffer.cpp"
)

# i915 attaches by default, pdi915=0 leaves the gpu to the GOP framebuffer for debugging
# its own framebuffer and presenter class names, PDDRMamdgpu can sit in the same aux KC
target_compile_definitions(PDDRMi915.kext PRIVATE PDDRM_OPT_OUT_BOOTARG="pdi915"
    PDDRM_INTEL_STOLEN=1 PDDRMFramebuffer=PDDRMFramebufferI915
    PDDRMPresenter=PDDRMPresenterI915)
target_include_directories(PDDRMi915.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOACPIFamily/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily"
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/include")
