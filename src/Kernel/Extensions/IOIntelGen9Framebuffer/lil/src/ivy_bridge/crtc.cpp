#include <stdbool.h>

#include <lil/imports.h>
#include "src/ivy_bridge/crtc.hpp"

#define  BLC_PWM_CTL 0x48250
#define  BLM_PWM_ENABLE (1u << 31)
#define  SBLC_BLM_CTL1 0xC8250
#define  BLM_PCH_PWM_ENABLE (1u << 31)
#define  BLC_PWM_CPU_CTL 0x48254
#define  CUR_CTL_BASE 0x70080
#define  PRI_CTL_BASE 0x70180
#define  PRI_LINOFF_BASE 0x70184
#define  PRI_SURF_BASE 0x7019C
#define  plane_ctl_off 0x1000
#define  PIPE_CONF_BASE 0x70008
#define  VGA_CONTROL 0x41000
#define  PF_CTRL_BASE 0x68880
#define  PF_WIN_SZ_BASE 0x68074
#define  PF_WIN_POS_BASE 0x68070
#define  panel_fitter_reg_off 0x800
#define  FDI_TX_CTL_BASE 0x60100
#define  FDI_RX_CTL_BASE 0xF000C
#define  fdi_reg_off 0x1000
#define FDI_LINK_TRAIN_NONE (3 << 28)
#define FDI_LINK_TRAIN_PATTERN_1 (0 << 28)
#define FDI_LINK_TRAIN_PATTERN_2 (1 << 28)
#define FDI_LINK_TRAIN_PATTERN_IDLE (2 << 28)
#define  FDI_LINK_TRAIN_NONE_IVB (3 << 8)
#define  FDI_LINK_TRAIN_PATTERN_1_IVB  (0 << 8)
#define  FDI_LINK_TRAIN_PATTERN_2_IVB  (1 << 8)
#define  FDI_LINK_TRAIN_PATTERN_MASK_CPT (3 << 8)
#define  FDI_LINK_TRAIN_PATTERN_1_CPT (0 << 8)
#define  FDI_LINK_TRAIN_PATTERN_2_CPT (1 << 8)
#define  PIPECONF_BPC_MASK (0x7 << 5)
#define  PP_CONTROL 0xC7204
#define  PP_STATUS 0xC7200
#define  PP_ON 1 << 0
#define  PP_STATUS_ON 1 << 31
#define  LVDS_CTL 0xE1180
#define  LVDS_PORT_EN (1u << 31)
#define  TRANS_CONF_BASE 0xF0008
#define  trans_reg_off 0x1000
#define  TRANS_ENABLE (1u << 31)
#define  TRANS_STATE_ENABLE (1 << 30)
#define  TRANS_WORKAROUND_REG_BASE 0xf0064
#define  trans_workwaround_off 0x1000
#define  TRANS_WORKAROUND_TIMING_OVERRIDE (1u << 31)
#define  DPLL_SEL 0xC7000
#define  FDI_PCDCLK (1 << 4)
#define  FDI_TX_PLL_ENABLE (1 << 14)
#define  FDI_RX_PLL_ENABLE (1 << 13)
#define  HTOTAL_BASE 0x60000
#define  HBLANK_BASE 0x60004
#define  HSYNC_BASE 0x60008
#define  VTOTAL_BASE 0x60008
#define  VBLANK_BASE 0x60010
#define  VSYNC_BASE 0x60014
#define  SRCSZ_BASE 0x6001C
#define  M_1_VALUE_BASE 0x60030
#define  N_1_VALUE_BASE 0x60034
#define  LINK_M_1_VALUE_BASE 0x60040
#define  LINK_N_1_VALUE_BASE 0x60044
#define  PIPE_ENABLE (1u << 31)
#define  PLANE_ENABLE (1u << 31)
#define  FDI_RX_TU_SIZE_BASE 0xF0030
#define  FDI_RX_IMR_BASE 0xf0018
#define  FDI_RX_SYMBOL_LOCK (1 << 9)
#define  FDI_RX_BIT_LOCK (1 << 8)
#define  FDI_COMPOSITE_SYNC (1 << 11)
#define  FDI_LINK_TRAIN_VOL_EMP_MASK (0x3f << 22)
#define  FDI_DP_PORT_WIDTH_MASK (7 << 19)
#define  FDI_LINK_TRAIN_AUTO (1 << 10)
#define  FDI_LINK_TRAIN_400MV_0DB_SNB_B (0x0 << 22)
#define  FDI_LINK_TRAIN_400MV_6DB_SNB_B (0x3a << 22)
#define  FDI_LINK_TRAIN_600MV_3_5DB_SNB_B (0x39 << 22)
#define  FDI_LINK_TRAIN_800MV_0DB_SNB_B (0x38 << 22)
#define  FDI_ENABLE (1u << 31)
#define  FDI_RX_MISC 0xF0010
#define  FDI_RX_TP1_TO_TP2_48 (2 << 20)
#define  FDI_RX_FDI_DELAY_90 (0x90 << 0)
#define  FDI_RX_IIR 0xf0014
#define  DPLLA_CTL 0xC6014
#define  DPLLB_CTL 0xC6014
#define  PCH_HTOTAL_BASE 0xE0000
#define  PCH_HBLANK_BASE 0xE0004
#define  PCH_HSYNC_BASE 0xE0008
#define  PCH_VTOTAL_BASE 0xE000C
#define  PCH_VBLANK_BASE 0xE0010
#define  PCH_VSYNC_BASE 0xE0014
#define  PCH_M_1 0xE0030
#define  PCH_N_1 0xE0034
#define  PCH_LINK_M_1 0xE0040
#define  PCH_LINK_N_1 0xE0044
#define  FDI_TX_ENHANCE_FRAME_ENABLE (1 << 18)
#define  FDI_RX_ENHANCE_FRAME_ENABLE (1 << 6)
#define  FDI_LINK_TRAIN_NORMAL_CPT (3 << 8)
#define  GMBUS_SELECT 0xC5100
#define  GMBUS_COMMAND_STATUS 0xC5104
#define  GMBUS_STATUS 0xC5108
#define  GMBUS_DATA 0xC510C
#define  GMBUS_IRMASK 0xC5110
#define  GMBUS_2BYTEINDEX 0xC5120
#define    GMBUS_AKSV_SELECT =	(1 << 11)
#define    GMBUS_RATE_100KHZ	(0 << 8)
#define    GMBUS_RATE_50KHZ	(1 << 8)
#define    GMBUS_RATE_400KHZ	(2 << 8) /* reserved on Pineview */
#define    GMBUS_RATE_1MHZ	(3 << 8) /* reserved on Pineview */
#define    GMBUS_HOLD_EXT	(1 << 7) /* 300ns hold time, rsvd on Pineview */
#define    GMBUS_BYTE_CNT_OVERRIDE (1 << 6)
#define    GMBUS_SW_CLR_INT	(1u << 31)
#define    GMBUS_SW_RDY		(1 << 30)
#define    GMBUS_ENT		(1 << 29) /* enable timeout */
#define    GMBUS_CYCLE_NONE	(0 << 25)
#define    GMBUS_CYCLE_WAIT	(1 << 25)
#define    GMBUS_CYCLE_INDEX	(2 << 25)
#define    GMBUS_CYCLE_STOP	(4 << 25)
#define    GMBUS_BYTE_COUNT_SHIFT 16
#define    GMBUS_BYTE_COUNT_MAX   256U
#define    GEN9_GMBUS_BYTE_COUNT_MAX 511U
#define    GMBUS_SLAVE_INDEX_SHIFT 8
#define    GMBUS_SLAVE_ADDR_SHIFT 1
#define    GMBUS_SLAVE_READ	(1 << 0)
#define    GMBUS_SLAVE_WRITE	(0 << 0)
#define    GMBUS_INUSE		(1 << 15)
#define    GMBUS_HW_WAIT_PHASE	(1 << 14)
#define    GMBUS_STALL_TIMEOUT	(1 << 13)
#define    GMBUS_INT		(1 << 12)
#define    GMBUS_HW_RDY		(1 << 11)
#define    GMBUS_SATOER		(1 << 10)
#define    GMBUS_ACTIVE		(1 << 9)
#define    GMBUS_SLAVE_TIMEOUT_EN (1 << 4)
#define    GMBUS_NAK_EN		(1 << 3)
#define    GMBUS_IDLE_EN		(1 << 2)
#define    GMBUS_HW_WAIT_EN	(1 << 1)
#define    GMBUS_HW_RDY_EN	(1 << 0)
#define    GMBUS_2BYTE_INDEX_EN	(1u << 31)

typedef struct PllParams {
    int n, m1, m2, p1, p2;
} PllParams;

static int compute_m(PllParams params) {
    return 5 * (params.m1 + 2) + (params.m2 + 2);
}

static int compute_p(PllParams params) {
    return params.p1 * params.p2;
}

static int compute_vco(PllParams params, int refclock) {
    int m = compute_m(params);
    return (refclock * m + (params.n + 2) / 2) / (params.n + 2);
}

static int compute_dot(PllParams params, int refclock) {
    int p = compute_p(params);
    return (compute_vco(params, refclock) + p / 2) / p;
}

static int abs (int i) {
  return i < 0 ? -i : i;
}

static bool check_params(PllParams params, int refclock, PllLilLimits limits) {
    if(params.n < limits.n.min || params.n > limits.n.max)
        return false;
    if(params.m1 < limits.m1.min || params.m1 > limits.m1.max)
        return false;
    if(params.m2 < limits.m2.min || params.m2 > limits.m2.max)
        return false;
    if(params.p1 < limits.p1.min || params.p1 > limits.p1.max)
        return false;

    if(params.m1 <= params.m2)
        return false;

    int m = compute_m(params);
    int p = compute_p(params);

    if(m < limits.m.min || m > limits.m.max)
        return false;
    if(p < limits.p.min || p > limits.p.max)
        return false;

    int dot = compute_dot(params, refclock);
    int vco = compute_vco(params, refclock);

    if(dot < limits.dot.min || dot > limits.dot.max)
        return false;
    if(vco < limits.vco.min || dot > limits.vco.max)
        return false;
    return true;
}

/*
 * Returns a set of divisors for the desired target clock with the given
 * refclk, or FALSE.  The returned values represent the clock equation:
 * reflck * (5 * (m1 + 2) + (m2 + 2)) / (n + 2) / p1 / p2.
 *
 * Target and reference clocks are specified in kHz.
 */
static bool find_params(PllLilLimits* limits, LilModeInfo* mode, PllParams *params) {
    bool found = false;
    /* approximately equals target * 0.00585 */
    int target = mode->clock;
    int err_most = (target >> 8) + (target >> 9);
    int p2 = limits->p2.slow;
    uint32_t refclk = 120000;

    for(int n = limits->n.min; n <= limits->n.max; ++n) {
        for(int m1 = limits->m1.max; m1 >= limits->m1.min; --m1) {
            for(int m2 = limits->m2.max; m2 >= limits->m2.min; --m2) {
                for(int p1 = limits->p1.max; p1 >= limits->p1.min; --p1) {
                    //n, m1, m2, p1, p2;
                    PllParams clock = {n, m1, m2, p1, p2};
                    if(!check_params(clock, refclk, *limits)) {
                        continue;
                    }
                    int error = abs(compute_dot(clock, refclk) - target);
                    if(error <= err_most) {
                        *params = clock;
                        found = true;
                        goto end;
                    }
                }
            }
        }
    }
    end:
    return found;
 }

static void set_mask (volatile uint32_t* reg, bool set, uint32_t mask) {
    if (set) {
        *reg = *reg | mask;
    } else {
        *reg = *reg & ~mask;
    }
}

static void wait_mask (volatile uint32_t* reg, bool set, uint32_t mask) {
    if (set) {
        while(!(*reg & mask)) {}
    } else {
        while((*reg & mask)) {}
    }
}

void lil_ivb_shutdown (struct LilGpu* gpu, struct LilCrtc* crtc) {
    //disable the backlight
    volatile uint32_t* cpu_backlight_ctl = (uint32_t*)(gpu->mmio_start + BLC_PWM_CPU_CTL);
    *cpu_backlight_ctl = 0;

    //TODO: it might be necessary to disable interrupts for the gpu here.
    //or at least underrun reporting since there is apparently an hw bug
    //that raises them during modesetting.

    //3: Disable panel power through panel power sequencing (this does not seem to be done in linux so for now we don't do it)
    volatile uint32_t* pp_control = (uint32_t*)(gpu->mmio_start + PP_CONTROL);
    volatile uint32_t* pp_status = (uint32_t*)(gpu->mmio_start + PP_STATUS);
    set_mask(pp_control, 0, PP_ON);
    wait_mask(pp_status, 0, PP_STATUS_ON);

    //4: Disable CPU planes (VGA or hires)
    volatile uint32_t* primary_plane_control = (uint32_t*)(gpu->mmio_start + PRI_CTL_BASE + crtc->pipe_id * 0x1000);
    volatile uint32_t* cursor_plane_control = (uint32_t*)(gpu->mmio_start + CUR_CTL_BASE + crtc->pipe_id * 0x1000);
    set_mask(primary_plane_control, 0, (1u << 31));
    set_mask(cursor_plane_control, 0, (1u << 31));

    //5: disable cpu pipe
    volatile uint32_t* pipe_config = (uint32_t*)(gpu->mmio_start + PIPE_CONF_BASE + crtc->pipe_id * 0x1000);
    set_mask(pipe_config, 0, (1u << 31));
    //6: Wait for CPU pipe off status
    wait_mask(pipe_config, 0, (1 << 30));

    //disable vga
    volatile uint32_t* vga_control = (uint32_t*)(gpu->mmio_start + VGA_CONTROL);
    set_mask(vga_control, 1, (1u << 31));

    //7: Disable CPU panel fitter
    volatile uint32_t* pf_ctl = (uint32_t*)(gpu->mmio_start + PF_CTRL_BASE + 0x800 * crtc->pipe_id);
    volatile uint32_t* pf_winsz = (uint32_t*)(gpu->mmio_start + PF_WIN_SZ_BASE + 0x800 * crtc->pipe_id);
    volatile uint32_t* pf_winpos = (uint32_t*)(gpu->mmio_start + PF_WIN_POS_BASE + 0x800 * crtc->pipe_id);
    *pf_ctl = 0;
    *pf_winsz = 0;
    *pf_winpos = 0;

    volatile uint32_t* fdi_rx_ctl = (uint32_t*)(gpu->mmio_start + FDI_RX_CTL_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* fdi_tx_ctl = (uint32_t*)(gpu->mmio_start + FDI_TX_CTL_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* pch_trans_conf = (uint32_t*)(gpu->mmio_start + TRANS_CONF_BASE + 0x1000 * crtc->pipe_id);
    //register used for different kinds of workarounds, the singular bits are unknown
    volatile uint32_t* pch_trans_workaround = (uint32_t*)(gpu->mmio_start + TRANS_WORKAROUND_REG_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* dpll_sel = (uint32_t*)(gpu->mmio_start + DPLL_SEL);
    volatile uint32_t* dpll_a_ctl = (uint32_t*)(gpu->mmio_start + DPLLA_CTL);
    volatile uint32_t* dpll_b_ctl = (uint32_t*)(gpu->mmio_start + DPLLB_CTL);

    if (!crtc->connector->on_pch) {
        //8: If disabling DisplayPort on PCH, write the DisplayPort control register bit 31 to 0b.
        //9: If disabling CPU embedded DisplayPort A
        //TODO add displayport support
    } else {
        //10: If disabling any port on PCH:
        //a: disable cpu fdi transmitter and pch fdi receiver, also clear the auto training bits in the same write as the disable bits
        set_mask(fdi_tx_ctl, 0, (1u << 31) | (1 << 10));
        (void)*fdi_tx_ctl;
        *fdi_rx_ctl &= ~((1u << 31) | (1 << 10));
        (void)*fdi_rx_ctl;
        lil_sleep(1);
        //b: disable port
        crtc->connector->set_state(
                gpu, crtc->connector,
                crtc->connector->get_state(gpu, crtc->connector) & ~(1u << 31)
            );
        //c: disable pch transcoder
        *pch_trans_conf &= ~TRANS_ENABLE;
        //d: Wait for PCH transcoder off status (PCH transcoder config register transcoder state)
        wait_mask(pch_trans_conf, 0, TRANS_STATE_ENABLE);
        set_mask(pch_trans_workaround, 0, TRANS_WORKAROUND_TIMING_OVERRIDE);
        //TODO e: Disable Transcoder DisplayPort Control if DisplayPort was used
        //f: Disable Transcoder DPLL Enable bit in DPLL_SEL
        //TODO: shared dpll support here
        //since we only support lvds at this point disable all dplls and enable only 1 later
        set_mask(dpll_sel, 0, (1 << 11) | (1 << 8) | (1 << 7) | (1 << 4) | (1 << 3) | (1 << 0));
        set_mask(dpll_a_ctl, 0, (1u << 31));
        set_mask(dpll_b_ctl, 0, (1u << 31));

        //TODO this should only be done if disabling the last pch transcoder
        set_mask(fdi_rx_ctl, 0, FDI_PCDCLK);
        //clear the auto training bits in the same write as the disable bits
        set_mask(fdi_tx_ctl, 0, FDI_TX_PLL_ENABLE | (1 << 10));
        (void)*fdi_tx_ctl;
        lil_sleep(100);
        set_mask(fdi_rx_ctl, 0, FDI_RX_PLL_ENABLE | (1 << 10));
        (void)*fdi_rx_ctl;
        lil_sleep(100);
    }
}

//TODO check what "panel requirements" are
void lil_ivb_commit_modeset (struct LilGpu* gpu, struct LilCrtc* crtc) {
    lil_sleep(10);
    PllParams params = {};
    if (!find_params(&crtc->connector->limits, &crtc->current_mode, &params)) {
        //todo errors
    }

    //1: Enable panel power as needed to retrieve panel configuration
    //a: Enable panel power override using AUX VDD enable override bit
    volatile uint32_t* pp_control = (uint32_t*)(gpu->mmio_start + PP_CONTROL);
    volatile uint32_t* pp_status = (uint32_t*)(gpu->mmio_start + PP_STATUS);
    set_mask(pp_control, 1, (1 << 3));
    //b: Wait for delay given in panel requirements
    lil_sleep(100);

    //this step is reordered due to the way linux does it
    //6: Configure CPU pipe timings, M/N/TU, and other pipe settings
    LilModeInfo mode = crtc->current_mode;
    volatile uint32_t* htotal = (uint32_t*)(gpu->mmio_start + HTOTAL_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* hblank = (uint32_t*)(gpu->mmio_start + HBLANK_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* hsync = (uint32_t*)(gpu->mmio_start  + HSYNC_BASE  + 0x1000 * crtc->pipe_id);
    volatile uint32_t* vtotal = (uint32_t*)(gpu->mmio_start + VTOTAL_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* vblank = (uint32_t*)(gpu->mmio_start + VBLANK_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* vsync = (uint32_t*)(gpu->mmio_start  + VSYNC_BASE  + 0x1000 * crtc->pipe_id);
    volatile uint32_t* srcsz = (uint32_t*)(gpu->mmio_start  + SRCSZ_BASE  + 0x1000 * crtc->pipe_id);
    *htotal = ((mode.htotal - 1) << 16) | (mode.hactive - 1);
    *hblank = ((mode.htotal - 1) << 16) | (mode.hactive - 1);
    *hsync = ((mode.hsyncEnd - 1) << 16) | (mode.hsyncStart - 1);
    *vtotal = ((mode.vtotal - 1) << 16) | (mode.vactive - 1);
    *vblank = ((mode.vtotal - 1) << 16) | (mode.vactive - 1);
    *vsync = ((mode.vsyncEnd - 1) << 16) | (mode.vsyncStart - 1);

    volatile uint32_t* pipe_m_1      = (uint32_t*)(gpu->mmio_start  + M_1_VALUE_BASE       + 0x1000 * crtc->pipe_id);
    volatile uint32_t* pipe_n_1      = (uint32_t*)(gpu->mmio_start  + N_1_VALUE_BASE       + 0x1000 * crtc->pipe_id);
    volatile uint32_t* pipe_link_m_1 = (uint32_t*)(gpu->mmio_start  + LINK_M_1_VALUE_BASE  + 0x1000 * crtc->pipe_id);
    volatile uint32_t* pipe_link_n_1 = (uint32_t*)(gpu->mmio_start  + LINK_N_1_VALUE_BASE  + 0x1000 * crtc->pipe_id);
    *pipe_m_1 = (0b111111 << 25) | compute_m(params);
    *pipe_n_1 = params.n;

    //enable connector again (done here because of linux)
    crtc->connector->set_state(
        gpu, crtc->connector,
        crtc->connector->get_state(gpu, crtc->connector) | (1u << 31)
    );

    //c: Leave panel power override enabled until later step
    //2: Enable PCH clock reference source and PCH SSC modulator, wait for warmup
    ////(already enabled in theory)
    //4: If enabling port on PCH:
    //a: Enable PCH FDI Receiver PLL, wait for warmup plus DMI latency
    volatile uint32_t* fdi_rx_ctl = (uint32_t*)(gpu->mmio_start + FDI_RX_CTL_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* fdi_tx_ctl = (uint32_t*)(gpu->mmio_start + FDI_TX_CTL_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* pch_trans_conf = (uint32_t*)(gpu->mmio_start + TRANS_CONF_BASE + 0x1000 * crtc->pipe_id);
    //register used for different kinds of workarounds, the singular bits are unknown
    volatile uint32_t* pch_trans_workaround = (uint32_t*)(gpu->mmio_start + TRANS_WORKAROUND_REG_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* dpll_sel = (uint32_t*)(gpu->mmio_start + DPLL_SEL);
    volatile uint32_t* dpll_a_ctl = (uint32_t*)(gpu->mmio_start + DPLLA_CTL);
    volatile uint32_t* dpll_b_ctl = (uint32_t*)(gpu->mmio_start + DPLLB_CTL);
    uint32_t val = *fdi_rx_ctl;
    val &= ~(7 << 19) | (0x7 << 16);
    *fdi_rx_ctl = val | FDI_RX_PLL_ENABLE;
    (void)*fdi_rx_ctl;
    lil_sleep(10);
    //b: Switch from Rawclk to PCDclk in FDI Receiver
    set_mask(fdi_rx_ctl, 1, FDI_PCDCLK);
    (void)*fdi_rx_ctl;
    lil_sleep(10);
    //c: Enable CPU FDI Transmitter PLL, wait for warmup
    set_mask(fdi_tx_ctl, 1, FDI_TX_PLL_ENABLE);
    (void)*fdi_tx_ctl;
    lil_sleep(10);
    //7: Enable CPU pipe
    volatile uint32_t* pipe_config = (uint32_t*)(gpu->mmio_start + PIPE_CONF_BASE + crtc->pipe_id * 0x1000);
    set_mask(pipe_config, 1, PIPE_ENABLE | (1 << 14));
    (void)*pipe_config;
    //8: Configure and enable CPU planes TODO do this based on planes
    volatile uint32_t* pri_surf = (uint32_t*)(gpu->mmio_start + PRI_SURF_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* pri_linoff = (uint32_t*)(gpu->mmio_start + PRI_LINOFF_BASE + 0x1000 * crtc->pipe_id);

    if (crtc->planes[0].enabled) {
        volatile uint32_t* primary_plane_control = (uint32_t*)(gpu->mmio_start + PRI_CTL_BASE + crtc->pipe_id * 0x1000);
        val = *primary_plane_control;
        val |= PLANE_ENABLE;
        val &= (0b1111 << 26);
        val |= (0b0110 << 26);
        *primary_plane_control = val;

        *pri_linoff  = crtc->planes[0].surface_address & 0xfff;
        *pri_surf  = crtc->planes[0].surface_address & ~0xfff;
    }

    volatile uint32_t* fdi_rx_tu_size = (uint32_t*)(gpu->mmio_start + FDI_RX_TU_SIZE_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* fdi_rx_imr = (uint32_t*)(gpu->mmio_start + FDI_RX_IMR_BASE + 0x1000 * crtc->pipe_id);
    volatile uint32_t* fdi_rx_misc = (uint32_t*)(gpu->mmio_start + FDI_RX_MISC + 0x1000 * crtc->pipe_id);
    volatile uint32_t* fdi_rx_iir = (uint32_t*)(gpu->mmio_start + FDI_RX_IIR + 0x1000 * crtc->pipe_id);

    const int snb_b_fdi_train_param[4] = {
        FDI_LINK_TRAIN_400MV_0DB_SNB_B,
        FDI_LINK_TRAIN_400MV_6DB_SNB_B,
        FDI_LINK_TRAIN_600MV_3_5DB_SNB_B,
        FDI_LINK_TRAIN_800MV_0DB_SNB_B,
    };

    if (!crtc->connector->on_pch) {
    } else {
        int retry = 0;
        int i = 0;

        //9: If enabling port on PCH
        //a: Program PCH FDI Receiver TU size same as Transmitter TU size for TU error checking
        *fdi_rx_tu_size = *pipe_m_1 & (0x3f << 25);
        //b: if Auto Train FDI
        //i: Set pre-emphasis and voltage (iterate if training steps fail)
        //ii: Enable CPU FDI Transmitter and PCH FDI Receiver with auto training enabled
        //iii: Wait for FDI auto training time
        //iv: Read CPU FDI Transmitter register for auto train done
        val = *fdi_rx_imr;
        val &= ~FDI_RX_SYMBOL_LOCK;
        val &= ~FDI_RX_BIT_LOCK;
        *fdi_rx_imr = val;
        *fdi_rx_imr;
        (void)*fdi_rx_imr;

// TODO: enable SNB support
#if 0
        lil_sleep(150);

        uint32_t temp = *fdi_tx_ctl;
        temp &= ~FDI_DP_PORT_WIDTH_MASK;
        temp &= ~FDI_LINK_TRAIN_NONE;
        temp |= FDI_LINK_TRAIN_PATTERN_1;
        temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
        temp |= FDI_LINK_TRAIN_400MV_0DB_SNB_B;
        *fdi_tx_ctl = temp | FDI_ENABLE;

        *fdi_rx_misc = FDI_RX_TP1_TO_TP2_48 | FDI_RX_FDI_DELAY_90;

        temp = *fdi_rx_ctl;
        temp &= ~FDI_LINK_TRAIN_PATTERN_MASK_CPT;
        temp |= FDI_LINK_TRAIN_PATTERN_1_CPT;
        *fdi_rx_ctl = temp | FDI_ENABLE;

        *fdi_rx_ctl;
        (void)*fdi_rx_ctl;

        lil_sleep(150);

        for (i = 0; i < 4; i++) {
            temp = *fdi_tx_ctl;
            temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
            temp |= snb_b_fdi_train_param[i];
            *fdi_tx_ctl = temp;

            *fdi_tx_ctl;
            (void)*fdi_tx_ctl;

            lil_sleep(500);

            for (int retry = 0; retry < 5; retry++) {
                temp = *fdi_rx_iir;

                if (temp & FDI_RX_BIT_LOCK) {
                    *fdi_rx_iir = temp | FDI_RX_BIT_LOCK;
                    break;
                }
                lil_sleep(50);
            }

            if (retry < 5)
                break;
        }

        if (i == 4) {
            lil_panic("FDI train 1 failed!");
        }

        /* Train 2 */
        temp = *fdi_tx_ctl;
        temp &= ~FDI_LINK_TRAIN_NONE;
        temp |= FDI_LINK_TRAIN_PATTERN_2;
        temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
        temp |= FDI_LINK_TRAIN_400MV_0DB_SNB_B;
        *fdi_tx_ctl = temp;

        temp = *fdi_rx_ctl;
        temp &= ~FDI_LINK_TRAIN_PATTERN_MASK_CPT;
        temp |= FDI_LINK_TRAIN_PATTERN_2_CPT;
        *fdi_rx_ctl = temp;

        *fdi_rx_ctl;
        (void)*fdi_rx_ctl;

        lil_sleep(150);

        for (i = 0; i < 4; i++) {
            temp = *fdi_tx_ctl;
            temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
            temp |= snb_b_fdi_train_param[i];
            *fdi_tx_ctl = temp;

            *fdi_tx_ctl;
            (void)*fdi_tx_ctl;

            lil_sleep(500);

            for (retry = 0; retry < 5; retry++) {
                temp = *fdi_rx_iir;

                if (temp & FDI_RX_SYMBOL_LOCK) {
                    *fdi_rx_iir = temp | FDI_RX_SYMBOL_LOCK;
                    break;
                }
                lil_sleep(50);
            }

            if (retry < 5)
                break;
        }

        if (i == 4) {
            lil_panic("FDI train 2 failed!");
        }
#else
        lil_sleep(100);
        for(int i = 0; i < 8; i++) {
            /* disable first in case we need to retry */
            uint32_t temp = *fdi_tx_ctl;
            temp &= ~(FDI_LINK_TRAIN_AUTO | FDI_LINK_TRAIN_NONE_IVB);
            temp &= ~FDI_ENABLE;
            *fdi_tx_ctl = temp;

            temp = *fdi_rx_ctl;
            temp &= ~FDI_LINK_TRAIN_AUTO;
            temp &= ~FDI_LINK_TRAIN_PATTERN_MASK_CPT;
            temp &= ~FDI_ENABLE;
            *fdi_rx_ctl = temp;

            /* enable CPU FDI TX and PCH FDI RX */
            temp = *fdi_tx_ctl;
            temp &= ~FDI_DP_PORT_WIDTH_MASK;
            temp |= FDI_LINK_TRAIN_PATTERN_1_IVB;
            temp &= ~FDI_LINK_TRAIN_VOL_EMP_MASK;
            temp |= snb_b_fdi_train_param[i/2];
            temp |= FDI_COMPOSITE_SYNC;
            *fdi_tx_ctl = temp | FDI_ENABLE;

            *fdi_rx_misc = FDI_RX_TP1_TO_TP2_48 | FDI_RX_FDI_DELAY_90;

            temp = *fdi_rx_ctl;
            temp |= FDI_LINK_TRAIN_PATTERN_1_CPT;
            temp |= FDI_COMPOSITE_SYNC;
            *fdi_rx_ctl = temp | FDI_ENABLE;
            (void)fdi_rx_ctl;
            lil_sleep(1);

            for (int j = 0; j < 4; j++) {
                (void)*fdi_rx_iir;

                if (temp & FDI_RX_BIT_LOCK ||
                    (*fdi_rx_iir & FDI_RX_BIT_LOCK)) {
                    *fdi_rx_iir = temp | FDI_RX_BIT_LOCK;
                    break;
                }
                lil_sleep(1); /* should be 0.5us */
            }
            if (i == 4) {
                continue;
            }
            //train 2
            temp = *fdi_tx_ctl;
            temp &= ~FDI_LINK_TRAIN_NONE_IVB;
            temp |= FDI_LINK_TRAIN_PATTERN_2_IVB;
            *fdi_tx_ctl = temp;

            temp = *fdi_rx_ctl;
            temp &= ~FDI_LINK_TRAIN_PATTERN_MASK_CPT;
            temp |= FDI_LINK_TRAIN_PATTERN_2_CPT;
            *fdi_rx_ctl = temp;
            (void)*fdi_rx_ctl;

            lil_sleep(4); // should be 1.5us

            for (i = 0; i < 4; i++) {
                (void)*fdi_rx_iir;

                if (temp & FDI_RX_SYMBOL_LOCK ||
                    (*fdi_rx_iir & FDI_RX_SYMBOL_LOCK)) {
                    *fdi_rx_iir = temp | FDI_RX_SYMBOL_LOCK;
                    goto train_done;
                }
                lil_sleep(2); // should be 1.5us
            }
            if (i == 4) {
                //train failed
                lil_panic("link training failed");
            }
        }
        train_done:
	    //v: Enable PCH FDI Receiver Fill Start and Fill End Error Correction
	    set_mask(fdi_rx_ctl, 1,  (1 << 27) | (1 << 26));
#endif

	    //steps reordered because of linux
	    //d: Configure DPLL_SEL to set the DPLL to transcoder mapping and enable DPLL to the transcoder
        //TODO set DPLL_SEL based on the actual dpll chosen
        //in general figure out how dpll sharing works
        uint8_t pipe_to_sel_bit[] = {3, 7, 11};
	    set_mask(dpll_sel, 1, (1 << pipe_to_sel_bit[crtc->pipe_id]));
        set_mask(dpll_a_ctl, 1, (1u << 31));
        (void)dpll_a_ctl;
        lil_sleep(100);
	    //f: Configure PCH transcoder timings, M/N/TU, and other transcoder settings
        volatile uint32_t* pch_htotal = (uint32_t*)(gpu->mmio_start + PCH_HTOTAL_BASE + 0x1000 * crtc->pipe_id);
        volatile uint32_t* pch_hblank = (uint32_t*)(gpu->mmio_start + PCH_HBLANK_BASE + 0x1000 * crtc->pipe_id);
        volatile uint32_t* pch_hsync = (uint32_t*)(gpu->mmio_start  + PCH_HSYNC_BASE  + 0x1000 * crtc->pipe_id);
        volatile uint32_t* pch_vtotal = (uint32_t*)(gpu->mmio_start + PCH_VTOTAL_BASE + 0x1000 * crtc->pipe_id);
        volatile uint32_t* pch_vblank = (uint32_t*)(gpu->mmio_start + PCH_VBLANK_BASE + 0x1000 * crtc->pipe_id);
        volatile uint32_t* pch_vsync = (uint32_t*)(gpu->mmio_start  + PCH_VSYNC_BASE  + 0x1000 * crtc->pipe_id);
        *pch_htotal = ((mode.htotal - 1) << 16) | (mode.hactive - 1);
        *pch_hblank = ((mode.htotal - 1) << 16) | (mode.hactive - 1);
        *pch_hsync = ((mode.hsyncEnd - 1) << 16) | (mode.hsyncStart - 1);
        *pch_vtotal = ((mode.vtotal - 1) << 16) | (mode.vactive - 1);
        *pch_vblank = ((mode.vtotal - 1) << 16) | (mode.vactive - 1);
        *pch_vsync = ((mode.vsyncEnd - 1) << 16) | (mode.vsyncStart - 1);

        volatile uint32_t* pch_pipe_m_1      = (uint32_t*)(gpu->mmio_start  + PCH_M_1       + 0x1000 * crtc->pipe_id);
        volatile uint32_t* pch_pipe_n_1      = (uint32_t*)(gpu->mmio_start  + PCH_N_1       + 0x1000 * crtc->pipe_id);
        volatile uint32_t* pch_pipe_link_m_1 = (uint32_t*)(gpu->mmio_start  + PCH_LINK_M_1  + 0x1000 * crtc->pipe_id);
        volatile uint32_t* pch_pipe_link_n_1 = (uint32_t*)(gpu->mmio_start  + PCH_LINK_N_1  + 0x1000 * crtc->pipe_id);
        *pch_pipe_m_1 = (0b111111 << 25) | compute_m(params);
        *pch_pipe_n_1 = params.n;
        //the following procedure is not documented in the intel PRMs
        uint32_t temp = *fdi_tx_ctl;
        temp &= ~FDI_LINK_TRAIN_NONE_IVB;
        temp |= FDI_LINK_TRAIN_NONE_IVB | FDI_TX_ENHANCE_FRAME_ENABLE;
        *fdi_tx_ctl = temp;
        (void)*fdi_tx_ctl;
        temp = *fdi_rx_ctl;
        temp &= ~FDI_LINK_TRAIN_PATTERN_MASK_CPT;
        temp |= FDI_LINK_TRAIN_NORMAL_CPT;
        *fdi_rx_ctl = temp | FDI_RX_ENHANCE_FRAME_ENABLE | (1 << 27) | (1 << 26);
        *fdi_rx_ctl;
        lil_sleep(100);
	    //h: Enable PCH transcoder
        temp = *pch_trans_workaround;
        temp |= (1u << 31);
        temp &= ~(3 << 27);
        *pch_trans_workaround = temp;
        set_mask(pch_trans_conf, 1, (1u << 31));
        wait_mask(pch_trans_conf, 1, (1 << 30));
        (void)*pch_trans_conf;
        lil_sleep(100);
    }

    volatile uint32_t* pch_lvds = (uint32_t*)(gpu->mmio_start + 0xE1180);
    *pch_lvds |= (1 << 31);

    *pch_lvds;
    (void) pch_lvds;

    //11: Enable panel power through panel power sequencing
    set_mask(pp_control, 1, PP_ON);
    //12: Wait for panel power sequencing to reach enabled steady state
    wait_mask(pp_status, 1, PP_STATUS_ON);
    //15: Enable panel backlight
    //set_mask(backlight_pwm_data, 1, 0xffff);
    volatile uint32_t* cpu_backlight_ctl = (uint32_t*)(gpu->mmio_start + BLC_PWM_CPU_CTL);
    *cpu_backlight_ctl = 0xffff;
}
