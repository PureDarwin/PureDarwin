#pragma once

#include <stddef.h>
#include <lil/intel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Obtain a valid VBT header
 *
 * @param vbt Pointer to a VBT in memory
 * @param size The size of the VBT in bytes
 */
const struct vbt_header *vbt_get_header(const void *vbt, size_t size);

#ifdef __cplusplus
}
#endif
