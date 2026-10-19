// SPDX-License-Identifier: MIT
// amdgpu trace points, every one compiles away and the *_enabled checks report off
#ifndef _PD_AMDGPU_TRACE_FREEBSD_H_
#define _PD_AMDGPU_TRACE_FREEBSD_H_

#define trace_amdgpu_bo_create(...) do { } while (0)
#define trace_amdgpu_bo_list_set(...) do { } while (0)
#define trace_amdgpu_bo_move(...) do { } while (0)
#define trace_amdgpu_cs(...) do { } while (0)
#define trace_amdgpu_cs_bo_status(...) do { } while (0)
#define trace_amdgpu_cs_ioctl(...) do { } while (0)
#define trace_amdgpu_device_rreg(...) do { } while (0)
#define trace_amdgpu_device_wreg(...) do { } while (0)
#define trace_amdgpu_ib_pipe_sync(...) do { } while (0)
#define trace_amdgpu_iv(...) do { } while (0)
#define trace_amdgpu_pasid_allocated(...) do { } while (0)
#define trace_amdgpu_pasid_freed(...) do { } while (0)
#define trace_amdgpu_sched_run_job(...) do { } while (0)
#define trace_amdgpu_vm_bo_cs(...) do { } while (0)
#define trace_amdgpu_vm_bo_map(...) do { } while (0)
#define trace_amdgpu_vm_bo_mapping(...) do { } while (0)
#define trace_amdgpu_vm_bo_unmap(...) do { } while (0)
#define trace_amdgpu_vm_bo_update(...) do { } while (0)
#define trace_amdgpu_vm_copy_ptes(...) do { } while (0)
#define trace_amdgpu_vm_flush(...) do { } while (0)
#define trace_amdgpu_vm_grab_id(...) do { } while (0)
#define trace_amdgpu_vm_set_ptes(...) do { } while (0)
#define trace_amdgpu_vm_update_ptes(...) do { } while (0)
#define trace_amdgpu_runpm_reference_dumps(...) do { } while (0)
#define trace_amdgpu_reset_reg_dumps(...) do { } while (0)
#define trace_amdgpu_isolation(...) do { } while (0)
#define trace_amdgpu_cleaner_shader(...) do { } while (0)
#define trace_amdgpu_cs_enabled() false
#define trace_amdgpu_vm_bo_cs_enabled() false
#define trace_amdgpu_vm_bo_mapping_enabled() false

#endif
