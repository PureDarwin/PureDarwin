/*
 * PBDataReader.h
 *
 * Decoding half of ProtocolBuffer. Generated -readFrom: implementations drive
 * this: loop on PBReaderHasMoreData, PBReaderReadTag32AndType, switch on the
 * field number, and call the reader for the field's type.
 *
 * The ivars are visible because Apple's inline reader functions touch them
 * directly, and generated code calls those functions - keeping them inline (and
 * therefore keeping the layout public) is what makes that code compile
 * unchanged. Only the functions Apple exports from the framework are out of
 * line here; the rest stay inline exactly as they are on macOS.
 */

#ifndef _PROTOCOLBUFFER_PBDATAREADER_H_
#define _PROTOCOLBUFFER_PBDATAREADER_H_

#import <Foundation/Foundation.h>
#import <ProtocolBuffer/PBConstants.h>

/* The inline readers below use memcpy and the OSSwap* byte-order helpers, so
 * this header has to bring them in itself rather than rely on its includer. */
#include <libkern/OSByteOrder.h>
#include <string.h>

@interface PBDataReader : NSObject
{
@public
	const uint8_t	*_bytes;
	NSUInteger	_length;
	NSUInteger	_offset;
	NSUInteger	_mark;
	BOOL		_error;
	NSData		*_data;
}

- (instancetype)initWithData:(NSData *)data;
+ (instancetype)readerWithData:(NSData *)data;

@property (readonly) BOOL hasError;

@end

__BEGIN_DECLS

/*
 * Out-of-line: these allocate, or are large enough that inlining them at every
 * field would bloat generated code.
 */
NSString *PBReaderReadString(PBDataReader *reader);
NSData *PBReaderReadData(PBDataReader *reader);
BOOL PBReaderReadVarIntBuf(PBDataReader *reader, uint64_t *value);
BOOL PBReaderSkipValueWithTag(PBDataReader *reader, uint32_t tag, uint8_t type);
void PBReaderPlaceMark(PBDataReader *reader, NSUInteger mark);
void PBReaderRecallMark(PBDataReader *reader);
uint16_t PBReaderReadBigEndianFixed16(PBDataReader *reader);
uint32_t PBReaderReadBigEndianFixed32(PBDataReader *reader);
uint64_t PBReaderReadBigEndianFixed64(PBDataReader *reader);

/* Inline, matching the framework: generated code calls these per field. */

static inline BOOL
PBReaderHasError(PBDataReader *reader)
{
	return reader->_error;
}

static inline BOOL
PBReaderHasMoreData(PBDataReader *reader)
{
	return !reader->_error && reader->_offset < reader->_length;
}

static inline uint64_t
PBReaderReadVarint(PBDataReader *reader)
{
	uint64_t	value = 0;
	unsigned	shift = 0;

	while (shift < 64) {
		uint8_t	byte;

		if (reader->_offset >= reader->_length) {
			reader->_error = YES;
			return 0;
		}
		byte = reader->_bytes[reader->_offset++];
		value |= (uint64_t)(byte & 0x7f) << shift;
		if ((byte & 0x80) == 0) {
			return value;
		}
		shift += 7;
	}
	/* More than ten continuation bytes: the varint is malformed. */
	reader->_error = YES;
	return 0;
}

static inline BOOL
PBReaderReadTag32AndType(PBDataReader *reader, uint32_t *tag, uint8_t *type)
{
	uint64_t	key;

	key = PBReaderReadVarint(reader);
	if (reader->_error) {
		return NO;
	}
	*tag = (uint32_t)(key >> 3);
	*type = (uint8_t)(key & 0x07);
	/* Field numbers start at 1; tag 0 means a corrupt stream. */
	if (*tag == 0) {
		reader->_error = YES;
		return NO;
	}
	return YES;
}

static inline BOOL
PBReaderReadBOOL(PBDataReader *reader)
{
	return PBReaderReadVarint(reader) != 0;
}

static inline int32_t
PBReaderReadInt32(PBDataReader *reader)
{
	return (int32_t)PBReaderReadVarint(reader);
}

static inline uint32_t
PBReaderReadUint32(PBDataReader *reader)
{
	return (uint32_t)PBReaderReadVarint(reader);
}

static inline int64_t
PBReaderReadInt64(PBDataReader *reader)
{
	return (int64_t)PBReaderReadVarint(reader);
}

static inline uint64_t
PBReaderReadUint64(PBDataReader *reader)
{
	return PBReaderReadVarint(reader);
}

/* Zigzag: sint32/sint64 map signed values onto unsigned varints. */
static inline int32_t
PBReaderReadSint32(PBDataReader *reader)
{
	uint32_t	value = (uint32_t)PBReaderReadVarint(reader);

	return (int32_t)((value >> 1) ^ (~(value & 1) + 1));
}

static inline int64_t
PBReaderReadSint64(PBDataReader *reader)
{
	uint64_t	value = PBReaderReadVarint(reader);

	return (int64_t)((value >> 1) ^ (~(value & 1) + 1));
}

static inline uint32_t
PBReaderReadFixed32(PBDataReader *reader)
{
	uint32_t	value;

	if (reader->_length - reader->_offset < sizeof(value)) {
		reader->_error = YES;
		return 0;
	}
	memcpy(&value, reader->_bytes + reader->_offset, sizeof(value));
	reader->_offset += sizeof(value);
	return OSSwapLittleToHostInt32(value);
}

static inline uint64_t
PBReaderReadFixed64(PBDataReader *reader)
{
	uint64_t	value;

	if (reader->_length - reader->_offset < sizeof(value)) {
		reader->_error = YES;
		return 0;
	}
	memcpy(&value, reader->_bytes + reader->_offset, sizeof(value));
	reader->_offset += sizeof(value);
	return OSSwapLittleToHostInt64(value);
}

static inline float
PBReaderReadFloat(PBDataReader *reader)
{
	uint32_t	bits = PBReaderReadFixed32(reader);
	float		value;

	memcpy(&value, &bits, sizeof(value));
	return value;
}

static inline double
PBReaderReadDouble(PBDataReader *reader)
{
	uint64_t	bits = PBReaderReadFixed64(reader);
	double		value;

	memcpy(&value, &bits, sizeof(value));
	return value;
}

__END_DECLS

#endif /* _PROTOCOLBUFFER_PBDATAREADER_H_ */
