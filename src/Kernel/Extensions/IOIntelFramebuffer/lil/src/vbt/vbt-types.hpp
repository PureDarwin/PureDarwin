#pragma once

#include "src/pd_kext_prims.hpp"
#include <lil/vbt-types.h>
#include <stdint.h>

constexpr lil::string_view VBT_HEADER_SIGNATURE = {"$VBT", 4};
constexpr lil::string_view BDB_HEADER_SIGNATURE = {"BIOS_DATA_BLOCK ", 16};

struct bdb_header {
	uint8_t signature[16];
	uint16_t version;
	uint16_t header_size;
	uint16_t bdb_size;
} __attribute__((packed));

struct bdb_block_header {
	uint8_t id;
	uint16_t size;
} __attribute__((packed));

enum bdb_block_id {
	BDB_GENERAL_FEATURES = 1,
	BDB_GENERAL_DEFINITIONS = 2,
	BDB_DRIVER_FEATURES = 12,
	BDB_OEM_MODE = 20,
	BDB_EDP = 27,
	BDB_LVDS_OPTIONS = 40,
	BDB_LVDS_BLC = 43,
	BDB_FIXED_MODE_SET = 51,
	BDB_MIPI_SEQUENCE = 53,
	BDB_PRD_BOOT_TABLE = 253,
};

/* block 1 */
struct bdb_general_features : bdb_block_header {
	static constexpr bdb_block_id blockId = BDB_GENERAL_FEATURES;

	uint8_t panel_fitting: 2;
	uint8_t flexaim: 1;
	uint8_t msg_enable: 1;
	uint8_t clear_screen: 3;
	uint8_t color_flip: 1;

	uint8_t download_ext_vbt: 1;
	uint8_t enable_ssc: 1;
	uint8_t ssc_freq: 1;
	uint8_t enable_lfp_on_override: 1;
	uint8_t disable_ssc_ddt: 1;
	uint8_t underscan_vga_timings: 1;
	uint8_t display_clock_mode: 1;
	uint8_t vbios_hotplug_support: 1;

	uint8_t disable_smooth_vision: 1;
	uint8_t single_dvi: 1;
	uint8_t rotate_180: 1;
	uint8_t fdi_rx_polarity_inverted: 1;
	uint8_t vbios_extended_mode: 1;
	uint8_t copy_ilfp_dtd_to_sdvo_lvds_dtd: 1;
	uint8_t panel_best_fit_timing: 1;
	uint8_t ignore_strap_state: 1;

	uint8_t legacy_monitor_detect;

	uint8_t int_crt_support: 1;
	uint8_t int_tv_support: 1;
	uint8_t int_efp_support: 1;
	uint8_t dp_ssc_enable: 1;
	uint8_t dp_ssc_freq: 1;
	uint8_t dp_ssc_dongle_supported: 1;
	uint8_t _reserved0: 2;

	uint8_t tc_hpd_retry_timeout: 7;
	uint8_t _reserved1: 1;

	uint8_t afc_startup_config: 2;
	uint8_t _reserved2: 6;
} __attribute__((packed));

/* block 2 */
struct child_device {
	uint16_t handle;
#define DEVICE_HANDLE_CRT 0x0001
#define DEVICE_HANDLE_EFP1 0x0004
#define DEVICE_HANDLE_EFP2 0x0040
#define DEVICE_HANDLE_EFP3 0x0020
#define DEVICE_HANDLE_LFP1 0x0008
#define DEVICE_HANDLE_LFP2 0x0080

	uint16_t device_type;
#define DEVICE_TYPE_HDMI 0x60D2
#define DEVICE_TYPE_DP_DUAL_MODE 0x60D6
#define DEVICE_TYPE_DP 0x68C6

#define DEVICE_TYPE_CLASS_EXTENSION 15
#define DEVICE_TYPE_POWER_MANAGEMENT 14
#define DEVICE_TYPE_HOTPLUG_SIGNALING 13
#define DEVICE_TYPE_INTERNAL_CONNECTOR 12
#define DEVICE_TYPE_NOT_HDMI_OUTPUT 11
#define DEVICE_TYPE_MIPI_OUTPUT 10
#define DEVICE_TYPE_COMPOSITE_OUTPUT 9
#define DEVICE_TYPE_DUAL_CHANNEL 8
#define DEVICE_TYPE_CONTENT_PROTECTION 7
#define DEVICE_TYPE_HIGH_SPEED_LINK 6
#define DEVICE_TYPE_LVDS_SIGNALING 5
#define DEVICE_TYPE_TMDS_DVI_SIGNALING 4
#define DEVICE_TYPE_VIDEO_SIGNALING 3
#define DEVICE_TYPE_DISPLAYPORT_OUTPUT 2
#define DEVICE_TYPE_DIGITAL_OUTPUT 1
#define DEVICE_TYPE_ANALOG_OUTPUT 0

	union {
		uint8_t device_id[10];

		struct {
			uint8_t i2c_speed;
			uint8_t onboard_redriver;
			uint8_t ondock_redriver;
			uint8_t hdmi_level_shifter_value: 5;
			uint8_t hdmi_max_data_rate: 3;

			uint16_t dtd_buf_ptr;

			uint8_t edidless_efp: 1;
			uint8_t compression_enable: 1;
			uint8_t compression_method_cps: 1;
			uint8_t ganged_edp: 1;
			uint8_t lttpr_non_transparent: 1;
			uint8_t disable_compression_for_ext_disp: 1;
			uint8_t _reserved0: 2;

			uint8_t compression_structure_index: 4;
			uint8_t _reserved1: 4;

			uint8_t hdmi_max_frl_rate: 4;
			uint8_t hdmi_max_frl_rate_valid: 1;
			uint8_t _reserved2: 3;

			uint8_t _reserved3;
		} __attribute__((packed));
	} __attribute__((packed));

	uint16_t addin_offset;
	uint8_t dvo_port;
	uint8_t i2c_pin;
	uint8_t slave_addr;
	uint8_t ddc_pin;
	uint16_t edid_ptr;
	uint8_t dvo_cfg;

	union {
		struct {
			uint8_t dvo2_port;
			uint8_t i2c2_pin;
			uint8_t slave2_addr;
			uint8_t ddc2_pin;
		} __attribute__((packed));

		struct {
			uint8_t efp_routed: 1;
			uint8_t lane_reversal: 1;
			uint8_t lspcon: 1;
			uint8_t iboost: 1;
			uint8_t hpd_invert: 1;
			uint8_t use_vbt_vswing: 1;
			uint8_t dp_max_lane_count: 2;
			uint8_t hdmi_support: 1;
			uint8_t dp_support: 1;
			uint8_t tmds_support: 1;
			uint8_t support_reserved: 5;
			uint8_t aux_channel;
			uint8_t dongle_detect;
		} __attribute__((packed));
	} __attribute__((packed));

	uint8_t pipe_cap: 2;
	uint8_t sdvo_stall: 1;
	uint8_t hpd_status: 2;
	uint8_t integrated_encoder: 1;
	uint8_t capabilities_reserved: 2;
	uint8_t dvo_wiring;

	union {
		uint8_t dvo2_wiring;
		uint8_t mipi_bridge_type;
	} __attribute__((packed));

	uint16_t extended_type;
	uint8_t dvo_function;
	uint8_t dp_usb_type_c: 1;
	uint8_t tbt: 1;
	uint8_t flags2_reserved: 2;
	uint8_t dp_port_trace_length: 4;
	uint8_t dp_gpio_index;
	uint16_t dp_gpio_pin_num;
	uint8_t dp_iboost_level: 4;
	uint8_t hdmi_iboost_level: 4;
	uint8_t dp_max_link_rate: 3;
	uint8_t dp_max_link_rate_reserved: 5;
} __attribute__((packed));

struct bdb_general_definitions : bdb_block_header {
	static constexpr bdb_block_id blockId = BDB_GENERAL_DEFINITIONS;

	uint8_t crt_ddc_gmbus_pin;
	uint8_t dpms_non_acpi: 1;
	uint8_t skip_boot_crt_detect: 1;
	uint8_t dpms_aim: 1;
	uint8_t _reserved: 5;

	uint8_t boot_display[2];
	uint8_t child_dev_size;

	struct child_device child_dev[0];

	size_t child_devices() const {
		return (this->size - sizeof(*this) + sizeof(struct bdb_block_header)) / this->child_dev_size;
	}

	struct const_iterator {
		using iterator_category = lil::forward_iterator_tag;
		using iterator_concept = lil::forward_iterator_tag;
		using difference_type = ptrdiff_t;
		using value_type = struct child_device;
		using pointer = const struct child_device *;
		using reference = const struct child_device &;

		const_iterator()
		: ptr_{nullptr}, size_{0} {}

		const_iterator(const uint8_t *ptr, uint8_t size)
		: ptr_{ptr}, size_{size} {}

		reference operator*() const { return *reinterpret_cast<pointer>(ptr_); }
		pointer operator->() const { return reinterpret_cast<pointer>(ptr_); }

		const_iterator& operator++() { ptr_ += size_; return *this; }
		const_iterator operator++(int) { const_iterator tmp = *this; ptr_ += size_; return tmp; }

		friend bool operator== (const const_iterator& a, const const_iterator& b) { return a.ptr_ == b.ptr_; }
		friend bool operator!= (const const_iterator& a, const const_iterator& b) { return a.ptr_ != b.ptr_; }

	private:
		const uint8_t *ptr_;
		uint8_t size_;
	};

	const_iterator begin() const {
		return const_iterator{reinterpret_cast<const uint8_t *>(child_dev), child_dev_size};
	}

	const_iterator end() const {
		return const_iterator{reinterpret_cast<const uint8_t *>(child_dev) + (child_devices() * this->child_dev_size), child_dev_size};
	}
} __attribute__((packed));

/* block 12 */
struct bdb_driver_features : bdb_block_header {
	static constexpr bdb_block_id blockId = BDB_DRIVER_FEATURES;

	uint8_t _bits0;
	uint16_t boot_mode_x;
	uint16_t boot_mode_y;
	uint8_t boot_mode_bpp;
	uint8_t boot_mode_refresh;

	uint16_t enable_lfp_primary: 1;
	uint16_t selective_mode_pruning: 1;
	uint16_t dual_frequency: 1;
	uint16_t render_clock_freq: 1;
	uint16_t nt_clone_support: 1;
	uint16_t power_scheme_ui: 1;
	uint16_t sprite_display_assign: 1;
	uint16_t cui_aspect_scaling: 1;
	uint16_t preserve_aspect_ratio: 1;
	uint16_t sdvo_device_power_down: 1;
	uint16_t crt_hotplug: 1;
#define LVDS_CONFIG_NONE 0
#define LVDS_CONFIG_INTEGRATED 1
#define LVDS_CONFIG_SDVO 2
#define LVDS_CONFIG_EDP 3
	uint16_t lvds_config: 2;
	uint16_t tv_hotplug: 1;
	uint16_t hdmi_config: 2;

	uint8_t _bits1;

	uint16_t legacy_crt_max_x;
	uint16_t legacy_crt_max_y;
	uint8_t legacy_crt_max_refresh;

	uint8_t _bits2;

	/* 155+ */
	uint8_t custom_vbt_version;

	uint16_t _bits3;
} __attribute__((packed));

/* block 20 */
struct oem_mode_entry {
	uint8_t mode_flags;
	uint8_t display_flags;
	uint16_t x_res;
	uint16_t y_res;
	uint8_t bpp;
	uint8_t refresh;
	uint8_t dtd[18];
} __attribute__((packed));

struct bdb_oem_mode : bdb_block_header {
	static constexpr bdb_block_id blockId = BDB_OEM_MODE;

	uint8_t num;
	uint8_t entry_size;
	struct oem_mode_entry entries[6];
} __attribute__((packed));

/* block 27 */
struct edp_power_seq {
	uint16_t t3;
	uint16_t t8;
	uint16_t t9;
	uint16_t t10;
	uint16_t t12;
} __attribute__ ((packed));

struct edp_fast_link_params {
	uint8_t rate: 4;
	uint8_t lanes: 4;
	uint8_t preemphasis: 4;
	uint8_t vswing: 4;
} __attribute__((packed));

struct edp_pwm_delays {
	uint16_t pwm_on_to_backlight_enable;
	uint16_t backlight_disable_to_pwm_off;
} __attribute__((packed));

struct edp_full_link_params {
	uint8_t preemphasis:4;
	uint8_t vswing:4;
} __attribute__((packed));

struct edp_apical_params {
	uint32_t panel_oui;
	uint32_t dpcd_base_address;
	uint32_t dpcd_idridix_control_0;
	uint32_t dpcd_option_select;
	uint32_t dpcd_backlight;
	uint32_t ambient_light;
	uint32_t backlight_scale;
} __attribute__((packed));

struct bdb_edp : bdb_block_header {
	static constexpr bdb_block_id blockId = BDB_EDP;

	struct edp_power_seq power_seqs[16];
	uint32_t color_depth;
	struct edp_fast_link_params fast_link_params[16];
	uint32_t sdrrs_msa_timing_delay;

	uint16_t edp_s3d_feature;
	uint16_t edp_t3_optimization;
	uint64_t edp_vswing_preemph;
	uint16_t fast_link_training;
	uint16_t dpcd_600h_write_required;
	struct edp_pwm_delays pwm_delays[16];
	uint16_t full_link_params_provided;
	struct edp_full_link_params full_link_params[16];
	uint16_t apical_enable;
	struct edp_apical_params apical_params[16];
	uint16_t edp_fast_link_training_rate[16];
	uint16_t edp_max_port_link_rate[16];
} __attribute__((packed));

/* block 40 */
struct bdb_lvds_options : bdb_block_header {
	static constexpr bdb_block_id blockId = BDB_LVDS_OPTIONS;

	uint8_t panel_type;
	uint8_t panel_type2;

	uint8_t pfit_mode: 2;
	uint8_t pfit_text_mode_enhanced: 1;
	uint8_t pfit_gfx_mode_enhanced: 1;
	uint8_t pfit_ratio_auto: 1;
	uint8_t pixel_dither: 1;
	uint8_t lvds_edid: 1;
	uint8_t rsvd2: 1;
	uint8_t rsvd4;

	uint32_t lvds_panel_channel_bits;

	uint16_t ssc_bits;
	uint16_t ssc_freq;
	uint16_t ssc_ddt;

	uint16_t panel_color_depth;
	uint32_t dps_panel_type_bits;
	uint32_t blt_control_type_bits;

	uint16_t lcdvcc_s0_enable;
	uint32_t rotation;
	uint32_t position;
} __attribute__((packed));

/* block 43 */
struct lfp_backlight_data_entry {
	uint8_t type: 2;
	uint8_t active_low_pwm: 1;
	uint8_t _reserved0: 5;

	uint16_t pwm_freq_hz;
	uint8_t min_brightness;
	uint8_t _reserved1;
	uint8_t _reserved2;
} __attribute__((packed));

struct lfp_backlight_control_method {
	uint8_t type: 4;
	uint8_t controller: 4;
} __attribute__((packed));

struct bdb_lfp_blc : bdb_block_header {
	static constexpr bdb_block_id blockId = BDB_LVDS_BLC;

	uint8_t size;
	struct lfp_backlight_data_entry panel[16];
	uint8_t level[16];
	struct lfp_backlight_control_method backlight_control[16];
} __attribute__((packed));

/* block 51 */
struct bdb_fixed_mode_set : bdb_block_header {
	static constexpr bdb_block_id blockId = BDB_FIXED_MODE_SET;

	uint8_t feature_enable;
	uint32_t x_res;
	uint32_t y_res;
} __attribute__((packed));

/* block 254 */
struct prd_entry {
	uint8_t attach_bits;
	uint8_t pipe_a;
	uint8_t pipe_b;
} __attribute__((packed));

struct bdb_prd_boot_table : bdb_block_header {
	static constexpr bdb_block_id blockId = BDB_PRD_BOOT_TABLE;

	struct prd_entry entries[16];
	uint16_t num;
} __attribute__((packed));
