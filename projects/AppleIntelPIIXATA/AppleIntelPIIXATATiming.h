/*
 * Copyright (c) 1998-2006 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2000 Apple Computer, Inc.  All rights reserved. 
 *
 * AppleIntelPIIXATATiming.h - PIIX ATA controller timing tables.
 *
 * HISTORY
 *
 */

#ifndef _APPLEINTELPIIXATATIMING_H
#define _APPLEINTELPIIXATATIMING_H

#include "AppleIntelPIIXATAHW.h"

/*
 * PIIX IDETIM register programming.
 */
static const UInt16 piixIDETIM[][2] =
{
/*    Unit 0   Unit 1 */
    { 0xb30f,  0xf3f0 },   /* 0. Mask                   */
    { 0x8000,  0x8000 },   /* 1. PIO0/Compatible 600 ns */
    { 0x9007,  0x9007 },   /* 2. PIO2/SW2        240 ns */
    { 0xa107,  0xa107 },   /* 3. PIO3/MW1        180 ns */
    { 0xa307,  0xa307 }    /* 4. PIO4/MW2        120 ns */
};

/*
 * PIIX3 (or better) IDETIM register programming.
 */
static const UInt16 piix3IDETIM[][2] =
{
/*    Unit 0   Unit 1 */
    { 0xb30f,  0xc0f0 },   /* 0. Mask                   */
    { 0x8000,  0xc000 },   /* 1. PIO0/Compatible 600 ns */
    { 0x9007,  0xc070 },   /* 2. PIO2/SW2        240 ns */
    { 0xa107,  0xc070 },   /* 3. PIO3/MW1        180 ns */
    { 0xa307,  0xc070 }    /* 4. PIO4/MW2        120 ns */
};

/*
 * PIIX3 (or better) SIDETIM register programming.
 */
static const UInt16 piix3SIDETIM[][2] =
{
/*    Primary  Secondary */
    { 0x0f,    0xf0 },     /* 0. Mask                   */
    { 0x00,    0x00 },     /* 1. PIO0/Compatible 600 ns */
    { 0x04,    0x40 },     /* 2. PIO2/SW2        240 ns */
    { 0x09,    0x90 },     /* 3. PIO3/MW1        180 ns */
    { 0x0b,    0xb0 }      /* 4. PIO4/MW2        120 ns */
};

/*
 * PIO/DMA timing info.
 */
typedef struct {
    UInt8  mode;           /* mode number */
    UInt8  registerIndex;  /* register program index */
    UInt16 cycleTime;      /* nsec cycle time */
} PIIXTiming;

/*
 * MultiWord DMA timing table.
 * PCI bus speed assumed to be 33MHz.
 */
static const PIIXTiming
piixDMATiming[] =
{
/*  DMA  Index Cycle */
   { 0,   1,   480 },      /* Unsupported. Do not use. */
   { 1,   3,   180 },
   { 2,   4,   120 }
};

static const UInt8
piixDMATimingCount = sizeof(piixDMATiming) / sizeof(piixDMATiming[0]);

/*
 * PIO timing table.
 * PCI bus speed assumed to be 33MHz.
 */
static const PIIXTiming
piixPIOTiming[] =
{
/*  PIO  Index Cycle */
   { 0,   1,   600 },      /* Compatible timing ISP = 6, RTC = 14 */
   { 1,   1,   383 },      /* Unsupported. Do not use. */
   { 2,   2,   240 },
   { 3,   3,   180 },
   { 4,   4,   120 }
};

static const UInt8
piixPIOTimingCount = sizeof(piixPIOTiming) / sizeof(piixPIOTiming[0]);

/*
 * PIIX Ultra-DMA timing table.
 */
typedef struct {
    UInt8    mode;         /* mode number */
    UInt8    ct;           /* cycle time in PCI clocks */
    UInt8    rp;           /* ready to pause time in PCI clocks */
    UInt8    udmatim;      /* UDMATIM register bits */
    UInt16   cycle;        /* nsec cycle period  */
    UInt16   udmaClock;    /* U-DMA clock mask for primary drive 0 */
} PIIXUDMATiming;

static const PIIXUDMATiming
piixUDMATiming[] =
{
/*  Mode    CT     RP  UDMATIM   Cycle    U-DMA Clock */
   { 0,     4,     6,     0,     120,     0 },
   { 1,     3,     5,     1,     90,      0 },
   { 2,     2,     4,     2,     60,      0 },
   { 3,     3,     8,     1,     45,      kPIIX_PCI_IDECONFIG_PCB0      },
   { 4,     2,     8,     2,     30,      kPIIX_PCI_IDECONFIG_PCB0      },
   { 5,     1,    16,     1,     20,      kPIIX_PCI_IDECONFIG_FAST_PCB0 }
};

static const UInt8
piixUDMATimingCount = sizeof(piixUDMATiming) / sizeof(piixUDMATiming[0]);

#endif /* !_APPLEINTELPIIXATATIMING_H */
