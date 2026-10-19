#ifndef _ISA_CONFIG_H
#define _ISA_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
	uint32 id;
	uchar b[4];
} EISA_PRODUCT_ID;

/* uint32 MAKE_EISA_PRODUCT_ID(EISA_PRODUCT_ID* ptr, char ch0, char ch1, char ch2, uint12 prod_num, uint4 rev) */
/* example: MAKE_EISA_PRODUCT_ID(&foo_id, 'F','O','O', 0xdb1, 3); */
/* makes correct little-endian format */
#define	MAKE_EISA_PRODUCT_ID(ptr, ch0, ch1, ch2, prod_num, rev) \
	do { \
		(ptr)->b[0] = (uint8)(((((ch0) - 'A' + 1) & 0x1f) << 2) | ((((ch1) - 'A' + 1) & 0x18) >> 3));  	\
		(ptr)->b[1] = (uint8)(((((ch1) - 'A' + 1) & 0x07) << 5) | ((((ch2) - 'A' + 1) & 0x1f)     ));  	\
		(ptr)->b[2] = (uint8)((((prod_num) & 0xff0) >> 4)); \
		(ptr)->b[3] = (uint8)((((prod_num) & 0x00f) << 4) | ((rev) & 0xf) ); \
	} while(0)

/* Compare IDs without revision */
/* bool EQUAL_EISA_PRODUCT_ID(EISA_PRODUCT_ID id1, EISA_PRODUCT_ID id2); */
#define	EQUAL_EISA_PRODUCT_ID(id1, id2) \
	(((id1).b[0] == (id2).b[0]) && ((id1).b[1] == (id2).b[1]) && ((id1).b[2] == (id2).b[2]) && (((id1).b[3] & 0xf0) == ((id2).b[3] & 0xf0)))

#define B_MAX_COMPATIBLE_IDS 8

/* This structure is deprecated and will be going away in the next release */
struct isa_device_info {
	struct device_info info;

	uchar	csn;
	uchar	ldn;
	uint32	vendor_id;
	uint32	card_id;			/* a.k.a. serial number */
	uint32	logical_device_id;
	uint32	num_compatible_ids;
	uint32	compatible_ids[B_MAX_COMPATIBLE_IDS];
	char	card_name[B_OS_NAME_LENGTH];
	char	logical_device_name[B_OS_NAME_LENGTH];
};

/* So use this instead */
struct isa_info {
	uchar	csn;
	uchar	ldn;
	uint32	vendor_id;
	uint32	card_id;			/* a.k.a. serial number */
	uint32	logical_device_id;
	uint32	num_compatible_ids;
	uint32	compatible_ids[B_MAX_COMPATIBLE_IDS];
	char	card_name[B_OS_NAME_LENGTH];
	char	logical_device_name[B_OS_NAME_LENGTH];
};

/* for the mem_descriptor cookie */
#define B_ISA_MEMORY_32_BIT 0x100

#ifdef __cplusplus
}
#endif

#endif
