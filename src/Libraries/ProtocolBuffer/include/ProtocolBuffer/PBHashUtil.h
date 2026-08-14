#ifndef _PROTOCOLBUFFER_PBHASHUTIL_H_
#define _PROTOCOLBUFFER_PBHASHUTIL_H_

#import <Foundation/Foundation.h>

__BEGIN_DECLS

/*
 * Generated -hash implementations XOR one PBHashInt per set field, so a hash
 * that merely returned its argument would cancel equal values in different
 * fields against each other. Mix instead.
 */
static inline NSUInteger
PBHashInt(NSUInteger value)
{
	NSUInteger hash = value;

	hash ^= hash >> 33;
	hash *= (NSUInteger)0xff51afd7ed558ccdULL;
	hash ^= hash >> 33;
	hash *= (NSUInteger)0xc4ceb9fe1a85ec53ULL;
	hash ^= hash >> 33;
	return hash;
}

NSUInteger PBHashBytes(const void *bytes, NSUInteger length);

__END_DECLS

#endif /* _PROTOCOLBUFFER_PBHASHUTIL_H_ */
