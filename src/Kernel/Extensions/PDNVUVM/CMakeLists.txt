# PDNVUVM - NVIDIA's nvidia-uvm, unmodified from the 580 package (MIT files only), over linuxkpi and PDNVLoader
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")
set(nvuvm "${PD_NVIDIA_UVM_DIR}")

# the sources nvidia-uvm-sources.Kbuild lists, less uvm_common.c: the package's one GPL file, PD carries its own
file(STRINGS "${nvuvm}/nvidia-uvm/nvidia-uvm-sources.Kbuild" kbuild REGEX "^NVIDIA_UVM_SOURCES \\+= ")
set(uvm)
foreach(line IN LISTS kbuild)
    string(REGEX REPLACE "^NVIDIA_UVM_SOURCES \\+= nvidia-uvm/" "" src "${line}")
    string(STRIP "${src}" src)
    if(NOT src STREQUAL "uvm_common.c")
        list(APPEND uvm "${nvuvm}/nvidia-uvm/${src}")
    endif()
endforeach()

add_fbsdkpi_driver(PDNVUVM
    SHARED
    CLASS PDNVUVM
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDNVUVM
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${nvuvm}/nvidia-uvm" "${nvuvm}/nvidia" "${nvuvm}/common/inc" "${nvuvm}/nvidia-uvm/hwref"
    # what NVIDIA's Kbuild passes for nvidia-uvm (its Linux interface layer, not the BSD one nvidia-drm takes),
    # less __linux__, which would steer the FreeBSD headers under linuxkpi
    DEFINITIONS KBUILD_MODNAME=\"nvidia_uvm\" LINUXKPI_VERSION=70000 NVCPU_X86_64 NV_KERNEL_INTERFACE_LAYER
        NV_VERSION_STRING=\"580.196.07\" NVIDIA_UVM_ENABLED NVIDIA_UNDEF_LEGACY_BIT_MACROS Linux __KERNEL__ MODULE NVRM
        NV_UVM_ENABLE NV_FILESYSTEM_ACCESS_AVAILABLE=0 NV_SPECTRE_V2=0
        LINUXKPI_PARAM_PREFIX=nvidia_uvm_
    SOURCES ${uvm} "${CMAKE_CURRENT_SOURCE_DIR}/pd_uvm_mm.c" "${CMAKE_CURRENT_SOURCE_DIR}/pd_uvm_kthread.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/pd_uvm_common.c" "${CMAKE_CURRENT_SOURCE_DIR}/pd_uvm_glue.c" "${nvuvm}/nvidia/nv_uvm_interface.c"
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/PDNVUVM.cpp"
)
target_compile_options(PDNVUVM_driver PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/pd_uvm_pre.h" -fno-blocks -Wno-pointer-arith -Wno-format -Wno-cast-qual)
