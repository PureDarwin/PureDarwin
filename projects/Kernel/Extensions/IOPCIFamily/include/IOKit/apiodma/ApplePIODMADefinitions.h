//
//  ApplePIODMADefinitions.h
//  ApplePIODMA
//
//  Created by Kevin Strasberg on 6/26/20.
//

#ifndef _IOKIT_ApplePIODMADefinitions_H
#define _IOKIT_ApplePIODMADefinitions_H
#include <IOKit/IOTypes.h>

#define APIODMABit(bit)                  ((uint32_t)(1) << bit)
#define APIODMABitRange32(start, end)    (~(((uint32_t)(1) << start) - 1) & (((uint32_t)(1) << end) | (((uint32_t)(1) << end) - 1)))
#define APIODMABitRange64(start, end)    (~(((uint64_t)(1) << start) - 1) & (((uint64_t)(1) << end) | (((uint64_t)(1) << end) - 1)))
#define APIODMABitRangePhase(start, end) (start)

#define  kApplePIODMAID "piodma-id"

// Table 2.4. Next Pointer Packet Header, Revision 2.2.15
enum tApplePIODMANextPointerPacketHeader
{
    kApplePIODMANextPointerPacketHeaderNextSrcAddrLo      = APIODMABitRange32(32, 63),
    kApplePIODMANextPointerPacketHeaderNextSrcAddrLoPhase = APIODMABitRangePhase(32, 63),
    kApplePIODMANextPointerPacketHeaderNextSrcAddrHi      = APIODMABitRange32(28, 31),
    kApplePIODMANextPointerPacketHeaderNextSrcAddrHiPhase = APIODMABitRangePhase(28, 31),
    kApplePIODMANextPointerPacketHeaderRsvd               = APIODMABitRange32(26, 27),
    kApplePIODMANextPointerPacketHeaderRsvdPhase          = APIODMABitRangePhase(26, 27),
    kApplePIODMANextPointerPacketHeaderNextSize           = APIODMABitRange32(4, 25),
    kApplePIODMANextPointerPacketHeaderNextSizePhase      = APIODMABitRangePhase(4, 25),
    kApplePIODMANextPointerPacketHeaderExtendedType       = APIODMABitRange32(2, 3),
    kApplePIODMANextPointerPacketHeaderExtendedTypePhase  = APIODMABitRangePhase(2, 3),
    kApplePIODMANextPointerPacketHeaderType               = APIODMABitRange32(0, 1),
    kApplePIODMANextPointerPacketHeaderTypePhase          = APIODMABitRangePhase(0, 1)
};

typedef uint64_t ApplePIODMANextPointerPacketHeader;

// Table 2.5. Packet Header -- Generic Data Transfer, Revision 2.2.15
enum tApplePIODMAGenericPacket
{

    kApplePIODMAGenericPacketBufferAddressMask = APIODMABitRange64(0, 35),

    // Word 3 Register definitions (All offsets are based on the word)
    kApplePIODMAGenericPacketWord3TransferSize      = APIODMABitRange32(0, 31),
    kApplePIODMAGenericPacketWord3TransferSizePhase = APIODMABitRangePhase(0, 31),

    // Word 2 Register definitions (All offsets are based on the word)
    kApplePIODMAGenericPacketWord2TargetAddrOffset      = APIODMABitRange32(0, 31),
    kApplePIODMAGenericPacketWord2TargetAddrOffsetPhase = APIODMABitRangePhase(0, 31),

    // Word 1 Register definitions (All offsets are based on the word)
    kApplePIODMAGenericPacketWord1TargetAddrOffset      = APIODMABitRange32(28, 31),
    kApplePIODMAGenericPacketWord1TargetAddrOffsetWidth = 4,
    kApplePIODMAGenericPacketWord1TargetAddrOffsetPhase = APIODMABitRangePhase(28, 31),
    kApplePIODMAGenericPacketWord1BufferAddrOffset      = APIODMABitRange32(0, 27),
    kApplePIODMAGenericPacketWord1BufferAddrOffsetPhase = APIODMABitRangePhase(0, 27),

    // Word 0 Register definitions (All offsets are based on the word)
    kApplePIODMAGenericPacketWord0BufferAddrOffset      = APIODMABitRange32(24, 31),
    kApplePIODMAGenericPacketWord0BufferAddrOffsetWidth = 8,
    kApplePIODMAGenericPacketWord0BufferAddrOffsetPhase = APIODMABitRangePhase(24, 31),
    kApplePIODMAGenericPacketWord0TargetBARSel          = APIODMABitRange32(21, 23),
    kApplePIODMAGenericPacketWord0TargetBARSelPhase     = APIODMABitRangePhase(21, 23),
    kApplePIODMAGenericPacketWord0BufferBarSel          = APIODMABitRange32(18, 20),
    kApplePIODMAGenericPacketWord0BufferBarSelPhase     = APIODMABitRangePhase(18, 20),
    kApplePIODMAGenericPacketWord0TargetDeviceType      = APIODMABit(17),
    kApplePIODMAGenericPacketWord0TargetDeviceTypePhase = APIODMABitRangePhase(17, 17),
    kApplePIODMAGenericPacketWord0BufferDeviceType      = APIODMABit(16),
    kApplePIODMAGenericPacketWord0BufferDeviceTypePhase = APIODMABitRangePhase(16, 16),
    kApplePIODMAGenericPacketWord0TargetBurstType       = APIODMABit(15),
    kApplePIODMAGenericPacketWord0TargetBurstTypePhase  = APIODMABitRangePhase(15, 15),
    kApplePIODMAGenericPacketWord0BufferBurstType       = APIODMABit(14),
    kApplePIODMAGenericPacketWord0BufferBurstTypePhase  = APIODMABitRangePhase(14, 14),
    kApplePIODMAGenericPacketWord0Rsvd                  = APIODMABitRange32(4, 13),
    kApplePIODMAGenericPacketWord0RsvdPhase             = APIODMABitRangePhase(4, 13),
    kApplePIODMAGenericPacketWord0ExtendedType          = APIODMABitRange32(2, 3),
    kApplePIODMAGenericPacketWord0ExtendedTypePhase     = APIODMABitRangePhase(2, 3),
    kApplePIODMAGenericPacketWord0Type                  = APIODMABitRange32(0, 1),
    kApplePIODMAGenericPacketWord0TypePhase             = APIODMABitRangePhase(0, 1),

    kApplePIODMAGenericPacketWord0DeviceTypeMemoryValue = 0,
    kApplePIODMAGenericPacketWord0DeviceTypePIOValue    = 1,

    kApplePIODMAGenericPacketWord0ExtendedTypeValue = 1,
    kApplePIODMAGenericPacketWord0TypeValue         = 3,

    kApplePIODMAGenericPacketWord0TargetBurstTypeNonIncrementingValue = 0,
    kApplePIODMAGenericPacketWord0TargetBurstTypeIncrementingValue    = 1,
};

enum tApplePIODMAGenericPacketDeviceType
{
    kApplePIODMAGenericPacketDeviceTypeMemory = 0,
    kApplePIODMAGenericPacketDeviceTypePIO    = 1
};

typedef struct ApplePIODMAGenericPacket
{
    uint32_t word0;
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
} __attribute__((packed)) ApplePIODMAGenericPacket;




#endif /* _IOKIT_ApplePIODMADefinitions_H */
