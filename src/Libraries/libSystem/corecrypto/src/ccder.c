#include <corecrypto/ccder.h>
#include <stdio.h>
#include <stdlib.h>

// NOTE(@facekapow):
// the current implemention does absolutely **no** error checking
// TODO: implement various checks

size_t ccder_sizeof(ccder_tag tag, size_t len) {
	return ccder_sizeof_tag(tag) + ccder_sizeof_len(len) + len;
}

size_t ccder_sizeof_implicit_integer(ccder_tag implicit_tag,
                                     cc_size n, const cc_unit *s) {
	return ccder_sizeof_integer(n, s) - ccder_sizeof_tag(CCDER_INTEGER) + ccder_sizeof_tag(implicit_tag);
}

size_t ccder_sizeof_implicit_octet_string(ccder_tag implicit_tag,
                                          cc_size n, const cc_unit *s) {
	return ccder_sizeof_octet_string(n, s) - ccder_sizeof_tag(CCDER_OCTET_STRING) + ccder_sizeof_tag(implicit_tag);
}

size_t ccder_sizeof_implicit_raw_octet_string(ccder_tag implicit_tag,
                                              size_t s_size) {
	return ccder_sizeof_raw_octet_string(s_size) - ccder_sizeof_tag(CCDER_OCTET_STRING) + ccder_sizeof_tag(implicit_tag);
}

size_t ccder_sizeof_implicit_uint64(ccder_tag implicit_tag, uint64_t value) {
	return ccder_sizeof_uint64(value) - ccder_sizeof_tag(CCDER_INTEGER) + ccder_sizeof_tag(implicit_tag);
}

size_t ccder_sizeof_integer(cc_size n, const cc_unit *s) {
	// how convenient that prefect method for this already existed ;)
	size_t byteLen = ccn_write_int_size(n, s);
	return ccder_sizeof_tag(CCDER_INTEGER) + ccder_sizeof_len(byteLen) + byteLen;
}

size_t ccder_sizeof_len(size_t len) {
	if (len > 127) {
		// NOTE(@facekapow):
		// determine how many bytes are needed
		// see https://stackoverflow.com/a/2274499
		//
		// there's room for performance improvement here because we know
		// the absolute maximum size of `len` (since it's a `size_t`)
		// so i might add that in later

		size_t total = 0;
		do {
			len >>= 8;
			++total;
		} while (len > 0);
		return total + 1;
	} else {
		return 1;
	}
}

size_t ccder_sizeof_octet_string(cc_size n, const cc_unit *s) {
	// see `ccder_sizeof_integer`
	size_t byteLen = ccn_write_uint_size(n, s);
	return ccder_sizeof_tag(CCDER_OCTET_STRING) + ccder_sizeof_len(byteLen) + byteLen;
}

size_t ccder_sizeof_oid(ccoid_t oid) {
	return ccoid_size(oid);
}

size_t ccder_sizeof_raw_octet_string(size_t s_size) {
	return ccder_sizeof_tag(CCDER_OCTET_STRING) + ccder_sizeof_len(s_size) + s_size;
}

size_t ccder_sizeof_tag(ccder_tag tag) {
	tag &= CCDER_TAGNUM_MASK;
	if (tag > 30) {
		// see `ccder_sizeof_len`
		// we're doing the same thing here, but with
		// 7-bit groups instead of full 8-bit bytes

		size_t total = 0;
		do {
			tag >>= 7;
			++total;
		} while (tag > 0);
		return total;
	} else {
		return 1;
	}
}

size_t ccder_sizeof_uint64(uint64_t value) {
	size_t len = 0;

	// same approach as `ccder_sizeof_len`
	uint64_t tmp = value;
	do {
		// if the most significant bit of the first encoded byte is set,
		// add an empty leading byte (because we know `value` is unsigned)
		if (tmp >> 8 == 0 && tmp & 0x80)
			++len;

		tmp >>= 8;
		++len;
	} while (tmp > 0);

	return ccder_sizeof_tag(CCDER_INTEGER) + ccder_sizeof_len(len) + len;
}

// NOTE(@facekapow):
// ok, so i think i understand how Apple's slightly convoluted DER encoding/decoding API
// works. at first it seems totally stupid, but it makes a little sense in practice.
//
// not sure if i'm just stupid but there seems to be no (useful) documentation anywhere on *any* of the
// corecrypto APIs, so i'll document this API here.
//
// the API is designed so that you can nest calls to various `ccder_encode_*` functions
// and the order you see the calls in your code is the order of the generated structures.
// e.g.
//     ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE,
//       end_ptr, whole_der_ptr,
//       ccder_encode_uint64(some_random_value, whole_der_ptr,
//       ccder_encode_oid(some_random_oid, whole_der_ptr, end_ptr)))
// this would produce a SEQUENCE that contains an INTEGER of `some_random_value` and then an OBJECT IDENTIFIER
// of `some_random_oid`
//
// `der` is a constant pointer to the beginning of the *entire* DER structure. when the entire sequence of calls
// is completed, the pointer returned by the root call should be the same as `der`.
//
// `der_end` is one byte past the last byte of the entire DER structure

#define CCDER_TAG_MASK_SHIFT_BACK (sizeof(ccder_tag) * 8 - 3)

uint8_t *ccder_encode_tag(ccder_tag tag, const uint8_t *der, uint8_t *der_end) {
	size_t tag_len = ccder_sizeof_tag(tag);
	uint8_t* tag_start = der_end - tag_len;

	tag_start[0] = ((uint8_t)(tag >> CCDER_TAG_MASK_SHIFT_BACK)) << 5;

	if (tag_len > 1) {
		// long form
		// 0x1f == 0b00011111
		tag_start[0] |= 0x1f;
		ccder_tag rest = tag & CCDER_TAGNUM_MASK >> 5;
		for (size_t i = 0; i < tag_len - 1; ++i) {
			// 0x80 == 0b10000000
			// 0x7f == 0b01111111
			tag_start[i + 1] = (uint8_t)(i == tag_len - 1 ? 0 : 0x80) | (uint8_t)((rest & (0x7fU << (i * 7))) >> (i * 7));
		}
	} else {
		// short form
		tag_start[0] |= (uint8_t)(tag & 0x1f);
	}

	return tag_start;
}

uint8_t* ccder_encode_len(size_t len, const uint8_t* der, uint8_t* der_end) {
	// lol `len_len`
	size_t len_len = ccder_sizeof_len(len);
	uint8_t* len_start = der_end - len_len;

	// we trust that `ccder_sizeof_len` knows what it's doing,
	// so rather than perform the same checks it does again,
	// just check if the length of the length is greater than 1

	len_start[0] = (uint8_t)(len_len > 1 ? 1 : 0) << 7;

	if (len_len > 1) {
		--len_len;
		// long form
		len_start[0] |= (uint8_t)(len_len & 0x7f);
		++len_start;
		for (size_t i = len_len; i > 0; --i)
			len_start[len_len - i] = (uint8_t)((len >> ((i - 1) * 8)) & 0xff);
		--len_start;
	} else {
		// short form
		len_start[0] |= (uint8_t)(len & 0x7f);
	}

	return len_start;
}

uint8_t *
ccder_encode_tl(ccder_tag tag, size_t len, const uint8_t *der, uint8_t *der_end) {
	return ccder_encode_tag(tag, der, ccder_encode_len(len, der, der_end));
}

uint8_t *
ccder_encode_body_nocopy(size_t size, const uint8_t *der, uint8_t *der_end) {
	// NOTE(@facekapow):
	// this is a weird one, but if i'm not mistaken, i believe this is just supposed
	// to return a pointer to the beginning of the content, so assuming that
	// `der_end` is supposed to point to the end of the content, we just have
	// to subtract the pointers
	//
	// my hunch is further supported by the fact that the header labels this function
	// as a "pure" function
	return der_end - size;
}

uint8_t* ccder_encode_constructed_tl(ccder_tag tag, const uint8_t* body_end, const uint8_t* der, uint8_t* der_end) {
	return ccder_encode_tl(tag, (size_t)(body_end - der_end), der, der_end);
};

uint8_t *ccder_encode_oid(ccoid_t oid, const uint8_t *der, uint8_t *der_end) {
	size_t oid_total_len = ccder_sizeof_oid(oid);
	uint8_t* oid_entire_start = der_end - oid_total_len;
	memcpy(oid_entire_start, CCOID(oid), oid_total_len);
	return oid_entire_start;
}

uint8_t *ccder_encode_implicit_integer(ccder_tag implicit_tag,
                                       cc_size n, const cc_unit *s,
                                       const uint8_t *der, uint8_t *der_end) {
	size_t int_bytes = ccn_write_int_size(n, s);
	uint8_t* int_start = der_end - int_bytes;
	ccn_write_int(n, s, int_bytes, int_start);
	return ccder_encode_tl(implicit_tag, int_bytes, der, int_start);
}

uint8_t *ccder_encode_integer(cc_size n, const cc_unit *s,
                              const uint8_t *der, uint8_t *der_end) {
	size_t int_bytes = ccn_write_int_size(n, s);
	uint8_t* int_start = der_end - int_bytes;
	ccn_write_int(n, s, int_bytes, int_start);
	return ccder_encode_tl(CCDER_INTEGER, int_bytes, der, int_start);
}

uint8_t *ccder_encode_implicit_uint64(ccder_tag implicit_tag,
                                      uint64_t value,
                                      const uint8_t *der, uint8_t *der_end) {
	size_t int_bytes = 0;
	uint8_t* int_start = der_end - int_bytes;

	for (size_t i = 0; i < int_bytes; ++i) {
		int_start[int_bytes - 1 - i] = (uint8_t)((value & (0xff << (i * 8))) >> (i * 8));
	}

	return ccder_encode_tl(implicit_tag, int_bytes, der, int_start);
}

uint8_t *ccder_encode_uint64(uint64_t value,
                             const uint8_t *der, uint8_t *der_end) {
	size_t int_bytes = 0;
	uint8_t* int_start = der_end - int_bytes;

	for (size_t i = 0; i < int_bytes; ++i) {
		int_start[int_bytes - 1 - i] = (uint8_t)((value & (0xff << (i * 8))) >> (i * 8));
	}

	return ccder_encode_tl(CCDER_INTEGER, int_bytes, der, int_start);
}

// NOTE(@facekapow):
// i'm actually a bit unsure about how the non-raw variants of the octet string
// encoding functions are supposed to work, so i'm assuming that they're just
// supposed to serialize the CCNs much like the integer encoding functions

uint8_t *ccder_encode_implicit_octet_string(ccder_tag implicit_tag,
                                            cc_size n, const cc_unit *s,
                                            const uint8_t *der,
                                            uint8_t *der_end) {
	size_t str_bytes = ccn_write_uint_size(n, s);
	uint8_t* str_start = der_end - str_bytes;
	ccn_write_uint(n, s, str_bytes, str_start);
	return ccder_encode_tl(implicit_tag, str_bytes, der, str_start);
}

uint8_t *ccder_encode_octet_string(cc_size n, const cc_unit *s,
                                   const uint8_t *der, uint8_t *der_end) {
	size_t str_bytes = ccn_write_uint_size(n, s);
	uint8_t* str_start = der_end - str_bytes;
	ccn_write_uint(n, s, str_bytes, str_start);
	return ccder_encode_tl(CCDER_OCTET_STRING, str_bytes, der, str_start);
}

uint8_t *ccder_encode_implicit_raw_octet_string(ccder_tag implicit_tag,
                                                size_t s_size, const uint8_t *s,
                                                const uint8_t *der,
                                                uint8_t *der_end) {
	return ccder_encode_tl(implicit_tag, s_size, der, ccder_encode_body(s_size, s, der, der_end));
}

uint8_t *ccder_encode_raw_octet_string(size_t s_size, const uint8_t *s,
                                       const uint8_t *der, uint8_t *der_end) {
	return ccder_encode_tl(CCDER_OCTET_STRING, s_size, der, ccder_encode_body(s_size, s, der, der_end));
}

size_t ccder_encode_eckey_size(size_t priv_size, ccoid_t oid, size_t pub_size) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

uint8_t *ccder_encode_eckey(size_t priv_size, const uint8_t *priv_key,
                            ccoid_t oid,
                            size_t pub_size, const uint8_t *pub_key,
                            uint8_t *der, uint8_t *der_end) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}

uint8_t *
ccder_encode_body(size_t size, const uint8_t* body,
                  const uint8_t *der, uint8_t *der_end) {
	uint8_t* body_start = der_end - size;

	memcpy(body_start, body, size);

	return body_start;
}

const uint8_t *ccder_decode_tag(ccder_tag *tagp, const uint8_t *der, const uint8_t *der_end) {
	size_t tag_len = 1;
	*tagp = ((ccder_tag)(der[0] >> 5)) << CCDER_TAG_MASK_SHIFT_BACK;

	if ((der[0] & 0x1f) == 0x1f) {
		// long form
		// 0x80 == 0b10000000
		size_t i = 0;
		do {
			// 0x7f == 0b01111111
			*tagp |= (((ccder_tag)der[i + 1] & 0x7fU) << (i * 7));
			++i;
			++tag_len;
		} while (der[i + 1] & 0x80);
		*tagp <<= 5;
		*tagp |= 0x1f;
	} else {
		// short form
		*tagp |= der[0] & 0x1f;
	}

	return der + tag_len;
}

const uint8_t* ccder_decode_len(size_t* lenp, const uint8_t* der, const uint8_t* der_end) {
	size_t len_len = 1;

	if (der[0] & 0x80) {
		// long form
		len_len = der[0] & 0x7f;
		++der;
		for (size_t i = len_len; i > 0; --i)
			*lenp |= (size_t)der[len_len - i] << ((i - 1) * 8);
	} else {
		// short form
		*lenp = der[0] & 0x7f;
	}

	return der + len_len;
}

const uint8_t *ccder_decode_tl(ccder_tag expected_tag, size_t *lenp,
                               const uint8_t *der, const uint8_t *der_end) {
	ccder_tag tag_found = 0;
	return ccder_decode_len(lenp, ccder_decode_tag(&tag_found, der, der_end), der_end);
}

const uint8_t *
ccder_decode_constructed_tl(ccder_tag expected_tag, const uint8_t **body_end,
                            const uint8_t *der, const uint8_t *der_end) {
	size_t body_len = 0;
	const uint8_t* body_start = ccder_decode_tl(expected_tag, &body_len, der, der_end);
	*body_end = body_start + body_len;
	return body_start;
}

const uint8_t *
ccder_decode_sequence_tl(const uint8_t **body_end,
                         const uint8_t *der, const uint8_t *der_end) {
	// NOTE(@facekapow):
	// we don't just call `ccder_decode_constructed_tl` because in the future,
	// that method will check that the decoded tag is a CONSTRUCTED tag

	size_t body_len = 0;
	const uint8_t* body_start = ccder_decode_tl(CCDER_SEQUENCE, &body_len, der, der_end);
	*body_end = body_start + body_len;
	return body_start;
}

const uint8_t *ccder_decode_uint_n(cc_size *n, 
                                 const uint8_t *der, const uint8_t *der_end) {
	size_t num_len = 0;
	const uint8_t* num_start = ccder_decode_tl(CCDER_INTEGER, &num_len, der, der_end);
	while (num_start[0] == 0) {
		--num_len;
		++num_start;
	}
	*n = ccn_nof_size(num_len);
	return num_start + num_len;
}

const uint8_t *ccder_decode_uint(cc_size n, cc_unit *r,
                                 const uint8_t *der, const uint8_t *der_end) {
	size_t num_len = 0;
	const uint8_t* num_start = ccder_decode_tl(CCDER_INTEGER, &num_len, der, der_end);
	while (num_start[0] == 0) {
		--num_len;
		++num_start;
	}
	ccn_read_uint(n, r, num_len, num_start);
	return num_start + num_len;
}

const uint8_t *ccder_decode_uint64(uint64_t* r,
                                   const uint8_t *der, const uint8_t *der_end) {
	size_t num_len = 0;
	const uint8_t* num_start = ccder_decode_tl(CCDER_INTEGER, &num_len, der, der_end);
	while (num_start[0] == 0) {
		--num_len;
		++num_start;
	}

	*r = 0;

	for (size_t i = 0; i < num_len; ++i) {
		*r |= (uint64_t)num_start[num_len - 1 - i] << (i * 8);
	}

	return num_start + num_len;
}

const uint8_t *ccder_decode_seqii(cc_size n, cc_unit *r, cc_unit *s,
                                  const uint8_t *der, const uint8_t *der_end) {
	size_t seq_len = 0;
	return ccder_decode_uint(n, s, ccder_decode_uint(n, r, ccder_decode_tl(CCDER_SEQUENCE, &seq_len, der, der_end), der_end), der_end);
}

const uint8_t *ccder_decode_oid(ccoid_t *oidp,
                                const uint8_t *der, const uint8_t *der_end) {
	size_t oid_len = 0;
	const uint8_t* oid_start = ccder_decode_tl(CCDER_OBJECT_IDENTIFIER, &oid_len, der, der_end);
	size_t total_len = (size_t)(oid_start - der) + oid_len;

	CCOID(*oidp) = der;

	return oid_start + oid_len;
}

const uint8_t *ccder_decode_bitstring(const uint8_t **bit_string,
                                size_t *bit_length,
                                const uint8_t *der, const uint8_t *der_end) {
	size_t byte_len = 0;
	const uint8_t* bit_start = ccder_decode_tl(CCDER_BIT_STRING, &byte_len, der, der_end);

	// NOTE(@facekapow):
	// i'm not sure if Apple's API meant to include the *entire* bitstring including the leading
	// byte (which denotes the number of empty/padding bits at the end), but i'm going to assume
	// that it does. uncomment this if it's not supposed to include that byte
	//++bit_start;
	//--byte_len;

	*bit_string = bit_start;
	*bit_length = byte_len * 8 - bit_start[-1];

	return bit_start + byte_len;
}

const uint8_t *ccder_decode_eckey(uint64_t *version,
                                  size_t *priv_size, const uint8_t **priv_key,
                                  ccoid_t *oid,
                                  size_t *pub_size, const uint8_t **pub_key,
                                  const uint8_t *der, const uint8_t *der_end) {
	printf("DARLING CRYPTO STUB: %s\n", __PRETTY_FUNCTION__);
}
