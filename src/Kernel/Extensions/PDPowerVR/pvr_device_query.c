// SPDX-License-Identifier: GPL-2.0-only OR MIT
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// the uapi quirk and feature queries of pvr_device.c

#include "pvr_device.h"
#include "pvr_device_info.h"

bool
pvr_device_has_uapi_quirk(struct pvr_device *pvr_dev, u32 quirk)
{
	switch (quirk) {
	case 47217:
		return PVR_HAS_QUIRK(pvr_dev, 47217);
	case 48545:
		return PVR_HAS_QUIRK(pvr_dev, 48545);
	case 49927:
		return PVR_HAS_QUIRK(pvr_dev, 49927);
	case 51764:
		return PVR_HAS_QUIRK(pvr_dev, 51764);
	case 62269:
		return PVR_HAS_QUIRK(pvr_dev, 62269);
	default:
		return false;
	};
}

bool
pvr_device_has_uapi_enhancement(struct pvr_device *pvr_dev, u32 enhancement)
{
	switch (enhancement) {
	case 35421:
		return PVR_HAS_ENHANCEMENT(pvr_dev, 35421);
	case 42064:
		return PVR_HAS_ENHANCEMENT(pvr_dev, 42064);
	default:
		return false;
	};
}

bool
pvr_device_has_feature(struct pvr_device *pvr_dev, u32 feature)
{
	switch (feature) {
	case PVR_FEATURE_CLUSTER_GROUPING:
		return PVR_HAS_FEATURE(pvr_dev, cluster_grouping);

	case PVR_FEATURE_COMPUTE_MORTON_CAPABLE:
		return PVR_HAS_FEATURE(pvr_dev, compute_morton_capable);

	case PVR_FEATURE_FB_CDC_V4:
		return PVR_HAS_FEATURE(pvr_dev, fb_cdc_v4);

	case PVR_FEATURE_GPU_MULTICORE_SUPPORT:
		return PVR_HAS_FEATURE(pvr_dev, gpu_multicore_support);

	case PVR_FEATURE_ISP_ZLS_D24_S8_PACKING_OGL_MODE:
		return PVR_HAS_FEATURE(pvr_dev, isp_zls_d24_s8_packing_ogl_mode);

	case PVR_FEATURE_S7_TOP_INFRASTRUCTURE:
		return PVR_HAS_FEATURE(pvr_dev, s7_top_infrastructure);

	case PVR_FEATURE_TESSELLATION:
		return PVR_HAS_FEATURE(pvr_dev, tessellation);

	case PVR_FEATURE_TPU_DM_GLOBAL_REGISTERS:
		return PVR_HAS_FEATURE(pvr_dev, tpu_dm_global_registers);

	case PVR_FEATURE_VDM_DRAWINDIRECT:
		return PVR_HAS_FEATURE(pvr_dev, vdm_drawindirect);

	case PVR_FEATURE_VDM_OBJECT_LEVEL_LLS:
		return PVR_HAS_FEATURE(pvr_dev, vdm_object_level_lls);

	case PVR_FEATURE_ZLS_SUBTILE:
		return PVR_HAS_FEATURE(pvr_dev, zls_subtile);

	/* Derived features. */
	case PVR_FEATURE_CDM_USER_MODE_QUEUE: {
		u8 cdm_control_stream_format = 0;

		PVR_FEATURE_VALUE(pvr_dev, cdm_control_stream_format, &cdm_control_stream_format);
		return (cdm_control_stream_format >= 2 && cdm_control_stream_format <= 4);
	}

	case PVR_FEATURE_REQUIRES_FB_CDC_ZLS_SETUP:
		if (PVR_HAS_FEATURE(pvr_dev, fbcdc_algorithm)) {
			u8 fbcdc_algorithm = 0;

			PVR_FEATURE_VALUE(pvr_dev, fbcdc_algorithm, &fbcdc_algorithm);
			return (fbcdc_algorithm < 3 || PVR_HAS_FEATURE(pvr_dev, fb_cdc_v4));
		}
		return false;

	default:
		WARN(true, "Looking up undefined feature %u\n", feature);
		return false;
	}
}
