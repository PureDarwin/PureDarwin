include("${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/fbsdkpi.cmake")

set(PDFBSDIWLWIFI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glue.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/pd_crc32.c")
foreach(src
    iwl-dbg-tlv.c iwl-drv.c iwl-debug.c iwl-devtrace.c iwl-io.c
    iwl-nvm-parse.c iwl-nvm-utils.c iwl-trans.c iwl-phy-db.c iwl-utils.c
    cfg/22000.c cfg/7000.c cfg/8000.c cfg/9000.c cfg/ax210.c
    cfg/bz.c cfg/sc.c cfg/dr.c
    cfg/rf-fm.c cfg/rf-gf.c cfg/rf-hr.c cfg/rf-jf.c cfg/rf-pe.c cfg/rf-wh.c
    fw/dbg.c fw/dump.c fw/img.c fw/init.c
    fw/notif-wait.c fw/paging.c fw/pnvm.c fw/regulatory.c fw/rs.c fw/smem.c
    pcie/ctxt-info-v2.c pcie/ctxt-info.c pcie/drv.c pcie/utils.c
    pcie/gen1_2/rx.c pcie/gen1_2/trans-gen2.c pcie/gen1_2/trans.c
    pcie/gen1_2/tx-gen2.c pcie/gen1_2/tx.c
    mvm/binding.c mvm/coex.c mvm/ftm-initiator.c mvm/ftm-responder.c mvm/fw.c
    mvm/link.c mvm/mac-ctxt.c mvm/mac80211.c mvm/mld-key.c
    mvm/mld-mac.c mvm/mld-mac80211.c mvm/mld-sta.c mvm/nvm.c
    mvm/offloading.c mvm/ops.c mvm/phy-ctxt.c mvm/power.c mvm/ptp.c
    mvm/quota.c mvm/rfi.c mvm/rs-fw.c mvm/rs.c mvm/rx.c mvm/rxmq.c
    mvm/scan.c mvm/sf.c mvm/sta.c mvm/tdls.c mvm/time-event.c
    mvm/time-sync.c mvm/tt.c mvm/tx.c mvm/utils.c
    mld/agg.c mld/ap.c mld/coex.c mld/ftm-initiator.c mld/fw.c mld/iface.c
    mld/key.c mld/link.c mld/low_latency.c mld/mac80211.c mld/mcc.c mld/mld.c
    mld/mlo.c mld/nan.c mld/notif.c mld/phy.c mld/power.c mld/ptp.c
    mld/regulatory.c mld/roc.c mld/rx.c mld/scan.c
    mld/session-protect.c mld/sta.c mld/stats.c mld/thermal.c
    mld/time_sync.c mld/tlc.c mld/tx.c)
    list(APPEND PDFBSDIWLWIFI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/iwlwifi/${src}")
endforeach()

add_fbsdkpi_driver(PDFBSDiwlwifi
    CLASS PDFBSDiwlwifi
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDFBSDiwlwifi
    INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    LINUXKPI
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/iwlwifi"
    DEFINITIONS KBUILD_MODNAME=\"iwlwifi\" LINUXKPI_VERSION=70000
        LINUXKPI_WANT_LINUX_ACPI CONFIG_IWLDVM=0 CONFIG_IWLMVM=1 CONFIG_IWLMLD=1
        CONFIG_IWLWIFI_DEBUG=1 CONFIG_IWLWIFI_DEVICE_TRACING=1
    SOURCES ${PDFBSDIWLWIFI_SOURCES}
    XNU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDEthernet.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi/xnu/PDFBSDWlan.cpp"
)

# iwlwifi's sources see a few linux and FreeBSD names first
target_compile_options(PDFBSDiwlwifi_driver PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/pd_iwlwifi_pre.h")

# the iokit class is an IO80211Controller, so the apple80211 requests reach it
target_include_directories(PDFBSDiwlwifi.kext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../IO80211Family")
target_compile_definitions(PDFBSDiwlwifi.kext PRIVATE PDFBSD_WLAN=1
    PDFBSD_FIRMWARE_DIR=\"iwlwifi\")
add_dependencies(PDFBSDiwlwifi.kext IO80211Family.kext)
