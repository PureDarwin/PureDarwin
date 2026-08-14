/*
 * PBDataWriter.h
 *
 * Encoding half of ProtocolBuffer. Generated -writeTo: implementations call one
 * PBDataWriterWrite<Type>Field per set field, in field-number order.
 */

#ifndef _PROTOCOLBUFFER_PBDATAWRITER_H_
#define _PROTOCOLBUFFER_PBDATAWRITER_H_

#import <Foundation/Foundation.h>
#import <ProtocolBuffer/PBConstants.h>

@interface PBDataWriter : NSObject
{
@public
	NSMutableData	*_data;
	NSUInteger	_mark;
}

- (instancetype)init;

/* The encoded message. */
@property (readonly) NSData *immutableData;

@end

__BEGIN_DECLS

void PBDataWriterWriteBareVarint(PBDataWriter *writer, uint64_t value);

void PBDataWriterWriteBOOLField(PBDataWriter *writer, BOOL value, uint32_t field);
void PBDataWriterWriteInt32Field(PBDataWriter *writer, int32_t value, uint32_t field);
void PBDataWriterWriteInt64Field(PBDataWriter *writer, int64_t value, uint32_t field);
void PBDataWriterWriteUint32Field(PBDataWriter *writer, uint32_t value, uint32_t field);
void PBDataWriterWriteUint64Field(PBDataWriter *writer, uint64_t value, uint32_t field);
void PBDataWriterWriteSint32Field(PBDataWriter *writer, int32_t value, uint32_t field);
void PBDataWriterWriteSint64Field(PBDataWriter *writer, int64_t value, uint32_t field);
void PBDataWriterWriteFixed32Field(PBDataWriter *writer, uint32_t value, uint32_t field);
void PBDataWriterWriteFixed64Field(PBDataWriter *writer, uint64_t value, uint32_t field);
void PBDataWriterWriteSfixed32Field(PBDataWriter *writer, int32_t value, uint32_t field);
void PBDataWriterWriteSfixed64Field(PBDataWriter *writer, int64_t value, uint32_t field);
void PBDataWriterWriteFloatField(PBDataWriter *writer, float value, uint32_t field);
void PBDataWriterWriteDoubleField(PBDataWriter *writer, double value, uint32_t field);
void PBDataWriterWriteStringField(PBDataWriter *writer, NSString *value, uint32_t field);
void PBDataWriterWriteDataField(PBDataWriter *writer, NSData *value, uint32_t field);
void PBDataWriterPlaceMark(PBDataWriter *writer, uint32_t field);
void PBDataWriterRecallMark(PBDataWriter *writer);
void PBDataWriterWriteSubmessage(PBDataWriter *writer, id message, uint32_t field);
void PBDataWriterWriteSubgroup(PBDataWriter *writer, id message, uint32_t field);

__END_DECLS

#endif /* _PROTOCOLBUFFER_PBDATAWRITER_H_ */
