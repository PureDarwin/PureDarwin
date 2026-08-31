/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#ifndef _SCSI_COMMAND_OPERATION_CODES_H_
#define _SCSI_COMMAND_OPERATION_CODES_H_

#pragma mark About this file
/* This file contains the operation code definitions for all commands defined
 * by the SCSI specifications.  The commands are listed in three formats:
 * 1) All commands are listed in alphabetical order.  This list is the live
 * enumeration for all of the command constants.
 * 2) The commands are listed in ascending numerical order.
 * 3) The commands are grouped by Peripheral Device Type.
 * 
 * In the command listings by Peripheral Device Type, there will be a comment
 * following each command.  This comment indentifies the section of the related
 * specification where the commands is defined and the requirement type of the
 * command, Mandatory or Optional.
 * If a specification redefines an optional command from SPC as mandatory,
 * the command will be relisted in the Peripheral Device Type command list with
 * the mandatory tag next to it.
 * All commands that are listed in SPC as Device Type Specifc will be relisted
 * as a comment in all specifications lists that support that command with the
 * appropriate Mandatory or Optional tag for that specification.
 *
 * The section number and the requirement type of the command are based on the 
 * version of the specification listed in the header comment for the Peripheral
 * Device Type.  This data is provided for informational purposes only.  The 
 * specification document and version that the device adheres to as indicated 
 * by the data returned in response to the INQUIRY command should be used as
 * the authorative source for supported and required behavior of the device.
 * 
 * The SPC set is listed before all other Peripheral Device Type commands as
 * this is the base document from which all of the other documents are derived.  
 *
 * The Peripheral Device Types and associated command sets as defined by SPC-2,
 * section 7.4.1 are as follows:
 *  Peripheral Device Type                  Associated Command Specification
 *  ------------------------------------    -----------------------------------
 *  0x00 Direct Access Device               SBC - SCSI-3 Block Commands
 *  0x01 Sequential Access Device           SSC - SCSI-3 Stream Commands
 *  0x02 Printer Device                     SSC - SCSI-3 Stream Commands
 *  0x03 Processor Device                   SPC - SCSI Primary Commands-2
 *  0x04 Write Once Device                  SBC - SCSI-3 Block Commands
 *  0x05 CD-ROM Device                      MMC - SCSI Multimedia Commands-2
 *  0x06 Scanner Device                     SGC - SCSI-3 Graphics Commands
 *  0x07 Optical Memory Device              SBC - SCSI-3 Block Commands
 *  0x08 Medium Changer Device              SMC - SCSI-3 Medium Changer Cmds
 *  0x09 Communications Device              SSC - SCSI-3 Stream Commands
 *  0x0A - 0x0B Graphic Arts Prepress Dev   ASC IT8
 *  0x0C Storage Array Controller Device    SCC-2 - SCSI Controller Commands-2
 *  0x0D Enclosure Services                 SES - SCSI-3 Enclosure Services
 *  0x0E Simplified Direct Access Device    RBC - SCSI Reduced Block Commands
 *  0x0F Optical Card Reader/Writer Device  OCRW - SCSI Specification for 
 *                                              Optical Card Reader/Writer
 *  0x10 Reserved                    		No command specification
 *	0x11 Object-Based Storage Device		OSD - SCSI Object Based Storage 
 *												Device Commands
 *  0x12 - 0x14 Reserved                    No command specification
 *  0x15 Multimedia Media Access Engine     RMC - Reduced Multimedia Commands
 *  0x16 - 0x1E Reserved                    No command specification
 *  0x1F Unknown or No Device               No command specification
 */ 

#pragma mark -
#pragma mark Command Definitions by Name
/* All SCSI Commands listed in alphabetical order.  These are the live
 * definitions of the commands.  All other command lists are informative.
 */
enum 
{
    kSCSICmd_ACCESS_CONTROL_IN              = 0x86,
    kSCSICmd_ACCESS_CONTROL_OUT             = 0x87,
    kSCSICmd_BLANK                          = 0xA1,
    kSCSICmd_CHANGE_DEFINITION              = 0x40,
    kSCSICmd_CLOSE_TRACK_SESSION            = 0x5B,
    kSCSICmd_COMPARE                        = 0x39,
    kSCSICmd_COPY                           = 0x18,
    kSCSICmd_COPY_AND_VERIFY                = 0x3A,
    kSCSICmd_ERASE_10						= 0x2C,
    kSCSICmd_ERASE_12						= 0xAC,
    kSCSICmd_EXTENDED_COPY                  = 0x83,
    kSCSICmd_FORMAT_UNIT                    = 0x04,
    kSCSICmd_GET_CONFIGURATION              = 0x46,
    kSCSICmd_GET_EVENT_STATUS_NOTIFICATION  = 0x4A,
    kSCSICmd_GET_PERFORMANCE                = 0xAC,
    kSCSICmd_INQUIRY                        = 0x12,
    kSCSICmd_LOAD_UNLOAD_MEDIUM             = 0xA6,
    kSCSICmd_LOCK_UNLOCK_CACHE              = 0x36,
    kSCSICmd_LOCK_UNLOCK_CACHE_16           = 0x92,
    kSCSICmd_LOG_SELECT                     = 0x4C,
    kSCSICmd_LOG_SENSE                      = 0x4D,
    kSCSICmd_MAINTENANCE_IN               	= 0xA3,
    kSCSICmd_MAINTENANCE_OUT                = 0xA4,
    kSCSICmd_MECHANISM_STATUS               = 0xBD,
    kSCSICmd_MEDIUM_SCAN                    = 0x38,
    kSCSICmd_MODE_SELECT_6                  = 0x15,
    kSCSICmd_MODE_SELECT_10                 = 0x55,
    kSCSICmd_MODE_SENSE_6                   = 0x1A,
    kSCSICmd_MODE_SENSE_10                  = 0x5A,
    kSCSICmd_MOVE_MEDIUM_ATTACHED           = 0xA7,
    kSCSICmd_PAUSE_RESUME                   = 0x4B,
    kSCSICmd_PERSISTENT_RESERVE_IN          = 0x5E,
    kSCSICmd_PERSISTENT_RESERVE_OUT         = 0x5F,
    kSCSICmd_PLAY_AUDIO_10                  = 0x45,
    kSCSICmd_PLAY_AUDIO_12                  = 0xA5,
    kSCSICmd_PLAY_AUDIO_MSF                 = 0x47,
    kSCSICmd_PLAY_AUDIO_TRACK_INDEX         = 0x48,
    kSCSICmd_PLAY_CD                        = 0xBC,
    kSCSICmd_PLAY_RELATIVE_10               = 0x49,
    kSCSICmd_PLAY_RELATIVE_12               = 0xA9,
    kSCSICmd_PREFETCH                       = 0x34,
    kSCSICmd_PREFETCH_16                    = 0x90,
    kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL   = 0x1E,
    kSCSICmd_READ_6                         = 0x08,
    kSCSICmd_READ_10                        = 0x28,
    kSCSICmd_READ_12                        = 0xA8,
    kSCSICmd_READ_16                        = 0x88,
    kSCSICmd_READ_ATTRIBUTE                 = 0x8C,
    kSCSICmd_READ_BUFFER                    = 0x3C,
    kSCSICmd_READ_BUFFER_CAPACITY           = 0x5C,
    kSCSICmd_READ_CAPACITY                  = 0x25,
    kSCSICmd_READ_CD                        = 0xBE,
    kSCSICmd_READ_CD_MSF                    = 0xB9,
    kSCSICmd_READ_DEFECT_DATA_10            = 0x37,
    kSCSICmd_READ_DEFECT_DATA_12            = 0xB7,
    kSCSICmd_READ_DISC_INFORMATION          = 0x51,
    kSCSICmd_READ_DVD_STRUCTURE             = 0xAD,
    kSCSICmd_READ_ELEMENT_STATUS_ATTACHED   = 0xB4,
    kSCSICmd_READ_FORMAT_CAPACITIES         = 0x23,
    kSCSICmd_READ_GENERATION				= 0x29,
    kSCSICmd_READ_HEADER                    = 0x44,
    kSCSICmd_READ_LONG                      = 0x3E,
    kSCSICmd_READ_MASTER_CUE                = 0x59,
    kSCSICmd_READ_SUB_CHANNEL               = 0x42,
    kSCSICmd_READ_TOC_PMA_ATIP              = 0x43,
    kSCSICmd_READ_TRACK_INFORMATION         = 0x52,
    kSCSICmd_READ_UPDATED_BLOCK_10			= 0x2D,
    kSCSICmd_REASSIGN_BLOCKS                = 0x07,
    kSCSICmd_REBUILD                        = 0x81,
    kSCSICmd_RECEIVE                        = 0x08,
    kSCSICmd_RECEIVE_COPY_RESULTS           = 0x84,
    kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS    = 0x1C,
    kSCSICmd_REDUNDANCY_GROUP_IN			= 0xBA,
    kSCSICmd_REDUNDANCY_GROUP_OUT			= 0xBB,
    kSCSICmd_REGENERATE                     = 0x82,
    kSCSICmd_RELEASE_6                      = 0x17,
    kSCSICmd_RELEASE_10                     = 0x57,
    kSCSICmd_REPAIR_TRACK                   = 0x58,
    kSCSICmd_REPORT_DEVICE_IDENTIFIER    	= 0xA3,
    kSCSICmd_REPORT_KEY                     = 0xA4,
    kSCSICmd_REPORT_LUNS                    = 0xA0, 
    kSCSICmd_REQUEST_SENSE                  = 0x03,
    kSCSICmd_RESERVE_6                      = 0x16,
    kSCSICmd_RESERVE_10                     = 0x56,
    kSCSICmd_RESERVE_TRACK                  = 0x53,
    kSCSICmd_REZERO_UNIT                    = 0x01,
    kSCSICmd_SCAN_MMC                       = 0xBA,
    kSCSICmd_SEARCH_DATA_EQUAL_10           = 0x31,
    kSCSICmd_SEARCH_DATA_EQUAL_12           = 0xB1,
    kSCSICmd_SEARCH_DATA_HIGH_10            = 0x30,
    kSCSICmd_SEARCH_DATA_HIGH_12            = 0xB0,
    kSCSICmd_SEARCH_DATA_LOW_10             = 0x32,
    kSCSICmd_SEARCH_DATA_LOW_12             = 0xB2,
    kSCSICmd_SEEK_6                         = 0x0B,
    kSCSICmd_SEEK_10                        = 0x2B,
    kSCSICmd_SEND                           = 0x0A,
    kSCSICmd_SEND_CUE_SHEET                 = 0x5D,
    kSCSICmd_SEND_DIAGNOSTICS               = 0x1D,
    kSCSICmd_SEND_DVD_STRUCTURE             = 0xBF,
    kSCSICmd_SEND_EVENT                     = 0xA2,
    kSCSICmd_SEND_KEY                       = 0xA3,
    kSCSICmd_SEND_OPC_INFORMATION           = 0x54,
    kSCSICmd_SERVICE_ACTION_IN		        = 0x9E,
    kSCSICmd_SERVICE_ACTION_OUT		        = 0x9F,
    kSCSICmd_SET_CD_SPEED                   = 0xBB,
    kSCSICmd_SET_DEVICE_IDENTIFIER      	= 0xA4,
    kSCSICmd_SET_LIMITS_10                  = 0x33,
    kSCSICmd_SET_LIMITS_12                  = 0xB3,
    kSCSICmd_SET_READ_AHEAD                 = 0xA7,
    kSCSICmd_SET_STREAMING                  = 0xB6,
    kSCSICmd_SPARE_IN		                = 0xBC,
    kSCSICmd_SPARE_OUT		                = 0xBD,
    kSCSICmd_START_STOP_UNIT                = 0x1B,
    kSCSICmd_STOP_PLAY_SCAN                 = 0x4E,
    kSCSICmd_SYNCHRONIZE_CACHE              = 0x35,
    kSCSICmd_SYNCHRONIZE_CACHE_16           = 0x91,
    kSCSICmd_TEST_UNIT_READY                = 0x00,
	kSCSICmd_UPDATE_BLOCK					= 0x3D,
    kSCSICmd_VERIFY_10                      = 0x2F,
    kSCSICmd_VERIFY_12                      = 0xAF,
    kSCSICmd_VERIFY_16                      = 0x8F,
    kSCSICmd_VOLUME_SET_IN                  = 0xBE,
    kSCSICmd_VOLUME_SET_OUT                 = 0xBF,
    kSCSICmd_WRITE_6                        = 0x0A,
    kSCSICmd_WRITE_10                       = 0x2A,
    kSCSICmd_WRITE_12                       = 0xAA,
    kSCSICmd_WRITE_16                       = 0x8A,
    kSCSICmd_WRITE_AND_VERIFY_10            = 0x2E,
	kSCSICmd_WRITE_AND_VERIFY_12            = 0xAE,
	kSCSICmd_WRITE_AND_VERIFY_16            = 0x8E,
	kSCSICmd_WRITE_ATTRIBUTE	            = 0x8D,
    kSCSICmd_WRITE_BUFFER                   = 0x3B,
    kSCSICmd_WRITE_LONG                     = 0x3F,
    kSCSICmd_WRITE_SAME                     = 0x41,
    kSCSICmd_WRITE_SAME_16                  = 0x93,
    kSCSICmd_XDREAD                         = 0x52,
    kSCSICmd_XDWRITE                        = 0x50,
    kSCSICmd_XDWRITE_EXTENDED               = 0x80,
    kSCSICmd_XDWRITEREAD_10           		= 0x53,
    kSCSICmd_XPWRITE                        = 0x51,
    
    kSCSICmdVariableLengthCDB				= 0x7F
};

// Service Action Definitions for the Variable Length CDB (7Fh) command
enum
{
	kSCSIServiceAction_READ_32				= 0x0009,
	kSCSIServiceAction_VERIFY_32			= 0x000A,
	kSCSIServiceAction_WRITE_32				= 0x000B,
	kSCSIServiceAction_WRITE_AND_VERIFY_32	= 0x000C,
	kSCSIServiceAction_WRITE_SAME_32		= 0x000D,
	kSCSIServiceAction_XDREAD_32			= 0x0003,
	kSCSIServiceAction_XDWRITE_32			= 0x0004,
	kSCSIServiceAction_XDWRITEREAD_32		= 0x0007,
	kSCSIServiceAction_XPWRITE_32			= 0x0006
};

// Service Action Definitions for the MAINTENANCE IN (A3h) command
enum
{
	kSCSIServiceAction_REPORT_ALIASES				= 0x0B,
	kSCSIServiceAction_REPORT_DEVICE_IDENTIFIER		= 0x05,
	kSCSIServiceAction_REPORT_PRIORITY				= 0x0E,
	kSCSIServiceAction_REPORT_SUPPORTED_OPERATION_CODES				= 0x0C,
	kSCSIServiceAction_REPORT_SUPPORTED_TASK_MANAGEMENT_FUNCTIONS	= 0x0D,
	kSCSIServiceAction_REPORT_TARGET_PORT_GROUPS	= 0x0A	
};

// Service Action Definitions for the MAINTENANCE OUT (A4h) command
enum
{
	kSCSIServiceAction_CHANGE_ALIASES			= 0x0B,
	kSCSIServiceAction_SET_DEVICE_IDENTIFIER	= 0x06,
	kSCSIServiceAction_SET_PRIORITY				= 0x0E,
	kSCSIServiceAction_SET_TARGET_PORT_GROUPS	= 0x0A	
};

// Service Action Definitions for the SERVICE ACTION IN (9Eh) command
enum
{
	kSCSIServiceAction_READ_CAPACITY_16		= 0x10,
	kSCSIServiceAction_READ_LONG_16			= 0x11	
};

// Service Action Definitions for the SERVICE ACTION OUT (9Fh) command
enum
{
	kSCSIServiceAction_WRITE_LONG_16		= 0x11	
};

#pragma mark -
#pragma mark Command Definitions by Number
#if 0
enum 
{
};
#endif


#pragma mark -
#pragma mark All Types SPC Commands
/* Commands defined by the T10:1236-D SCSI Primary Commands-2 (SPC-2) 
 * command specification.  The definitions and section numbers are based on
 * section 7 of the revision 18, 21 May 2000 version of the specification.
 *
 * These commands are defined for all devices.
 */
 // Needs to be updated for SPC-3
#if 0
enum 
{
    kSCSICmd_CHANGE_DEFINITION              = 0x40, /* Obsolete */
    kSCSICmd_COMPARE                        = 0x39, /* Sec. 7.2: Optional */
    kSCSICmd_COPY                           = 0x18, /* Sec. 7.3: Optional */
    kSCSICmd_COPY_AND_VERIFY                = 0x3A, /* Sec. 7.4: Optional */
    kSCSICmd_EXTENDED_COPY                  = 0x83, /* Sec. 7.5: Optional */
    kSCSICmd_INQUIRY                        = 0x12, /* Sec. 7.6: Mandatory */
    kSCSICmd_LOG_SELECT                     = 0x4C, /* Sec. 7.7: Optional */
    kSCSICmd_LOG_SENSE                      = 0x4D, /* Sec. 7.8: Optional */
    kSCSICmd_MODE_SELECT_6                  = 0x15, /* Sec. 7.9: Device Type
                                                     * Specific */
    kSCSICmd_MODE_SELECT_10                 = 0x55, /* Sec. 7.10: Device Type
                                                     * Specific */
    kSCSICmd_MODE_SENSE_6                   = 0x1A, /* Sec. 7.11: Device Type
                                                     * Specific */
    kSCSICmd_MODE_SENSE_10                  = 0x5A, /* Sec. 7.12: Device Type
                                                     * Specific */
    kSCSICmd_MOVE_MEDIUM_ATTACHED           = 0xA7, /* Defined in SMC */
    kSCSICmd_PERSISTENT_RESERVE_IN          = 0x5E, /* Sec. 7.13: Device Type
                                                     * Specific */
    kSCSICmd_PERSISTENT_RESERVE_OUT         = 0x5F, /* Sec. 7.14: Device Type
                                                     * Specific */
    kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL   = 0x1E, /* Sec. 7.15: Device Type
                                                     * Specific */
    kSCSICmd_READ_BUFFER                    = 0x3C, /* Sec. 7.16: Optional */
    kSCSICmd_READ_ELEMENT_STATUS_ATTACHED   = 0xB4, /* Defined in SMC */
    kSCSICmd_RECEIVE_COPY_RESULTS           = 0x84, /* Sec. 7.17: Optional */
    kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS    = 0x1C, /* Sec. 7.18: Optional */
    kSCSICmd_RELEASE_10                     = 0x57, /* Sec. 7.19: Device Type
                                                     * Specific */
    kSCSICmd_RELEASE_6                      = 0x17, /* Sec. 7.20: Device Type
                                                     * Specific */
    kSCSICmd_REPORT_DEVICE_IDENTIFIER    	= 0xA3, /* Sec. 7.21: Optional */
    kSCSICmd_REPORT_LUNS                    = 0xA0, /* Sec. 7.22: Mandatory for
                                                     * LUN Supporting devices*/
    kSCSICmd_REQUEST_SENSE                  = 0x03, /* Sec. 7.23: Device Type
                                                     * Specific */
    kSCSICmd_RESERVE_10                     = 0x56, /* Sec. 7.24: Device Type
                                                     * Specific */
    kSCSICmd_RESERVE_6                      = 0x16, /* Sec. 7.25: Device Type
                                                     * Specific */
    kSCSICmd_SEND_DIAGNOSTICS               = 0x1D, /* Sec. 7.26: Optional */
    kSCSICmd_SET_DEVICE_IDENTIFIER       	= 0xA4, /* Sec. 7.27: Optional */
    kSCSICmd_TEST_UNIT_READY                = 0x00, /* Sec. 7.28: Mandatory */
    kSCSICmd_WRITE_BUFFER                   = 0x3B  /* Sec. 7.29: Optional */
};
#endif

#pragma mark -
#pragma mark 0x00 SBC Direct Access Commands
/* Commands defined by the T10:990-D SCSI-3 Block Commands (SBC) command
 * specification.  The definitions and section numbers are based on section 6.1
 * of the revision 8c, 13 November 1997 version of the specification. 
 */
 // Needs to be updated for SBC-2
#if 0
enum
{
    kSCSICmd_CHANGE_DEFINITION              = 0x40, /* Obsolete */
    kSCSICmd_COMPARE                        = 0x39, /* SPC: Optional */
    kSCSICmd_COPY                           = 0x18, /* SPC: Optional */
    kSCSICmd_COPY_AND_VERIFY                = 0x3A, /* SPC: Optional*/
    kSCSICmd_FORMAT_UNIT                    = 0x04, /* Sec. 6.1.1: Mandatory */
    kSCSICmd_INQUIRY                        = 0x12, /* SPC: Mandatory */
    kSCSICmd_LOCK_UNLOCK_CACHE              = 0x36, /* Sec. 6.1.2: Optional */
    kSCSICmd_LOG_SELECT                     = 0x4C, /* SPC: Optional */
    kSCSICmd_LOG_SENSE                      = 0x4D, /* SPC: Optional */
    kSCSICmd_MODE_SELECT_6                  = 0x15, /* SPC: Optional */
    kSCSICmd_MODE_SELECT_10                 = 0x55, /* SPC: Optional */
    kSCSICmd_MODE_SENSE_6                   = 0x1A, /* SPC: Optional */
    kSCSICmd_MODE_SENSE_10                  = 0x5A, /* SPC: Optional */
    kSCSICmd_MOVE_MEDIUM_ATTACHED           = 0xA7, /* SMC: Optional */
    kSCSICmd_PERSISTENT_RESERVE_IN          = 0x5E, /* SPC: Optional */
    kSCSICmd_PERSISTENT_RESERVE_OUT         = 0x5F, /* SPC: Optional */
    kSCSICmd_PREFETCH                       = 0x34, /* Sec. 6.1.3: Optional */
    kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL   = 0x1E, /* SPC: Optional */
    kSCSICmd_READ_6                         = 0x08, /* Sec. 6.1.4: Mandatory */
    kSCSICmd_READ_10                        = 0x28, /* Sec. 6.1.5: Mandatory */
    kSCSICmd_READ_12                        = 0xA8, /* Sec. 6.2.4: Optional */
    kSCSICmd_READ_BUFFER                    = 0x3C, /* SPC: Optional */
    kSCSICmd_READ_CAPACITY                  = 0x25, /* Sec. 6.1.6: Mandatory */
    kSCSICmd_READ_DEFECT_DATA_10            = 0x37, /* Sec. 6.1.7: Optional */
    kSCSICmd_READ_DEFECT_DATA_12            = 0xB7, /* Sec. 6.2.5: Optional */
    kSCSICmd_READ_ELEMENT_STATUS_ATTACHED   = 0xB4, /* SMC: Optional */
    kSCSICmd_READ_GENERATION				= 0x29,	/* Sec. 6.2.6: Optional */
    kSCSICmd_READ_LONG                      = 0x3E, /* Sec. 6.1.8: Optional */
    kSCSICmd_READ_UPDATED_BLOCK_10			= 0x2D, /* Sec. 6.2.7: Optional */
    kSCSICmd_REASSIGN_BLOCKS                = 0x07, /* Sec. 6.1.9: Optional */
    kSCSICmd_REBUILD                        = 0x81, /* Sec. 6.1.10: Optional */
    kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS    = 0x1C, /* SPC: Optional */
    kSCSICmd_REGENERATE                     = 0x82, /* Sec. 6.1.11: Optional */
    kSCSICmd_RELEASE_6                      = 0x17, /* SPC: Optional */
    kSCSICmd_RELEASE_10                     = 0x57, /* SPC: Mandatory */
    kSCSICmd_REPORT_LUNS                    = 0xA0, /* SPC: Optional */
    kSCSICmd_REQUEST_SENSE                  = 0x03, /* SPC: Mandatory */
    kSCSICmd_RESERVE_6                      = 0x16, /* SPC: Optional */
    kSCSICmd_RESERVE_10                     = 0x56, /* SPC: Mandatory */
    kSCSICmd_REZERO_UNIT                    = 0x01, /* Obsolete */
    kSCSICmd_SEARCH_DATA_EQUAL_10           = 0x31, /* Obsolete */
    kSCSICmd_SEARCH_DATA_HIGH_10            = 0x30, /* Obsolete */
    kSCSICmd_SEARCH_DATA_LOW_10             = 0x32, /* Obsolete */
    kSCSICmd_SEEK_6                         = 0x0B, /* Obsolete */
    kSCSICmd_SEEK_10                        = 0x2B, /* Sec. 6.1.12: Optional */
    kSCSICmd_SEND_DIAGNOSTICS               = 0x1D, /* SPC: Mandatory */
    kSCSICmd_SET_LIMITS_10                  = 0x33, /* Sec. 6.1.13: Optional */
    kSCSICmd_SET_LIMITS_12                  = 0xB3, /* Sec. 6.2.8: Optional */
    kSCSICmd_START_STOP_UNIT                = 0x1B, /* Sec. 6.1.14: Optional */
    kSCSICmd_SYNCHRONIZE_CACHE              = 0x35, /* Sec. 6.1.15: Optional */
    kSCSICmd_TEST_UNIT_READY                = 0x00, /* SPC: Mandatory */
	kSCSICmd_UPDATE_BLOCK					= 0x3D,	/* Sec. 6.2.9: Optional */
    kSCSICmd_VERIFY_10                      = 0x2F, /* Sec. 6.1.16: Optional */
    kSCSICmd_WRITE_6                        = 0x0A, /* Sec. 6.1.17: Optional */
    kSCSICmd_WRITE_10                       = 0x2A, /* Sec. 6.1.18: Optional */
    kSCSICmd_WRITE_12                       = 0xAA, /* Sec. 6.2.13: Optional */
    kSCSICmd_WRITE_AND_VERIFY_10            = 0x2E, /* Sec. 6.1.19: Optional */
    kSCSICmd_WRITE_AND_VERIFY_12            = 0xAE, /* Sec. 6.2.15: Optional */
    kSCSICmd_WRITE_BUFFER                   = 0x3B, /* SPC: Optional */
    kSCSICmd_WRITE_LONG                     = 0x3F, /* Sec. 6.1.20: Optional */
    kSCSICmd_WRITE_SAME                     = 0x41, /* Sec. 6.1.21: Optional */
    kSCSICmd_XDREAD                         = 0x52, /* Sec. 6.1.22: Optional */
    kSCSICmd_XDWRITE                        = 0x50, /* Sec. 6.1.23: Optional */
    kSCSICmd_XDWRITE_EXTENDED               = 0x80, /* Sec. 6.1.24: Optional */
    kSCSICmd_XPWRITE                        = 0x51  /* Sec. 6.1.25: Optional */
};
#endif

#pragma mark -
#pragma mark 0x01 SSC Sequential Access Commands
/* Commands defined by the T10:997-D SCSI-3 Stream Commands (SSC) command
 * specification.  The definitions and section numbers are based on section 5
 * of the revision 22, January 1, 2000 version of the specification. 
 */
#if 0
enum
{
    kSCSICmd_CHANGE_DEFINITION              = 0x40, /* Obsolete */
    kSCSICmd_COMPARE                        = 0x39, /* SPC: Optional */
    kSCSICmd_COPY                           = 0x18, /* SPC: Optional */
    kSCSICmd_COPY_AND_VERIFY                = 0x3A, /* SPC: Optional */
    kSCSICmd_ERASE                          = 0x19, /* Sec. 5.3.1: Mandatory */
    kSCSICmd_FORMAT_MEDIUM                  = 0x04, /* Sec. 5.3.2: Optional */
    kSCSICmd_INQUIRY                        = 0x12, /* SPC: Mandatory */
    kSCSICmd_LOAD_UNLOAD                    = 0x1B, /* Sec. 5.3.3: Optional */
    kSCSICmd_LOCATE                         = 0x2B, /* Sec. 5.3.4: Optional */
    kSCSICmd_LOG_SELECT                     = 0x4C, /* SPC: Optional */
    kSCSICmd_LOG_SENSE                      = 0x4D, /* SPC: Optional */
    kSCSICmd_MODE_SELECT_6                  = 0x15, /* SPC: Mandatory */
    kSCSICmd_MODE_SELECT_10                 = 0x55, /* SPC: Optional */
    kSCSICmd_MODE_SENSE_6                   = 0x1A, /* SPC: Mandatory */
    kSCSICmd_MODE_SENSE_10                  = 0x5A, /* SPC: Optional */
    kSCSICmd_MOVE_MEDIUM                    = 0xA5, /* SMC: Optional */
    kSCSICmd_MOVE_MEDIUM_ATTACHED           = 0xA7, /* SMC: Optional */
    kSCSICmd_PERSISTENT_RESERVE_IN          = 0x5E, /* SPC: Optional */
    kSCSICmd_PERSISTENT_RESERVE_OUT         = 0x5F, /* SPC: Optional */
    kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL   = 0x1E, /* SPC: Optional */
    kSCSICmd_READ_6                         = 0x08, /* Sec. 5.3.5: Mandatory */
    kSCSICmd_READ_BLOCK_LIMITS              = 0x05, /* Sec. 5.3.6: Mandatory */
    kSCSICmd_READ_BUFFER                    = 0x3C, /* SPC: Optional */
    kSCSICmd_READ_ELEMENT_STATUS            = 0xB8, /* SMC: Optional */
    kSCSICmd_READ_ELEMENT_STATUS_ATTACHED   = 0xB4, /* SMC: Optional */
    kSCSICmd_READ_POSITION                  = 0x34, /* Sec. 5.3.7: Mandatory */
    kSCSICmd_READ_REVERSE                   = 0x0F, /* Sec. 5.3.8: Optional */
    kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS    = 0x1C, /* SPC: Optional */
    kSCSICmd_RECOVER_BUFFERED_DATA          = 0x14, /* Sec. 5.3.9: Optional */
    kSCSICmd_RELEASE_6                      = 0x17, /* SPC: Mandatory */
    kSCSICmd_RELEASE_10                     = 0x57, /* SPC: Mandatory */
    kSCSICmd_REPORT_DENSITY_SUPPORT         = 0x44, /* Sec. 5.3.10: Mandatory*/
    kSCSICmd_REPORT_LUNS                    = 0xA0, /* SPC: Mandatory */
    kSCSICmd_REQUEST_SENSE                  = 0x03, /* SPC: Mandatory */
    kSCSICmd_RESERVE_6                      = 0x16, /* SPC: Mandatory */
    kSCSICmd_RESERVE_10                     = 0x56, /* SPC: Mandatory */
    kSCSICmd_REWIND                         = 0x01, /* Sec. 5.3.11: Mandatory*/
    kSCSICmd_SEND_DIAGNOSTICS               = 0x1D, /* SPC: Mandatory */
    kSCSICmd_SPACE                          = 0x11, /* Sec. 5.3.12: Mandatory*/
    kSCSICmd_TEST_UNIT_READY                = 0x00, /* SPC: Mandatory */
    kSCSICmd_VERIFY_6                       = 0x13, /* Sec. 5.3.13: Optional */
    kSCSICmd_WRITE_6                        = 0x0A, /* Sec. 5.3.14: Mandatory*/
    kSCSICmd_WRITE_BUFFER                   = 0x3B, /* SPC: Optional */
    kSCSICmd_WRITE_FILEMARKS                = 0x10  /* Sec. 5.3.15: Mandatory*/
};
#endif

#pragma mark -
#pragma mark 0x02 SSC Printer Commands
/* Commands defined by the T10:997-D SCSI-3 Stream Commands (SSC) command
 * specification.  The definitions and section numbers are based on section 6
 * of the revision 22, January 1, 2000 version of the specification. 
 */
#if 0
enum 
{
    kSCSICmd_CHANGE_DEFINITION              = 0x40, /* Obsolete */
    kSCSICmd_COMPARE                        = 0x39, /* SPC: Optional */
    kSCSICmd_COPY                           = 0x18, /* SPC: Optional */
    kSCSICmd_COPY_AND_VERIFY                = 0x3A, /* SPC: Optional */
    kSCSICmd_FORMAT                         = 0x04, /* Sec. 6.2.1: Optional */
    kSCSICmd_INQUIRY                        = 0x12, /* SPC: Mandatory */
    kSCSICmd_LOG_SELECT                     = 0x4C, /* SPC: Optional */
    kSCSICmd_LOG_SENSE                      = 0x4D, /* SPC: Optional */
    kSCSICmd_MODE_SELECT_6                  = 0x15, /* SPC: Mandatory */
    kSCSICmd_MODE_SELECT_10                 = 0x55, /* SPC: Optional */
    kSCSICmd_MODE_SENSE_6                   = 0x1A, /* SPC: Mandatory */
    kSCSICmd_MODE_SENSE_10                  = 0x5A, /* SPC: Optional */
    kSCSICmd_PERSISTENT_RESERVE_IN          = 0x5E, /* SPC: Optional */
    kSCSICmd_PERSISTENT_RESERVE_OUT         = 0x5F, /* SPC: Optional */
    kSCSICmd_PRINT                          = 0x0A, /* Sec. 6.2.2: Mandatory */
    kSCSICmd_READ_BUFFER                    = 0x3C, /* SPC: Optional */
    kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS    = 0x1C, /* SPC: Optional */
    kSCSICmd_RECOVER_BUFFERED_DATA          = 0x14, /* Sec. 6.2.3: Optional */
    kSCSICmd_RELEASE_6                      = 0x17, /* SPC: Mandatory */
    kSCSICmd_RELEASE_10                     = 0x57, /* SPC: Mandatory */
    kSCSICmd_REPORT_LUNS                    = 0xA0, /* SPC: Mandatory */
    kSCSICmd_REQUEST_SENSE                  = 0x03, /* SPC: Mandatory */
    kSCSICmd_RESERVE_6                      = 0x16, /* SPC: Mandatory */
    kSCSICmd_RESERVE_10                     = 0x56, /* SPC: Mandatory */
    kSCSICmd_SEND_DIAGNOSTICS               = 0x1D, /* SPC: Mandatory */
    kSCSICmd_SLEW_AND_PRINT                 = 0x0B, /* Sec. 6.2.4: Optional */
    kSCSICmd_STOP_PRINT                     = 0x1B, /* Sec. 6.2.5: Optional */
    kSCSICmd_SYNCHRONIZE_BUFFER             = 0x10, /* Sec. 6.2.6: Optional */
    kSCSICmd_TEST_UNIT_READY                = 0x00, /* SPC: Mandatory */
    kSCSICmd_WRITE_BUFFER                   = 0x3B  /* SPC: Optional */
};
#endif
  
#pragma mark -
#pragma mark 0x03 SPC Processor Commands
/* Commands defined by the T10:1236-D SCSI Primary Commands-2 (SPC-2) 
 * command specification.  The definitions and section numbers are based on
 * section 9 of the revision 18, 21 May 2000 version of the specification.
 */
#if 0
enum 
{
    kSCSICmd_CHANGE_DEFINITION              = 0x40, /* Obsolete */
    kSCSICmd_COMPARE                        = 0x39, /* Sec. 7.2: Optional */
    kSCSICmd_COPY                           = 0x18, /* Sec. 7.3: Optional */
    kSCSICmd_COPY_AND_VERIFY                = 0x3A, /* Sec. 7.4: Optional */
    kSCSICmd_EXTENDED_COPY                  = 0x83, /* Sec. 7.5: Optional */
    kSCSICmd_INQUIRY                        = 0x12, /* Sec. 7.6: Mandatory */
    kSCSICmd_LOG_SELECT                     = 0x4C, /* Sec. 7.7: Optional */
    kSCSICmd_LOG_SENSE                      = 0x4D, /* Sec. 7.8: Optional */
    kSCSICmd_PERSISTENT_RESERVE_IN          = 0x5E, /* Sec. 7.13: Optional */
    kSCSICmd_PERSISTENT_RESERVE_OUT         = 0x5F, /* Sec. 7.14: Optional */
    kSCSICmd_READ_BUFFER                    = 0x3C, /* Sec. 7.16: Optional */
    kSCSICmd_RECEIVE                        = 0x08, /* Sec. 9.2: Optional */
    kSCSICmd_RECEIVE_COPY_RESULTS           = 0x84, /* Sec. 7.17: Optional */
    kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS    = 0x1C, /* Sec. 7.18: Optional */
    kSCSICmd_RELEASE_10                     = 0x57, /* Sec. 7.19: Optional */
    kSCSICmd_RELEASE_6                      = 0x17, /* Sec. 7.20: Optional */
    kSCSICmd_REPORT_LUNS                    = 0xA0, /* Sec. 7.22: Optional */
    kSCSICmd_REQUEST_SENSE                  = 0x03, /* Sec. 7.23: Mandatory */
    kSCSICmd_RESERVE_10                     = 0x56, /* Sec. 7.24: Optional */
    kSCSICmd_RESERVE_6                      = 0x16, /* Sec. 7.25: Optional */
    kSCSICmd_SEND                           = 0x0A, /* Sec. 9.3: Optional */
    kSCSICmd_SEND_DIAGNOSTICS               = 0x1D, /* Sec. 7.26: Mandatory */
    kSCSICmd_TEST_UNIT_READY                = 0x00, /* Sec. 7.27: Mandatory */
    kSCSICmd_WRITE_BUFFER                   = 0x3B  /* Sec. 7.29: Optional */
};
#endif

#pragma mark -
#pragma mark 0x04 SBC Write Once Commands
/* Commands defined by the T10:990-D SCSI-3 Block Commands (SBC) command
 * specification.  The definitions and section numbers are based on section 6.3
 * of the revision 8c, 13 November 1997 version of the specification. 
 */
#if 0
enum
{
    kSCSICmd_CHANGE_DEFINITION              = 0x40, /* SPC: Optional */
    kSCSICmd_COMPARE                        = 0x39, /* SPC: Optional */
    kSCSICmd_COPY                           = 0x18, /* SPC: Optional */
    kSCSICmd_COPY_AND_VERIFY                = 0x3A, /* SPC: Optional*/
    kSCSICmd_INQUIRY                        = 0x12, /* SPC: Mandatory */
    kSCSICmd_LOCK_UNLOCK_CACHE              = 0x36, /* Sec. 6.1.2: Optional */
    kSCSICmd_LOG_SELECT                     = 0x4C, /* SPC: Optional */
    kSCSICmd_LOG_SENSE                      = 0x4D, /* SPC: Optional */
    kSCSICmd_MEDIUM_SCAN                    = 0x38, /* Sec. 6.2.3: Optional */
    kSCSICmd_MODE_SELECT_6                  = 0x15, /* SPC: Optional */
    kSCSICmd_MODE_SELECT_10                 = 0x55, /* SPC: Optional */
    kSCSICmd_MODE_SENSE_6                   = 0x1A, /* SPC: Optional */
    kSCSICmd_MODE_SENSE_10                  = 0x5A, /* SPC: Optional */
    kSCSICmd_MOVE_MEDIUM                    = 0xA5, /* SMC: Optional */
    kSCSICmd_PERSISTENT_RESERVE_IN          = 0x5E, /* SPC: Optional */
    kSCSICmd_PERSISTENT_RESERVE_OUT         = 0x5F, /* SPC: Optional */
    kSCSICmd_PREFETCH                       = 0x34, /* Sec. 6.1.3: Optional */
    kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL   = 0x1E, /* SPC: Optional */
    kSCSICmd_READ_6                         = 0x08, /* Sec. 6.1.4: Optional */
    kSCSICmd_READ_10                        = 0x28, /* Sec. 6.1.5: Mandatory */
    kSCSICmd_READ_12                        = 0xA8, /* Sec. 6.2.4: Optional */
    kSCSICmd_READ_BUFFER                    = 0x3C, /* SPC: Optional */
    kSCSICmd_READ_CAPACITY                  = 0x25, /* Sec. 6.1.6: Mandatory */
    kSCSICmd_READ_ELEMENT_STATUS            = 0xB8, /* SMC: Optional */
    kSCSICmd_READ_LONG                      = 0x3E, /* Sec. 6.1.8: Optional */
    kSCSICmd_REASSIGN_BLOCKS                = 0x07, /* Sec. 6.1.9: Optional */
    kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS    = 0x1C, /* SPC: Optional */
    kSCSICmd_RELEASE_6                      = 0x17, /* SPC: Optional */
    kSCSICmd_RELEASE_10                     = 0x57, /* SPC: Mandatory */
    kSCSICmd_REQUEST_SENSE                  = 0x03, /* SPC: Mandatory */
    kSCSICmd_RESERVE_6                      = 0x16, /* SPC: Optional */
    kSCSICmd_RESERVE_10                     = 0x56, /* SPC: Mandatory */
    kSCSICmd_REZERO_UNIT                    = 0x01, /* Obsolete */
    kSCSICmd_SEARCH_DATA_EQUAL_10           = 0x31, /* Obsolete */
    kSCSICmd_SEARCH_DATA_EQUAL_12           = 0xB1, /* Obsolete */
    kSCSICmd_SEARCH_DATA_HIGH_10            = 0x30, /* Obsolete */
    kSCSICmd_SEARCH_DATA_HIGH_12            = 0xB0, /* Obsolete */
    kSCSICmd_SEARCH_DATA_LOW_10             = 0x32, /* Obsolete */
    kSCSICmd_SEARCH_DATA_LOW_12             = 0xB2, /* Obsolete */
    kSCSICmd_SEEK_6                         = 0x0B, /* Obsolete */
    kSCSICmd_SEEK_10                        = 0x2B, /* Sec. 6.1.12: Optional */
    kSCSICmd_SEND_DIAGNOSTICS               = 0x1D, /* SPC: Mandatory */
    kSCSICmd_SET_LIMITS_10                  = 0x33, /* Sec. 6.1.13: Optional */
    kSCSICmd_SET_LIMITS_12                  = 0xB3, /* Sec. 6.2.8: Optional */
    kSCSICmd_START_STOP_UNIT                = 0x1B, /* Sec. 6.1.14: Optional */
    kSCSICmd_SYNCHRONIZE_CACHE              = 0x35, /* Sec. 6.1.15: Optional */
    kSCSICmd_TEST_UNIT_READY                = 0x00, /* SPC: Mandatory */
    kSCSICmd_VERIFY_10                      = 0x2F, /* Sec. 6.2.10: Optional */
    kSCSICmd_VERIFY_12                      = 0xAF, /* Sec. 6.2.11: Optional */
    kSCSICmd_WRITE_6                        = 0x0A, /* Sec. 6.1.17: Optional */
    kSCSICmd_WRITE_10                       = 0x2A, /* Sec. 6.2.10: Mandatory*/
    kSCSICmd_WRITE_12                       = 0xAA, /* Sec. 6.2.13: Optional */
    kSCSICmd_WRITE_AND_VERIFY_10            = 0x2E, /* Sec. 6.2.14: Optional */
    kSCSICmd_WRITE_AND_VERIFY_12            = 0xAE, /* Sec. 6.2.15: Optional */
    kSCSICmd_WRITE_BUFFER                   = 0x3B, /* SPC: Optional */
    kSCSICmd_WRITE_LONG                     = 0x3F  /* Sec. 6.1.20: Optional */
};
#endif

#pragma mark -
#pragma mark 0x05 MMC CD-ROM Commands
/* Commands defined by the T10:1363-D SCSI Multimedia Commands-3 (MMC-3) 
 * specification.  The definitions and section numbers are based on section 6.1
 * of the revision 01, March 03, 2000 version of the specification. 
 *
 * NOTE: The comments following each command may not be accurate.  These are
 * not from the MMC-3 specification, but have been derived from the SCSI-2 and
 * original MMC specifications.  Unlike the other SCSI command specifications,
 * MMC-2 and MMC-3 do not provide a command requirement type and therefore does
 * not relist the SPC commands with these requirements as they apply to MMC
 * devices.  The MMC-2 and MMC-3 specifications also refer back to the SBC
 * specification which seems invalid since MMC devices do not represent a
 * Peripheral Device Type defined by SBC.  It is assumed that the SBC
 * references refer to the Peripheral Device Type 0x00 - Direct Access Commands
 * definitions from that specification.
 */
#if 0
enum 
{
    kSCSICmd_BLANK                          = 0xA1, /* Sec. 6.1.1: */
    kSCSICmd_CHANGE_DEFINITION              = 0x40, /* Obsolete */
    kSCSICmd_CLOSE_TRACK_SESSION            = 0x5B, /* Sec. 6.1.2: */
    kSCSICmd_COMPARE                        = 0x39, /* SPC: Optional */
    kSCSICmd_COPY                           = 0x18, /* SPC: Optional */
    kSCSICmd_COPY_AND_VERIFY                = 0x3A, /* SPC: Optional */
    kSCSICmd_ERASE                          = 0x2C, /* SBC: */
    kSCSICmd_FORMAT_UNIT                    = 0x04, /* Sec. 6.1.3: */
    kSCSICmd_GET_CONFIGURATION              = 0x46, /* Sec. 6.1.4: */
    kSCSICmd_GET_EVENT_STATUS_NOTIFICATION  = 0x4A, /* Sec. 6.1.5: */
    kSCSICmd_GET_PERFORMANCE                = 0xAC, /* Sec. 6.1.6: */
    kSCSICmd_INQUIRY                        = 0x12, /* SPC: Mandatory */
    kSCSICmd_LOAD_UNLOAD_MEDIUM             = 0xA6, /* Sec. 6.1.7: */
    kSCSICmd_LOG_SELECT                     = 0x4C, /* SPC: Optional */
    kSCSICmd_LOG_SENSE                      = 0x4D, /* SPC: Optional */
    kSCSICmd_MECHANISM_STATUS               = 0xBD, /* Sec. 6.1.8: */
    kSCSICmd_MODE_SELECT_6                  = 0x15, /* SPC: Mandatory */
    kSCSICmd_MODE_SELECT_10                 = 0x55, /* SPC: Mandatory */
    kSCSICmd_MODE_SENSE_6                   = 0x1A, /* SPC: Mandatory */
    kSCSICmd_MODE_SENSE_10                  = 0x5A, /* SPC: Mandatory */
    kSCSICmd_PAUSE_RESUME                   = 0x4B, /* Sec. 6.1.9: */
    kSCSICmd_PLAY_AUDIO_10                  = 0x45, /* Sec. 6.1.10: */
    kSCSICmd_PLAY_AUDIO_12                  = 0xA5, /* Sec. 6.1.11: */
    kSCSICmd_PLAY_AUDIO_MSF                 = 0x47, /* Sec. 6.1.12: */
    kSCSICmd_PLAY_AUDIO_TRACK_INDEX         = 0x48, /* Obsolete */
    kSCSICmd_PLAY_CD                        = 0xBC, /* Sec. 6.1.13: */
    kSCSICmd_PLAY_RELATIVE_10               = 0x49, /* Obsolete */
    kSCSICmd_PLAY_RELATIVE_12               = 0xA9, /* Obsolete */
    kSCSICmd_PREFETCH                       = 0x34, /* Optional */
    kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL   = 0x1E, /* Optional */
    kSCSICmd_READ_6                         = 0x08, /* Optional */
    kSCSICmd_READ_10                        = 0x28, /* Mandatory */
    kSCSICmd_READ_12                        = 0xA8, /* Optional */
    kSCSICmd_READ_BUFFER                    = 0x3C, /* Optional */
    kSCSICmd_READ_BUFFER_CAPACITY           = 0x5C, /* Sec. 6.1.15: */
    kSCSICmd_READ_CD                        = 0xBE, /* Sec. 6.1.16: */
    kSCSICmd_READ_CD_MSF                    = 0xB9, /* Sec. 6.1.17: */
    kSCSICmd_READ_CAPACITY                  = 0x25, /* Sec. 6.1.18: */
    kSCSICmd_READ_DISC_INFORMATION          = 0x51, /* Sec. 6.1.19: */
    kSCSICmd_READ_DVD_STRUCTURE             = 0xAD, /* Sec. 6.1.20: */
    kSCSICmd_READ_FORMAT_CAPACITIES         = 0x23, /* Sec. 6.1.21: */
    kSCSICmd_READ_HEADER                    = 0x44, /* Sec. 6.1.22: */
    kSCSICmd_READ_LONG                      = 0x3E, /* Optional */
    kSCSICmd_READ_MASTER_CUE                = 0x59, /* Sec. 6.1.23: */
    kSCSICmd_READ_SUB_CHANNEL               = 0x42, /* Sec. 6.1.24: */
    kSCSICmd_READ_TOC_PMA_ATIP              = 0x43, /* Sec. 6.1.25: */
    kSCSICmd_READ_TRACK_INFORMATION         = 0x52, /* Sec. 6.1.27: */
    kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS    = 0x1C, /* Optional */
    kSCSICmd_RELEASE_6                      = 0x17, /* Mandatory */
    kSCSICmd_RELEASE_10                     = 0x57, /* Optional */
    kSCSICmd_REPAIR_TRACK                   = 0x58, /* Sec. 6.1.28: */
    kSCSICmd_REPORT_KEY                     = 0xA4, /* Sec. 6.1.29: */
    kSCSICmd_REQUEST_SENSE                  = 0x03, /* Mandatory */
    kSCSICmd_RESERVE_6                      = 0x16, /* Mandatory */
    kSCSICmd_RESERVE_10                     = 0x56, /* Optional */
    kSCSICmd_RESERVE_TRACK                  = 0x53, /* Sec. 6.1.30: */
    kSCSICmd_SCAN_MMC                       = 0xBA, /* Sec. 6.1.31: */
    kSCSICmd_SEARCH_DATA_EQUAL_10           = 0x31, /* Obsolete */
    kSCSICmd_SEARCH_DATA_EQUAL_12           = 0xB1, /* Obsolete */
    kSCSICmd_SEARCH_DATA_HIGH_10            = 0x30, /* Obsolete */
    kSCSICmd_SEARCH_DATA_HIGH_12            = 0xB0, /* Obsolete */
    kSCSICmd_SEARCH_DATA_LOW_10             = 0x32, /* Obsolete */
    kSCSICmd_SEARCH_DATA_LOW_12             = 0xB2, /* Obsolete */
    kSCSICmd_SEEK_6                         = 0x0B, /* Obsolete */
    kSCSICmd_SEEK_10                        = 0x2B, /* SBC: */
    kSCSICmd_SEND_CUE_SHEET                 = 0x5D, /* Sec. 6.1.32: */
    kSCSICmd_SEND_DIAGNOSTICS               = 0x1D, /* Mandatory */
    kSCSICmd_SEND_DVD_STRUCTURE             = 0xBF, /* Sec. 6.1.33: */
    kSCSICmd_SEND_EVENT                     = 0xA2, /* Sec. 6.1.34: */
    kSCSICmd_SEND_KEY                       = 0xA3, /* Sec. 6.1.35: */
    kSCSICmd_SEND_OPC_INFORMATION           = 0x54, /* Sec. 6.1.36: */
    kSCSICmd_SET_CD_SPEED                   = 0xBB, /* Sec. 6.1.37: */
    kSCSICmd_SET_LIMITS_10                  = 0x33, /* Optional */
    kSCSICmd_SET_LIMITS_12                  = 0xB3, /* Optional */
    kSCSICmd_SET_READ_AHEAD                 = 0xA7, /* Sec. 6.1.38: */
    kSCSICmd_SET_STREAMING                  = 0xB6, /* Sec. 6.1.39: */
    kSCSICmd_START_STOP_UNIT                = 0x1B, /* Optional */
    kSCSICmd_STOP_PLAY_SCAN                 = 0x4E, /* Sec. 6.1.40: */
    kSCSICmd_SYNCHRONIZE_CACHE              = 0x35, /* Sec. 6.1.41: */
    kSCSICmd_TEST_UNIT_READY                = 0x00, /* Mandatory */
    kSCSICmd_VERIFY_10                      = 0x2F, /* Optional */
    kSCSICmd_VERIFY_12                      = 0xAF, /* Optional */
    kSCSICmd_WRITE_10                       = 0x2A, /* Sec. 6.1.42: */
    kSCSICmd_WRITE_12                       = 0xAA, /* Sec. 6.1.43: */
    kSCSICmd_WRITE_AND_VERIFY_10            = 0x2E, /* Sec. 6.1.44: */
    kSCSICmd_WRITE_BUFFER                   = 0x3B  /* Optional */
};
#endif

#pragma mark -
#pragma mark 0x06 SGC Scanner Commands
/* Commands defined by the T10:998-D SCSI-3 Graphics Commands (SGC)
 * specification.  The definitions and section numbers are based on section 6
 * of the revision 0, April 1995 version of the specification. 
 */
#if 0
enum 
{
    kSCSICmd_CHANGE_DEFINITION              = 0x40, /* SPC: Optional */
    kSCSICmd_COMPARE                        = 0x39, /* SPC: Optional */
    kSCSICmd_COPY                           = 0x18, /* SPC: Optional */
    kSCSICmd_COPY_AND_VERIFY                = 0x3A, /* SPC: Optional */
    kSCSICmd_GET_DATA_BUFFER_STATUS         = 0x34, /* Sec. 6.1.1: Optional */
    kSCSICmd_GET_WINDOW                     = 0x25, /* Sec. 6.1.2: Optional */
    kSCSICmd_INQUIRY                        = 0x12, /* SPC: Mandatory */
    kSCSICmd_LOG_SELECT                     = 0x4C, /* SPC: Optional */
    kSCSICmd_LOG_SENSE                      = 0x4D, /* SPC: Optional */
    kSCSICmd_MODE_SELECT_6                  = 0x15, /* SPC: Optional */
    kSCSICmd_MODE_SELECT_10                 = 0x55, /* SPC: Optional */
    kSCSICmd_MODE_SENSE_6                   = 0x1A, /* SPC: Optional */
    kSCSICmd_MODE_SENSE_10                  = 0x5A, /* SPC: Optional */
    kSCSICmd_OBJECT_POSITION                = 0x31, /* Sec. 6.1.3: Optional */
    kSCSICmd_PORT_STATUS                    = 0x11, /* SPC (??): Mandatory
                                                     * for dual port devices */
    kSCSICmd_READ                           = 0x28, /* Sec. 6.1.4: Mandatory */
    kSCSICmd_READ_BUFFER                    = 0x3C, /* SPC: Optional */
    kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS    = 0x1C, /* SPC: Optional */
    kSCSICmd_RELEASE_6                      = 0x17, /* SPC: Mandatory */
    kSCSICmd_REQUEST_SENSE                  = 0x03, /* SPC: Mandatory */
    kSCSICmd_RESERVE_6                      = 0x16, /* SPC: Mandatory */
    kSCSICmd_SCAN                           = 0x1B, /* Sec. 6.1.5: Optional */
    kSCSICmd_SEND                           = 0x1B, /* Sec. 6.1.6: Optional */
    kSCSICmd_SEND_DIAGNOSTICS               = 0x1D, /* SPC: Mandatory */
    kSCSICmd_SET_WINDOW                     = 0x24, /* Sec. 6.1.7: Mandatory */
    kSCSICmd_TEST_UNIT_READY                = 0x00, /* SPC: Mandatory */
    kSCSICmd_WRITE_BUFFER                   = 0x3B  /* SPC: Optional */
};
#endif

#pragma mark -
#pragma mark 0x07 SBC Optical Media Commands
/* Commands defined by the T10:990-D SCSI-3 Block Commands (SBC) 
 * (revision 8c, 13 November 1998) command specification.  
 */

#pragma mark -
#pragma mark 0x08 SMC Medium Changer Commands
/* Commands defined by the T10:1228-D SCSI-3 Medium Changer Commands-2 (SMC-2)
 * (revision 0, March 16, 2000) command specification.  
 */
#if 0
enum
{
    /* Commands For Independent Medium Changers */
    kSCSICmd_EXCHANGE_MEDIUM                = 0xA6, /* Optional */
    kSCSICmd_INITIALIZE_ELEMENT_STATUS      = 0x07, /* Optional */
    kSCSICmd_MODE_SELECT_6                  = 0x15, /* Optional */
    kSCSICmd_MODE_SELECT_10                 = 0x55, /* Optional */
    kSCSICmd_MODE_SENSE_6                   = 0x1A, /* Optional */
    kSCSICmd_MODE_SENSE_10                  = 0x5A, /* Optional */
    kSCSICmd_MOVE_MEDIUM                    = 0xA5, /* Mandatory */
    kSCSICmd_PERSISTENT_RESERVE_IN          = 0x5E, /* Optional */
    kSCSICmd_PERSISTENT_RESERVE_OUT         = 0x5F, /* Optional */
    kSCSICmd_POSITION_TO_ELEMENT            = 0x2B, /* Optional */
    kSCSICmd_READ_ELEMENT_STATUS            = 0xB8, /* Mandatory */
    kSCSICmd_RELEASE_ELEMENT_6              = 0x16, /* Optional */
    kSCSICmd_RELEASE_ELEMENT_10             = 0x56, /* Optional */
    kSCSICmd_REQUEST_VOLUME_ELEMENT_ADDRESS = 0xB5, /* Optional */
    kSCSICmd_REQUEST_SENSE                  = 0x03, /* Mandatory */
    kSCSICmd_RESERVE_ELEMENT_6              = 0x16, /* Optional */
    kSCSICmd_RESERVE_ELEMENT_10             = 0x56  /* Optional */
};
#endif

#pragma mark -
#pragma mark 0x09 SSC Communications Commands
/* Commands defined by the T10:997-D SCSI-3 Stream Commands (SSC) 
 * (revision 22, January 1, 2000) command specification. 
 */
  
#pragma mark -
#pragma mark 0x0A ASC IT8 Prepress Commands
#pragma mark 0x0B ASC IT8 Prepress Commands
/* Commands defined by the ASC IT8 <title goes here> specification
 * (revision xx, month day, year) command specification.  
 */
#if 0
enum
{
};
#endif

#pragma mark -
#pragma mark 0x0C SCC Array Controller Commands
/* Commands defined by the ANSI NCITS.318-199x SCSI Controller
 * Commands (SCC-2) ratified command specification.  
 */
#if 0
enum
{
    kSCSICmd_MAINTENANCE_IN                 = 0xA3, /* Mandatory */
    kSCSICmd_MAINTENANCE_OUT                = 0xA4, /* Optional */
    kSCSICmd_MODE_SELECT_6                  = 0x15, /* Optional */
    kSCSICmd_MODE_SELECT_10                 = 0x55, /* Optional */
    kSCSICmd_MODE_SENSE_6                   = 0x1A, /* Optional */
    kSCSICmd_MODE_SENSE_10                  = 0x5A, /* Optional */
    kSCSICmd_PERSISTENT_RESERVE_IN          = 0x5E, /* Optional */
    kSCSICmd_PERSISTENT_RESERVE_OUT         = 0x5F, /* Optional */
    kSCSICmd_PORT_STATUS                    = 0x1F, /* Optional */
    kSCSICmd_REDUNDANCY_GROUP_IN            = 0xBA, /* Mandatory */
    kSCSICmd_REDUNDANCY_GROUP_OUT           = 0xBB, /* Optional */
    kSCSICmd_RELEASE_6                      = 0x17, /* Optional */
    kSCSICmd_RELEASE_10                     = 0x57, /* Optional */
    kSCSICmd_REPORT_LUNS                    = 0xA0, /* Mandatory */
    kSCSICmd_REQUEST_SENSE                  = 0x03, /* Mandatory */
    kSCSICmd_RESERVE_6                      = 0x16, /* Optional */
    kSCSICmd_RESERVE_10                     = 0x56, /* Optional*/
    kSCSICmd_SEND_DIAGNOSTICS               = 0x1D, /* Optional */
    kSCSICmd_SPARE_IN                       = 0xBC, /* Mandatory */
    kSCSICmd_SPARE_OUT                      = 0xBD  /* Optional */
};
#endif

#pragma mark -
#pragma mark 0x0D SES Enclosure Services Commands
/* Commands defined by the T10:1212-D SCSI-3 Enclosure Services (SES) 
 * (revision 8b, February 11, 1998) command specification.  
 */
#if 0
enum
{
    kSCSICmd_MODE_SELECT_6                  = 0x15, /* Optional */
    kSCSICmd_MODE_SELECT_10                 = 0x55, /* Optional */
    kSCSICmd_MODE_SENSE_6                   = 0x1A, /* Optional */
    kSCSICmd_MODE_SENSE_10                  = 0x5A, /* Optional */
    kSCSICmd_PERSISTENT_RESERVE_IN          = 0x5E, /* Optional */
    kSCSICmd_PERSISTENT_RESERVE_OUT         = 0x5F, /* Optional */
    kSCSICmd_RECEIVE_DIAGNOSTICS_RESULTS    = 0x17, /* Mandatory */
    kSCSICmd_RELEASE_6                      = 0x17, /* Optional */
    kSCSICmd_RELEASE_10                     = 0x57, /* Optional */
    kSCSICmd_REQUEST_SENSE                  = 0x03, /* Mandatory */
    kSCSICmd_RESERVE_6                      = 0x16, /* Optional */
    kSCSICmd_RESERVE_10                     = 0x56, /* Optional */
    kSCSICmd_SEND_DIAGNOSTICS               = 0x1D  /* Mandatory */
};
#endif

#pragma mark -
#pragma mark 0x0E RBC Reduced Block Commands
/* Commands defined by the T10:1240-D Reduced Block Commands (RBC) 
 * (revision 10a, August 18, 1999) command specification.  
 */
#if 0
enum
{
    kSCSICmd_FORMAT_UNIT                    = 0x04, /* Optional */
    kSCSICmd_READ_10                        = 0x28, /* Mandatory */
    kSCSICmd_READ_CAPACITY                  = 0x25, /* Mandatory */
    kSCSICmd_START_STOP_UNIT                = 0x1B, /* Mandatory */
    kSCSICmd_SYNCHRONIZE_CACHE              = 0x35, /* Optional */
    kSCSICmd_VERIFY_10                      = 0x2F, /* Mandatory */
    kSCSICmd_WRITE_10                       = 0x2A, /* Mandatory */
    kSCSICmd_WRITE_BUFFER                   = 0x3B  /* Mandatory for fixed media
     *                                              Optional for removable */
};
#endif

#pragma mark -
#pragma mark 0x0F OCRW Optical Card Commands
/* Commands defined by the ISO/IEC 14776-381 SCSI Specification for
 * Optical Card Reader/Writer (OCRW) ratified command specification.  
 */
#if 0
enum
{
};
#endif

#pragma mark -
#pragma mark 0x11 OSD Object-based Storage Commands
/* Commands defined by the T10:1355-D Object-based Storage Commands (OSD) 
 * (revision 1, 18 May 2000) command specification.  
 */
#if 0
enum
{
};
#endif

#pragma mark -
#pragma mark 0x15 RMC Simplified Multimedia Commands
/* Commands defined by the T10:1364-D Reduced Multimedia Commands (RMC) 
 * (revision 1, November 11, 1999) command specification.  
 */
#if 0
enum
{
};
#endif

#endif	/* _SCSI_COMMAND_OPERATION_CODES_H_ */