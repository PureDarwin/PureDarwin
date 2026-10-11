#ifndef AIRJITD_EXEC_H
#define AIRJITD_EXEC_H

#include <stdint.h>

#include "bridge.h"

// compiles stage id from the metallib at mtl for the CPU path, 0 when ready
typedef int (*pd_stage_ready)(uint32_t id, const uint8_t *mtl, uint32_t size, const char *what);

// a job on the Vulkan executor when it is built in, turned on and takes the job, otherwise its stages
// are compiled and it runs on the CPU. pd_exec_on_vulkan says where the last job ran
int pd_exec_draw(struct PDDrawRequest *r, uint8_t *base, uint32_t size, uint32_t serial, pd_stage_ready ready);
int pd_exec_compute(struct PDComputeRequest *r, uint8_t *base, uint32_t size, pd_stage_ready ready);
int pd_exec_tile(struct PDTileRequest *r, uint8_t *base, uint32_t size, pd_stage_ready ready);

extern int pd_exec_on_vulkan;
extern int pd_exec_on_opengl;

#endif
