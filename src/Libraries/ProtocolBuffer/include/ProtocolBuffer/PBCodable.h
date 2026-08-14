/*
 * PBCodable.h
 *
 * Base class for protocompiler-generated message classes. The generated
 * subclass supplies -readFrom:, -writeTo:, -copyTo:, -mergeFrom:, -isEqual: and
 * -hash; PBCodable itself provides the data<->object convenience on top of
 * those, plus the unknown-field passthrough.
 */

#ifndef _PROTOCOLBUFFER_PBCODABLE_H_
#define _PROTOCOLBUFFER_PBCODABLE_H_

#import <Foundation/Foundation.h>
#import <ProtocolBuffer/PBDataReader.h>
#import <ProtocolBuffer/PBDataWriter.h>

@interface PBCodable : NSObject
{
	NSData	*_unknownFields;
}

/* Overridden by every generated subclass. The base implementations do nothing
 * so a subclass that omits one is inert rather than crashing. */
- (BOOL)readFrom:(PBDataReader *)reader;
- (void)writeTo:(PBDataWriter *)writer;

/* Encode to / decode from a bare message (no length prefix). */
- (NSData *)data;
- (BOOL)parseFromData:(NSData *)data;
+ (instancetype)parseFromData:(NSData *)data;

/* Fields the reader did not recognise, kept so a decode/encode round trip does
 * not silently drop them. */
@property (retain) NSData *unknownFields;

@end

#endif /* _PROTOCOLBUFFER_PBCODABLE_H_ */
