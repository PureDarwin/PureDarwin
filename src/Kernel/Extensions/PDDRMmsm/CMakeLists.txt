# CMakeLists.txt - the clean-room adreno driver over the msm uapi, generations behind pd_msm_gpu_funcs
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/drm.cmake")

set(drmkmod "${PD_DRMKMOD_DIR}")
pd_drm_layout("${drmkmod}" drminc drmdefs drmopts)
pd_drm_core_includes("${drmkmod}" coreinc)

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
_fbsdkpi_objects(PDDRMmsm_drm SOURCES ${core}
    INCLUDES ${lkpiinc} ${coreinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
        KBUILD_MODNAME=\"drm\" LINUXKPI_PARAM_PREFIX=drm_
    OPTIONS ${lkpiopt})

# the driver itself, clean room from the adreno-cleanroom spec
set(msmsrc pd_msm_drv.c pd_msm_bo.c pd_msm_gpu.c pd_msm_smmu.c pd_msm_a3xx.c pd_msm_gcc8916.c)
list(TRANSFORM msmsrc PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
_fbsdkpi_objects(PDDRMmsm_msm SOURCES ${msmsrc}
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/include/uapi" "${drmkmod}/include/uapi/drm" ${lkpiinc} ${coreinc}
    DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${drmdefs}
        KBUILD_MODNAME=\"msm\" LINUXKPI_PARAM_PREFIX=msm_
    OPTIONS ${lkpiopt})

add_fbsdkpi_driver(PDDRMmsm
    CLASS PDDRMmsm
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDDRMmsm
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI_DRM
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
    LKPI_INCLUDES ${drminc}
    LKPI_DEFINITIONS ${drmdefs} KBUILD_MODNAME=\"linuxkpi\"
    LKPI_OPTIONS ${drmopts}
    OBJECTS PDDRMmsm_drm PDDRMmsm_msm
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDDRMPlatformDevice.cpp"
)

# the adreno attaches only when asked to with the pdmsm=1 boot-arg
target_compile_definitions(PDDRMmsm.kext PRIVATE PDDRM_OPT_IN_BOOTARG="pdmsm"
    PDDRMFramebuffer=PDDRMFramebufferMsm PDDRMPresenter=PDDRMPresenterMsm)
target_include_directories(PDDRMmsm.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/drm/include")
