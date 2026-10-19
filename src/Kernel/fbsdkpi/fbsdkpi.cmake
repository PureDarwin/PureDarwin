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

set(FBSDKPI_IFLIB_SOURCES
    device_if.c ifdi_if.c iflib.c kobj.c mp_ring.c strsep.c
    subr_gtaskqueue.c)

# one object library of FreeBSD-side code, which never sees an xnu header
function(_fbsdkpi_objects target)
    cmake_parse_arguments(FO "" "" "SOURCES;INCLUDES;DEFINITIONS;OPTIONS" ${ARGN})
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
        # names like printf, malloc and wakeup must not leak to later kexts
        -fvisibility=hidden
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

function(add_fbsdkpi_driver name)
    cmake_parse_arguments(FD "IFLIB;LINUXKPI;LINUXKPI_DRM" "CLASS;BUNDLE_IDENTIFIER;INFO_PLIST"
        "SOURCES;INCLUDES;DEFINITIONS;FIRMWARE;XNU_SOURCES;LKPI_INCLUDES;LKPI_DEFINITIONS;LKPI_OPTIONS;OBJECTS" ${ARGN})

    set(core)
    foreach(src IN LISTS FBSDKPI_NETWORK_SOURCES)
        list(APPEND core "${FBSDKPI_DIR}/haiku/freebsd_network/${src}")
    endforeach()
    list(APPEND core
        "${FBSDKPI_DIR}/pd/pd_haiku_kernel.cpp"
        "${FBSDKPI_DIR}/pd/pd_pci.c"
        "${FBSDKPI_DIR}/pd/pd_driver.c"
        "${FBSDKPI_DIR}/pd/pd_kernel_util.c"
        "${FBSDKPI_DIR}/pd/pd_firmware.c"
        "${FBSDKPI_DIR}/haiku/kernel_util/RadixBitmap.cpp"
        "${FBSDKPI_DIR}/haiku/kernel_util/Random.cpp")
    _fbsdkpi_firmware(${name} firmware ${FD_FIRMWARE})
    _fbsdkpi_objects(${name}_core SOURCES ${core} ${firmware})
    set(objects $<TARGET_OBJECTS:${name}_core>)

    # iflib and the driver build as FreeBSD drivers, as in haiku's jamfiles
    set(iflibinc)
    if(FD_IFLIB)
        set(iflib)
        foreach(src IN LISTS FBSDKPI_IFLIB_SOURCES)
            list(APPEND iflib "${FBSDKPI_DIR}/haiku/freebsd_iflib/${src}")
        endforeach()
        set(iflibinc "${FBSDKPI_DIR}/haiku/freebsd_iflib/compat")
        _fbsdkpi_objects(${name}_iflib SOURCES ${iflib}
            INCLUDES "${FBSDKPI_DIR}/haiku/freebsd_iflib" ${iflibinc}
            DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ALTQ)
        list(APPEND objects $<TARGET_OBJECTS:${name}_iflib>)
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
    foreach(obj IN LISTS FD_OBJECTS)
        list(APPEND objects $<TARGET_OBJECTS:${obj}>)
    endforeach()

    if(FD_SOURCES)
        _fbsdkpi_objects(${name}_driver SOURCES ${FD_SOURCES}
            INCLUDES ${FD_INCLUDES} ${iflibinc} ${lkpiinc}
            DEFINITIONS FBSD_DRIVER=1 _XOPEN_SOURCE ${FD_DEFINITIONS}
            OPTIONS ${lkpiopt})
        list(APPEND objects $<TARGET_OBJECTS:${name}_driver>)
    endif()

    add_kext_bundle(${name}.kext INFO_PLIST ${FD_INFO_PLIST} KERNEL_PRIVATE
        BUNDLE_IDENTIFIER ${FD_BUNDLE_IDENTIFIER} BUNDLE_VERSION 1.0.0)
    set_property(TARGET ${name}.kext PROPERTY OUTPUT_NAME ${name})
    set(xnusrc "${FBSDKPI_DIR}/xnu/pd_fbsd_os.cpp")
    if(FD_XNU_SOURCES)
        list(APPEND xnusrc ${FD_XNU_SOURCES})
    else()
        list(APPEND xnusrc "${FBSDKPI_DIR}/xnu/PDFBSDEthernet.cpp")
    endif()
    target_sources(${name}.kext PRIVATE ${xnusrc} ${objects})
    set_property(SOURCE ${xnusrc} TARGET_DIRECTORY ${name}.kext
        APPEND PROPERTY COMPILE_OPTIONS -fvisibility=hidden)

    # IONetworkingFamily's headers through the usual shim directory
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
    target_compile_definitions(${name}.kext PRIVATE
        __PRIVATE_SPI__ PDFBSD_CLASS=${FD_CLASS})
    target_compile_options(${name}.kext PRIVATE
        -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
        -Wno-unused-parameter -Wno-deprecated-declarations)
    set_property(TARGET ${name}.kext PROPERTY CXX_STANDARD 14)
    add_dependencies(${name}.kext IONetworkingFamily.kext)
    install(TARGETS ${name}.kext DESTINATION System/Library/Extensions
        COMPONENT BaseSystem)
endfunction()
