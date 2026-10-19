include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")

set(drmkmod "${PD_DRMKMOD_DIR}")
pd_drm_layout("${drmkmod}" drminc drmdefs drmopts)
pd_drm_core_includes("${drmkmod}" coreinc)
pd_drm_amdgpu_includes("${drmkmod}" amdinc)

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
_fbsdkpi_objects(PDDRMamdgpu_drm SOURCES ${core}
    INCLUDES ${lkpiinc} ${coreinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
        KBUILD_MODNAME=\"drm\" LINUXKPI_PARAM_PREFIX=drm_
    OPTIONS ${lkpiopt})

set(amd)
set(amdfp)
foreach(src IN LISTS PD_DRM_AMDGPU_SOURCES)
    get_filename_component(base "${src}" NAME)
    if(base IN_LIST PD_DRM_AMDGPU_FP_SOURCES)
        list(APPEND amdfp "${drmkmod}/drivers/gpu/drm/${src}")
    else()
        list(APPEND amd "${drmkmod}/drivers/gpu/drm/${src}")
    endif()
endforeach()
foreach(src IN LISTS PD_DRM_AMDGPU_PD_SOURCES)
    list(APPEND amd "${PD_DRM_DIR}/src/${src}")
endforeach()
set(amddefs FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
    KBUILD_MODNAME=\"amdgpu\" LINUXKPI_PARAM_PREFIX=amdgpu_)
_fbsdkpi_objects(PDDRMamdgpu_amd SOURCES ${amd}
    INCLUDES ${lkpiinc} ${coreinc} ${amdinc}
    DEFINITIONS ${amddefs}
    OPTIONS ${lkpiopt})
# dc's display mode library computes in floating point between kernel_fpu_begin
# and kernel_fpu_end
_fbsdkpi_objects(PDDRMamdgpu_amdfp SOURCES ${amdfp}
    INCLUDES ${lkpiinc} ${coreinc} ${amdinc}
    DEFINITIONS ${amddefs}
    OPTIONS ${lkpiopt} -msse -msse2 -mstack-alignment=4)

add_fbsdkpi_driver(PDDRMamdgpu
    CLASS PDDRMamdgpu
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDDRMamdgpu
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI_DRM
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
    LKPI_INCLUDES ${drminc}
    LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
    LKPI_OPTIONS ${drmopts}
    OBJECTS PDDRMamdgpu_drm PDDRMamdgpu_amd PDDRMamdgpu_amdfp
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMDevice.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMFramebuffer.cpp"
)

# amdgpu attaches by default, pdamdgpu=0 leaves the gpu to the GOP framebuffer for debugging
target_compile_definitions(PDDRMamdgpu.kext PRIVATE PDDRM_OPT_OUT_BOOTARG="pdamdgpu")

# the vbios comes out of the VFCT table the acpi platform expert hands out
target_include_directories(PDDRMamdgpu.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOACPIFamily/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily"
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/include")
