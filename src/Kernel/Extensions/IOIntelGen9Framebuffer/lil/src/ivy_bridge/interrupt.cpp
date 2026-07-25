#include "src/ivy_bridge/interrupt.hpp"

void lil_ivb_enable_display_interrupt (LilGpu* gpu, uint32_t enable_mask) {
    volatile uint32_t* de_isr = (uint32_t*)(gpu->mmio_start + 0x44000);
    volatile uint32_t* de_imr = (uint32_t*)(gpu->mmio_start + 0x44004);
    //iir is write 1 to clear
    volatile uint32_t* de_iir = (uint32_t*)(gpu->mmio_start + 0x44008);
    volatile uint32_t* de_ier = (uint32_t*)(gpu->mmio_start + 0x4400C);
    //global interrupt enable (this bit is only present in the IER register).
    *de_ier |= (1u << 31);
    if (enable_mask & VBLANK_A) {
        *de_iir |= 1;
        *de_imr &= ~(1);
        *de_ier |= (1);
    }

    if (enable_mask & VSYNC_A) {
        *de_iir |= 1 << 1;
        *de_imr &= ~(1 << 1);
        *de_ier |= (1 << 1);
    }

    if (enable_mask & VBLANK_B) {
        *de_iir |= 1 << 5;
        *de_imr &= ~(1 << 5);
        *de_ier |= (1 << 5);
    }

    if (enable_mask & VSYNC_B) {
        *de_iir |= 1 << 6;
        *de_imr &= ~(1 << 6);
        *de_ier |= (1 << 6);
    }

    if (enable_mask & VBLANK_C) {
        *de_iir |= 1 << 10;
        *de_imr &= ~(1 << 10);
        *de_ier |= (1 << 10);
    }

    if (enable_mask & VSYNC_C) {
        *de_iir |= 1 << 11;
        *de_imr &= ~(1 << 11);
        *de_ier |= (1 << 11);
    }
}

void lil_ivb_process_display_interrupt (struct LilGpu* gpu) {
    volatile uint32_t* de_iir = (uint32_t*)(gpu->mmio_start + 0x44008);
    gpu->connectors[0].vblank = *de_iir & (1);
    gpu->connectors[0].vsync = *de_iir & (1 << 1);
    /*TODO only one connector is supported at the moment
    gpu->connectors[1].vblank = *de_iir & (1 << 5);
    gpu->connectors[1].vsync = *de_iir & (1 << 6);
    gpu->connectors[2].vblank = *de_iir & (1 << 10);
    gpu->connectors[2].vsync = *de_iir & (1 << 11);
    */
    //write 1 to clear interrupt conditions
    *de_iir |= 0b11;
}
