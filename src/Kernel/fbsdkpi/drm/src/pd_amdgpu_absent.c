// SPDX-License-Identifier: MIT
// IP blocks amdgpu's tables name that are left out (vkms, RDNA4 VCN 5 and JPEG 5 with a GPL header)
// each declines in early_init, so a GPU that needs one fails to initialise cleanly
#include "amdgpu.h"
#include "amdgpu_vkms.h"
#include "vcn_v5_0_0.h"
#include "vcn_v5_0_1.h"
#include "jpeg_v5_0_0.h"
#include "jpeg_v5_0_1.h"

static int
pd_absent_early_init(struct amdgpu_ip_block *ip_block)
{
	dev_err(ip_block->adev->dev, "%s is not built on PureDarwin\n",
	    ip_block->version->funcs->name);
	return -ENODEV;
}

#define PD_ABSENT_BLOCK(sym, nm, ty, ma, mi)				\
	static const struct amd_ip_funcs sym##_funcs = {		\
		.name = nm,						\
		.early_init = pd_absent_early_init,			\
	};								\
	const struct amdgpu_ip_block_version sym = {			\
		.type = ty, .major = ma, .minor = mi, .rev = 0,		\
		.funcs = &sym##_funcs,					\
	}

PD_ABSENT_BLOCK(amdgpu_vkms_ip_block, "amdgpu_vkms", AMD_IP_BLOCK_TYPE_DCE, 1, 0);
PD_ABSENT_BLOCK(vcn_v5_0_0_ip_block, "vcn_v5_0_0", AMD_IP_BLOCK_TYPE_VCN, 5, 0);
PD_ABSENT_BLOCK(vcn_v5_0_1_ip_block, "vcn_v5_0_1", AMD_IP_BLOCK_TYPE_VCN, 5, 0);
PD_ABSENT_BLOCK(jpeg_v5_0_0_ip_block, "jpeg_v5_0_0", AMD_IP_BLOCK_TYPE_JPEG, 5, 0);
PD_ABSENT_BLOCK(jpeg_v5_0_1_ip_block, "jpeg_v5_0_1", AMD_IP_BLOCK_TYPE_JPEG, 5, 0);
