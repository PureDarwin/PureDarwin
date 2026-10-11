// pdscanout: the screen pixmap in a PDPowerVR scanout buffer the display engine reads directly. the
// server draws into it through a cached mapping and cleans what it drew, so nothing is copied

#ifndef PDSCANOUT_H
#define PDSCANOUT_H

#include <stdint.h>

struct pdscanout {
	uint32_t conn;
	uint32_t handle;
	uint8_t *base;
	uint64_t size;
	uint32_t width, height, pitch;
};

// 0 once the buffer is on screen, any failure leaves the caller on its shadow copy
int pdscanout_open(struct pdscanout *s, uint32_t width, uint32_t height, uint32_t pitch, char *why, int whylen);
// push the cache lines of a drawn rectangle out to memory, where the engine reads
void pdscanout_clean(struct pdscanout *s, int x1, int y1, int x2, int y2, int cpp);
void pdscanout_sync(void);
void pdscanout_close(struct pdscanout *s);

#endif
