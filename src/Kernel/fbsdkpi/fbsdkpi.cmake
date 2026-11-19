set(FBSDKPI_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(FBSDKPI_NETWORK_SOURCES
    fbsd_ether.c fbsd_if.c fbsd_if_media.c fbsd_kern_mbuf.c fbsd_uipc_mbuf.c
    fbsd_uipc_mbuf2.c fbsd_subr_bufring.c fbsd_subr_sbuf.c fbsd_time.c
    fbsd_timingsafe.c bus.cpp bus_dma.cpp callout.cpp clock.c condvar.cpp
    device.c eventhandler.c if.c kthread.cpp libkern.cpp malloc.cpp mbuf.c
    mutex.c pci.cpp priv.cpp smp.c subr_autoconf.cpp synch.cpp systm.cpp
    sysinit.c taskqueue.c unit.cpp)

# linuxkpi and the PureDarwin runtime under it, shared by wifi and drm drivers
set(FBSDKPI_LINUXKPI_SOURCES
    linux_compat.c linux_devres.c linux_domain.c linux_firmware.c
    linux_hrtimer.c linux_idr.c linux_interrupt.c linux_kobject.c
    linux_kthread.c linux_lock.c linux_math.c linux_pci.c linux_radix.c
    linux_rcu.c linux_schedule.c linux_seq_file.c linux_simple_attr.c
    linux_slab.c linux_tasklet.c linux_timesub.c linux_work.c linux_xarray.c)

set(FBSDKPI_LINUXKPI_PD_SOURCES
    pd_linux_current.c pd_linux_page.c pd_lkpi_kern.c pd_lkpi_uma.c
    pd_lkpi_libkern.c pd_lkpi_misc.c pd_lkpi_acpi.c
    pd_linux_of.c pd_linux_platform.c pd_linux_clk.c)

# linux wifi drivers add net80211 and the wlan glue
set(FBSDKPI_LINUXKPI_WLAN_SOURCES linux_80211.c linux_80211_macops.c
    linux_netdev.c linux_skbuff.c)
set(FBSDKPI_LINUXKPI_WLAN_PD_SOURCES pd_lkpi_wlan.c pd_lkpi_wlanctl.c pd_wpa.c)

# drm drivers add these
set(FBSDKPI_LINUXKPI_DRM_SOURCES linux_cmdline.c linux_folio.c
    linux_hdmi.c linux_seq_buf.c linux_shrinker.c linux_siphash.c)

set(FBSDKPI_NET80211_SOURCES
    ieee80211.c ieee80211_acl.c ieee80211_action.c ieee80211_adhoc.c
    ieee80211_ageq.c ieee80211_amrr.c ieee80211_crypto.c
    ieee80211_crypto_ccmp.c ieee80211_crypto_none.c ieee80211_crypto_tkip.c
    ieee80211_crypto_wep.c ieee80211_dfs.c ieee80211_freebsd.c ieee80211_ht.c
    ieee80211_hostap.c ieee80211_input.c ieee80211_ioctl.c ieee80211_monitor.c
    ieee80211_node.c ieee80211_output.c ieee80211_phy.c ieee80211_power.c
    ieee80211_proto.c ieee80211_radiotap.c ieee80211_ratectl.c
    ieee80211_ratectl_none.c ieee80211_regdomain.c ieee80211_scan.c
    ieee80211_scan_sta.c ieee80211_scan_sw.c ieee80211_sta.c
    ieee80211_superg.c ieee80211_vht.c ieee80211_wds.c ieee80211_xauth.c)

# haiku's freebsd usb shim, for drivers that sit on a usb interface
set(FBSDKPI_USB_SOURCES
    usb.cpp usb_util.c dev/usb/fbsd_usb_error.c dev/usb/fbsd_usb_util.c
    dev/usb/fbsd_usb_lookup.c dev/usb/usb_process.c)

set(FBSDKPI_IFLIB_SOURCES
    device_if.c ifdi_if.c iflib.c kobj.c mp_ring.c strsep.c
    subr_gtaskqueue.c)

# one object library of FreeBSD-side code, which never sees an xnu header
function(_fbsdkpi_objects target)
    cmake_parse_arguments(FO "VISIBLE" "" "SOURCES;INCLUDES;DEFINITIONS;OPTIONS" ${ARGN})
    add_library(${target} OBJECT ${FO_SOURCES})
    file(GLOB osdirs LIST_DIRECTORIES true "${FBSDKPI_DIR}/pd/haiku-headers/os/*")
    target_include_directories(${target} BEFORE PRIVATE
        ${FO_INCLUDES}
        "${FBSDKPI_DIR}/haiku/freebsd_network/compat"
        "${FBSDKPI_DIR}/pd/haiku-api"
        "${FBSDKPI_DIR}/haiku/freebsd_network"
        "${FBSDKPI_DIR}/pd"
        "${FBSDKPI_DIR}"
        "${FBSDKPI_DIR}/pd/haiku-headers/private-kernel"
        "${FBSDKPI_DIR}/pd/haiku-headers"
        "${FBSDKPI_DIR}/pd/haiku-headers/os"
        # posix ahead of os/*, else a case-insensitive fs answers <string.h> with os/support/String.h
        "${FBSDKPI_DIR}/pd/haiku-headers/posix"
        ${osdirs})
    target_compile_definitions(${target} PRIVATE
        _KERNEL=1 _KERNEL_MODE=1 __HAIKU__=1 __PUREDARWIN__=1
        # the kernel's own panic stays reachable from the xnu side
        panic=fbsd_panic
        ${FO_DEFINITIONS})
    target_compile_options(${target} PRIVATE
        -ffreestanding -nostdlibinc -fno-common -fno-stack-protector
        # names like printf, malloc and wakeup must not leak to later kexts, the shared layer exports its own
        $<$<NOT:$<BOOL:${FO_VISIBLE}>>:-fvisibility=hidden>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -std=gnu++17 -fapple-kext>
        $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
        -Wno-everything
        ${FO_OPTIONS})
    if(NOT PUREDARWIN_ARM64 AND NOT PUREDARWIN_ARM32)
        target_compile_options(${target} PRIVATE -mno-red-zone)
    endif()
endfunction()

# firmware images are linked in, named by their path below the firmware root
# argument pairs: <name used by the driver> <file>
function(_fbsdkpi_firmware name out)
    set(asm "${CMAKE_CURRENT_BINARY_DIR}/${name}_firmware.S")
    set(table "${CMAKE_CURRENT_BINARY_DIR}/${name}_firmware.c")
    set(asmtext ".section __TEXT,__const\n")
    set(tabletext "#include \"pd_firmware.h\"\n")
    set(entries "")
    set(index 0)
    set(args ${ARGN})
    list(LENGTH args n)
    while(index LESS n)
        list(GET args ${index} fwname)
        math(EXPR next "${index} + 1")
        list(GET args ${next} fwfile)
        math(EXPR i "${index} / 2")
        string(APPEND asmtext ".p2align 4\n.globl _pdfw_${i}_start\n.private_extern _pdfw_${i}_start\n_pdfw_${i}_start:\n.incbin \"${fwfile}\"\n.globl _pdfw_${i}_end\n.private_extern _pdfw_${i}_end\n_pdfw_${i}_end:\n")
        string(APPEND tabletext "extern const unsigned char pdfw_${i}_start[], pdfw_${i}_end[];\n")
        string(APPEND entries "    { \"${fwname}\", pdfw_${i}_start, pdfw_${i}_end },\n")
        math(EXPR index "${index} + 2")
    endwhile()
    string(APPEND tabletext "const struct pd_firmware_image pd_firmware_images[] = {\n${entries}    { 0, 0, 0 }\n};\n")
    math(EXPR count "${n} / 2")
    string(APPEND tabletext "const int pd_firmware_image_count = ${count};\n")
    file(WRITE "${asm}" "${asmtext}")
    file(WRITE "${table}" "${tabletext}")
    set_property(SOURCE "${asm}" PROPERTY LANGUAGE C)
    set(${out} "${table}" PARENT_SCOPE)
    if(n GREATER 0)
        set(${out} "${table}" "${asm}" PARENT_SCOPE)
    endif()
endfunction()

# a library kext over PDFreeBSDKPI: its start routine hands the layer the SYSINIT sets in its own binary
function(_fbsdkpi_library_start name)
    _fbsdkpi_objects(${name}_start SOURCES "${FBSDKPI_DIR}/pd/pd_fbsd_library.c" OPTIONS ${FBSDKPI_NAMES})
    target_sources(${name}.kext PRIVATE $<TARGET_OBJECTS:${name}_start>)
endfunction()

# the shared layer: core and iflib once, as a library kext the drivers link against
set(FBSDKPI_NAMES "SHELL:-include ${FBSDKPI_DIR}/pd/pd_kpi_names.h")

function(_fbsdkpi_core_sources out usb)
    set(core)
    foreach(src IN LISTS FBSDKPI_NETWORK_SOURCES)
        list(APPEND core "${FBSDKPI_DIR}/haiku/freebsd_network/${src}")
    endforeach()
    list(APPEND core
        "${FBSDKPI_DIR}/pd/pd_haiku_kernel.cpp"
        "${FBSDKPI_DIR}/pd/pd_pci.c"
        "${FBSDKPI_DIR}/pd/pd_fbsd_glue.c"
        "${FBSDKPI_DIR}/pd/pd_driver.c"
        "${FBSDKPI_DIR}/pd/pd_kernel_util.c"
        "${FBSDKPI_DIR}/pd/pd_firmware.c"
        "${FBSDKPI_DIR}/haiku/kernel_util/RadixBitmap.cpp"
        "${FBSDKPI_DIR}/haiku/kernel_util/Random.cpp")
    if(usb)
        foreach(src IN LISTS FBSDKPI_USB_SOURCES)
            list(APPEND core "${FBSDKPI_DIR}/haiku/freebsd_network/${src}")
        endforeach()
        list(APPEND core "${FBSDKPI_DIR}/pd/pd_usb.cpp")
    endif()
    set(${out} ${core} PARENT_SCOPE)
endfunction()

function(_fbsdkpi_xnu_target name class)
    set(shim "${CMAKE_CURRENT_BINARY_DIR}/shim-include")
    set(ionf "${FBSDKPI_DIR}/../Extensions/IONetworkingFamily")
    file(MAKE_DIRECTORY "${shim}/IOKit/network")
    foreach(hdr
        IOBasicOutputQueue.h IOEthernetController.h IOEthernetInterface.h
        IOEthernetStats.h IOGatedOutputQueue.h IOKernelDebugger.h
        IOMbufMemoryCursor.h IOMbufQueue.h IONetworkController.h
        IONetworkControllerPrivate.h IONetworkData.h IONetworkDebug.h
        IONetworkInterface.h IONetworkMedium.h IONetworkStack.h
        IONetworkStats.h IONetworkTypesPrivate.h IONetworkUserClient.h
        IOOutputQueue.h IOPacketQueue.h)
        file(CREATE_LINK "${ionf}/${hdr}" "${shim}/IOKit/network/${hdr}" SYMBOLIC)
    endforeach()
    target_include_directories(${name}.kext PRIVATE
        "${FBSDKPI_DIR}"
        "${FBSDKPI_DIR}/pd"
        "${FBSDKPI_DIR}/xnu"
        "${shim}"
        "${FBSDKPI_DIR}/../Extensions/IOPCIFamily/include"
        "${FBSDKPI_DIR}/../xnu/EXTERNAL_HEADERS")
    target_compile_definitions(${name}.kext PRIVATE __PRIVATE_SPI__ PDFBSD_CLASS=${class})
    target_compile_options(${name}.kext PRIVATE
        -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
        -Wno-unused-parameter -Wno-deprecated-declarations)
    set_property(TARGET ${name}.kext PROPERTY CXX_STANDARD 14)
    add_dependencies(${name}.kext IONetworkingFamily.kext)
endfunction()

function(add_fbsdkpi_library name)
    cmake_parse_arguments(FL "" "BUNDLE_IDENTIFIER;INFO_PLIST" "" ${ARGN})
    _fbsdkpi_core_sources(core OFF)
    set(shared PD_FBSD_SHARED=1)
    _fbsdkpi_objects(${name}_core SOURCES ${core} VISIBLE
        DEFINITIONS ${shared} OPTIONS ${FBSDKPI_NAMES})
    set(iflib)
    foreach(src IN LISTS FBSDKPI_IFLIB_SOURCES)
        list(APPEND iflib "${FBSDKPI_DIR}/haiku/freebsd_iflib/${src}")
    endforeach()
    _fbsdkpi_objects(${name}_iflib SOURCES ${iflib} VISIBLE
        INCLUDES "${FBSDKPI_DIR}/haiku/freebsd_iflib" "${FBSDKPI_DIR}/haiku/freebsd_iflib/compat"
        DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ALTQ ${shared} OPTIONS ${FBSDKPI_NAMES})
    add_kext_bundle(${name}.kext INFO_PLIST ${FL_INFO_PLIST} KERNEL_PRIVATE
        BUNDLE_IDENTIFIER ${FL_BUNDLE_IDENTIFIER} BUNDLE_VERSION 1.0.0)
    set_property(TARGET ${name}.kext PROPERTY OUTPUT_NAME ${name})
    target_sources(${name}.kext PRIVATE "${FBSDKPI_DIR}/xnu/pd_fbsd_os.cpp"
        $<TARGET_OBJECTS:${name}_core> $<TARGET_OBJECTS:${name}_iflib>)
    _fbsdkpi_xnu_target(${name} ${name})
    install(TARGETS ${name}.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
endfunction()

# haiku's usb shim once for the usb drivers, linked to PDFreeBSDKPI and IOUSBFamily
function(add_fbsdkpi_usb_library name)
    cmake_parse_arguments(FL "" "BUNDLE_IDENTIFIER;INFO_PLIST" "" ${ARGN})
    set(usb)
    foreach(src IN LISTS FBSDKPI_USB_SOURCES)
        list(APPEND usb "${FBSDKPI_DIR}/haiku/freebsd_network/${src}")
    endforeach()
    list(APPEND usb "${FBSDKPI_DIR}/pd/pd_usb.cpp")
    _fbsdkpi_objects(${name}_usb SOURCES ${usb} VISIBLE
        DEFINITIONS PD_FBSD_SHARED=1 OPTIONS ${FBSDKPI_NAMES})
    add_kext_bundle(${name}.kext INFO_PLIST ${FL_INFO_PLIST} KERNEL_PRIVATE
        BUNDLE_IDENTIFIER ${FL_BUNDLE_IDENTIFIER} BUNDLE_VERSION 1.0.0
        MAIN_FUNCTION pd_fbsd_library_start ANTIMAIN_FUNCTION pd_fbsd_library_stop)
    _fbsdkpi_library_start(${name})
    set_property(TARGET ${name}.kext PROPERTY OUTPUT_NAME ${name})
    target_sources(${name}.kext PRIVATE "${FBSDKPI_DIR}/xnu/pd_fbsd_usb.cpp" $<TARGET_OBJECTS:${name}_usb>)
    _fbsdkpi_xnu_target(${name} ${name})
    target_link_libraries(${name}.kext PRIVATE IOUSBFamilyHeaders)
    target_compile_definitions(${name}.kext PRIVATE PDFBSD_USB=1)
    if(TARGET PDFreeBSDKPI.kext)
        add_dependencies(${name}.kext PDFreeBSDKPI.kext)
    endif()
    install(TARGETS ${name}.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
endfunction()

# linuxkpi, net80211 and the wlan glue once for every linuxkpi driver, linked to PDFreeBSDKPI
set(FBSDKPI_LKPI_INCLUDES
    "${FBSDKPI_DIR}/pd/lkpi-override"
    "${FBSDKPI_DIR}/freebsd/linuxkpi/common/include"
    "${FBSDKPI_DIR}/freebsd/linuxkpi/dummy/include"
    "${FBSDKPI_DIR}/pd/fbsd-extra"
    "${FBSDKPI_DIR}/freebsd"
    "${FBSDKPI_DIR}/freebsd/crypto/rijndael"
    "${FBSDKPI_DIR}/haiku/freebsd_iflib/compat")
set(FBSDKPI_LKPI_OPTIONS "SHELL:-include ${FBSDKPI_DIR}/pd/fbsd-extra/pd_lkpi_pre.h"
    -Werror=implicit-function-declaration)
set(FBSDKPI_LKPI_NAMES "SHELL:-include ${FBSDKPI_DIR}/pd/pd_lkpi_names.h")
set(FBSDKPI_DRM_NAMES "SHELL:-include ${FBSDKPI_DIR}/pd/pd_drm_names.h")

# the one linuxkpi serves drm and wifi alike: it and every driver on it see drm-kmod's linux headers and its
# LINUXKPI_VERSION, so structs match. drm's own environment (pd_drm_pre.h, its CONFIG_ set) stays with the drm
# core and drivers, the wifi drivers keep theirs
function(_fbsdkpi_drm_lkpi incs defs opts)
    if(NOT PD_DRMKMOD_DIR)
        message(FATAL_ERROR "the shared linuxkpi needs PD_DRMKMOD_DIR, the drm-kmod tree")
    endif()
    include("${FBSDKPI_DIR}/drm/drm.cmake")
    pd_drm_layout("${PD_DRMKMOD_DIR}" drminc drmdefs drmopts)
    set(${incs} ${drminc} ${FBSDKPI_LKPI_INCLUDES} PARENT_SCOPE)
    set(${defs} LINUXKPI_VERSION=61400 PARENT_SCOPE)
    set(${opts} PARENT_SCOPE)
endfunction()

function(add_fbsdkpi_lkpi_library name)
    cmake_parse_arguments(FL "" "BUNDLE_IDENTIFIER;INFO_PLIST" "" ${ARGN})
    set(lkpi)
    _fbsdkpi_drm_lkpi(lkinc lkdefs lkopts)
    foreach(src IN LISTS FBSDKPI_LINUXKPI_SOURCES FBSDKPI_LINUXKPI_WLAN_SOURCES)
        list(APPEND lkpi "${FBSDKPI_DIR}/freebsd/linuxkpi/common/src/${src}")
    endforeach()
    foreach(src IN LISTS FBSDKPI_LINUXKPI_PD_SOURCES FBSDKPI_LINUXKPI_WLAN_PD_SOURCES)
        list(APPEND lkpi "${FBSDKPI_DIR}/pd/lkpi/${src}")
    endforeach()
    foreach(src IN LISTS FBSDKPI_NET80211_SOURCES)
        list(APPEND lkpi "${FBSDKPI_DIR}/freebsd/net80211/${src}")
    endforeach()
    list(APPEND lkpi
        "${FBSDKPI_DIR}/freebsd/crypto/rijndael/rijndael-alg-fst.c"
        "${FBSDKPI_DIR}/freebsd/crypto/rijndael/rijndael-api.c"
        "${FBSDKPI_DIR}/freebsd/crypto/sha1.c")
    set(shared PD_FBSD_SHARED=1 PD_LKPI_WLAN_DEFS=1)
    _fbsdkpi_objects(${name}_lkpi SOURCES ${lkpi} VISIBLE
        INCLUDES ${lkinc}
        DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${shared} ${lkdefs} KBUILD_MODNAME=\"linuxkpi\"
        OPTIONS ${FBSDKPI_NAMES} ${FBSDKPI_LKPI_NAMES} ${FBSDKPI_LKPI_OPTIONS} ${lkopts})
    _fbsdkpi_objects(${name}_glue SOURCES "${FBSDKPI_DIR}/pd/pd_lkpi_glue.c" VISIBLE
        INCLUDES "${FBSDKPI_DIR}/haiku/freebsd_iflib/compat"
        DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${shared}
        OPTIONS ${FBSDKPI_NAMES} ${FBSDKPI_LKPI_NAMES})
    add_kext_bundle(${name}.kext INFO_PLIST ${FL_INFO_PLIST} KERNEL_PRIVATE
        BUNDLE_IDENTIFIER ${FL_BUNDLE_IDENTIFIER} BUNDLE_VERSION 1.0.0
        MAIN_FUNCTION pd_fbsd_library_start ANTIMAIN_FUNCTION pd_fbsd_library_stop)
    _fbsdkpi_library_start(${name})
    set_property(TARGET ${name}.kext PROPERTY OUTPUT_NAME ${name})
    target_sources(${name}.kext PRIVATE "${FBSDKPI_DIR}/xnu/pd_lkpi_kext.cpp"
        $<TARGET_OBJECTS:${name}_lkpi> $<TARGET_OBJECTS:${name}_glue>)
    _fbsdkpi_xnu_target(${name} ${name})
    if(TARGET PDFreeBSDKPI.kext)
        add_dependencies(${name}.kext PDFreeBSDKPI.kext)
    endif()
    install(TARGETS ${name}.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
endfunction()

# the drm core, ttm, dma-buf and the drm-only linuxkpi pieces once, in drm's environment, for every drm driver
function(add_fbsdkpi_drm_library name)
    cmake_parse_arguments(FL "" "BUNDLE_IDENTIFIER;INFO_PLIST" "" ${ARGN})
    if(NOT PD_DRMKMOD_DIR)
        message(FATAL_ERROR "${name} needs PD_DRMKMOD_DIR, the drm-kmod tree")
    endif()
    include("${FBSDKPI_DIR}/drm/drm.cmake")
    set(drmkmod "${PD_DRMKMOD_DIR}")
    pd_drm_layout("${drmkmod}" drminc drmdefs drmopts)
    pd_drm_core_includes("${drmkmod}" coreinc)
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
    set(lkdrm)
    foreach(src IN LISTS FBSDKPI_LINUXKPI_DRM_SOURCES)
        list(APPEND lkdrm "${FBSDKPI_DIR}/freebsd/linuxkpi/common/src/${src}")
    endforeach()
    set(inc ${drminc} ${FBSDKPI_LKPI_INCLUDES} ${coreinc})
    set(defs FBSD_DRIVER=1 _XOPEN_SOURCE PD_FBSD_SHARED=1 PD_LKPI_WLAN_DEFS=1 ${drmdefs})
    set(opts ${FBSDKPI_NAMES} ${FBSDKPI_LKPI_NAMES} ${FBSDKPI_DRM_NAMES} ${FBSDKPI_LKPI_OPTIONS} ${drmopts})
    _fbsdkpi_objects(${name}_drm SOURCES ${core} VISIBLE INCLUDES ${inc}
        DEFINITIONS ${defs} KBUILD_MODNAME=\"drm\" LINUXKPI_PARAM_PREFIX=drm_ OPTIONS ${opts})
    _fbsdkpi_objects(${name}_lkdrm SOURCES ${lkdrm} VISIBLE INCLUDES ${inc}
        DEFINITIONS ${defs} KBUILD_MODNAME=\"linuxkpi\" OPTIONS ${opts})
    add_kext_bundle(${name}.kext INFO_PLIST ${FL_INFO_PLIST} KERNEL_PRIVATE
        BUNDLE_IDENTIFIER ${FL_BUNDLE_IDENTIFIER} BUNDLE_VERSION 1.0.0
        MAIN_FUNCTION pd_fbsd_library_start ANTIMAIN_FUNCTION pd_fbsd_library_stop)
    _fbsdkpi_library_start(${name})
    set_property(TARGET ${name}.kext PROPERTY OUTPUT_NAME ${name})
    target_sources(${name}.kext PRIVATE "${FBSDKPI_DIR}/xnu/pd_lkpi_kext.cpp"
        $<TARGET_OBJECTS:${name}_drm> $<TARGET_OBJECTS:${name}_lkdrm>)
    _fbsdkpi_xnu_target(${name} ${name})
    foreach(lib PDFreeBSDKPI PDLinuxKPI)
        if(TARGET ${lib}.kext)
            add_dependencies(${name}.kext ${lib}.kext)
        endif()
    endforeach()
    install(TARGETS ${name}.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
endfunction()

function(add_fbsdkpi_driver name)
    cmake_parse_arguments(FD "IFLIB;LINUXKPI;LINUXKPI_DRM;USB;SHARED" "CLASS;BUNDLE_IDENTIFIER;INFO_PLIST"
        "SOURCES;INCLUDES;DEFINITIONS;FIRMWARE;XNU_SOURCES;LKPI_INCLUDES;LKPI_DEFINITIONS;LKPI_OPTIONS;OBJECTS" ${ARGN})

    # a SHARED driver links PDFreeBSDKPI.kext for the layer and builds only its own code, under the shared names
    set(names)
    set(shlkinc)
    set(shlkdefs)
    set(shlkopts)
    if(FD_SHARED AND FD_LINUXKPI)
        # the driver's own objects see linuxkpi as the shared linuxkpi was built
        _fbsdkpi_drm_lkpi(shlkinc shlkdefs shlkopts)
    endif()
    if(FD_SHARED)
        set(names ${FBSDKPI_NAMES})
        if(FD_LINUXKPI OR FD_LINUXKPI_DRM)
            list(APPEND names ${FBSDKPI_LKPI_NAMES})
        endif()
        if(FD_LINUXKPI_DRM)
            list(APPEND names ${FBSDKPI_DRM_NAMES})
        endif()
    endif()
    _fbsdkpi_core_sources(core "${FD_USB}")
    # the firmware table, empty or not, stays with the driver: the shared layer reads it through the record
    _fbsdkpi_firmware(${name} firmware ${FD_FIRMWARE})
    if(FD_SHARED)
        _fbsdkpi_objects(${name}_firmware SOURCES ${firmware})
        set(objects $<TARGET_OBJECTS:${name}_firmware>)
    else()
        _fbsdkpi_objects(${name}_core SOURCES ${core} ${firmware})
        set(objects $<TARGET_OBJECTS:${name}_core>)
    endif()

    # iflib and the driver build as FreeBSD drivers, as in haiku's jamfiles
    set(iflibinc)
    if(FD_IFLIB)
        set(iflib)
        foreach(src IN LISTS FBSDKPI_IFLIB_SOURCES)
            list(APPEND iflib "${FBSDKPI_DIR}/haiku/freebsd_iflib/${src}")
        endforeach()
        set(iflibinc "${FBSDKPI_DIR}/haiku/freebsd_iflib/compat")
        if(NOT FD_SHARED)
            _fbsdkpi_objects(${name}_iflib SOURCES ${iflib}
                INCLUDES "${FBSDKPI_DIR}/haiku/freebsd_iflib" ${iflibinc}
                DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ALTQ)
            list(APPEND objects $<TARGET_OBJECTS:${name}_iflib>)
        endif()
    endif()

    # linuxkpi and net80211 see FreeBSD headers ahead of haiku's compat ones
    set(lkpiinc)
    set(lkpiopt)
    if(FD_LINUXKPI OR FD_LINUXKPI_DRM)
        # a drm kext puts its own headers first, so every file sees the same structs
        set(lkpiinc ${FD_LKPI_INCLUDES}
            "${FBSDKPI_DIR}/pd/lkpi-override"
            "${FBSDKPI_DIR}/freebsd/linuxkpi/common/include"
            "${FBSDKPI_DIR}/freebsd/linuxkpi/dummy/include"
            "${FBSDKPI_DIR}/pd/fbsd-extra"
            "${FBSDKPI_DIR}/freebsd"
            "${FBSDKPI_DIR}/freebsd/crypto/rijndael"
            "${FBSDKPI_DIR}/haiku/freebsd_iflib/compat")
        set(lkpiopt "SHELL:-include ${FBSDKPI_DIR}/pd/fbsd-extra/pd_lkpi_pre.h"
            ${FD_LKPI_OPTIONS}
            -Werror=implicit-function-declaration)
        set(lkpi)
        set(lkpisrc ${FBSDKPI_LINUXKPI_SOURCES})
        set(lkpipd ${FBSDKPI_LINUXKPI_PD_SOURCES})
        if(FD_LINUXKPI_DRM)
            list(APPEND lkpisrc ${FBSDKPI_LINUXKPI_DRM_SOURCES})
        else()
            list(APPEND lkpisrc ${FBSDKPI_LINUXKPI_WLAN_SOURCES})
            list(APPEND lkpipd ${FBSDKPI_LINUXKPI_WLAN_PD_SOURCES})
        endif()
        foreach(src IN LISTS lkpisrc)
            list(APPEND lkpi "${FBSDKPI_DIR}/freebsd/linuxkpi/common/src/${src}")
        endforeach()
        foreach(src IN LISTS lkpipd)
            list(APPEND lkpi "${FBSDKPI_DIR}/pd/lkpi/${src}")
        endforeach()
        if(NOT FD_LINUXKPI_DRM)
            foreach(src IN LISTS FBSDKPI_NET80211_SOURCES)
                list(APPEND lkpi "${FBSDKPI_DIR}/freebsd/net80211/${src}")
            endforeach()
            list(APPEND lkpi
                "${FBSDKPI_DIR}/freebsd/crypto/rijndael/rijndael-alg-fst.c"
                "${FBSDKPI_DIR}/freebsd/crypto/rijndael/rijndael-api.c"
                "${FBSDKPI_DIR}/freebsd/crypto/sha1.c")
        endif()
        if(NOT FD_SHARED)
            _fbsdkpi_objects(${name}_lkpi SOURCES ${lkpi}
                INCLUDES ${lkpiinc}
                DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${FD_LKPI_DEFINITIONS}
                OPTIONS ${lkpiopt})
            # tasklets run on iflib's grouptask queues
            _fbsdkpi_objects(${name}_lkpiglue SOURCES "${FBSDKPI_DIR}/pd/pd_lkpi_glue.c"
                "${FBSDKPI_DIR}/haiku/freebsd_iflib/subr_gtaskqueue.c"
                INCLUDES "${FBSDKPI_DIR}/haiku/freebsd_iflib/compat"
                DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE)
            list(APPEND objects $<TARGET_OBJECTS:${name}_lkpi> $<TARGET_OBJECTS:${name}_lkpiglue>)
        endif()
    endif()
    foreach(obj IN LISTS FD_OBJECTS)
        list(APPEND objects $<TARGET_OBJECTS:${obj}>)
    endforeach()
    # the record the driver's xnu class hands the layer, built from this driver's glue names
    set(recdefs)
    if(FD_XNU_SOURCES AND NOT FD_XNU_SOURCES MATCHES "PDFBSDEthernet.cpp")
        set(recdefs PD_FBSD_NO_NET=1)
    endif()
    _fbsdkpi_objects(${name}_record SOURCES "${FBSDKPI_DIR}/pd/pd_fbsd_driver_record.c"
        DEFINITIONS ${recdefs} OPTIONS ${names})
    list(APPEND objects $<TARGET_OBJECTS:${name}_record>)

    if(FD_SOURCES)
        _fbsdkpi_objects(${name}_driver SOURCES ${FD_SOURCES}
            INCLUDES ${FD_INCLUDES} ${iflibinc} ${shlkinc} ${lkpiinc}
            DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${FD_DEFINITIONS} ${shlkdefs}
            OPTIONS ${names} ${lkpiopt} ${shlkopts})
        # an undeclared call links to nothing and panics on first use, so iflib drivers fail the build
        if(FD_IFLIB)
            target_compile_options(${name}_driver PRIVATE -Werror=implicit-function-declaration)
        endif()
        list(APPEND objects $<TARGET_OBJECTS:${name}_driver>)
    endif()

    # a SHARED driver depends on the libraries it links, added here so one plist serves shared and legacy builds
    set(plist ${FD_INFO_PLIST})
    if(FD_SHARED)
        set(libs org.puredarwin.kpi.freebsd)
        if(FD_USB)
            list(APPEND libs org.puredarwin.kpi.freebsd.usb)
        endif()
        if(FD_LINUXKPI OR FD_LINUXKPI_DRM)
            list(APPEND libs org.puredarwin.kpi.linux)
        endif()
        if(FD_LINUXKPI_DRM)
            list(APPEND libs org.puredarwin.kpi.drm)
        endif()
        file(READ ${FD_INFO_PLIST} text)
        set(add "")
        foreach(lib IN LISTS libs)
            string(FIND "${text}" "<key>${lib}</key>" at)
            if(at EQUAL -1)
                string(APPEND add "\n\t\t<key>${lib}</key>\n\t\t<string>1.0.0</string>")
            endif()
        endforeach()
        string(REGEX REPLACE "(<key>OSBundleLibraries</key>[ \t\r\n]*<dict>)" "\\1${add}" text "${text}")
        set(plist "${CMAKE_CURRENT_BINARY_DIR}/${name}-Info.plist")
        file(WRITE ${plist} "${text}")
    endif()
    add_kext_bundle(${name}.kext INFO_PLIST ${plist} KERNEL_PRIVATE
        BUNDLE_IDENTIFIER ${FD_BUNDLE_IDENTIFIER} BUNDLE_VERSION 1.0.0)
    set_property(TARGET ${name}.kext PROPERTY OUTPUT_NAME ${name})
    set(xnusrc)
    if(NOT FD_SHARED)
        list(APPEND xnusrc "${FBSDKPI_DIR}/xnu/pd_fbsd_os.cpp")
    endif()
    if(FD_USB AND NOT FD_SHARED)
        list(APPEND xnusrc "${FBSDKPI_DIR}/xnu/pd_fbsd_usb.cpp")
    endif()
    if(FD_XNU_SOURCES)
        list(APPEND xnusrc ${FD_XNU_SOURCES})
    else()
        list(APPEND xnusrc "${FBSDKPI_DIR}/xnu/PDFBSDEthernet.cpp")
    endif()
    target_sources(${name}.kext PRIVATE ${xnusrc} ${objects})
    set_property(SOURCE ${xnusrc} TARGET_DIRECTORY ${name}.kext
        APPEND PROPERTY COMPILE_OPTIONS -fvisibility=hidden)
    _fbsdkpi_xnu_target(${name} ${FD_CLASS})
    if(FD_SHARED)
        foreach(lib PDFreeBSDKPI PDFreeBSDKPIUSB PDLinuxKPI PDDRM)
            if(TARGET ${lib}.kext)
                add_dependencies(${name}.kext ${lib}.kext)
            endif()
        endforeach()
    endif()
    if(FD_USB)
        target_link_libraries(${name}.kext PRIVATE IOUSBFamilyHeaders)
        target_compile_definitions(${name}.kext PRIVATE PDFBSD_USB=1)
    endif()
    install(TARGETS ${name}.kext DESTINATION System/Library/Extensions
        COMPONENT BaseSystem)
endfunction()
