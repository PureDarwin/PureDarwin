set(PD_DRM_DIR "${CMAKE_CURRENT_LIST_DIR}")
include("${PD_DRM_DIR}/amdgpu-sources.cmake")
include("${PD_DRM_DIR}/i915-sources.cmake")
include("${PD_DRM_DIR}/nouveau-sources.cmake")

set(PD_DRM_CORE_SOURCES
    drm_atomic.c drm_atomic_helper.c drm_atomic_state_helper.c drm_atomic_uapi.c
    drm_aperture.c drm_auth.c drm_buddy.c drm_blend.c drm_bridge.c drm_cache.c
    drm_client.c drm_client_event.c drm_client_modeset.c drm_color_mgmt.c
    drm_connector.c drm_crtc.c drm_crtc_helper.c drm_damage_helper.c drm_drv.c
    drm_dumb_buffers.c drm_edid.c drm_eld.c drm_displayid.c drm_encoder.c
    drm_exec.c drm_file.c drm_flip_work.c drm_fourcc.c drm_framebuffer.c
    drm_gem.c drm_gpuvm.c drm_ioctl.c drm_lease.c drm_mm.c drm_modes.c
    drm_mode_config.c drm_mode_object.c drm_modeset_lock.c drm_modeset_helper.c
    drm_os_freebsd.c drm_panel.c drm_panel_orientation_quirks.c drm_pci.c
    drm_plane.c drm_plane_helper.c drm_print.c drm_prime.c drm_probe_helper.c
    drm_property.c drm_rect.c drm_self_refresh_helper.c drm_suballoc.c
    drm_syncobj.c drm_sysfs.c drm_vblank.c drm_vblank_work.c drm_vma_manager.c
    clients/drm_client_setup.c
    scheduler/sched_main.c scheduler/sched_fence.c scheduler/sched_entity.c
    display/drm_scdc_helper.c display/drm_display_helper_mod.c
    display/drm_dp_dual_mode_helper.c display/drm_dp_mst_topology.c
    display/drm_dp_helper.c display/drm_dsc_helper.c display/drm_hdmi_helper.c
    display/drm_hdmi_audio_helper.c display/drm_hdmi_state_helper.c
    ttm/ttm_bo.c ttm/ttm_bo_util.c ttm/ttm_bo_vm.c ttm/ttm_device.c
    ttm/ttm_execbuf_util.c ttm/ttm_module.c ttm/ttm_pool.c
    ttm/ttm_range_manager.c ttm/ttm_resource.c ttm/ttm_sys_manager.c
    ttm/ttm_tt.c)

# FreeBSD's BSD-licensed fences and Linux's MIT reservation objects
set(PD_DRM_DMABUF_SOURCES dma-fence.c dma-fence-array.c dma-fence-chain.c
    dma-resv.c)
set(PD_DRM_VIDEO_SOURCES aperture.c)

# PureDarwin's own pieces, MIT
set(PD_DRM_PD_SOURCES pd_dma_buf.c pd_dma_fence_unwrap.c pd_sync_file.c
    pd_drm_managed.c pd_drm_gem_helpers.c pd_drm_glue.c pd_drm_os.c pd_drm_cdev.c
    pd_drm_shmem.c pd_drm_printf.c
    pd_drm_testclient.c pd_lfile_lkpi.c)
set(PD_DRM_AMDGPU_PD_SOURCES pd_amdgpu_absent.c pd_amdgpu_absent_blocks.c
    pd_amdgpu_fb.c pd_amdgpu_mmap.c)

set(PD_DRM_KCONFIG APERTURE_HELPERS ARCH_HAVE_NMI_SAFE_CMPXCHG
    BACKLIGHT_CLASS_DEVICE FB NEED_SG_DMA_LENGTH PCI PM PM_SLEEP SMP
    SUSPEND VIDEO_CMDLINE 64BIT DRM_CLIENT DRM_CLIENT_SETUP DRM_EXEC DRM_GPUVM)
# arm64 boards come from the device tree, with no acpi, dmi or amd display
if(PUREDARWIN_ARM64)
    list(APPEND PD_DRM_KCONFIG ARM64)
else()
    list(APPEND PD_DRM_KCONFIG X86 X86_64 DMI ACPI DRM_AMD_DC DRM_AMD_DC_FP)
endif()

# headers come first for every file in a drm kext, linuxkpi's own included,
# so all of them agree on the struct layouts
function(pd_drm_layout drmkmod incs defs opts)
    set(${incs}
        "${PD_DRM_DIR}/include"
        "${drmkmod}/linuxkpi/bsd/include" PARENT_SCOPE)
    set(d LINUXKPI_VERSION=61400 PD_LKPI_DRM=1 CONFIG_DRM_FBDEV_OVERALLOC=100)
    foreach(c IN LISTS PD_DRM_KCONFIG)
        list(APPEND d CONFIG_${c})
    endforeach()
    set(${defs} ${d} PARENT_SCOPE)
    set(lk "${FBSDKPI_DIR}/freebsd/linuxkpi/common/include/linux")
    set(${opts}
        "SHELL:-include ${lk}/compiler_types.h"
        "SHELL:-include ${lk}/kconfig.h"
        "SHELL:-include ${PD_DRM_DIR}/include/pd_drm_pre.h"
        # linux zeroes stack locals by default and drm relies on it (amdgpu_vm_bo_update)
        -ftrivial-auto-var-init=zero PARENT_SCOPE)
endfunction()

# the include directories after linuxkpi's, for drm and its helpers
function(pd_drm_core_includes drmkmod out)
    set(${out}
        "${drmkmod}/drivers/gpu/drm"
        "${drmkmod}/include"
        "${drmkmod}/include/drm"
        "${drmkmod}/include/uapi"
        "${drmkmod}/drivers/gpu"
        "${drmkmod}/drivers/gpu/drm/scheduler" PARENT_SCOPE)
endfunction()

function(pd_drm_amdgpu_includes drmkmod out)
    set(a "${drmkmod}/drivers/gpu/drm/amd")
    set(l)
    foreach(d "" acp/include amdgpu amdkfd display display/amdgpu_dm display/dc
        display/dc/clk_mgr display/dc/dccg display/dc/dml2 display/dc/dml2/dml21
        display/dc/dml2/dml21/src/dml2_core display/dc/dml2/dml21/src/dml2_mcg
        display/dc/dml2/dml21/src/dml2_dpmm display/dc/dml2/dml21/src/dml2_pmo
        display/dc/dml2/dml21/src/dml2_standalone_libraries
        display/dc/dml2/dml21/src/inc display/dc/dml2/dml21/inc display/dc/dio
        display/dc/dpp display/dc/dsc display/dc/dwb display/dc/hpo
        display/dc/hubp display/dc/hubbub display/dc/hwss display/dc/inc
        display/dc/inc/hw display/dc/mmhubbub display/dc/mpc display/dc/opp
        display/dc/optc display/dc/pg display/dc/resource display/dc/spl
        display/dmub/inc display/include display/modules/hdcp
        display/modules/inc include include/asic_reg pm pm/inc pm/legacy-dpm
        pm/powerplay pm/powerplay/inc pm/powerplay/hwmgr pm/powerplay/smumgr
        pm/swsmu pm/swsmu/inc pm/swsmu/inc/pmfw_if pm/swsmu/smu11
        pm/swsmu/smu12 pm/swsmu/smu13 pm/swsmu/smu14)
        list(APPEND l "${a}/${d}")
    endforeach()
    set(${out} ${l} PARENT_SCOPE)
endfunction()

# the i915 kconfig values drm-kmod's kconfig.mk sets, without error capture and dp tunnels
set(PD_DRM_I915_KCONFIG DRM_I915_FORCE_PROBE=\"*\" DRM_I915_REQUEST_TIMEOUT=20000
    DRM_I915_USERFAULT_AUTOSUSPEND=250 DRM_I915_STOP_TIMEOUT=100
    DRM_I915_PREEMPT_TIMEOUT=640 DRM_I915_PREEMPT_TIMEOUT_COMPUTE=7500
    DRM_I915_HEARTBEAT_INTERVAL=2500 DRM_I915_TIMESLICE_DURATION=1
    DRM_I915_MAX_REQUEST_BUSYWAIT=8000 DRM_I915_FENCE_TIMEOUT=10000 X86_PAT
    AS_MOVNTDQA)

function(pd_drm_i915_includes drmkmod out)
    set(${out}
        "${drmkmod}/drivers/gpu/drm/i915"
        "${drmkmod}/drivers/gpu/drm/i915/display" PARENT_SCOPE)
endfunction()

function(pd_drm_nouveau_includes drmkmod out)
    set(n "${drmkmod}/drivers/gpu/drm/nouveau")
    set(${out} "${n}/include" "${n}/include/nvkm" "${n}/nvkm" "${n}" PARENT_SCOPE)
endfunction()
