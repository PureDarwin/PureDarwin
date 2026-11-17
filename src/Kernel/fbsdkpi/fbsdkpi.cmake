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
    pd_lkpi_libkern.c pd_lkpi_misc.c
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
        ${osdirs}
        "${FBSDKPI_DIR}/pd/haiku-headers/posix")
    target_compile_definitions(${target} PRIVATE
        _KERNEL=1 _KERNEL_MODE=1 __HAIKU__=1 __PUREDARWIN__=1
        # the kernel's own panic stays reachable from the xnu side
        panic=fbsd_panic
        ${FO_DEFINITIONS})
    target_compile_options(${target} PRIVATE
        -ffreestanding -nostdlibinc -fno-common -fno-stack-protector
        # names like printf, malloc and wakeup must not leak to later kexts; the shared layer exports its own
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

# linuxkpi, net80211 and the wlan glue once, for the wifi drivers; links PDFreeBSDKPI
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

function(add_fbsdkpi_lkpi_library name)
    cmake_parse_arguments(FL "" "BUNDLE_IDENTIFIER;INFO_PLIST" "" ${ARGN})
    set(lkpi)
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
        INCLUDES ${FBSDKPI_LKPI_INCLUDES}
        DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${shared}
        OPTIONS ${FBSDKPI_NAMES} ${FBSDKPI_LKPI_NAMES} ${FBSDKPI_LKPI_OPTIONS})
    _fbsdkpi_objects(${name}_glue SOURCES "${FBSDKPI_DIR}/pd/pd_lkpi_glue.c" VISIBLE
        INCLUDES "${FBSDKPI_DIR}/haiku/freebsd_iflib/compat"
        DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${shared}
        OPTIONS ${FBSDKPI_NAMES} ${FBSDKPI_LKPI_NAMES})
    add_kext_bundle(${name}.kext INFO_PLIST ${FL_INFO_PLIST} KERNEL_PRIVATE
        BUNDLE_IDENTIFIER ${FL_BUNDLE_IDENTIFIER} BUNDLE_VERSION 1.0.0)
    set_property(TARGET ${name}.kext PROPERTY OUTPUT_NAME ${name})
    target_sources(${name}.kext PRIVATE "${FBSDKPI_DIR}/xnu/pd_lkpi_kext.cpp"
        $<TARGET_OBJECTS:${name}_lkpi> $<TARGET_OBJECTS:${name}_glue>)
    _fbsdkpi_xnu_target(${name} ${name})
    if(TARGET PDFreeBSDKPI.kext)
        add_dependencies(${name}.kext PDFreeBSDKPI.kext)
    endif()
    install(TARGETS ${name}.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
endfunction()

function(add_fbsdkpi_driver name)
    cmake_parse_arguments(FD "IFLIB;LINUXKPI;LINUXKPI_DRM;USB;SHARED" "CLASS;BUNDLE_IDENTIFIER;INFO_PLIST"
        "SOURCES;INCLUDES;DEFINITIONS;FIRMWARE;XNU_SOURCES;LKPI_INCLUDES;LKPI_DEFINITIONS;LKPI_OPTIONS;OBJECTS" ${ARGN})

    # a SHARED driver links PDFreeBSDKPI.kext for the layer and builds only its own code, under the shared names
    set(names)
    if(FD_SHARED)
        if(FD_USB OR FD_LINUXKPI_DRM)
            message(FATAL_ERROR "${name}: the shared layer has no usb or drm linuxkpi yet")
        endif()
        set(names ${FBSDKPI_NAMES})
        if(FD_LINUXKPI)
            list(APPEND names ${FBSDKPI_LKPI_NAMES})
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
    # the record PDFBSDEthernet hands the layer, built from this driver's glue names
    list(FIND FD_XNU_SOURCES "${FBSDKPI_DIR}/xnu/PDFBSDEthernet.cpp" ethidx)
    if(NOT FD_XNU_SOURCES OR NOT ethidx EQUAL -1 OR FD_XNU_SOURCES MATCHES "PDFBSDEthernet.cpp")
        _fbsdkpi_objects(${name}_record SOURCES "${FBSDKPI_DIR}/pd/pd_fbsd_driver_record.c" OPTIONS ${names})
        list(APPEND objects $<TARGET_OBJECTS:${name}_record>)
    endif()

    if(FD_SOURCES)
        _fbsdkpi_objects(${name}_driver SOURCES ${FD_SOURCES}
            INCLUDES ${FD_INCLUDES} ${iflibinc} ${lkpiinc}
            DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${FD_DEFINITIONS}
            OPTIONS ${names} ${lkpiopt})
        # an undeclared call links to nothing and panics on first use, so iflib drivers fail the build
        if(FD_IFLIB)
            target_compile_options(${name}_driver PRIVATE -Werror=implicit-function-declaration)
        endif()
        list(APPEND objects $<TARGET_OBJECTS:${name}_driver>)
    endif()

    add_kext_bundle(${name}.kext INFO_PLIST ${FD_INFO_PLIST} KERNEL_PRIVATE
        BUNDLE_IDENTIFIER ${FD_BUNDLE_IDENTIFIER} BUNDLE_VERSION 1.0.0)
    set_property(TARGET ${name}.kext PROPERTY OUTPUT_NAME ${name})
    set(xnusrc)
    if(NOT FD_SHARED)
        list(APPEND xnusrc "${FBSDKPI_DIR}/xnu/pd_fbsd_os.cpp")
    endif()
    if(FD_USB)
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
    if(FD_SHARED AND TARGET PDFreeBSDKPI.kext)
        add_dependencies(${name}.kext PDFreeBSDKPI.kext)
    endif()
    if(FD_USB)
        target_link_libraries(${name}.kext PRIVATE IOUSBFamilyHeaders)
        target_compile_definitions(${name}.kext PRIVATE PDFBSD_USB=1)
    endif()
    install(TARGETS ${name}.kext DESTINATION System/Library/Extensions
        COMPONENT BaseSystem)
endfunction()
