# PDDRMnvidia - NVIDIA's nvidia-drm, unmodified from the 580 package, over drm-kmod and the modeset core in PDNVLoader
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")

set(drmkmod "${PD_DRMKMOD_DIR}")
set(nvdrm "${PD_NVIDIA_DRM_DIR}")
pd_drm_layout("${drmkmod}" drminc drmdefs drmopts)
pd_drm_core_includes("${drmkmod}" coreinc)

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
# x86 links the drm core, linuxkpi and fbsdkpi from PDDRM and the libraries under it, arm64 still carries its own
set(shared)
set(names)
set(drmobj)
if(PUREDARWIN_ARCH STREQUAL "x86_64")
    set(shared SHARED)
    set(names ${FBSDKPI_NAMES} ${FBSDKPI_LKPI_NAMES} ${FBSDKPI_DRM_NAMES})
else()
    _fbsdkpi_objects(PDDRMnvidia_drm SOURCES ${core}
        INCLUDES ${lkpiinc} ${coreinc}
        DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
            KBUILD_MODNAME=\"drm\" LINUXKPI_PARAM_PREFIX=drm_
        OPTIONS ${lkpiopt})
    set(drmobj PDDRMnvidia_drm)
endif()

# the sources nvidia-drm-sources.mk lists, with PD's conftest answers in front of the package's common headers
set(nv nvidia-drm.c nvidia-drm-drv.c nvidia-drm-utils.c nvidia-drm-crtc.c nvidia-drm-encoder.c
    nvidia-drm-connector.c nvidia-drm-gem.c nvidia-drm-fb.c nvidia-drm-modeset.c nvidia-drm-fence.c
    nvidia-drm-helper.c nv-pci-table.c nvidia-drm-gem-nvkms-memory.c
    nvidia-drm-gem-user-memory.c nvidia-drm-gem-dma-buf.c nvidia-drm-format.c nvidia-drm-os-interface.c)
list(TRANSFORM nv PREPEND "${nvdrm}/nvidia-drm/")
# nv-kthread-q.c is linux kthread internals, PD brings its own queue in its place
list(APPEND nv "${CMAKE_CURRENT_SOURCE_DIR}/pd_nv_kthread_q.c"
    # the device probe and the display client's buffers, and the display client PDDRMFramebuffer talks to
    "${CMAKE_CURRENT_SOURCE_DIR}/pd_nvidia_fb.c" "${PD_DRM_DIR}/src/pd_amdgpu_fb.c")
_fbsdkpi_objects(PDDRMnvidia_nvidia SOURCES ${nv}
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}" "${nvdrm}/nvidia-drm" "${nvdrm}/common/inc" ${lkpiinc} ${coreinc}
        "${PD_DRM_DIR}/include"
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs} PD_DRM_FB_DRIVER_BUFFERS NV_BSD NVCPU_X86_64 NV_KERNEL_INTERFACE_LAYER
        NV_VERSION_STRING=\"580.196.07\"
        KBUILD_MODNAME=\"nvidia_drm\" LINUXKPI_PARAM_PREFIX=nvidia_drm_
    OPTIONS ${names} ${lkpiopt} "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/pd_nvdrm_pre.h"
        -Wno-pointer-arith -Wno-format -Wno-cast-qual)

add_fbsdkpi_driver(PDDRMnvidia
    ${shared}
    CLASS PDDRMnvidia
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDDRMnvidia
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI_DRM
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
    LKPI_INCLUDES ${drminc}
    LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
    LKPI_OPTIONS ${drmopts}
    OBJECTS ${drmobj} PDDRMnvidia_nvidia
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/PDDRMnvidia.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMFramebuffer.cpp"
)

# its own framebuffer and presenter class names, so it shares the aux KC with the other drm drivers
target_compile_definitions(PDDRMnvidia.kext PRIVATE
    PDDRMFramebuffer=PDDRMFramebufferNVIDIA PDDRMPresenter=PDDRMPresenterNVIDIA)
target_include_directories(PDDRMnvidia.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOACPIFamily/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily"
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/include")
