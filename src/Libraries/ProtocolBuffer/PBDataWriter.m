/*
 * PBDataWriter: protobuf encoding.
 */

#import <ProtocolBuffer/PBDataWriter.h>
#import <ProtocolBuffer/PBCodable.h>

#include <stddef.h>
#include <string.h>

@implementation PBDataWriter

- (instancetype)init
{
	self = [super init];
	if (self == nil) {
		return nil;
	}
	_data = [[NSMutableData alloc] init];
	_mark = 0;
	return self;
}

- (void)dealloc
{
	[_data release];
	[super dealloc];
}

- (NSData *)immutableData
{
	return [NSData dataWithData:_data];
}

@end

static void
pb_writer_append(PBDataWriter *writer, const void *bytes, NSUInteger length)
{
	[writer->_data appendBytes:bytes length:length];
}

void
PBDataWriterWriteBareVarint(PBDataWriter *writer, uint64_t value)
{
	uint8_t		buf[PB_MAX_VARINT_LENGTH];
	NSUInteger	used = 0;

	do {
		uint8_t	byte = (uint8_t)(value & 0x7f);

		value >>= 7;
		if (value != 0) {
			byte |= 0x80;
		}
		buf[used++] = byte;
	} while (value != 0);
	pb_writer_append(writer, buf, used);
}

/* Field key: field number in the upper bits, wire type in the low three. */
static void
pb_writer_write_tag(PBDataWriter *writer, uint32_t field, uint8_t type)
{
	PBDataWriterWriteBareVarint(writer, ((uint64_t)field << 3) | type);
}

static void
pb_writer_write_fixed32(PBDataWriter *writer, uint32_t value)
{
	uint32_t	le = OSSwapHostToLittleInt32(value);

	pb_writer_append(writer, &le, sizeof(le));
}

static void
pb_writer_write_fixed64(PBDataWriter *writer, uint64_t value)
{
	uint64_t	le = OSSwapHostToLittleInt64(value);

	pb_writer_append(writer, &le, sizeof(le));
}

void
PBDataWriterWriteBOOLField(PBDataWriter *writer, BOOL value, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_VARINT);
	PBDataWriterWriteBareVarint(writer, value ? 1 : 0);
}

/*
 * Negative int32 is sign-extended to 64 bits before encoding. That is protobuf's
 * rule, not an oversight: it costs ten bytes but keeps int32 and int64 wire
 * compatible. sint32/sint64 exist for callers who care about the size.
 */
void
PBDataWriterWriteInt32Field(PBDataWriter *writer, int32_t value, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_VARINT);
	PBDataWriterWriteBareVarint(writer, (uint64_t)(int64_t)value);
}

void
PBDataWriterWriteInt64Field(PBDataWriter *writer, int64_t value, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_VARINT);
	PBDataWriterWriteBareVarint(writer, (uint64_t)value);
}

void
PBDataWriterWriteUint32Field(PBDataWriter *writer, uint32_t value, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_VARINT);
	PBDataWriterWriteBareVarint(writer, value);
}

void
PBDataWriterWriteUint64Field(PBDataWriter *writer, uint64_t value, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_VARINT);
	PBDataWriterWriteBareVarint(writer, value);
}

void
PBDataWriterWriteSint32Field(PBDataWriter *writer, int32_t value, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_VARINT);
	PBDataWriterWriteBareVarint(writer,
				    (uint32_t)((value << 1) ^ (value >> 31)));
}

void
PBDataWriterWriteSint64Field(PBDataWriter *writer, int64_t value, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_VARINT);
	PBDataWriterWriteBareVarint(writer,
				    (uint64_t)((value << 1) ^ (value >> 63)));
}

void
PBDataWriterWriteFixed32Field(PBDataWriter *writer, uint32_t value, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_FIXED32);
	pb_writer_write_fixed32(writer, value);
}

void
PBDataWriterWriteFixed64Field(PBDataWriter *writer, uint64_t value, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_FIXED64);
	pb_writer_write_fixed64(writer, value);
}

void
PBDataWriterWriteSfixed32Field(PBDataWriter *writer, int32_t value, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_FIXED32);
	pb_writer_write_fixed32(writer, (uint32_t)value);
}

void
PBDataWriterWriteSfixed64Field(PBDataWriter *writer, int64_t value, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_FIXED64);
	pb_writer_write_fixed64(writer, (uint64_t)value);
}

void
PBDataWriterWriteFloatField(PBDataWriter *writer, float value, uint32_t field)
{
	uint32_t	bits;

	memcpy(&bits, &value, sizeof(bits));
	pb_writer_write_tag(writer, field, TYPE_FIXED32);
	pb_writer_write_fixed32(writer, bits);
}

void
PBDataWriterWriteDoubleField(PBDataWriter *writer, double value, uint32_t field)
{
	uint64_t	bits;

	memcpy(&bits, &value, sizeof(bits));
	pb_writer_write_tag(writer, field, TYPE_FIXED64);
	pb_writer_write_fixed64(writer, bits);
}

void
PBDataWriterWriteStringField(PBDataWriter *writer, NSString *value, uint32_t field)
{
	NSData	*utf8;

	if (value == nil) {
		return;
	}
	utf8 = [value dataUsingEncoding:NSUTF8StringEncoding];
	pb_writer_write_tag(writer, field, TYPE_BYTES);
	PBDataWriterWriteBareVarint(writer, [utf8 length]);
	pb_writer_append(writer, [utf8 bytes], [utf8 length]);
}

void
PBDataWriterWriteDataField(PBDataWriter *writer, NSData *value, uint32_t field)
{
	if (value == nil) {
		return;
	}
	pb_writer_write_tag(writer, field, TYPE_BYTES);
	PBDataWriterWriteBareVarint(writer, [value length]);
	pb_writer_append(writer, [value bytes], [value length]);
}

/*
 * A submessage's length precedes its body but is only known afterwards. Writing
 * the body to a second writer and copying it in costs one extra copy and avoids
 * having to reserve, then shift, a variable-width length in place.
 */
void
PBDataWriterWriteSubmessage(PBDataWriter *writer, id message, uint32_t field)
{
	PBDataWriter	*nested;
	NSData		*body;

	if (message == nil) {
		return;
	}
	nested = [[PBDataWriter alloc] init];
	[(PBCodable *)message writeTo:nested];
	body = [nested immutableData];
	pb_writer_write_tag(writer, field, TYPE_BYTES);
	PBDataWriterWriteBareVarint(writer, [body length]);
	pb_writer_append(writer, [body bytes], [body length]);
	[nested release];
}

/* Groups are protobuf's deprecated nesting form: no length, just a matching
 * END_GROUP tag. */
void
PBDataWriterWriteSubgroup(PBDataWriter *writer, id message, uint32_t field)
{
	if (message == nil) {
		return;
	}
	pb_writer_write_tag(writer, field, TYPE_START_GROUP);
	[(PBCodable *)message writeTo:writer];
	pb_writer_write_tag(writer, field, TYPE_END_GROUP);
}

void
PBDataWriterPlaceMark(PBDataWriter *writer, uint32_t field)
{
	pb_writer_write_tag(writer, field, TYPE_BYTES);
	writer->_mark = [writer->_data length];
}

void
PBDataWriterRecallMark(PBDataWriter *writer)
{
	writer->_mark = 0;
}
