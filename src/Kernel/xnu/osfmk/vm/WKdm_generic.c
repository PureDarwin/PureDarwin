// portable c wkdm for architectures without a hand written codec, same layout as WKdm_new.h:
// a three word header of offsets, then tags, full words, 4-bit queue positions and 10-bit low bits

#include <vm/WKdm_new.h>
#include <kern/assert.h>
#include <kern/debug.h>
#include <stdint.h>
#include <string.h>

#define WK_PAGE_WORDS           (PAGE_SIZE / sizeof(WK_word))
#define WK_HEADER_WORDS         3
#define WK_TAGS_WORDS           (WK_PAGE_WORDS / 16)
#define WK_FULL_START           (WK_HEADER_WORDS + WK_TAGS_WORDS)
#define WK_DICT_SIZE            16
#define WK_LOW_BITS             10
#define WK_LOW_MASK             ((1u << WK_LOW_BITS) - 1)

#define WK_TAG_ZERO             0
#define WK_TAG_PARTIAL          1
#define WK_TAG_MISS             2
#define WK_TAG_EXACT            3

// the scratch page holds one tag byte, one queue position byte and one low bits halfword per word
_Static_assert(WK_PAGE_WORDS * 4 <= WKdm_SCRATCH_BUF_SIZE_INTERNAL, "wkdm scratch too small");

// the dictionary slot only depends on the high bits so partial matches land where their twin was
static inline unsigned int
wk_hash(WK_word w)
{
	return ((w >> WK_LOW_BITS) * 2654435761u) >> 28;
}

static inline void
wk_dict_init(WK_word *dict)
{
	for (unsigned int i = 0; i < WK_DICT_SIZE; i++) {
		dict[i] = 1;
	}
}

int
WKdm_compress_new(const WK_word *src_buf, WK_word *dest_buf, WK_word *scratch, unsigned int limit)
{
	uint8_t *tags = (uint8_t *)scratch;
	uint8_t *qpos = tags + WK_PAGE_WORDS;
	uint16_t *lows = (uint16_t *)(qpos + WK_PAGE_WORDS);
	unsigned int limit_words = limit / sizeof(WK_word);
	unsigned int nfull = 0, nq = 0, nl = 0;
	WK_word dict[WK_DICT_SIZE];

	if (limit_words <= WK_FULL_START) {
		return -1;
	}
	wk_dict_init(dict);

	WK_word *full = dest_buf + WK_FULL_START;
	for (unsigned int i = 0; i < WK_PAGE_WORDS; i++) {
		WK_word w = src_buf[i];
		if (w == 0) {
			tags[i] = WK_TAG_ZERO;
			continue;
		}
		unsigned int slot = wk_hash(w);
		WK_word d = dict[slot];
		if (d == w) {
			tags[i] = WK_TAG_EXACT;
			qpos[nq++] = (uint8_t)slot;
		} else if ((d >> WK_LOW_BITS) == (w >> WK_LOW_BITS)) {
			tags[i] = WK_TAG_PARTIAL;
			qpos[nq++] = (uint8_t)slot;
			lows[nl++] = (uint16_t)(w & WK_LOW_MASK);
			dict[slot] = w;
		} else {
			tags[i] = WK_TAG_MISS;
			if (WK_FULL_START + nfull >= limit_words) {
				return -1;
			}
			full[nfull++] = w;
			dict[slot] = w;
		}
	}

	unsigned int qpos_start = WK_FULL_START + nfull;
	unsigned int lows_start = qpos_start + (nq + 7) / 8;
	unsigned int lows_end = lows_start + (nl + 2) / 3;
	if (lows_end > limit_words) {
		return -1;
	}

	for (unsigned int t = 0; t < WK_TAGS_WORDS; t++) {
		WK_word packed = 0;
		for (unsigned int j = 0; j < 16; j++) {
			packed |= (WK_word)tags[t * 16 + j] << (2 * j);
		}
		dest_buf[WK_HEADER_WORDS + t] = packed;
	}
	for (unsigned int q = 0; q < nq; q += 8) {
		WK_word packed = 0;
		for (unsigned int j = 0; j < 8 && q + j < nq; j++) {
			packed |= (WK_word)qpos[q + j] << (4 * j);
		}
		dest_buf[qpos_start + q / 8] = packed;
	}
	for (unsigned int l = 0; l < nl; l += 3) {
		WK_word packed = 0;
		for (unsigned int j = 0; j < 3 && l + j < nl; j++) {
			packed |= (WK_word)lows[l + j] << (WK_LOW_BITS * j);
		}
		dest_buf[lows_start + l / 3] = packed;
	}

	dest_buf[0] = qpos_start;
	dest_buf[1] = lows_start;
	dest_buf[2] = lows_end;
	return (int)(lows_end * sizeof(WK_word));
}

void
WKdm_decompress_new(WK_word *src_buf, WK_word *dest_buf, __unused WK_word *scratch, unsigned int bytes)
{
	unsigned int qpos_start = src_buf[0];
	unsigned int lows_start = src_buf[1];
	unsigned int lows_end = src_buf[2];
	WK_word dict[WK_DICT_SIZE];

	if (qpos_start < WK_FULL_START || lows_start < qpos_start || lows_end < lows_start ||
	    lows_end * sizeof(WK_word) > bytes) {
		panic("WKdm(%p): corrupt header 0x%x 0x%x 0x%x size %u", src_buf,
		    qpos_start, lows_start, lows_end, bytes);
	}
	wk_dict_init(dict);

	const WK_word *full = src_buf + WK_FULL_START;
	const WK_word *full_end = src_buf + qpos_start;
	unsigned int nq = 0, nl = 0;
	unsigned int nq_max = (lows_start - qpos_start) * 8;
	unsigned int nl_max = (lows_end - lows_start) * 3;

	for (unsigned int i = 0; i < WK_PAGE_WORDS; i++) {
		unsigned int tag = (src_buf[WK_HEADER_WORDS + i / 16] >> (2 * (i % 16))) & 3;
		WK_word w;
		unsigned int slot;
		switch (tag) {
		case WK_TAG_ZERO:
			w = 0;
			break;
		case WK_TAG_MISS:
			if (full >= full_end) {
				panic("WKdm(%p): full words overrun", src_buf);
			}
			w = *full++;
			dict[wk_hash(w)] = w;
			break;
		default:
			if (nq >= nq_max) {
				panic("WKdm(%p): queue positions overrun", src_buf);
			}
			slot = (src_buf[qpos_start + nq / 8] >> (4 * (nq % 8))) & 0xf;
			nq++;
			w = dict[slot];
			if (tag == WK_TAG_PARTIAL) {
				if (nl >= nl_max) {
					panic("WKdm(%p): low bits overrun", src_buf);
				}
				w = (w & ~WK_LOW_MASK) |
				    ((src_buf[lows_start + nl / 3] >> (WK_LOW_BITS * (nl % 3))) & WK_LOW_MASK);
				nl++;
				dict[slot] = w;
			}
			break;
		}
		dest_buf[i] = w;
	}
}
