/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#ifndef _SECOTRPACKETDATA_H_
#define _SECOTRPACKETDATA_H_

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFData.h>

#include <corecrypto/ccn.h>

#include <CommonCrypto/CommonDigest.h>

#include <Security/SecBase.h>

#include <Security/SecOTRPackets.h>

#include <AssertMacros.h>

#include <security_utilities/simulatecrash_assert.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN

static
OSStatus ReadAndVerifyByte(const uint8_t *_Nonnull *_Nonnull bytes, size_t*size, uint8_t expected);

static
OSStatus ReadAndVerifyShort(const uint8_t *_Nonnull *_Nonnull bytes, size_t*size, uint16_t expected);

static
OSStatus ReadAndVerifyMessageType(const uint8_t *_Nonnull *_Nonnull bytes, size_t*size, OTRMessageType expected);

static
OSStatus SizeAndSkipDATA(const uint8_t *_Nonnull *_Nonnull bytes, size_t *size,
                         const uint8_t *_Nonnull *_Nonnull dataBytes, size_t *dataSize);
static
OSStatus SizeAndSkipMPI(const uint8_t *_Nonnull *_Nonnull bytes, size_t *size,
                        const uint8_t *_Nonnull *_Nonnull mpiBytes, size_t *mpiSize);


static
OSStatus ReadLongLongCompact(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, uint64_t* value);

static
OSStatus ReadLongLong(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, uint64_t* value);

static
OSStatus ReadLong(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, uint32_t* value);

static
OSStatus ReadShort(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, uint16_t* value);

static
OSStatus ReadByte(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, uint8_t* value);

static
OSStatus ReadMessageType(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, OTRMessageType* type);

static
OSStatus ReadMPI(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, cc_size n, cc_unit *x);

static
OSStatus ReadDATA(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, size_t* dataSize, uint8_t* data);

static
OSStatus CreatePublicKey(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, _Nonnull SecOTRPublicIdentityRef *_Nonnull publicId);

static
CFMutableDataRef CFDataCreateMutableFromOTRDATA(CFAllocatorRef _Nullable allocator, const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr);

static
void AppendLongLongCompact(CFMutableDataRef appendTo, uint64_t value);

static
void AppendLongLong(CFMutableDataRef appendTo, uint64_t value);

static
void AppendLong(CFMutableDataRef appendTo, uint32_t value);

static
void AppendShort(CFMutableDataRef appendTo, uint16_t value);

static
void AppendByte(CFMutableDataRef appendTo, uint8_t type);

static
void AppendMessageType(CFMutableDataRef appendTo, OTRMessageType type);

static
void AppendMPI(CFMutableDataRef appendTo, cc_size n, const cc_unit *x);

static
void AppendDATA(CFMutableDataRef appendTo, size_t size, const uint8_t*data);

static
void AppendPublicKey(CFMutableDataRef appendTo, SecOTRPublicIdentityRef publicId);

    
//
// Inline implementation
//

static uint16_t kCurrentOTRVersion = 0x2;
    
static inline OSStatus ReadLongLong(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, uint64_t* value)
{
    require(bytesPtr != NULL, fail);
    require(sizePtr != NULL, fail);
    require(value != NULL, fail);
    require(*sizePtr >= 8, fail);

    *value = ((uint64_t)(*bytesPtr)[0]) << 56 |
    ((uint64_t)(*bytesPtr)[1]) << 48 |
    ((uint64_t)(*bytesPtr)[2]) << 40 |
    ((uint64_t)(*bytesPtr)[3]) << 32 |
    ((uint64_t)(*bytesPtr)[4]) << 24 |
    ((uint64_t)(*bytesPtr)[5]) << 16 |
    ((uint64_t)(*bytesPtr)[6]) << 8  |
    ((uint64_t)(*bytesPtr)[7]) << 0;

    *bytesPtr += 8;
    *sizePtr -= 8;

    return errSecSuccess;
fail:
    return errSecParam;
}

static inline OSStatus ReadLongLongCompact(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, uint64_t* value)
{
    bool moreBytes = true;

    require(bytesPtr != NULL, fail);
    require(sizePtr != NULL, fail);
    require(value != NULL, fail);

    *value = 0;

    while (moreBytes && *sizePtr > 0) {
        uint8_t thisByte = **bytesPtr;

        moreBytes = (0x80 & thisByte) != 0;

        *value <<= 7;
        *value |= (thisByte & 0x7F);

        ++*bytesPtr;
        --*sizePtr;
    }

fail:
    return !moreBytes ? errSecSuccess : errSecDecode;
}

static inline OSStatus ReadLong(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, uint32_t* value)
{
    require(bytesPtr != NULL, fail);
    require(sizePtr != NULL, fail);
    require(value != NULL, fail);
    require(*sizePtr >= 4, fail);
    
    *value = (uint32_t)(*bytesPtr)[0] << 24 |
    (uint32_t)(*bytesPtr)[1] << 16 |
    (uint32_t)(*bytesPtr)[2] << 8  |
    (uint32_t)(*bytesPtr)[3] << 0;
    
    *bytesPtr += 4;
    *sizePtr -= 4;
    
    return errSecSuccess;
fail:
    return errSecParam;
}
    
static inline OSStatus ReadShort(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, uint16_t* value)
{
    require(bytesPtr != NULL, fail);
    require(sizePtr != NULL, fail);
    require(value != NULL, fail);
    require(*sizePtr >= 2, fail);
    
    *value = (*bytesPtr)[0] << 8  |
    (*bytesPtr)[1] << 0;
    
    *bytesPtr += 2;
    *sizePtr -= 2;
    
    return errSecSuccess;
fail:
    return errSecParam;
}

static inline OSStatus ReadByte(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, uint8_t* value)
{
    require(bytesPtr != NULL, fail);
    require(sizePtr != NULL, fail);
    require(value != NULL, fail);
    require(*sizePtr >= 1, fail);
    
    *value = *bytesPtr[0];
    
    *bytesPtr += 1;
    *sizePtr -= 1;
    
    return errSecSuccess;
fail:
    return errSecParam;
}

static inline OSStatus ReadByteAsBool(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, bool* value)
{
    uint8_t byte = 0;

    OSStatus result = ReadByte(bytesPtr, sizePtr, &byte);

    if (result == noErr)
        *value = byte != 0;

    return result;
}
    
static inline OSStatus ReadMessageType(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, OTRMessageType* type)
{
    OSStatus result = errSecParam;
    uint8_t value;

    require(type != NULL, fail);
    require_noerr_quiet(result = ReadByte(bytesPtr, sizePtr, &value), fail);
    
    *type = value;
fail:
    return result;
}

static inline OSStatus ReadMPI(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, cc_size n, cc_unit *x)
{
    require(bytesPtr != NULL, fail);
    require(sizePtr != NULL, fail);
    require(x != NULL, fail);
    require_quiet(*sizePtr >= 5, fail);
    
    uint32_t mpiLength;
    
    ReadLong(bytesPtr, sizePtr, &mpiLength);
    
    require_quiet(mpiLength <= *sizePtr, fail);
    
    ccn_read_uint(n, x, mpiLength, *bytesPtr);
    
    *bytesPtr += mpiLength;
    *sizePtr -= mpiLength;
    
    return errSecSuccess;
fail:
    return errSecParam;
    
}
    
static inline OSStatus ReadDATA(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, size_t* dataSize, uint8_t* data)
{
    require(bytesPtr != NULL, fail);
    require(sizePtr != NULL, fail);
    require(data != NULL, fail);
    require_quiet(*sizePtr >= 5, fail);
    
    uint32_t dataLength;
    
    ReadLong(bytesPtr, sizePtr, &dataLength);
    
    require_quiet(dataLength <= *sizePtr, fail);
    memmove(data, bytesPtr, dataLength);
    
    *bytesPtr += dataLength;
    *sizePtr -= dataLength;
    
    *dataSize = dataLength;
    
    return errSecSuccess;
fail:
    return errSecParam;
    
}
    
static inline OSStatus CreatePublicKey(const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr, _Nonnull SecOTRPublicIdentityRef *_Nonnull publicId)
{
    require(bytesPtr != NULL, fail);
    require(sizePtr != NULL, fail);
    require(publicId != NULL, fail);
    require(*sizePtr >= 7, fail);

    uint16_t type = 0;
    ReadShort(bytesPtr, sizePtr, &type);

    require_quiet(type == 0xF000, fail);
    require_quiet(*sizePtr >= 5, fail);
    
    uint32_t serializedIDLength = 0;
    ReadLong(bytesPtr, sizePtr, &serializedIDLength);
    
    require_quiet(*sizePtr >= serializedIDLength, fail);
    require_quiet(((CFIndex)serializedIDLength) >= 0, fail);
    
    CFDataRef serializedBytes = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, *bytesPtr, (CFIndex)serializedIDLength, kCFAllocatorNull);
    
    *publicId = SecOTRPublicIdentityCreateFromData(kCFAllocatorDefault, serializedBytes, NULL);
    
    *bytesPtr += serializedIDLength;
    *sizePtr -= serializedIDLength;
    
    if(serializedBytes)
        CFRelease(serializedBytes);
    
    return errSecSuccess;
fail:
    return errSecParam;
    
}
    
static inline CFMutableDataRef CFDataCreateMutableFromOTRDATA(CFAllocatorRef _Nullable allocator, const uint8_t *_Nonnull *_Nonnull bytesPtr, size_t*sizePtr)
{
    CFMutableDataRef result = NULL;
    uint32_t sizeInStream;
    require_noerr_quiet(ReadLong(bytesPtr, sizePtr, &sizeInStream), exit);
    require_quiet(sizeInStream <= *sizePtr, exit);
    require_quiet(((CFIndex)sizeInStream) >= 0, exit);
    
    result = CFDataCreateMutable(allocator, 0);
    
    CFDataAppendBytes(result, *bytesPtr, (CFIndex)sizeInStream);
    
    *bytesPtr += sizeInStream;
    *sizePtr -= sizeInStream;

exit:
    return result;
}


//
// Parse and verify functions
//
static inline OSStatus ReadAndVerifyByte(const uint8_t *_Nonnull *_Nonnull bytes, size_t*size, uint8_t expected)
{
    uint8_t found;
    OSStatus result = ReadByte(bytes, size, &found);
    require_noerr_quiet(result, exit);
    require_action_quiet(found == expected, exit, result = errSecDecode);
exit:
    return result;
}

static inline OSStatus ReadAndVerifyShort(const uint8_t *_Nonnull *_Nonnull bytes, size_t*size, uint16_t expected)
{
    uint16_t found;
    OSStatus result = ReadShort(bytes, size, &found);
    require_noerr_quiet(result, exit);
    require_action_quiet(found == expected, exit, result = errSecDecode);
exit:
    return result;
}

static inline OSStatus ReadAndVerifyMessageType(const uint8_t *_Nonnull *_Nonnull bytes, size_t*size, OTRMessageType expected)
{
    OTRMessageType found;
    OSStatus result = ReadMessageType(bytes, size, &found);
    require_noerr_quiet(result, exit);
    require_action_quiet(found == expected, exit, result = errSecDecode);
exit:
    return result;
}

static inline OSStatus ReadAndVerifyVersion(const uint8_t *_Nonnull *_Nonnull bytes, size_t*size)
{
    return ReadAndVerifyShort(bytes, size, kCurrentOTRVersion);
}

static inline OSStatus ReadAndVerifyHeader(const uint8_t *_Nonnull *_Nonnull bytes, size_t*size, OTRMessageType expected)
{
    OSStatus result = ReadAndVerifyVersion(bytes, size);
    require_noerr_quiet(result, exit);
    
    result = ReadAndVerifyMessageType(bytes, size, expected);
    require_noerr_quiet(result, exit);
    
exit:
    return result;
}

static inline OSStatus ReadHeader(const uint8_t *_Nonnull *_Nonnull bytes, size_t*size, OTRMessageType *messageType)
{
    OSStatus result = ReadAndVerifyVersion(bytes, size);
    require_noerr_quiet(result, exit);
    
    result = ReadMessageType(bytes, size, messageType);
    require_noerr_quiet(result, exit);
    
exit:
    return result;
}

static inline OSStatus SizeAndSkipDATA(const uint8_t *_Nonnull *_Nonnull bytes, size_t *size,
                                const uint8_t *_Nonnull *_Nonnull dataBytes, size_t *dataSize)
{
    OSStatus result;
    uint32_t sizeRead;
    result = ReadLong(bytes, size, &sizeRead);
    
    require_noerr_quiet(result, exit);
    require_action_quiet(sizeRead <= *size, exit, result = errSecDecode);
    
    *dataSize = sizeRead;
    *dataBytes = *bytes;
    *bytes += sizeRead;
    *size -= sizeRead;
exit:
    return result;
}

static inline OSStatus SizeAndSkipMPI(const uint8_t *_Nonnull *_Nonnull bytes, size_t *size,
                               const uint8_t *_Nonnull *_Nonnull mpiBytes, size_t *mpiSize)
{
    // MPIs looke like data for skipping.
    return SizeAndSkipDATA(bytes, size, mpiBytes, mpiSize);
}

    
//
// Appending functions
//
static inline void AppendLongLongCompact(CFMutableDataRef appendTo, uint64_t value)
{
    uint8_t compact[(sizeof(value) * 8 + 7) / 7]; // We can only need enough bytes to hold 8/7 expansion.

    uint8_t *end = compact + sizeof(compact);
    uint8_t *lastFilled = end;

    --lastFilled;
    *lastFilled = (value & 0x7F);

    for (value >>= 7; value != 0; value >>= 7) {
        --lastFilled;
        *lastFilled = (value & 0x7f) | 0x80;
    }

    CFDataAppendBytes(appendTo, lastFilled, end - lastFilled);
}

static inline void AppendLongLong(CFMutableDataRef appendTo, uint64_t value)
{
    uint8_t bigEndian[sizeof(value)] = { value >> 56, value >> 48, value >> 40, value >> 32,
                                         value >> 24, value >> 16, value >> 8 , value >> 0  };
    
    CFDataAppendBytes(appendTo, bigEndian, sizeof(bigEndian));
}

static inline void AppendLong(CFMutableDataRef appendTo, uint32_t value)
{
    uint8_t bigEndian[sizeof(value)] = { value >> 24, value >> 16, value >> 8, value };
    
    CFDataAppendBytes(appendTo, bigEndian, sizeof(bigEndian));
}

static inline void AppendShort(CFMutableDataRef appendTo, uint16_t value)
{
    uint8_t bigEndian[sizeof(value)] = { value >> 8, value };
    
    CFDataAppendBytes(appendTo, bigEndian, sizeof(bigEndian));
}

static inline void AppendByte(CFMutableDataRef appendTo, uint8_t byte)
{
    CFDataAppendBytes(appendTo, &byte, 1);
}

static inline void AppendMessageType(CFMutableDataRef appendTo, OTRMessageType type)
{
    AppendByte(appendTo, type);
}

static inline void AppendMPI(CFMutableDataRef appendTo, cc_size n, const cc_unit *x)
{
    size_t size = ccn_write_uint_size(n, x);
    /* 64 bits cast: we are appending an identity, whose size is hardcoded and less then 2^32 bytes */
    /* Worst case is we encoded a truncated length. No security issue. */
    assert(size<UINT32_MAX); /* Debug check */
    AppendLong(appendTo, (uint32_t)size);
    assert(((CFIndex)size) >= 0);
    CFIndex startOffset = CFDataGetLength(appendTo);
    CFDataIncreaseLength(appendTo, (CFIndex)size);
    uint8_t* insertionPtr = CFDataGetMutableBytePtr(appendTo) + startOffset;
    ccn_write_uint(n, x, size, insertionPtr);
}

static inline void AppendDATA(CFMutableDataRef appendTo, size_t size, const uint8_t*data)
{
    /* 64 bits cast: we are appending Public Key or Signature, whose sizes are hardcoded and less then 2^32 bytes */
    /* Worst case is we encoded a truncated length. No security issue. */
    assert(size<=UINT32_MAX); /* Debug check */
    AppendLong(appendTo, (uint32_t)size);
    assert(((CFIndex)size) >= 0);
    CFDataAppendBytes(appendTo, data, (CFIndex)size);
}
    
static inline void AppendCFDataAsDATA(CFMutableDataRef appendTo, CFDataRef dataToAppend)
{
    AppendDATA(appendTo, (size_t)CFDataGetLength(dataToAppend), CFDataGetBytePtr(dataToAppend));
}

static inline void AppendPublicKey(CFMutableDataRef appendTo, SecOTRPublicIdentityRef publicId)
{
    AppendShort(appendTo, 0xF000); // Custom type reserved by no one
    
    CFMutableDataRef serializedID = CFDataCreateMutable(kCFAllocatorDefault, 0);

    SecOTRPIAppendSerialization(publicId, serializedID, NULL);
    AppendDATA(appendTo, (size_t)CFDataGetLength(serializedID), CFDataGetBytePtr(serializedID));
    
    if(serializedID)
        CFRelease(serializedID);
}

static inline void AppendVersion(CFMutableDataRef appendTo)
{
    AppendShort(appendTo, kCurrentOTRVersion);
}

static inline void AppendHeader(CFMutableDataRef appendTo, OTRMessageType type)
{
    AppendVersion(appendTo);
    AppendMessageType(appendTo, type);
}

CF_ASSUME_NONNULL_END

__END_DECLS

#endif
