#ifndef _PROTOCOLBUFFER_PBCONSTANTS_H_
#define _PROTOCOLBUFFER_PBCONSTANTS_H_

#import <Foundation/Foundation.h>

/* Wire types, the low 3 bits of a field tag. */
enum {
	TYPE_VARINT		= 0,
	TYPE_FIXED64		= 1,
	TYPE_BYTES		= 2,
	TYPE_START_GROUP	= 3,
	TYPE_END_GROUP		= 4,
	TYPE_FIXED32		= 5,
};

/* A varint never exceeds ten bytes: 64 bits at seven bits per byte. */
#define PB_MAX_VARINT_LENGTH	10

#endif /* _PROTOCOLBUFFER_PBCONSTANTS_H_ */
