/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_SCSI_CMDS_INQUIRY_H_
#define _IOKIT_SCSI_CMDS_INQUIRY_H_


// This file contains all the definitions for the data returned from
// the INQUIRY (0x12) command.


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#if KERNEL
#include <IOKit/IOTypes.h>
#else
#include <CoreFoundation/CoreFoundation.h>
#endif


#if 0
#pragma mark ¥ INQUIRY Default Page 0 Definitions
#pragma mark -
#endif

// Definitions for sizes related to the INQUIRY data
enum
{
	kINQUIRY_StandardDataHeaderSize			= 5,
	kINQUIRY_MaximumDataSize				= 255	
};

// Sizes for some of the inquiry data fields
enum
{
	kINQUIRY_VENDOR_IDENTIFICATION_Length	= 8,
	kINQUIRY_PRODUCT_IDENTIFICATION_Length	= 16,
	kINQUIRY_PRODUCT_REVISION_LEVEL_Length	= 4
};

// This structure defines the format of the required standard data that is 
// returned for the INQUIRY command.  This is the data that is required to
// be returned from all devices.
typedef struct SCSICmd_INQUIRY_StandardData
{
	UInt8		PERIPHERAL_DEVICE_TYPE;				// 7-5 = Qualifier. 4-0 = Device type.
	UInt8		RMB;								// 7 = removable
	UInt8		VERSION;							// 7/6 = ISO/IEC, 5-3 = ECMA, 2-0 = ANSI.
	UInt8		RESPONSE_DATA_FORMAT;				// 7 = AERC, 6 = Obsolete, 5 = NormACA, 4 = HiSup 3-0 = Response data format. (SPC-3 obsoletes AERC)
													// If ANSI Version = 0, this is ATAPI and bits 7-4 = ATAPI version.
	UInt8		ADDITIONAL_LENGTH;					// Number of additional bytes available in inquiry data
	UInt8		SCCSReserved;						// SCC-2 device flag and reserved fields (SPC-3 adds PROTECT 3PC TPGS, and ACC)
	UInt8		flags1;								// First byte of support flags (See SPC-3 section 6.4.2)
	UInt8		flags2;								// Second byte of support flags (Byte 7) (See SPC-3 section 6.4.2)
	char		VENDOR_IDENTIFICATION[kINQUIRY_VENDOR_IDENTIFICATION_Length];
	char		PRODUCT_IDENTIFICATION[kINQUIRY_PRODUCT_IDENTIFICATION_Length];
	char		PRODUCT_REVISION_LEVEL[kINQUIRY_PRODUCT_REVISION_LEVEL_Length];
} SCSICmd_INQUIRY_StandardData;
typedef SCSICmd_INQUIRY_StandardData * SCSICmd_INQUIRY_StandardDataPtr;


// This structure defines the all of the fields that can be returned in
// repsonse to the INQUIRy request for the standard data.  There is no
// requirement as to how much of the additional data must be returned by a device.
typedef struct SCSICmd_INQUIRY_StandardDataAll
{
	UInt8		PERIPHERAL_DEVICE_TYPE;				// 7-5 = Qualifier. 4-0 = Device type.
	UInt8		RMB;								// 7 = removable
	UInt8		VERSION;							// 7/6 = ISO/IEC, 5-3 = ECMA, 2-0 = ANSI.
	UInt8		RESPONSE_DATA_FORMAT;				// 7 = AERC, 6 = Obsolete, 5 = NormACA, 4 = HiSup 3-0 = Response data format.
													// If ANSI Version = 0, this is ATAPI and bits 7-4 = ATAPI version.
	UInt8		ADDITIONAL_LENGTH;					// Number of additional bytes available in inquiry data
	UInt8		SCCSReserved;						// SCC-2 device flag and reserved fields
	UInt8		flags1;								// First byte of support flags (Byte 6)
	UInt8		flags2;								// Second byte of support flags (Byte 7)
	char		VENDOR_IDENTIFICATION[kINQUIRY_VENDOR_IDENTIFICATION_Length];
	char		PRODUCT_IDENTIFICATION[kINQUIRY_PRODUCT_IDENTIFICATION_Length];
	char		PRODUCT_REVISION_LEVEL[kINQUIRY_PRODUCT_REVISION_LEVEL_Length];
	
	// Following is the optional data that may be returned by a device.
	UInt8		VendorSpecific1[20];
	UInt8		flags3;								// Third byte of support flags, mainly SPI-3 (Byte 56)
	UInt8		Reserved1;
	UInt16		VERSION_DESCRIPTOR[8];
	UInt8		Reserved2[22];
	UInt8		VendorSpecific2[160];
} SCSICmd_INQUIRY_StandardDataAll;

#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Byte 0 Definitions
#pragma mark -
#endif

// Inquiry Peripheral Qualifier
enum
{
	kINQUIRY_PERIPHERAL_QUALIFIER_Connected					= 0x00,
	kINQUIRY_PERIPHERAL_QUALIFIER_SupportedButNotConnected	= 0x20,
	kINQUIRY_PERIPHERAL_QUALIFIER_NotSupported				= 0x60,
	kINQUIRY_PERIPHERAL_QUALIFIER_Mask						= 0xE0
};

// Inquiry Peripheral Device types
enum
{
	kINQUIRY_PERIPHERAL_TYPE_DirectAccessSBCDevice				= 0x00,
	kINQUIRY_PERIPHERAL_TYPE_SequentialAccessSSCDevice			= 0x01,
	kINQUIRY_PERIPHERAL_TYPE_PrinterSSCDevice					= 0x02,
	kINQUIRY_PERIPHERAL_TYPE_ProcessorSPCDevice					= 0x03,
	kINQUIRY_PERIPHERAL_TYPE_WriteOnceSBCDevice					= 0x04,
	kINQUIRY_PERIPHERAL_TYPE_CDROM_MMCDevice					= 0x05,
	kINQUIRY_PERIPHERAL_TYPE_ScannerSCSI2Device					= 0x06,
	kINQUIRY_PERIPHERAL_TYPE_OpticalMemorySBCDevice				= 0x07,
	kINQUIRY_PERIPHERAL_TYPE_MediumChangerSMCDevice				= 0x08,
	kINQUIRY_PERIPHERAL_TYPE_CommunicationsSSCDevice			= 0x09,
	/* 0x0A - 0x0B ASC IT8 Graphic Arts Prepress Devices */
	kINQUIRY_PERIPHERAL_TYPE_StorageArrayControllerSCC2Device	= 0x0C,
	kINQUIRY_PERIPHERAL_TYPE_EnclosureServicesSESDevice			= 0x0D,
	kINQUIRY_PERIPHERAL_TYPE_SimplifiedDirectAccessRBCDevice	= 0x0E,
	kINQUIRY_PERIPHERAL_TYPE_OpticalCardReaderOCRWDevice		= 0x0F,
	/* 0x10 - 0x1E Reserved Device Types */
	kINQUIRY_PERIPHERAL_TYPE_ObjectBasedStorageDevice			= 0x11,
	kINQUIRY_PERIPHERAL_TYPE_AutomationDriveInterface			= 0x12,
	kINQUIRY_PERIPHERAL_TYPE_WellKnownLogicalUnit				= 0x1E,
	kINQUIRY_PERIPHERAL_TYPE_UnknownOrNoDeviceType				= 0x1F,
	
	
	kINQUIRY_PERIPHERAL_TYPE_Mask								= 0x1F
};


#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Byte 1 Definitions
#pragma mark -
#endif

// Inquiry Removable Bit field definitions
enum
{
	kINQUIRY_PERIPHERAL_RMB_MediumFixed 						= 0x00,
	kINQUIRY_PERIPHERAL_RMB_MediumRemovable 					= 0x80,
	kINQUIRY_PERIPHERAL_RMB_BitMask 							= 0x80
};


#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Byte 2 Definitions
#pragma mark -
#endif

// Inquiry ISO/IEC Version field definitions
enum
{
	kINQUIRY_ISO_IEC_VERSION_Mask								= 0xC0
};

// Inquiry ECMA Version field definitions
enum
{
	kINQUIRY_ECMA_VERSION_Mask									= 0x38
};

// Inquiry ANSI Version field definitions
enum
{
	kINQUIRY_ANSI_VERSION_NoClaimedConformance					= 0x00,
	kINQUIRY_ANSI_VERSION_SCSI_1_Compliant						= 0x01,
	kINQUIRY_ANSI_VERSION_SCSI_2_Compliant						= 0x02,
	kINQUIRY_ANSI_VERSION_SCSI_SPC_Compliant					= 0x03,
	kINQUIRY_ANSI_VERSION_SCSI_SPC_2_Compliant					= 0x04,
	kINQUIRY_ANSI_VERSION_SCSI_SPC_3_Compliant					= 0x05,
	kINQUIRY_ANSI_VERSION_Mask									= 0x07
};


#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Byte 3 Definitions
#pragma mark -
#endif

enum
{
	// Bit definitions
	// Bits 0-3: RESPONSE DATA FORMAT
	kINQUIRY_Byte3_HISUP_Bit				= 4,
	kINQUIRY_Byte3_NORMACA_Bit				= 5,
	// Bit 6 is Obsolete
	kINQUIRY_Byte3_AERC_Bit					= 7,
	
	// Masks
	kINQUIRY_RESPONSE_DATA_FORMAT_Mask		= 0x0F, // Bits 0-3
	kINQUIRY_Byte3_HISUP_Mask				= (1 << kINQUIRY_Byte3_HISUP_Bit),
	kINQUIRY_Byte3_NORMACA_Mask				= (1 << kINQUIRY_Byte3_NORMACA_Bit),
	// Bit 6 is Obsolete
	kINQUIRY_Byte3_AERC_Mask				= (1 << kINQUIRY_Byte3_AERC_Bit)
};


#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Byte 5 Definitions
#pragma mark -
#endif

enum
{
	// Bit definitions
	kINQUIRY_Byte5_SCCS_Bit					= 7,
	kINQUIRY_Byte5_ACC_Bit					= 6,
	// Bits 4-5: TPGS
	kINQUIRY_Byte5_3PC_Bit					= 3,
	// Bits 1-2: Reserved
	kINQUIRY_Byte5_PROTECT_Bit				= 0,
	
	// Masks
	kINQUIRY_Byte5_SCCS_Mask				= (1 << kINQUIRY_Byte5_SCCS_Bit),
	kINQUIRY_Byte5_ACC_Mask					= (1 << kINQUIRY_Byte5_ACC_Bit),
	kINQUIRY_Byte5_TPGS_Mask				= 0x18,
	kINQUIRY_Byte5_3PC_Mask					= (1 << kINQUIRY_Byte5_3PC_Bit),
	// Bits 1-2: Reserved
	kINQUIRY_Byte5_PROTECT_Mask				= (1 << kINQUIRY_Byte5_PROTECT_Bit)
};


#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Byte 6 Definitions
#pragma mark -
#endif

// Inquiry Byte 6 features (flags1 field)
enum
{
	// Byte offset
	kINQUIRY_Byte6_Offset					= 6,
	
	// Bit definitions
	kINQUIRY_Byte6_ADDR16_Bit				= 0,	// SPI Specific
	// Bit 1 is Obsolete
	// Bit 2 is Obsolete
	kINQUIRY_Byte6_MCHNGR_Bit				= 3,
	kINQUIRY_Byte6_MULTIP_Bit				= 4,
	kINQUIRY_Byte6_VS_Bit					= 5,	
	kINQUIRY_Byte6_ENCSERV_Bit				= 6,	
	kINQUIRY_Byte6_BQUE_Bit					= 7,	

	// Masks
	kINQUIRY_Byte6_ADDR16_Mask				= (1 << kINQUIRY_Byte6_ADDR16_Bit),	// SPI Specific
	// Bit 1 is Obsolete
	// Bit 2 is Obsolete
	kINQUIRY_Byte6_MCHNGR_Mask				= (1 << kINQUIRY_Byte6_MCHNGR_Bit),
	kINQUIRY_Byte6_MULTIP_Mask				= (1 << kINQUIRY_Byte6_MULTIP_Bit),
	kINQUIRY_Byte6_VS_Mask					= (1 << kINQUIRY_Byte6_VS_Bit),
	kINQUIRY_Byte6_ENCSERV_Mask				= (1 << kINQUIRY_Byte6_ENCSERV_Bit),
	kINQUIRY_Byte6_BQUE_Mask				= (1 << kINQUIRY_Byte6_BQUE_Bit)
};


#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Byte 7 Definitions
#pragma mark -
#endif

// Inquiry Byte 7 features (flags2 field)
enum
{
	// Byte offset
	kINQUIRY_Byte7_Offset					= 7,
	
	// Bit definitions
	kINQUIRY_Byte7_VS_Bit					= 0,
	kINQUIRY_Byte7_CMDQUE_Bit				= 1,
	kINQUIRY_Byte7_TRANDIS_Bit				= 2,	// SPI Specific
	kINQUIRY_Byte7_LINKED_Bit				= 3,
	kINQUIRY_Byte7_SYNC_Bit					= 4,	// SPI Specific
	kINQUIRY_Byte7_WBUS16_Bit				= 5,	// SPI Specific
	// Bit 6 is Obsolete
	kINQUIRY_Byte7_RELADR_Bit				= 7,
	
	// Masks
	kINQUIRY_Byte7_VS_Mask					= (1 << kINQUIRY_Byte7_VS_Bit),
	kINQUIRY_Byte7_CMDQUE_Mask				= (1 << kINQUIRY_Byte7_CMDQUE_Bit),
	kINQUIRY_Byte7_TRANDIS_Mask				= (1 << kINQUIRY_Byte7_TRANDIS_Bit),// SPI Specific
	kINQUIRY_Byte7_LINKED_Mask				= (1 << kINQUIRY_Byte7_LINKED_Bit),
	kINQUIRY_Byte7_SYNC_Mask				= (1 << kINQUIRY_Byte7_SYNC_Bit),	// SPI Specific
	kINQUIRY_Byte7_WBUS16_Mask				= (1 << kINQUIRY_Byte7_WBUS16_Bit),	// SPI Specific
	// Bit 6 is Obsolete
	kINQUIRY_Byte7_RELADR_Mask				= (1 << kINQUIRY_Byte7_RELADR_Bit)
};


#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Byte 56 Definitions
#pragma mark -
#endif

// Inquiry Byte 56 features (for devices that report an ANSI VERSION of
// kINQUIRY_ANSI_VERSION_SCSI_SPC_Compliant or later)
// These are SPI-3 Specific
enum
{
	// Byte offset
	kINQUIRY_Byte56_Offset					= 56,

	// Bit definitions
	kINQUIRY_Byte56_IUS_Bit					= 0,
	kINQUIRY_Byte56_QAS_Bit					= 1,
	// Bits 2 and 3 are the CLOCKING bits
	// All other bits are reserved
	
	kINQUIRY_Byte56_IUS_Mask				= (1 << kINQUIRY_Byte56_IUS_Bit),
	kINQUIRY_Byte56_QAS_Mask				= (1 << kINQUIRY_Byte56_QAS_Bit),
	kINQUIRY_Byte56_CLOCKING_Mask			= 0x0C,

	// Definitions for the CLOCKING bits
	kINQUIRY_Byte56_CLOCKING_ONLY_ST		= 0x00,
	kINQUIRY_Byte56_CLOCKING_ONLY_DT		= 0x04,
	// kINQUIRY_Byte56_CLOCKING_RESERVED	= 0x08,
	kINQUIRY_Byte56_CLOCKING_ST_AND_DT		= 0x0C
};

// IORegistry property names for information derived from the Inquiry data.
// The Peripheral Device Type is the only property that the 
// generic Logical Unit Drivers will use to match.
#define kIOPropertySCSIPeripheralDeviceType		"Peripheral Device Type"

// The bit size of the property
#define kIOPropertySCSIPeripheralDeviceTypeSize	8

// These properties are listed in order of matching priority.
#define kIOPropertySCSIVendorIdentification		"Vendor Identification"
#define kIOPropertySCSIProductIdentification	"Product Identification"
#define kIOPropertySCSIProductRevisionLevel		"Product Revision Level"


// Interesting page codes for INQUIRY command
enum
{
	kINQUIRY_Page00_PageCode				= 0x00,
	kINQUIRY_Page80_PageCode				= 0x80,
	kINQUIRY_Page83_PageCode				= 0x83
};	


#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Supported Vital Products Page 00 Definitions
#pragma mark -
#endif

// This section contians all structures and definitions used by the INQUIRY
// command in response to a request for page 83h - Device Identification Page 
typedef struct SCSICmd_INQUIRY_Page00_Header
{
	UInt8		PERIPHERAL_DEVICE_TYPE;				// 7-5 = Qualifier. 4-0 = Device type.
	UInt8		PAGE_CODE;							// Must be equal to 00h
	UInt8		RESERVED;							// reserved field
	UInt8		PAGE_LENGTH;						// n-3 bytes
} SCSICmd_INQUIRY_Page00_Header;

#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Device ID Page 80 Definitions
#pragma mark -
#endif


typedef struct SCSICmd_INQUIRY_Page80_Header
{
	UInt8		PERIPHERAL_DEVICE_TYPE;				// 7-5 = Qualifier. 4-0 = Device type.
	UInt8		PAGE_CODE;							// Must be equal to 80h
	UInt8		RESERVED;							// reserved field
	UInt8		PAGE_LENGTH;						// n-3 bytes
	UInt8		PRODUCT_SERIAL_NUMBER;				// 4-n
} SCSICmd_INQUIRY_Page80_Header;


#define kIOPropertySCSIINQUIRYUnitSerialNumber		"INQUIRY Unit Serial Number"


#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Device ID Page 83 Definitions
#pragma mark -
#endif

// This section contians all structures and definitions used by the INQUIRY
// command in response to a request for page 83h - Device Identification Page 

typedef struct SCSICmd_INQUIRY_Page83_Header
{
	UInt8		PERIPHERAL_DEVICE_TYPE;				// 7-5 = Qualifier. 4-0 = Device type.
	UInt8		PAGE_CODE;							// Must be equal to 83h
	UInt8		RESERVED;							// reserved field
	UInt8		PAGE_LENGTH;						// n-3 bytes
} SCSICmd_INQUIRY_Page83_Header;

typedef struct SCSICmd_INQUIRY_Page83_Header_SPC_16
{
	UInt8		PERIPHERAL_DEVICE_TYPE;				// 7-5 = Qualifier. 4-0 = Device type.
	UInt8		PAGE_CODE;							// Must be equal to 83h
	UInt16		PAGE_LENGTH;						// n-3 bytes
} SCSICmd_INQUIRY_Page83_Header_SPC_16;

typedef struct SCSICmd_INQUIRY_Page83_Identification_Descriptor
{
	UInt8		CODE_SET;							// 7-4 = Protocol Identifier. 3-0 = Code Set
	UInt8		IDENTIFIER_TYPE;					// 7 = PIV 5-4 = ASSOCIATION 3-0 = Identifier
	UInt8		RESERVED;							
	UInt8		IDENTIFIER_LENGTH;
	UInt8		IDENTIFIER;
} SCSICmd_INQUIRY_Page83_Identification_Descriptor;

// Definitions for the Code Set field
enum
{
	kINQUIRY_Page83_CodeSetReserved			= 0x0,
	kINQUIRY_Page83_CodeSetBinaryData		= 0x1,
	kINQUIRY_Page83_CodeSetASCIIData		= 0x2,
	kINQUIRY_Page83_CodeSetUTF8Data			= 0x3,
	// 0x3 - 0xF

	kINQUIRY_Page83_CodeSetMask				= 0xF
};	
		
// Definitions for the Association field
enum
{
	// Association is related to a target or logical unit
	kINQUIRY_Page83_AssociationDevice 		= 0x00,
	
	// SPC-3 - Association is changed to be specific to 
	// Logical Units
	kINQUIRY_Page83_AssociationLogicalUnit	= 0x00,
	
	// Assocition is related to a Target Port
	kINQUIRY_Page83_AssociationTargetPort	= 0x10,
	
	// SPC-3 - Added as specific association to
	// a Target device.
	kINQUIRY_Page83_AssociationTargetDevice	= 0x20,

	kINQUIRY_Page83_AssociationMask			= 0x30
};	
		
// Definitions for the Identifier type field
enum
{
	kINQUIRY_Page83_IdentifierTypeUndefined					= 0x0,
	kINQUIRY_Page83_IdentifierTypeVendorID					= 0x1,
	kINQUIRY_Page83_IdentifierTypeIEEE_EUI64				= 0x2,
	kINQUIRY_Page83_IdentifierTypeFCNameIdentifier			= 0x3,
	kINQUIRY_Page83_IdentifierTypeRelativePortIdentifier	= 0x4,
	kINQUIRY_Page83_IdentifierTypeTargetPortGroup			= 0x5,
	kINQUIRY_Page83_IdentifierTypeLogicalUnitGroup			= 0x6,
	kINQUIRY_Page83_IdentifierTypeMD5LogicalUnitIdentifier  = 0x7,
	kINQUIRY_Page83_IdentifierTypeSCSINameString			= 0x8,
	// 0x5 - 0xF

	kINQUIRY_Page83_IdentifierTypeMask						= 0xF
};	

#define kIOPropertySCSIINQUIRYDeviceIdentification		"INQUIRY Device Identification"
#define kIOPropertySCSIINQUIRYDeviceIdCodeSet			"Code Set"
#define kIOPropertySCSIINQUIRYDeviceIdType				"Identifier Type"
#define kIOPropertySCSIINQUIRYDeviceIdAssociation		"Association"
#define kIOPropertySCSIINQUIRYDeviceIdentifier			"Identifier"
		

#endif	/* _IOKIT_SCSI_CMDS_INQUIRY_H_ */