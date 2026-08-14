/*
 * PBDataReader: protobuf decoding.
 *
 * Only the entry points Apple exports from ProtocolBuffer are here; the rest
 * are inline in the header, where generated code expects them.
 */

#import <ProtocolBuffer/PBDataReader.h>

#include <string.h>

@implementation PBDataReader

- (instancetype)initWithData:(NSData *)data
{
	self = [super init];
	if (self == nil) {
		return nil;
	}
	_data = [data retain];
	_bytes = (const uint8_t *)[data bytes];
	_length = [data length];
	_offset = 0;
	_mark = 0;
	_error = NO;
	return self;
}

+ (instancetype)readerWithData:(NSData *)data
{
	return [[[self alloc] initWithData:data] autorelease];
}

- (void)dealloc
{
	[_data release];
	[super dealloc];
}

- (BOOL)hasError
{
	return _error;
}

@end

/*
 * Length-delimited values carry a varint length followed by that many bytes.
 * Returns NO and flags the reader on a length that runs past the buffer, which
 * is the only way a truncated or hostile message can be caught here.
 */
static BOOL
pb_reader_read_length(PBDataReader *reader, NSUInteger *lengthp)
{
	uint64_t	length;

	length = PBReaderReadVarint(reader);
	if (reader->_error) {
		return NO;
	}
	if (length > (uint64_t)(reader->_length - reader->_offset)) {
		reader->_error = YES;
		return NO;
	}
	*lengthp = (NSUInteger)length;
	return YES;
}

NSString *
PBReaderReadString(PBDataReader *reader)
{
	NSUInteger	length;
	NSString	*string;

	if (!pb_reader_read_length(reader, &length)) {
		return nil;
	}
	string = [[[NSString alloc] initWithBytes:reader->_bytes + reader->_offset
					   length:length
					 encoding:NSUTF8StringEncoding]
			 autorelease];
	reader->_offset += length;
	/* protobuf requires string fields to be valid UTF-8; anything else is a
	 * malformed message rather than an empty string. */
	if (string == nil) {
		reader->_error = YES;
	}
	return string;
}

NSData *
PBReaderReadData(PBDataReader *reader)
{
	NSUInteger	length;
	NSData		*data;

	if (!pb_reader_read_length(reader, &length)) {
		return nil;
	}
	data = [NSData dataWithBytes:reader->_bytes + reader->_offset
			      length:length];
	reader->_offset += length;
	return data;
}

BOOL
PBReaderReadVarIntBuf(PBDataReader *reader, uint64_t *value)
{
	uint64_t	read;

	read = PBReaderReadVarint(reader);
	if (reader->_error) {
		return NO;
	}
	if (value != NULL) {
		*value = read;
	}
	return YES;
}

BOOL
PBReaderSkipValueWithTag(PBDataReader *reader, uint32_t tag, uint8_t type)
{
	NSUInteger	length;

	(void)tag;
	switch (type) {
	case TYPE_VARINT:
		(void)PBReaderReadVarint(reader);
		break;
	case TYPE_FIXED64:
		if (reader->_length - reader->_offset < 8) {
			reader->_error = YES;
			return NO;
		}
		reader->_offset += 8;
		break;
	case TYPE_BYTES:
		if (!pb_reader_read_length(reader, &length)) {
			return NO;
		}
		reader->_offset += length;
		break;
	case TYPE_START_GROUP:
		/* Skip to the matching END_GROUP, honouring nesting. */
		for (;;) {
			uint32_t	inner_tag;
			uint8_t		inner_type;

			if (!PBReaderReadTag32AndType(reader, &inner_tag,
						      &inner_type)) {
				return NO;
			}
			if (inner_type == TYPE_END_GROUP) {
				break;
			}
			if (!PBReaderSkipValueWithTag(reader, inner_tag,
						      inner_type)) {
				return NO;
			}
		}
		break;
	case TYPE_END_GROUP:
		/* Consumed by the caller's loop, nothing of its own to skip. */
		break;
	case TYPE_FIXED32:
		if (reader->_length - reader->_offset < 4) {
			reader->_error = YES;
			return NO;
		}
		reader->_offset += 4;
		break;
	default:
		reader->_error = YES;
		return NO;
	}
	return !reader->_error;
}

void
PBReaderPlaceMark(PBDataReader *reader, NSUInteger mark)
{
	reader->_mark = mark;
}

void
PBReaderRecallMark(PBDataReader *reader)
{
	reader->_offset = reader->_mark;
}

/*
 * Big-endian fixed reads. These are not protobuf field types - they serve the
 * length-prefixed stream framing PBMessageStreamReader uses - so they take the
 * bytes as they are rather than byte-swapping from little-endian.
 */
uint16_t
PBReaderReadBigEndianFixed16(PBDataReader *reader)
{
	uint16_t	value;

	if (reader->_length - reader->_offset < sizeof(value)) {
		reader->_error = YES;
		return 0;
	}
	memcpy(&value, reader->_bytes + reader->_offset, sizeof(value));
	reader->_offset += sizeof(value);
	return OSSwapBigToHostInt16(value);
}

uint32_t
PBReaderReadBigEndianFixed32(PBDataReader *reader)
{
	uint32_t	value;

	if (reader->_length - reader->_offset < sizeof(value)) {
		reader->_error = YES;
		return 0;
	}
	memcpy(&value, reader->_bytes + reader->_offset, sizeof(value));
	reader->_offset += sizeof(value);
	return OSSwapBigToHostInt32(value);
}

uint64_t
PBReaderReadBigEndianFixed64(PBDataReader *reader)
{
	uint64_t	value;

	if (reader->_length - reader->_offset < sizeof(value)) {
		reader->_error = YES;
		return 0;
	}
	memcpy(&value, reader->_bytes + reader->_offset, sizeof(value));
	reader->_offset += sizeof(value);
	return OSSwapBigToHostInt64(value);
}
