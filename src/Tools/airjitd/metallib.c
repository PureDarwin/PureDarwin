#include "metallib.h"

#include <string.h>

#include "airjitd.h"

static const uint8_t wrapper_magic[4] = { 0xde, 0xc0, 0x17, 0x0b };
static const uint8_t raw_magic[4] = { 'B', 'C', 0xc0, 0xde };
static const uint8_t name_magic[4] = { 'N', 'A', 'M', 'E' };

static uint32_t rd32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static const uint8_t *find_bytes(const uint8_t *p, uint32_t n, const uint8_t *q, uint32_t m) {
  if (m > n)
    return 0;

  for (uint32_t i = 0; i <= n - m; i++) {
    if (!memcmp(p + i, q, m))
      return p + i;
  }

  return 0;
}

int pd_metallib_open(const uint8_t *mtl, uint32_t size, struct pd_metallib *lib) {
  const uint8_t *bc = find_bytes(mtl, size, wrapper_magic, 4);
  const uint8_t *namep = find_bytes(mtl, size, name_magic, 4);
  uint32_t nlen;

  // precompiled air64 libraries carry raw bitcode after the function list
  if (!bc)
    bc = find_bytes(mtl, size, raw_magic, 4);

  if (!bc || !namep || namep + 6 > mtl + size) {
    logmsg("AIRJITD: metallib: %s", !bc ? "no bitcode found" : "no NAME tag");
    return PD_ERR_NO_BITCODE;
  }

  nlen = namep[4] | (uint32_t)namep[5] << 8;

  if (!nlen || nlen >= sizeof(lib->entry) || namep + 6 + nlen > mtl + size)
    return PD_ERR_BAD_NAME;

  memcpy(lib->entry, namep + 6, nlen);
  lib->entry[nlen - 1] = 0;
  lib->bitcode_len = size - (uint32_t)(bc - mtl);

  // the wrapper header {magic, version, offset, size, cputype} bounds the stream
  if (!memcmp(bc, wrapper_magic, 4) && lib->bitcode_len >= 20) {
    uint32_t off = rd32(bc + 8), sz = rd32(bc + 12);
    if (off <= lib->bitcode_len && sz <= lib->bitcode_len - off) {
      bc += off;
      lib->bitcode_len = sz;
    }
  }

  lib->bitcode = bc;
  return 0;
}
