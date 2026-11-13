// pdnv_port.h - the ported interface layer's names: what the resource manager can call, and its own entry points
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PDNV_PORT_H
#define PDNV_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

// every import the port defines, X(name). A new os_* or nv_* function goes here and in its source file
#define PDNV_PORTED(X) \
	X(os_alloc_mem) X(os_free_mem) X(os_mem_set) X(os_mem_copy) X(os_mem_cmp) \
	X(os_string_compare) X(os_string_copy) X(os_string_length) X(os_strtoul) X(os_snprintf) \
	X(out_string) X(nv_printf) X(nv_log_error) \
	X(os_add_record_for_crashLog) X(os_delete_record_for_crashLog) \
	X(os_dbg_init) X(os_dbg_set_level) X(os_dbg_breakpoint) X(os_dump_stack) X(os_bug_check) \
	X(os_alloc_spinlock) X(os_free_spinlock) X(os_acquire_spinlock) X(os_release_spinlock) \
	X(os_semaphore_may_sleep) X(os_is_isr) \
	X(os_alloc_mutex) X(os_free_mutex) X(os_acquire_mutex) X(os_cond_acquire_mutex) X(os_release_mutex) \
	X(os_alloc_semaphore) X(os_free_semaphore) X(os_acquire_semaphore) X(os_cond_acquire_semaphore) \
	X(os_release_semaphore) \
	X(os_alloc_rwlock) X(os_free_rwlock) X(os_acquire_rwlock_read) X(os_acquire_rwlock_write) \
	X(os_cond_acquire_rwlock_read) X(os_cond_acquire_rwlock_write) X(os_release_rwlock_read) \
	X(os_release_rwlock_write) \
	X(os_get_monotonic_time_ns) X(os_get_monotonic_time_ns_hr) X(os_get_monotonic_tick_resolution_ns) \
	X(os_get_system_time) X(os_delay_us) X(os_delay) \
	X(os_get_cpu_count) X(os_get_cpu_number) X(os_get_cpu_frequency) \
	X(os_get_current_process) X(os_get_current_process_name) X(os_get_current_thread) X(os_get_euid) \
	X(os_is_administrator) X(os_check_access) X(os_get_current_process_flags) \
	X(os_pci_init_handle) X(os_pci_read_byte) X(os_pci_read_word) X(os_pci_read_dword) \
	X(os_pci_write_byte) X(os_pci_write_word) X(os_pci_write_dword) X(os_pci_remove_supported) X(os_pci_remove) \
	X(os_map_kernel_space) X(os_unmap_kernel_space) X(nv_requires_dma_remap) X(nv_set_dma_address_size) \
	X(nv_alloc_pages) X(nv_free_pages) X(nv_alloc_kernel_mapping) X(nv_free_kernel_mapping) X(nv_get_kern_phys_address) \
	X(nv_get_dev_minor) X(nv_get_disp_smmu_stream_ids) X(nv_get_screen_info) X(os_disable_console_access) \
	X(os_enable_console_access) X(nv_acpi_methods_init) X(nv_acpi_methods_uninit) X(nv_acpi_dsm_method) \
	X(nv_acpi_dod_method) X(nv_acpi_mux_method) X(nv_get_device_memory_config) X(os_pat_supported) \
	X(nv_put_firmware) X(os_get_pid_info) X(os_put_pid_info) X(os_queue_work_item) X(os_flush_work_queue) X(nv_acpi_ddc_method) X(os_schedule) X(nv_start_rc_timer) X(nv_stop_rc_timer) X(nv_i2c_add_adapter) X(nv_i2c_del_adapter) \
	X(nv_get_ctl_state) X(nv_get_file_private) X(nv_put_file_private) X(nv_is_gpu_accessible) X(os_memcpy_from_user) X(os_memcpy_to_user) \
	X(nv_add_mapping_context_to_file) X(nv_alloc_user_mapping) X(nv_free_user_mapping) X(os_match_mmap_offset) \
	X(nv_check_usermap_access_params) X(nv_acquire_mmap_lock) X(nv_release_mmap_lock) X(nv_set_safe_to_mmap_locked) X(nv_post_event) X(nv_get_event) X(os_get_random_bytes) \
	X(nv_is_chassis_notebook) X(nv_acpi_is_battery_present) X(nv_platform_supports_s0ix) \
	X(os_is_efi_enabled) X(os_is_init_ns) X(os_is_grid_supported) X(os_get_grid_csp_support) \
	X(os_is_vgx_hyper) X(os_is_nvswitch_present) X(os_device_vm_present) X(os_get_is_openrm) \
	X(os_get_max_user_va) X(os_flush_cpu_write_combine_buffer) \
	X(os_registry_init)

// the resource manager's own functions the port calls back, filled from the linked object's exports
struct pdnv_rm_calls {
	void *write_registry_dword;         // NV_STATUS (nvidia_stack_t *, nv_state_t *, const char *, NvU32)
	void *write_registry_string;        // NV_STATUS (nvidia_stack_t *, nv_state_t *, const char *, const char *, NvU32)
	void *parse_option_string;          // void (nvidia_stack_t *, const char *)
	void *is_altstack_in_use;           // NvBool (void)
	void *is_supported_device;          // NV_STATUS (nvidia_stack_t *, nv_state_t *)
	void *init_private_state;           // NvBool (nvidia_stack_t *, nv_state_t *)
	void *init_event_locks;             // NvBool (nvidia_stack_t *, nv_state_t *)
	void *init_adapter;                 // NvBool (nvidia_stack_t *, nv_state_t *)
	void *isr;                          // NvBool (nvidia_stack_t *, nv_state_t *, NvU32 *)
	void *isr_bh;                       // void (nvidia_stack_t *, nv_state_t *)
	void *execute_work_item;            // void (nvidia_stack_t *, void *)
	void *run_rc_callback;              // NV_STATUS (nvidia_stack_t *, nv_state_t *)
	void *ioctl;                        // NV_STATUS (nvidia_stack_t *, nv_state_t *, nv_file_private_t *, NvU32, void *, NvU32)
	void *cleanup_file_private;         // void (nvidia_stack_t *, nv_state_t *, nv_file_private_t *)
	void *perform_version_check;        // NV_STATUS (nvidia_stack_t *, void *, NvU32)
};
extern struct pdnv_rm_calls pdnv_rm;

// pdnv_imports.c: the loader's resolver, every unported function import goes to a named trap
struct pdelf_target;
int pdnv_resolve(void *ctx, const char *name, struct pdelf_target *t);

// pdnv_trap.c
void *pdnv_trap_slot(const char *name);    // NULL once the 512 traps are used up
void pdnv_trap_set_trace(int on);

#ifdef __cplusplus
}
#endif

#endif
