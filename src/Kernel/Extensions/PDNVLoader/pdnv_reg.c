/*
 * SPDX-FileCopyrightText: Copyright (c) 2000-2018 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-FileCopyrightText: Copyright (c) 2026 The PureDarwin Project
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

// pdnv_reg.c - the registry parameter table and os_registry_init, changed by the PureDarwin Project from
// NVIDIA's kernel/nvidia/os-registry.c (580.196.07) for xnu: the resource manager is called through pointers
// to the linked object, and the stack helpers stand in for nv_kmem_cache_alloc_stack
#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>
#include <string.h>

#include <nvtypes.h>
#include <nvstatus.h>
#include <os-interface.h>
#include <nv.h>

// the table and its NVreg_* options, plain variables as with no module parameters
#define NV_DEFINE_REGISTRY_KEY_TABLE
#include "nv-reg.h"

#include "pdnv_port.h"

struct pdnv_rm_calls pdnv_rm;

typedef NV_STATUS (NV_API_CALL *write_dword_fn)(nvidia_stack_t *, nv_state_t *, const char *, NvU32);
typedef NV_STATUS (NV_API_CALL *write_string_fn)(nvidia_stack_t *, nv_state_t *, const char *, const char *,
    NvU32);
typedef void (NV_API_CALL *parse_option_fn)(nvidia_stack_t *, const char *);
typedef NvBool (NV_API_CALL *altstack_fn)(void);

const void *pdnv_parms_table = nv_parms;
const unsigned long pdnv_parms_size = sizeof(nv_parms);

// an nvidia_stack_t when the resource manager wants its own stack, else NULL, as on Linux
int
pdnv_stack_alloc(nvidia_stack_t **sp)
{
	*sp = NULL;
	if (pdnv_rm.is_altstack_in_use == NULL || !((altstack_fn)pdnv_rm.is_altstack_in_use)())
		return 0;

	*sp = IOMallocAligned(sizeof(nvidia_stack_t), 16);
	if (*sp == NULL)
		return -1;

	(*sp)->size = sizeof((*sp)->stack);
	(*sp)->top = (*sp)->stack + (*sp)->size;
	return 0;
}

void
pdnv_stack_free(nvidia_stack_t *sp)
{
	if (sp != NULL)
		IOFreeAligned(sp, sizeof(nvidia_stack_t));
}

NV_STATUS NV_API_CALL
os_registry_init(void)
{
	write_dword_fn write_dword = (write_dword_fn)pdnv_rm.write_registry_dword;
	write_string_fn write_string = (write_string_fn)pdnv_rm.write_registry_string;
	parse_option_fn parse_option = (parse_option_fn)pdnv_rm.parse_option_string;
	nvidia_stack_t *sp = NULL;
	nv_parm_t *entry;
	unsigned int i;

	if (write_dword == NULL || write_string == NULL || parse_option == NULL)
		return NV_ERR_NOT_SUPPORTED;

	if (pdnv_stack_alloc(&sp) != 0)
		return NV_ERR_NO_MEMORY;

	// the module options Linux takes on insmod, from boot-args here: pdnvreg="Key=Value;...", pdnvmsg=RmMsg
	static char sRegistryDwords[256], sRmMsg[128];
	if (PE_parse_boot_argn("pdnvreg", sRegistryDwords, sizeof(sRegistryDwords)))
		NVreg_RegistryDwords = sRegistryDwords;
	if (PE_parse_boot_argn("pdnvmsg", sRmMsg, sizeof(sRmMsg)))
		NVreg_RmMsg = sRmMsg;

	if (NVreg_RmNvlinkBandwidth != NULL)
		write_string(sp, NULL, "RmNvlinkBandwidth", NVreg_RmNvlinkBandwidth,
		    strlen(NVreg_RmNvlinkBandwidth));

	if (NVreg_RmMsg != NULL)
		write_string(sp, NULL, "RmMsg", NVreg_RmMsg, strlen(NVreg_RmMsg));

	// CoherentGPUMemoryMode=driver just implies the older EnableUserNUMAManagement=0 option
	if (NVreg_CoherentGPUMemoryMode != NULL && strcmp(NVreg_CoherentGPUMemoryMode, "driver") == 0)
		NVreg_EnableUserNUMAManagement = 0;

	parse_option(sp, NVreg_RegistryDwords);

	for (i = 0; (entry = &nv_parms[i])->name != NULL; i++)
		write_dword(sp, NULL, entry->name, *entry->data);

	pdnv_stack_free(sp);
	return NV_OK;
}
