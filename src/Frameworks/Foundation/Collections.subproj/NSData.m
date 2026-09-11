/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * NSData / NSMutableData over CFData.
 *
 * CFData and CFMutableData share one CFTypeID, so the bridge can register only
 * one class for both. That class therefore has to answer the mutable selectors
 * as well, which is why the work lives in NSCFData - a subclass of
 * NSMutableData - rather than being split across NSData and NSMutableData the
 * way the headers are. This mirrors real Foundation, where __NSCFData is
 * likewise a subclass of NSMutableData.
 *
 * Mutability is enforced by CFData itself: CFDataAppendBytes on an immutable
 * CFData traps, exactly as -appendBytes:length: on an immutable NSData does on
 * macOS. Nothing here needs to re-check it.
 *
 * NSData and NSMutableData are class clusters: +alloc produces a plain
 * instance of the abstract class, and each -init... releases it and returns the
 * CF-backed object instead.
 */

#import <Foundation/NSData.h>
#include <stdio.h>
#include <CoreFoundation/CFBase.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/ForFoundationOnly.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

@interface NSCFData : NSMutableData
@end

/* Convenience constructors are autoreleased; CFAutorelease is the CF-side entry
 * point for exactly this and keeps the ownership rules right without assuming a
 * particular Foundation pool implementation. */
static inline id
__NSDataAutorelease(CFTypeRef cf)
{
	if (cf == NULL) {
		return nil;
	}
	return (id)CFAutorelease(cf);
}

static CFDataRef
__NSDataCreateWithContentsOfFile(NSString *path)
{
	struct stat status;
	UInt8 *bytes = NULL;
	ssize_t offset = 0;
	int fd;

	if (path == nil || (fd = open([path cString], O_RDONLY)) < 0) {
		return NULL;
	}
	if (fstat(fd, &status) != 0 || status.st_size < 0) {
		close(fd);
		return NULL;
	}
	if (status.st_size != 0) {
		bytes = malloc((size_t)status.st_size);
		if (bytes == NULL) {
			close(fd);
			return NULL;
		}
		while (offset < status.st_size) {
			ssize_t amount = read(fd, bytes + offset,
				(size_t)(status.st_size - offset));

			if (amount <= 0) {
				free(bytes);
				close(fd);
				return NULL;
			}
			offset += amount;
		}
	}
	close(fd);

	CFDataRef result = CFDataCreate(kCFAllocatorDefault, bytes,
		(CFIndex)status.st_size);
	free(bytes);
	return result;
}

@implementation NSData

+ (instancetype)data
{
	return __NSDataAutorelease(CFDataCreate(kCFAllocatorDefault, NULL, 0));
}

+ (instancetype)dataWithBytes:(const void *)bytes length:(NSUInteger)length
{
	return __NSDataAutorelease(CFDataCreate(kCFAllocatorDefault,
						(const UInt8 *)bytes,
						(CFIndex)length));
}

+ (instancetype)dataWithBytesNoCopy:(void *)bytes length:(NSUInteger)length
{
	return [self dataWithBytesNoCopy:bytes length:length freeWhenDone:YES];
}

+ (instancetype)dataWithBytesNoCopy:(void *)bytes
			     length:(NSUInteger)length
		       freeWhenDone:(BOOL)freeWhenDone
{
	/* kCFAllocatorNull as the byte deallocator means CF adopts the buffer
	 * without ever freeing it - the "no copy, not mine to free" case. */
	return __NSDataAutorelease(
		CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
					    (const UInt8 *)bytes,
					    (CFIndex)length,
					    freeWhenDone ? kCFAllocatorDefault
							 : kCFAllocatorNull));
}

+ (instancetype)dataWithData:(NSData *)data
{
	if (data == nil) {
		return nil;
	}
	return __NSDataAutorelease(CFDataCreateCopy(kCFAllocatorDefault,
						    (CFDataRef)data));
}

+ (instancetype)dataWithContentsOfFile:(NSString *)path
{
	return __NSDataAutorelease(__NSDataCreateWithContentsOfFile(path));
}

/* The reading options only hint at mapping and caching; the bytes are the same
 * either way, so they are accepted and ignored. */
+ (instancetype)dataWithContentsOfFile:(NSString *)path
                               options:(NSDataReadingOptions)options
                                 error:(NSError **)error
{
	NSData *data = [self dataWithContentsOfFile:path];

	if (data == nil && error != NULL) {
		*error = nil;
	}
	return data;
}

- (instancetype)init
{
	[self release];
	return (id)CFDataCreate(kCFAllocatorDefault, NULL, 0);
}

- (instancetype)initWithBytes:(const void *)bytes length:(NSUInteger)length
{
	[self release];
	return (id)CFDataCreate(kCFAllocatorDefault, (const UInt8 *)bytes,
				(CFIndex)length);
}

/* The two-argument form takes ownership, i.e. it is the three-argument form
 * with freeWhenDone:YES. Only the latter existed. */
- (instancetype)initWithBytesNoCopy:(void *)bytes length:(NSUInteger)length
{
	return [self initWithBytesNoCopy:bytes length:length freeWhenDone:YES];
}

- (instancetype)initWithBytesNoCopy:(void *)bytes
			     length:(NSUInteger)length
		       freeWhenDone:(BOOL)freeWhenDone
{
	[self release];
	return (id)CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
					       (const UInt8 *)bytes,
					       (CFIndex)length,
					       freeWhenDone ? kCFAllocatorDefault
							    : kCFAllocatorNull);
}

- (instancetype)initWithData:(NSData *)data
{
	[self release];
	if (data == nil) {
		return nil;
	}
	return (id)CFDataCreateCopy(kCFAllocatorDefault, (CFDataRef)data);
}

- (instancetype)initWithContentsOfFile:(NSString *)path
{
	[self release];
	return (id)__NSDataCreateWithContentsOfFile(path);
}

- (id)copyWithZone:(NSZone *)zone
{
	return (id)CFDataCreate(kCFAllocatorDefault, [self bytes],
	                        (CFIndex)[self length]);
}

- (id)mutableCopyWithZone:(NSZone *)zone
{
	CFMutableDataRef result =
	    CFDataCreateMutable(kCFAllocatorDefault, 0);

	if (result != NULL) {
		CFDataAppendBytes(result, [self bytes], (CFIndex)[self length]);
	}
	return (id)result;
}

/*
 * The accessors below are only reached on a real CFData, since every creation
 * path returns one. NSCFData overrides them all; these exist so a direct
 * [NSData alloc] that never got -init'd fails predictably instead of by
 * message-to-garbage.
 */
- (const void *)bytes
{
	return NULL;
}

- (NSUInteger)length
{
	return 0;
}

- (void)getBytes:(void *)buffer length:(NSUInteger)length
{
	[self getBytes:buffer range:NSMakeRange(0, length)];
}

- (void)getBytes:(void *)buffer range:(NSRange)range
{
	CFDataGetBytes((CFDataRef)self,
		       CFRangeMake((CFIndex)range.location,
				   (CFIndex)range.length),
		       (UInt8 *)buffer);
}

- (NSData *)subdataWithRange:(NSRange)range
{
	const uint8_t	*base = (const uint8_t *)[self bytes];

	if (base == NULL) {
		return nil;
	}
	return [NSData dataWithBytes:base + range.location
			      length:range.length];
}

- (BOOL)isEqualToData:(NSData *)other
{
	if (other == nil) {
		return NO;
	}
	if (self == other) {
		return YES;
	}
	return CFEqual((CFTypeRef)self, (CFTypeRef)other) ? YES : NO;
}

/* Bridged to CFDataRef, so identity comes from the CF layer. Without these the
 * NSObject versions apply and compare pointers, which makes any dictionary or
 * set keyed by value fail to find an equal-but-distinct object. */
- (NSUInteger)hash {
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)other {
    if (self == other) {
        return YES;
    }
    if (other == nil || ![other isKindOfClass:[NSData class]]) {
        return NO;
    }
    return CFEqual((CFTypeRef)self, (CFTypeRef)other) ? YES : NO;
}


/* Atomic writes go to a temporary in the same directory and are renamed, so a
 * reader never observes a partial file. */
- (BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically
{
	const char *target = [path fileSystemRepresentation];
	char temporary[1024];
	const char *writePath = target;

	if (atomically) {
		snprintf(temporary, sizeof(temporary), "%s.XXXXXX", target);
		writePath = temporary;
	}

	int fd = atomically ? mkstemp(temporary)
	                    : open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (fd < 0) {
		return NO;
	}

	const void *bytes = [self bytes];
	size_t remaining = (size_t)[self length];
	BOOL ok = YES;

	while (remaining > 0) {
		ssize_t written = write(fd, bytes, remaining);

		if (written <= 0) {
			ok = NO;
			break;
		}
		bytes = (const char *)bytes + written;
		remaining -= (size_t)written;
	}
	close(fd);

	if (ok && atomically) {
		ok = (rename(writePath, target) == 0);
		if (!ok) {
			unlink(writePath);
		}
	}
	return ok;
}


- (BOOL)writeToFile:(NSString *)path options:(NSDataWritingOptions)options
              error:(NSError **)error
{
	BOOL ok = [self writeToFile:path
	                 atomically:(options & NSDataWritingAtomic) != 0];

	if (!ok && error != NULL) {
		*error = nil;
	}
	return ok;
}

@end

@implementation NSMutableData

+ (instancetype)data
{
	return [self dataWithCapacity:0];
}

+ (instancetype)dataWithCapacity:(NSUInteger)capacity
{
	return __NSDataAutorelease(CFDataCreateMutable(kCFAllocatorDefault, 0));
}

+ (instancetype)dataWithLength:(NSUInteger)length
{
	CFMutableDataRef	data;

	data = CFDataCreateMutable(kCFAllocatorDefault, 0);
	if (data == NULL) {
		return nil;
	}
	/* CFDataSetLength zero-fills any growth, which is what
	 * +dataWithLength: promises. */
	CFDataSetLength(data, (CFIndex)length);
	return __NSDataAutorelease(data);
}

+ (instancetype)dataWithBytes:(const void *)bytes length:(NSUInteger)length
{
	CFMutableDataRef	data;

	data = CFDataCreateMutable(kCFAllocatorDefault, 0);
	if (data == NULL) {
		return nil;
	}
	CFDataAppendBytes(data, (const UInt8 *)bytes, (CFIndex)length);
	return __NSDataAutorelease(data);
}

+ (instancetype)dataWithData:(NSData *)data
{
	NSMutableData	*copy;

	if (data == nil) {
		return nil;
	}
	copy = [self dataWithCapacity:0];
	[copy appendData:data];
	return copy;
}

- (instancetype)init
{
	[self release];
	return (id)CFDataCreateMutable(kCFAllocatorDefault, 0);
}

- (instancetype)initWithCapacity:(NSUInteger)capacity
{
	[self release];
	/* See +dataWithCapacity: - a non-zero CF capacity is a hard ceiling. */
	return (id)CFDataCreateMutable(kCFAllocatorDefault, 0);
}

- (instancetype)initWithLength:(NSUInteger)length
{
	CFMutableDataRef	data;

	[self release];
	data = CFDataCreateMutable(kCFAllocatorDefault, 0);
	if (data == NULL) {
		return nil;
	}
	CFDataSetLength(data, (CFIndex)length);
	return (id)data;
}

- (instancetype)initWithBytes:(const void *)bytes length:(NSUInteger)length
{
	CFMutableDataRef	data;

	[self release];
	data = CFDataCreateMutable(kCFAllocatorDefault, 0);
	if (data == NULL) {
		return nil;
	}
	CFDataAppendBytes(data, (const UInt8 *)bytes, (CFIndex)length);
	return (id)data;
}

- (void *)mutableBytes
{
	return NULL;
}

- (void)setLength:(NSUInteger)length
{
	CFDataSetLength((CFMutableDataRef)self, (CFIndex)length);
}

- (void)appendBytes:(const void *)bytes length:(NSUInteger)length
{
	CFDataAppendBytes((CFMutableDataRef)self, (const UInt8 *)bytes,
			  (CFIndex)length);
}

- (void)appendData:(NSData *)other
{
	if (other == nil) {
		return;
	}
	CFDataAppendBytes((CFMutableDataRef)self,
			  (const UInt8 *)[other bytes],
			  (CFIndex)[other length]);
}

- (void)replaceBytesInRange:(NSRange)range withBytes:(const void *)bytes
{
	CFDataReplaceBytes((CFMutableDataRef)self,
			   CFRangeMake((CFIndex)range.location,
				       (CFIndex)range.length),
			   (const UInt8 *)bytes, (CFIndex)range.length);
}

- (void)replaceBytesInRange:(NSRange)range
                  withBytes:(const void *)bytes
                     length:(NSUInteger)length
{
	CFDataReplaceBytes((CFMutableDataRef)self,
			   CFRangeMake((CFIndex)range.location,
				       (CFIndex)range.length),
			   (const UInt8 *)bytes, (CFIndex)length);
}

- (void)increaseLengthBy:(NSUInteger)extra
{
	[self setLength:[self length] + extra];
}

- (void)resetBytesInRange:(NSRange)range
{
	uint8_t	*base = (uint8_t *)[self mutableBytes];

	if (base == NULL) {
		return;
	}
	memset(base + range.location, 0, range.length);
}

- (void)setData:(NSData *)data
{
	CFDataSetLength((CFMutableDataRef)self, 0);
	[self appendData:data];
}

@end

/*
 * The bridged class. Every CFData - mutable or not - carries this isa, so it
 * has to answer both halves of the interface.
 */
@implementation NSCFData

- (const void *)bytes
{
	return (const void *)CFDataGetBytePtr((CFDataRef)self);
}

- (NSUInteger)length
{
	return (NSUInteger)CFDataGetLength((CFDataRef)self);
}

- (void *)mutableBytes
{
	return (void *)CFDataGetMutableBytePtr((CFMutableDataRef)self);
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFDataBridgeInit(void) {
	_CFRuntimeBridgeClasses(CFDataGetTypeID(), "NSCFData");
}
#endif
