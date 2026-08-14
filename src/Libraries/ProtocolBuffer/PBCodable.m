/*
 * PBCodable: base class for generated message types.
 */

#import <ProtocolBuffer/PBCodable.h>
#import <ProtocolBuffer/PBHashUtil.h>

#include <stddef.h>

@implementation PBCodable

@synthesize unknownFields = _unknownFields;

- (void)dealloc
{
	[_unknownFields release];
	[super dealloc];
}

/*
 * Generated subclasses override both. Doing nothing here rather than raising
 * keeps a subclass that implements only one direction usable for the other.
 */
- (BOOL)readFrom:(PBDataReader *)reader
{
	(void)reader;
	return YES;
}

- (void)writeTo:(PBDataWriter *)writer
{
	(void)writer;
}

- (NSData *)data
{
	PBDataWriter	*writer;
	NSData		*data;

	writer = [[PBDataWriter alloc] init];
	[self writeTo:writer];
	data = [writer immutableData];
	[writer release];
	return data;
}

- (BOOL)parseFromData:(NSData *)data
{
	PBDataReader	*reader;
	BOOL		ok;

	reader = [[PBDataReader alloc] initWithData:data];
	ok = [self readFrom:reader] && ![reader hasError];
	[reader release];
	return ok;
}

+ (instancetype)parseFromData:(NSData *)data
{
	id	message;

	message = [[[self alloc] init] autorelease];
	if (![message parseFromData:data]) {
		return nil;
	}
	return message;
}

@end

NSUInteger
PBHashBytes(const void *bytes, NSUInteger length)
{
	const uint8_t	*p = (const uint8_t *)bytes;
	NSUInteger	hash = 0;

	if (p == NULL) {
		return 0;
	}
	/* FNV-1a: cheap, and -hash only needs to spread values, not resist
	 * collisions from an adversary. */
	hash = (NSUInteger)0xcbf29ce484222325ULL;
	for (NSUInteger i = 0; i < length; i++) {
		hash ^= p[i];
		hash *= (NSUInteger)0x100000001b3ULL;
	}
	return hash;
}
