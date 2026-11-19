// pdnv_uvm.c - what PDNVUVM links against here: the resource manager's GPU ops and the device calls nv.c gives nvidia-uvm
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
//
// The rm_gpu_ops_* functions live in the linked object, not in this kext's symbol table. Each name below is a
// jump through a slot that pdnv_uvm_link fills from the object's exports, so PDNVUVM calls them by name. A
// name the object does not export jumps to a function that answers NV_ERR_NOT_SUPPORTED.
#include <IOKit/IOLib.h>
#include <kern/thread_call.h>
#include <string.h>

#include <nvtypes.h>
#include <nvstatus.h>
#include <os-interface.h>
#include <nv.h>

#include "pdnv_port.h"
#include "pd_elf_rel.h"

#define PDNV_UVM_RM_CALLS(X) \
	X(rm_gpu_ops_address_space_create) \
	X(rm_gpu_ops_address_space_destroy) \
	X(rm_gpu_ops_bind_channel_resources) \
	X(rm_gpu_ops_ccsl_context_clear) \
	X(rm_gpu_ops_ccsl_context_init) \
	X(rm_gpu_ops_ccsl_decrypt) \
	X(rm_gpu_ops_ccsl_encrypt) \
	X(rm_gpu_ops_ccsl_encrypt_with_iv) \
	X(rm_gpu_ops_ccsl_increment_iv) \
	X(rm_gpu_ops_ccsl_log_encryption) \
	X(rm_gpu_ops_ccsl_query_message_pool) \
	X(rm_gpu_ops_ccsl_rotate_iv) \
	X(rm_gpu_ops_ccsl_rotate_key) \
	X(rm_gpu_ops_ccsl_sign) \
	X(rm_gpu_ops_channel_allocate) \
	X(rm_gpu_ops_channel_destroy) \
	X(rm_gpu_ops_create_session) \
	X(rm_gpu_ops_destroy_access_cntr_info) \
	X(rm_gpu_ops_destroy_fault_info) \
	X(rm_gpu_ops_destroy_session) \
	X(rm_gpu_ops_device_create) \
	X(rm_gpu_ops_device_destroy) \
	X(rm_gpu_ops_disable_access_cntr) \
	X(rm_gpu_ops_dup_address_space) \
	X(rm_gpu_ops_dup_allocation) \
	X(rm_gpu_ops_dup_memory) \
	X(rm_gpu_ops_enable_access_cntr) \
	X(rm_gpu_ops_flush_replayable_fault_buffer) \
	X(rm_gpu_ops_free_duped_handle) \
	X(rm_gpu_ops_get_channel_resource_ptes) \
	X(rm_gpu_ops_get_ecc_info) \
	X(rm_gpu_ops_get_external_alloc_phys_addrs) \
	X(rm_gpu_ops_get_external_alloc_ptes) \
	X(rm_gpu_ops_get_fb_info) \
	X(rm_gpu_ops_get_gpu_info) \
	X(rm_gpu_ops_get_non_replayable_faults) \
	X(rm_gpu_ops_get_nvlink_info) \
	X(rm_gpu_ops_get_p2p_caps) \
	X(rm_gpu_ops_get_pma_object) \
	X(rm_gpu_ops_has_pending_non_replayable_faults) \
	X(rm_gpu_ops_init_access_cntr_info) \
	X(rm_gpu_ops_init_fault_info) \
	X(rm_gpu_ops_memory_alloc_fb) \
	X(rm_gpu_ops_memory_alloc_sys) \
	X(rm_gpu_ops_memory_cpu_map) \
	X(rm_gpu_ops_memory_cpu_ummap) \
	X(rm_gpu_ops_memory_free) \
	X(rm_gpu_ops_own_access_cntr_intr) \
	X(rm_gpu_ops_own_page_fault_intr) \
	X(rm_gpu_ops_p2p_object_create) \
	X(rm_gpu_ops_p2p_object_destroy) \
	X(rm_gpu_ops_paging_channel_allocate) \
	X(rm_gpu_ops_paging_channel_destroy) \
	X(rm_gpu_ops_paging_channel_push_stream) \
	X(rm_gpu_ops_paging_channels_map) \
	X(rm_gpu_ops_paging_channels_unmap) \
	X(rm_gpu_ops_pma_alloc_pages) \
	X(rm_gpu_ops_pma_free_pages) \
	X(rm_gpu_ops_pma_pin_pages) \
	X(rm_gpu_ops_pma_register_callbacks) \
	X(rm_gpu_ops_pma_unregister_callbacks) \
	X(rm_gpu_ops_query_caps) \
	X(rm_gpu_ops_query_ces_caps) \
	X(rm_gpu_ops_release_channel) \
	X(rm_gpu_ops_report_fatal_error) \
	X(rm_gpu_ops_report_non_replayable_fault) \
	X(rm_gpu_ops_retain_channel) \
	X(rm_gpu_ops_service_device_interrupts_rm) \
	X(rm_gpu_ops_set_page_directory) \
	X(rm_gpu_ops_stop_channel) \
	X(rm_gpu_ops_toggle_prefetch_faults) \
	X(rm_gpu_ops_tsg_allocate) \
	X(rm_gpu_ops_tsg_destroy) \
	X(rm_gpu_ops_unset_page_directory) \
	X(rm_is_altstack_in_use)

enum {
#define X(n) PDNV_UVM_SLOT_##n,
	PDNV_UVM_RM_CALLS(X)
#undef X
	PDNV_UVM_NSLOTS
};

static const char *const pdnv_uvm_names[PDNV_UVM_NSLOTS] = {
#define X(n) #n,
	PDNV_UVM_RM_CALLS(X)
#undef X
};

static NV_STATUS
pdnv_uvm_unsupported(void)
{
	return NV_ERR_NOT_SUPPORTED;
}

// one slot per name, each starting at the fallback
void *pdnv_uvm_slots[PDNV_UVM_NSLOTS];

// the jumps: each loads its slot number and goes through pdnv_uvm_jump, which logs the call's name when pdnvtrace is
// set (the six argument registers kept) and then jumps through the slot
int pdnv_uvm_trace;
void pdnv_uvm_log(unsigned slot);
__asm__(".text\n"
	".p2align 4\n_pdnv_uvm_jump:\n"
	"\tcmpl $0, _pdnv_uvm_trace(%rip)\n"
	"\tje 1f\n"
	"\tpushq %rdi\n\tpushq %rsi\n\tpushq %rdx\n\tpushq %rcx\n\tpushq %r8\n\tpushq %r9\n\tpushq %rax\n"
	"\tmovl %eax, %edi\n"
	"\tcall _pdnv_uvm_log\n"
	"\tpopq %rax\n\tpopq %r9\n\tpopq %r8\n\tpopq %rcx\n\tpopq %rdx\n\tpopq %rsi\n\tpopq %rdi\n"
	"1:\tleaq _pdnv_uvm_slots(%rip), %r11\n"
	"\tjmpq *(%r11,%rax,8)\n"
	".globl _rm_gpu_ops_address_space_create\n.p2align 4\n_rm_gpu_ops_address_space_create:\n\tmovl $0, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_address_space_destroy\n.p2align 4\n_rm_gpu_ops_address_space_destroy:\n\tmovl $1, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_bind_channel_resources\n.p2align 4\n_rm_gpu_ops_bind_channel_resources:\n\tmovl $2, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_ccsl_context_clear\n.p2align 4\n_rm_gpu_ops_ccsl_context_clear:\n\tmovl $3, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_ccsl_context_init\n.p2align 4\n_rm_gpu_ops_ccsl_context_init:\n\tmovl $4, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_ccsl_decrypt\n.p2align 4\n_rm_gpu_ops_ccsl_decrypt:\n\tmovl $5, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_ccsl_encrypt\n.p2align 4\n_rm_gpu_ops_ccsl_encrypt:\n\tmovl $6, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_ccsl_encrypt_with_iv\n.p2align 4\n_rm_gpu_ops_ccsl_encrypt_with_iv:\n\tmovl $7, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_ccsl_increment_iv\n.p2align 4\n_rm_gpu_ops_ccsl_increment_iv:\n\tmovl $8, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_ccsl_log_encryption\n.p2align 4\n_rm_gpu_ops_ccsl_log_encryption:\n\tmovl $9, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_ccsl_query_message_pool\n.p2align 4\n_rm_gpu_ops_ccsl_query_message_pool:\n\tmovl $10, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_ccsl_rotate_iv\n.p2align 4\n_rm_gpu_ops_ccsl_rotate_iv:\n\tmovl $11, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_ccsl_rotate_key\n.p2align 4\n_rm_gpu_ops_ccsl_rotate_key:\n\tmovl $12, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_ccsl_sign\n.p2align 4\n_rm_gpu_ops_ccsl_sign:\n\tmovl $13, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_channel_allocate\n.p2align 4\n_rm_gpu_ops_channel_allocate:\n\tmovl $14, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_channel_destroy\n.p2align 4\n_rm_gpu_ops_channel_destroy:\n\tmovl $15, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_create_session\n.p2align 4\n_rm_gpu_ops_create_session:\n\tmovl $16, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_destroy_access_cntr_info\n.p2align 4\n_rm_gpu_ops_destroy_access_cntr_info:\n\tmovl $17, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_destroy_fault_info\n.p2align 4\n_rm_gpu_ops_destroy_fault_info:\n\tmovl $18, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_destroy_session\n.p2align 4\n_rm_gpu_ops_destroy_session:\n\tmovl $19, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_device_create\n.p2align 4\n_rm_gpu_ops_device_create:\n\tmovl $20, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_device_destroy\n.p2align 4\n_rm_gpu_ops_device_destroy:\n\tmovl $21, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_disable_access_cntr\n.p2align 4\n_rm_gpu_ops_disable_access_cntr:\n\tmovl $22, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_dup_address_space\n.p2align 4\n_rm_gpu_ops_dup_address_space:\n\tmovl $23, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_dup_allocation\n.p2align 4\n_rm_gpu_ops_dup_allocation:\n\tmovl $24, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_dup_memory\n.p2align 4\n_rm_gpu_ops_dup_memory:\n\tmovl $25, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_enable_access_cntr\n.p2align 4\n_rm_gpu_ops_enable_access_cntr:\n\tmovl $26, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_flush_replayable_fault_buffer\n.p2align 4\n_rm_gpu_ops_flush_replayable_fault_buffer:\n\tmovl $27, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_free_duped_handle\n.p2align 4\n_rm_gpu_ops_free_duped_handle:\n\tmovl $28, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_get_channel_resource_ptes\n.p2align 4\n_rm_gpu_ops_get_channel_resource_ptes:\n\tmovl $29, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_get_ecc_info\n.p2align 4\n_rm_gpu_ops_get_ecc_info:\n\tmovl $30, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_get_external_alloc_phys_addrs\n.p2align 4\n_rm_gpu_ops_get_external_alloc_phys_addrs:\n\tmovl $31, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_get_external_alloc_ptes\n.p2align 4\n_rm_gpu_ops_get_external_alloc_ptes:\n\tmovl $32, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_get_fb_info\n.p2align 4\n_rm_gpu_ops_get_fb_info:\n\tmovl $33, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_get_gpu_info\n.p2align 4\n_rm_gpu_ops_get_gpu_info:\n\tmovl $34, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_get_non_replayable_faults\n.p2align 4\n_rm_gpu_ops_get_non_replayable_faults:\n\tmovl $35, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_get_nvlink_info\n.p2align 4\n_rm_gpu_ops_get_nvlink_info:\n\tmovl $36, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_get_p2p_caps\n.p2align 4\n_rm_gpu_ops_get_p2p_caps:\n\tmovl $37, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_get_pma_object\n.p2align 4\n_rm_gpu_ops_get_pma_object:\n\tmovl $38, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_has_pending_non_replayable_faults\n.p2align 4\n_rm_gpu_ops_has_pending_non_replayable_faults:\n\tmovl $39, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_init_access_cntr_info\n.p2align 4\n_rm_gpu_ops_init_access_cntr_info:\n\tmovl $40, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_init_fault_info\n.p2align 4\n_rm_gpu_ops_init_fault_info:\n\tmovl $41, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_memory_alloc_fb\n.p2align 4\n_rm_gpu_ops_memory_alloc_fb:\n\tmovl $42, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_memory_alloc_sys\n.p2align 4\n_rm_gpu_ops_memory_alloc_sys:\n\tmovl $43, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_memory_cpu_map\n.p2align 4\n_rm_gpu_ops_memory_cpu_map:\n\tmovl $44, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_memory_cpu_ummap\n.p2align 4\n_rm_gpu_ops_memory_cpu_ummap:\n\tmovl $45, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_memory_free\n.p2align 4\n_rm_gpu_ops_memory_free:\n\tmovl $46, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_own_access_cntr_intr\n.p2align 4\n_rm_gpu_ops_own_access_cntr_intr:\n\tmovl $47, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_own_page_fault_intr\n.p2align 4\n_rm_gpu_ops_own_page_fault_intr:\n\tmovl $48, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_p2p_object_create\n.p2align 4\n_rm_gpu_ops_p2p_object_create:\n\tmovl $49, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_p2p_object_destroy\n.p2align 4\n_rm_gpu_ops_p2p_object_destroy:\n\tmovl $50, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_paging_channel_allocate\n.p2align 4\n_rm_gpu_ops_paging_channel_allocate:\n\tmovl $51, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_paging_channel_destroy\n.p2align 4\n_rm_gpu_ops_paging_channel_destroy:\n\tmovl $52, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_paging_channel_push_stream\n.p2align 4\n_rm_gpu_ops_paging_channel_push_stream:\n\tmovl $53, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_paging_channels_map\n.p2align 4\n_rm_gpu_ops_paging_channels_map:\n\tmovl $54, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_paging_channels_unmap\n.p2align 4\n_rm_gpu_ops_paging_channels_unmap:\n\tmovl $55, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_pma_alloc_pages\n.p2align 4\n_rm_gpu_ops_pma_alloc_pages:\n\tmovl $56, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_pma_free_pages\n.p2align 4\n_rm_gpu_ops_pma_free_pages:\n\tmovl $57, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_pma_pin_pages\n.p2align 4\n_rm_gpu_ops_pma_pin_pages:\n\tmovl $58, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_pma_register_callbacks\n.p2align 4\n_rm_gpu_ops_pma_register_callbacks:\n\tmovl $59, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_pma_unregister_callbacks\n.p2align 4\n_rm_gpu_ops_pma_unregister_callbacks:\n\tmovl $60, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_query_caps\n.p2align 4\n_rm_gpu_ops_query_caps:\n\tmovl $61, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_query_ces_caps\n.p2align 4\n_rm_gpu_ops_query_ces_caps:\n\tmovl $62, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_release_channel\n.p2align 4\n_rm_gpu_ops_release_channel:\n\tmovl $63, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_report_fatal_error\n.p2align 4\n_rm_gpu_ops_report_fatal_error:\n\tmovl $64, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_report_non_replayable_fault\n.p2align 4\n_rm_gpu_ops_report_non_replayable_fault:\n\tmovl $65, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_retain_channel\n.p2align 4\n_rm_gpu_ops_retain_channel:\n\tmovl $66, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_service_device_interrupts_rm\n.p2align 4\n_rm_gpu_ops_service_device_interrupts_rm:\n\tmovl $67, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_set_page_directory\n.p2align 4\n_rm_gpu_ops_set_page_directory:\n\tmovl $68, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_stop_channel\n.p2align 4\n_rm_gpu_ops_stop_channel:\n\tmovl $69, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_toggle_prefetch_faults\n.p2align 4\n_rm_gpu_ops_toggle_prefetch_faults:\n\tmovl $70, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_tsg_allocate\n.p2align 4\n_rm_gpu_ops_tsg_allocate:\n\tmovl $71, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_tsg_destroy\n.p2align 4\n_rm_gpu_ops_tsg_destroy:\n\tmovl $72, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_gpu_ops_unset_page_directory\n.p2align 4\n_rm_gpu_ops_unset_page_directory:\n\tmovl $73, %eax\n\tjmp _pdnv_uvm_jump\n"
	".globl _rm_is_altstack_in_use\n.p2align 4\n_rm_is_altstack_in_use:\n\tmovl $74, %eax\n\tjmp _pdnv_uvm_jump\n"
);

// one line per resource manager call nvidia-uvm makes, with pdnvtrace
void
pdnv_uvm_log(unsigned slot)
{
	if (slot < PDNV_UVM_NSLOTS)
		IOLog("pdnv-uvm: %s\n", pdnv_uvm_names[slot]);
}

extern int PE_parse_boot_argn(const char *, void *, int);

void
pdnv_uvm_link(const struct pdelf_obj *obj)
{
	unsigned i, found = 0, trace = 0;

	if (PE_parse_boot_argn("pdnvtrace", &trace, sizeof(trace)))
		pdnv_uvm_trace = trace != 0;

	for (i = 0; i < PDNV_UVM_NSLOTS; i++) {
		uint64_t a = pdelf_export(obj, pdnv_uvm_names[i]);
		pdnv_uvm_slots[i] = a != 0 ? (void *)a : (void *)&pdnv_uvm_unsupported;
		found += a != 0;
	}
	IOLog("PDNVLoader: %u of %u resource manager calls for nvidia-uvm found\n", found, (unsigned)PDNV_UVM_NSLOTS);
}

// the resource manager's per-device calls that nv.c makes for nvidia-uvm
typedef const NvU8 *(*rm_uuid_raw_fn)(nvidia_stack_t *, nv_state_t *);
typedef NV_STATUS (*rm_client_count_fn)(nvidia_stack_t *, nv_state_t *, NvBool);
typedef NV_STATUS (*rm_ref_power_fn)(nvidia_stack_t *, nv_state_t *, nv_dynamic_power_mode_t);
typedef void (*rm_unref_power_fn)(nvidia_stack_t *, nv_state_t *, nv_dynamic_power_mode_t);
static rm_uuid_raw_fn rm_get_gpu_uuid_raw_p;
static rm_client_count_fn rm_set_external_kernel_client_count_p;
static rm_ref_power_fn rm_ref_dynamic_power_p;
static rm_unref_power_fn rm_unref_dynamic_power_p;

void
pdnv_uvm_link_device_calls(const struct pdelf_obj *obj)
{
	rm_get_gpu_uuid_raw_p = (rm_uuid_raw_fn)pdelf_export(obj, "rm_get_gpu_uuid_raw");
	rm_set_external_kernel_client_count_p = (rm_client_count_fn)pdelf_export(obj, "rm_set_external_kernel_client_count");
	rm_ref_dynamic_power_p = (rm_ref_power_fn)pdelf_export(obj, "rm_ref_dynamic_power");
	rm_unref_dynamic_power_p = (rm_unref_power_fn)pdelf_export(obj, "rm_unref_dynamic_power");
}

// the GPUs the resource manager has taken, which nvidia-uvm finds by UUID
#define PDNV_UVM_MAX_GPUS 32
static nv_state_t *pdnv_uvm_gpus[PDNV_UVM_MAX_GPUS];
static IOLock *pdnv_uvm_gpus_lock;

void
pdnv_uvm_gpu_add(nv_state_t *nv)
{
	unsigned i;

	if (pdnv_uvm_gpus_lock == NULL)
		pdnv_uvm_gpus_lock = IOLockAlloc();
	IOLockLock(pdnv_uvm_gpus_lock);
	for (i = 0; i < PDNV_UVM_MAX_GPUS; i++) {
		if (pdnv_uvm_gpus[i] == NULL) {
			pdnv_uvm_gpus[i] = nv;
			break;
		}
	}
	IOLockUnlock(pdnv_uvm_gpus_lock);
}

// a GPU's UUID, asked of the resource manager the first time and cached in its state as nv.c does
static const NvU8 *
gpu_uuid(nvidia_stack_t *sp, nv_state_t *nv)
{
	const NvU8 *uuid = nv_get_cached_uuid(nv);

	if (uuid == NULL && rm_get_gpu_uuid_raw_p != NULL && sp != NULL) {
		uuid = rm_get_gpu_uuid_raw_p(sp, nv);
		if (uuid != NULL) {
			memcpy(nv->nv_uuid_cache.uuid, uuid, GPU_UUID_LEN);
			nv->nv_uuid_cache.valid = NV_TRUE;
			uuid = nv->nv_uuid_cache.uuid;
		}
	}
	return uuid;
}

static nv_state_t *
find_uuid(const NvU8 *uuid, nvidia_stack_t *sp)
{
	nv_state_t *found = NULL;
	unsigned i;

	if (pdnv_uvm_gpus_lock == NULL)
		return NULL;
	IOLockLock(pdnv_uvm_gpus_lock);
	for (i = 0; i < PDNV_UVM_MAX_GPUS && found == NULL; i++) {
		nv_state_t *nv = pdnv_uvm_gpus[i];
		const NvU8 *u = nv != NULL ? gpu_uuid(sp, nv) : NULL;
		if (u != NULL && memcmp(u, uuid, GPU_UUID_LEN) == 0)
			found = nv;
	}
	IOLockUnlock(pdnv_uvm_gpus_lock);
	return found;
}

// GPUs here are brought up once at probe and stay up, so taking one is counting the kernel client
int
nvidia_dev_get_uuid(const NvU8 *uuid, nvidia_stack_t *sp)
{
	nv_state_t *nv = find_uuid(uuid, sp);

	if (nv == NULL)
		return -19;     // ENODEV
	if (rm_set_external_kernel_client_count_p != NULL)
		(void)rm_set_external_kernel_client_count_p(sp, nv, NV_TRUE);
	return 0;
}

void
nvidia_dev_put_uuid(const NvU8 *uuid, nvidia_stack_t *sp)
{
	nv_state_t *nv = find_uuid(uuid, sp);

	if (nv != NULL && rm_set_external_kernel_client_count_p != NULL)
		(void)rm_set_external_kernel_client_count_p(sp, nv, NV_FALSE);
}

int
nvidia_dev_block_gc6(const NvU8 *uuid, nvidia_stack_t *sp)
{
	nv_state_t *nv = find_uuid(uuid, sp);

	if (nv == NULL)
		return -19;
	if (rm_ref_dynamic_power_p != NULL && rm_ref_dynamic_power_p(sp, nv, NV_DYNAMIC_PM_FINE) != NV_OK)
		return -22;     // EINVAL
	return 0;
}

int
nvidia_dev_unblock_gc6(const NvU8 *uuid, nvidia_stack_t *sp)
{
	nv_state_t *nv = find_uuid(uuid, sp);

	if (nv == NULL)
		return -19;
	if (rm_unref_dynamic_power_p != NULL)
		rm_unref_dynamic_power_p(sp, nv, NV_DYNAMIC_PM_FINE);
	return 0;
}

// where a GPU sits on PCI and what it can address, for PDNVUVM to find its linuxkpi pci_dev
int
pdnv_uvm_gpu_location(const NvU8 *uuid, NvU32 *domain, NvU8 *bus, NvU8 *slot, NvU8 *function, NvU64 *dma_start,
    NvU64 *dma_limit)
{
	nv_state_t *nv = find_uuid(uuid, NULL);

	if (nv == NULL)
		return -19;
	*domain = nv->pci_info.domain;
	*bus = nv->pci_info.bus;
	*slot = nv->pci_info.slot;
	*function = nv->pci_info.function;
	*dma_start = 0;
	*dma_limit = pdnv_gpu_dma_mask(nv);
	return 0;
}

// nvidia-uvm's interrupt entry (nv_uvm_event_interrupt), which PDNVUVM gives here when it starts: the GPU's interrupts
// go to it before the resource manager, as nv.c's ISR does, so UVM sees its page faults and access counters
typedef NV_STATUS (*pdnv_uvm_isr_fn)(const NvU8 *uuid);
static pdnv_uvm_isr_fn volatile pdnv_uvm_isr;

// nv_schedule_uvm_isr's thread, below: the resource manager may ask from its own interrupt, and nvidia-uvm's handler
// may sleep
static thread_call_t pdnv_uvm_isr_call;
static void pdnv_uvm_isr_thread(thread_call_param_t nv, thread_call_param_t unused);

void
pdnv_uvm_set_isr(void *fn)
{
	if (pdnv_uvm_isr_call == NULL)
		pdnv_uvm_isr_call = thread_call_allocate(pdnv_uvm_isr_thread, NULL);
	pdnv_uvm_isr = (pdnv_uvm_isr_fn)fn;
}

// whether nvidia-uvm has given its interrupt entry
int
pdnv_uvm_listening(void)
{
	return pdnv_uvm_isr != NULL;
}

// NV_OK when the interrupt was nvidia-uvm's
unsigned int
pdnv_uvm_interrupt(nv_state_t *nv)
{
	pdnv_uvm_isr_fn fn = pdnv_uvm_isr;
	const NvU8 *uuid = nv_get_cached_uuid(nv);

	return fn != NULL && uuid != NULL ? fn(uuid) : NV_ERR_NO_INTR_PENDING;
}

static void
pdnv_uvm_isr_thread(thread_call_param_t nv, thread_call_param_t unused)
{
	(void)unused;
	(void)pdnv_uvm_interrupt((nv_state_t *)nv);
}

// the resource manager asking for nvidia-uvm's interrupt handling
void NV_API_CALL
nv_schedule_uvm_isr(nv_state_t *nv)
{
	if (pdnv_uvm_isr_call == NULL)
		return;
	thread_call_enter1(pdnv_uvm_isr_call, nv);
}

// peer-to-peer between GPUs, which nothing here sets up
NV_STATUS NV_API_CALL
nv_schedule_uvm_drain_p2p(NvU8 *uuid)
{
	(void)uuid;
	return NV_ERR_NOT_SUPPORTED;
}

void NV_API_CALL
nv_schedule_uvm_resume_p2p(NvU8 *uuid)
{
	(void)uuid;
}
