#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <lil/pch.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t GpuAddr;
struct LilGpu;

typedef enum LilError {
	LIL_SUCCESS,
	LIL_TIMEOUT,
	LIL_INVALID_PLL,
} LilError;

typedef enum LilConnectorType {
    HDMI,
    LVDS,
    DISPLAYPORT,
    EDP
} LilConnectorType;

typedef struct LilModeInfo {
	/* pixel clock in KHz */
	uint32_t clock;
	int vactive;
	int vsyncStart;
	int vsyncEnd;
	int vtotal;
	int hactive;
	int hsyncStart;
	int hsyncEnd;
	int htotal;
	int bpc;
	int bpp;
	int hsyncPolarity;
	int vsyncPolarity;
	uint16_t horizontalMm;
	uint16_t verticalMm;
} LilModeInfo;

/*
 * Note: intel gpus only have one valid (connector, encoder, crtc)
 */

enum LilDdiId {
	DDI_A,
	DDI_B,
	DDI_C,
	DDI_D,
	DDI_E,
};

typedef enum LilPlaneType {
    PRIMARY, CURSOR, SPRITE,
} LilPlaneType;

//generic plane
typedef struct LilPlane {
    LilPlaneType type;
    GpuAddr surface_address;
    bool enabled;
    uint32_t pipe_id;
	uint32_t pixel_format;

    bool (*update_surface) (struct LilGpu* gpu, struct LilPlane* plane, GpuAddr surface_address, GpuAddr line_stride);
    uint32_t *(*get_formats) (struct LilGpu* gpu, size_t *num);
} LilPlane;

struct LilConnector;

typedef enum LilTranscoder {
    TRANSCODER_A,
    TRANSCODER_B,
    TRANSCODER_C,
    TRANSCODER_EDP,
    INVALID_TRANSCODER,
} LilTranscoder;

typedef enum LilPllId {
	INVALID_PLL = 0,
	LCPLL1 = 1,
	LCPLL2 = 2,
	WRPLL1 = 3,
	WRPLL2 = 4,
} LilPllId;

typedef struct LilCrtc {
    LilModeInfo current_mode;
    struct LilConnector* connector;

    LilTranscoder transcoder;

    uint32_t pipe_id;

    uint32_t num_planes;
    LilPlane* planes;

    void (*shutdown) (struct LilGpu* gpu, struct LilCrtc* crtc);
    void (*commit_modeset) (struct LilGpu* gpu, struct LilCrtc* crtc);

	LilPllId pll_id;
} LilCrtc;

typedef struct LilEncoderEdp {
	uint8_t backlight_control_method_type;
	uint8_t backlight_inverter_type;
	uint8_t backlight_inverter_polarity;
	uint16_t pwm_inv_freq;
	uint32_t initial_brightness;
	uint8_t min_brightness;
	uint16_t pwm_on_to_backlight_enable;

	uint8_t edp_vswing_preemph;
	bool edp_iboost;
	bool edp_port_reversal;
	bool ssc_bits;
	bool edp_downspread;
	bool edp_fast_link_training;
	bool edp_fast_link_training_supported;
	uint8_t edp_fast_link_rate;
	uint8_t edp_fast_link_lanes;
	uint8_t edp_max_link_rate;
	uint8_t edp_max_lanes;
	uint8_t edp_lane_count;
	uint8_t edp_dpcd_rev;
	int edp_color_depth;
	uint8_t edp_balance_leg_val;

	bool edp_vbios_hotplug_support;
	bool edp_full_link_params_provided;

	uint32_t t1_t3;
	uint32_t t8;
	uint32_t t9;
	uint32_t t10;
	uint32_t t11_12;

	bool t3_optimization;
	bool dynamic_cdclk_supported;

	uint16_t supported_link_rates[8];
	size_t supported_link_rates_len;
} LilEncoderEdp;

typedef struct LilEncoderDp {
	bool vbios_hotplug_support;
	uint8_t ddc_pin;
	uint8_t aux_ch;
	uint8_t onboard_redriver_emph_vswing;
	uint8_t dp_max_link_rate;
	uint8_t dp_lane_count;
	bool support_tps3_pattern;
	bool support_enhanced_frame_caps;
} LilEncoderDp;

typedef struct LilEncoderHdmi {
	uint8_t ddc_pin;
	uint8_t aux_ch;
	uint8_t iboost_level;
	uint8_t hdmi_level_shift;
	bool iboost;
} LilEncoderHdmi;

typedef struct LilEncoder {
	union {
		LilEncoderEdp edp;
		LilEncoderDp dp;
		LilEncoderHdmi hdmi;
	};
} LilEncoder;

typedef struct LilConnectorInfo {
    uint32_t num_modes;
    LilModeInfo* modes;
} LilConnectorInfo;

typedef struct LilMinMax {
    int min, max;
} LilMinMax;

typedef struct LilLimit {
    int dot_limit;
    int slow, fast;
} LilLimit;

typedef struct PllLilLimits {
    LilMinMax dot, vco, n, m, m1, m2, p, p1;
    LilLimit p2;
} PllLilLimits;

enum LilAuxChannel {
	AUX_CH_A,
	AUX_CH_B,
	AUX_CH_C,
	AUX_CH_D,
	AUX_CH_E,
	AUX_CH_F,
	AUX_CH_G,
	AUX_CH_H,
	AUX_CH_I,
};

typedef struct LilConnector {
    bool (*is_connected) (struct LilGpu* gpu, struct LilConnector* connector);
    LilConnectorInfo (*get_connector_info) (struct LilGpu* gpu, struct LilConnector* connector);
    void (*set_state) (struct LilGpu* gpu, struct LilConnector* connector, uint32_t state);
    uint32_t (*get_state) (struct LilGpu* gpu, struct LilConnector* connector);

    uint32_t id;
    LilConnectorType type;

    PllLilLimits limits;

    LilCrtc* crtc;
	LilEncoder *encoder;

    bool on_pch;

    //this field is changed by lil_process_interrupt
    //indicates whether vertical sync/blank is happening for this connector.
    bool vsync;
    bool vblank;

	enum LilDdiId ddi_id;
	enum LilAuxChannel aux_ch;
} LilConnector;

typedef enum LilInterruptEnableMask {
    VBLANK_A, VSYNC_A,
    VBLANK_B, VSYNC_B,
    VBLANK_C, VSYNC_C
} LilInterruptEnableMask;

typedef struct LilGpu {
    uint32_t max_connectors;
    uint32_t num_connectors;
    LilConnector *connectors;

    uintptr_t gpio_start;
    uintptr_t mmio_start;
    uintptr_t vram;
    uintptr_t gtt_address;
    size_t gtt_size;
	size_t stolen_memory_pages;

    void (*enable_display_interrupt) (struct LilGpu* gpu, uint32_t enable_mask);
    void (*process_interrupt) (struct LilGpu* gpu);

    void (*vmem_clear) (struct LilGpu* gpu);
    void (*vmem_map) (struct LilGpu* gpu, uint64_t host, GpuAddr gpu_addr);

	void *dev;
} LilGpu;

/*
 * Fills out all the structures relating to the gpu
 *
 * The general modesetting procedure consists of the following:
 * - initialize the gpu
 * - set current_mode in the crtc you want to modeset
 * - allocate memory for the planes that are going to be used
 * - set the surface_address for the planes
 * - call commit_modeset
 */
bool lil_init_gpu(LilGpu **ret, void *pci_device, uint16_t pch_id);


#ifdef __cplusplus
}
#endif
