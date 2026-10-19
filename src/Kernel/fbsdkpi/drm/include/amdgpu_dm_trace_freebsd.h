// SPDX-License-Identifier: MIT
// amdgpu_dm trace points, every one compiles away
#ifndef _PD_AMDGPU_DM_TRACE_FREEBSD_H_
#define _PD_AMDGPU_DM_TRACE_FREEBSD_H_

// users of the trace header expect the dc core types to come with it
#include "core_types.h"

#define trace_amdgpu_dc_performance(...) do { } while (0)
#define trace_amdgpu_dc_rreg(...) do { } while (0)
#define trace_amdgpu_dc_wreg(...) do { } while (0)
#define trace_amdgpu_dm_atomic_check_begin(...) do { } while (0)
#define trace_amdgpu_dm_atomic_check_finish(...) do { } while (0)
#define trace_amdgpu_dm_atomic_commit_tail_begin(...) do { } while (0)
#define trace_amdgpu_dm_atomic_commit_tail_finish(...) do { } while (0)
#define trace_amdgpu_dm_atomic_update_cursor(...) do { } while (0)
#define trace_amdgpu_dm_connector_atomic_check(...) do { } while (0)
#define trace_amdgpu_dm_crtc_atomic_check(...) do { } while (0)
#define trace_amdgpu_dm_dc_clocks_state(...) do { } while (0)
#define trace_amdgpu_dm_dce_clocks_state(...) do { } while (0)
#define trace_amdgpu_dm_dc_pipe_state(...) do { } while (0)
#define trace_amdgpu_dm_plane_atomic_check(...) do { } while (0)
#define trace_amdgpu_dmub_trace_high_irq(...) do { } while (0)
#define trace_amdgpu_refresh_rate_track(...) do { } while (0)
#define trace_dcn_fpu(...) do { } while (0)
#define trace_dcn_optc_lock_unlock_state(...) do { } while (0)
#define amdgpu_dm_trace_dcn_fpu(...) do { } while (0)

#endif
