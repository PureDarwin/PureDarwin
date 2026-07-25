#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define fourcc_code(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define FORMAT_RGB565 fourcc_code('R', 'G', '1', '6') /* [15:0] R:G:B 5:6:5 little endian */
#define FORMAT_BGR565 fourcc_code('B', 'G', '1', '6') /* [15:0] B:G:R 5:6:5 little endian */

#define FORMAT_XRGB8888 fourcc_code('X', 'R', '2', '4') /* [31:0] x:R:G:B 8:8:8:8 little endian */
#define FORMAT_XBGR8888 fourcc_code('X', 'B', '2', '4') /* [31:0] x:B:G:R 8:8:8:8 little endian */
#define FORMAT_RGBX8888 fourcc_code('R', 'X', '2', '4') /* [31:0] R:G:B:x 8:8:8:8 little endian */
#define FORMAT_BGRX8888 fourcc_code('B', 'X', '2', '4') /* [31:0] B:G:R:x 8:8:8:8 little endian */

#ifdef __cplusplus
}
#endif

