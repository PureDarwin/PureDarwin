// pdfb: maps the console framebuffer (IOGOPFramebuffer) into the task and blits frames into it

#ifndef PDFB_H
#define PDFB_H

#include <stddef.h>
#include <stdint.h>

struct pdfb {
	uint8_t *base;
	uint64_t size;
	uint32_t width, height, stride, bpp;
	uint32_t service, connect;
	uint32_t *row;
	int clean;
};

int pdfb_open(struct pdfb *fb);
void pdfb_close(struct pdfb *fb);

// copies a w x h 32-bit image, scaled by an integer factor and centred, swap exchanges red and blue
void pdfb_blit(struct pdfb *fb, const uint8_t *src, uint32_t w, uint32_t h, size_t pitch, uint32_t scale,
    int swap);

#endif
