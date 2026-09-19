/*
 * WSL guest protocol messages, transcribed to C from Microsoft's
 * src/shared/inc/lxinitshared.h as of WSL 2.7.14.
 *
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License: permission is hereby granted, free of
 * charge, to any person obtaining a copy of this software and associated
 * documentation files, to deal in the Software without restriction, subject to
 * including this notice. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND.
 */
#ifndef WSLPROTO_H
#define WSLPROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WSL_INIT_PORT           50000U
#define WSL_INVALID_PORT        0xffffffffU
#define WSL_PROCESS_SOCKETS     5

enum {
	LxInitMessageCreateSession = 2,
	LxInitMessageCreateSessionResponse = 3,
	LxInitMessageInitialize = 5,
	LxInitMessageInitializeResponse = 6,
	LxInitMessageTimezoneInformation = 7,
	LxInitMessageCreateProcessUtilityVm = 8,
	LxInitMessageExitStatus = 9,
	LxInitMessageWindowSizeChanged = 10,
	LxInitMessageRemountDrvfs = 13,
	LxInitMessageTerminateInstance = 14,
	LxInitCreateProcess = 27,
	LxMiniInitMessageLaunchInit = 29,
	LxMiniInitMessageCreateInstanceResult = 33,
	LxMiniInitMessageEarlyConfig = 36,
	LxMiniInitMessageInitialConfig = 37,
	LxMiniInitMessageGuestCapabilities = 43,
	LxGnsMessageInterfaceConfiguration = 53,
	LxGnsMessageResult = 54,
	LxGnsMessageNotification = 55,
	LxMessageResultBool = 76,
	LxMessageResultInt32 = 77,
	LxMessageResultUint32 = 78,
};

enum { STEP_NONE = 0, STEP_REQUEST = 1, STEP_FIRST_REPLY = 2 };

#define LxInitCreateProcessFlagAllowOOBE 0x20

struct wsl_header {
	uint32_t type;
	uint32_t size;
	uint32_t transaction_id;
	uint32_t transaction_step;
};

// The kernel version string follows seccomp_available in 2.7.14
// (offset 17) and two swiotlb fields in later releases (offset 40)
#define WSL_CAPS_VERSION_OFFSET_2_7   17
#define WSL_CAPS_VERSION_OFFSET_NEXT  40

struct wsl_guest_capabilities {
	struct wsl_header hdr;
	bool seccomp_available;
};

struct wsl_early_config {
	struct wsl_header hdr;
	uint32_t swap_lun;
	uint32_t system_distro_device_type;
	uint32_t system_distro_device_id;
	int32_t page_reporting_order;
	uint32_t memory_reclaim_mode;
	uint32_t dns_tunneling_ip;
	bool enable_debug_shell;
	bool enable_dns_tunneling;
	bool enable_safe_mode;
	bool default_kernel;
	uint32_t kernel_modules_device_id;
	uint32_t hostname_offset;
	uint32_t kernel_modules_list_offset;
};

struct wsl_initial_config {
	struct wsl_header hdr;
	int32_t entropy_size;
	uint32_t entropy_offset;
	bool enable_gui_apps;
	bool mount_gpu_shares;
	bool enable_inbox_gpu_libs;
	uint32_t networking_mode;
	uint32_t port_tracker_type;
	uint16_t ephemeral_start;
	uint16_t ephemeral_end;
	bool enable_dhcp_client;
	bool disable_ipv6;
	int32_t dhcp_timeout;
};

struct wsl_launch_init {
	struct wsl_header hdr;
	uint32_t mount_device_type;
	uint32_t device_id;
	uint32_t fs_type_offset;
	uint32_t mount_options_offset;
	uint32_t vm_id_offset;
	uint32_t distribution_name_offset;
	uint32_t shared_memory_root_offset;
	uint32_t install_path_offset;
	uint32_t user_profile_offset;
	uint32_t flags;
	uint32_t connect_port;
};

struct wsl_create_instance_result {
	struct wsl_header hdr;
	int32_t result;
	uint32_t failure_step;
	uint64_t pid;
	uint32_t connect_port;
	uint32_t warnings_offset;
};

struct wsl_initialize_response {
	struct wsl_header hdr;
	uint32_t plan9_port;
	uint32_t default_uid;
	uint32_t interop_port;
	bool systemd_enabled;
	uint64_t pid_namespace;
	uint32_t flavor_index;
	uint32_t version_index;
	char buffer[];
};

struct wsl_create_session_response {
	struct wsl_header hdr;
	uint32_t port;
};

struct wsl_create_process {
	struct wsl_header hdr;
	uint16_t rows;
	uint16_t columns;
	uint32_t filename_offset;
	uint32_t cwd_offset;
	uint32_t command_line_offset;
	uint16_t command_line_count;
	uint32_t environment_offset;
	uint16_t environment_count;
	uint32_t nt_environment_offset;
	uint16_t nt_environment_count;
	uint32_t nt_path_offset;
	uint32_t shell_options;
	uint32_t username_offset;
	uint32_t default_uid;
	int32_t flags;
};

// Run a program in the root namespace, stdin/stdout on one socket (e.g. the telemetry agent)
struct wsl_root_create_process {
	struct wsl_header hdr;
	uint32_t path_index;
	uint32_t command_line_index;
};

struct wsl_result_int32 {
	struct wsl_header hdr;
	int32_t result;
};

// Networking-channel (GNS) messages carry JSON. Notifications prefix it with the adapter GUID
struct wsl_gns_result {
	struct wsl_header hdr;
	int32_t result;
};

struct wsl_result_uint32 {
	struct wsl_header hdr;
	uint32_t result;
};

struct wsl_result_bool {
	struct wsl_header hdr;
	bool result;
};

struct wsl_exit_status {
	struct wsl_header hdr;
	int32_t exit_code;
};

struct wsl_window_size {
	struct wsl_header hdr;
	uint16_t rows;
	uint16_t columns;
};

// Layouts must match the service's (MSVC x64) view of the structs
_Static_assert(offsetof(struct wsl_early_config, enable_dns_tunneling) == 41, "early config");
_Static_assert(offsetof(struct wsl_early_config, hostname_offset) == 48, "early config");
_Static_assert(offsetof(struct wsl_initial_config, dhcp_timeout) == 44, "initial config");
_Static_assert(offsetof(struct wsl_launch_init, connect_port) == 56, "launch init");
_Static_assert(sizeof(struct wsl_create_instance_result) == 40, "instance result");
_Static_assert(offsetof(struct wsl_initialize_response, buffer) == 48, "initialize response");
_Static_assert(offsetof(struct wsl_create_process, flags) == 68, "create process");
_Static_assert(sizeof(struct wsl_result_bool) == 20, "result bool");

#endif
