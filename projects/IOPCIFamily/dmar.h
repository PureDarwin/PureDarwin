/*
 * Copyright (c) 2012 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef struct acpi_table_header
{
    uint8_t                   Reserved[0x24];
} ACPI_TABLE_HEADER;

/*******************************************************************************
 *
 * DMAR - DMA Remapping table
 *        Version 1
 *
 * Conforms to "Intel Virtualization Technology for Directed I/O",
 * Version 1.2, Sept. 2008
 *
 ******************************************************************************/

typedef struct acpi_table_dmar
{
    ACPI_TABLE_HEADER         Header;             /* Common ACPI table header */
    uint8_t                   Width;              /* Host Address Width */
    uint8_t                   Flags;
    uint8_t                   Reserved[10];

} ACPI_TABLE_DMAR;

/* Masks for Flags field above */

#define ACPI_DMAR_INTR_REMAP        (1)


/* DMAR subtable header */

typedef struct acpi_dmar_header
{
    uint16_t                  Type;
    uint16_t                  Length;

} ACPI_DMAR_HEADER;

/* Values for subtable type in ACPI_DMAR_HEADER */

enum AcpiDmarType
{
    ACPI_DMAR_TYPE_HARDWARE_UNIT        = 0,
    ACPI_DMAR_TYPE_RESERVED_MEMORY      = 1,
    ACPI_DMAR_TYPE_ATSR                 = 2,
    ACPI_DMAR_HARDWARE_AFFINITY         = 3,
    ACPI_DMAR_TYPE_RESERVED             = 4     /* 4 and greater are reserved */
};


/* DMAR Device Scope structure */

typedef struct acpi_dmar_device_scope
{
    uint8_t                   EntryType;
    uint8_t                   Length;
    uint16_t                  Reserved;
    uint8_t                   EnumerationId;
    uint8_t                   Bus;

} ACPI_DMAR_DEVICE_SCOPE;

/* Values for EntryType in ACPI_DMAR_DEVICE_SCOPE */

enum AcpiDmarScopeType
{
    ACPI_DMAR_SCOPE_TYPE_NOT_USED       = 0,
    ACPI_DMAR_SCOPE_TYPE_ENDPOINT       = 1,
    ACPI_DMAR_SCOPE_TYPE_BRIDGE         = 2,
    ACPI_DMAR_SCOPE_TYPE_IOAPIC         = 3,
    ACPI_DMAR_SCOPE_TYPE_HPET           = 4,
    ACPI_DMAR_SCOPE_TYPE_RESERVED       = 5     /* 5 and greater are reserved */
};

typedef struct acpi_dmar_pci_path
{
    uint8_t                   Device;
    uint8_t                   Function;

} ACPI_DMAR_PCI_PATH;


/*
 * DMAR Sub-tables, correspond to Type in ACPI_DMAR_HEADER
 */

/* 0: Hardware Unit Definition */

typedef struct acpi_dmar_hardware_unit
{
    ACPI_DMAR_HEADER          Header;
    uint8_t                   Flags;
    uint8_t                   Reserved;
    uint16_t                  Segment;
    uint64_t                  Address;            /* Register Base Address */

} ACPI_DMAR_HARDWARE_UNIT;

/* Masks for Flags field above */

#define ACPI_DMAR_INCLUDE_ALL       (1)


/* 1: Reserved Memory Definition */

typedef struct acpi_dmar_reserved_memory
{
    ACPI_DMAR_HEADER          Header;
    uint16_t                  Reserved;
    uint16_t                  Segment;
    uint64_t                  BaseAddress;        /* 4K aligned base address */
    uint64_t                  EndAddress;         /* 4K aligned limit address */

} ACPI_DMAR_RESERVED_MEMORY;

/* Masks for Flags field above */

#define ACPI_DMAR_ALLOW_ALL         (1)


/* 2: Root Port ATS Capability Reporting Structure */

typedef struct acpi_dmar_atsr
{
    ACPI_DMAR_HEADER          Header;
    uint8_t                   Flags;
    uint8_t                   Reserved;
    uint16_t                  Segment;

} ACPI_DMAR_ATSR;

/* Masks for Flags field above */

#define ACPI_DMAR_ALL_PORTS         (1)


/* 3: Remapping Hardware Static Affinity Structure */

typedef struct acpi_dmar_rhsa
{
    ACPI_DMAR_HEADER          Header;
    uint32_t                  Reserved;
    uint64_t                  BaseAddress;
    uint32_t                  ProximityDomain;

} ACPI_DMAR_RHSA;

