// SPDX-License-Identifier: MIT
// radeon trace points, every one compiles away
#ifndef _PD_RADEON_TRACE_FREEBSD_H_
#define _PD_RADEON_TRACE_FREEBSD_H_

#define trace_radeon_bo_create(...) do { } while (0)
#define trace_radeon_cs(...) do { } while (0)
#define trace_radeon_fence_emit(...) do { } while (0)
#define trace_radeon_fence_wait_begin(...) do { } while (0)
#define trace_radeon_fence_wait_end(...) do { } while (0)
#define trace_radeon_semaphore_signale(...) do { } while (0)
#define trace_radeon_semaphore_wait(...) do { } while (0)
#define trace_radeon_vm_bo_update(...) do { } while (0)
#define trace_radeon_vm_flush(...) do { } while (0)
#define trace_radeon_vm_grab_id(...) do { } while (0)
#define trace_radeon_vm_set_page(...) do { } while (0)

#endif
