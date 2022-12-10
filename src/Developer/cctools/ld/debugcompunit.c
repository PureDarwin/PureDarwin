/*
 * Copyright (c) 2006 Apple Computer, Inc.  All Rights Reserved.
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
#ifndef KLD
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "dwarf2.h"
#include "debugcompunit.h"

/* Read in a word of fixed size, which may be unaligned, in the
   appropriate endianness.  */
#define read_16(p) (little_endian		\
		    ? ((p)[1] << 8 | (p)[0])	\
		    : ((p)[0] << 8 | (p)[1]))
#define read_32(p) (little_endian					    \
		    ? ((p)[3] << 24 | (p)[2] << 16 | (p)[1] << 8 | (p)[0])  \
		    : ((p)[0] << 24 | (p)[1] << 16 | (p)[2] << 8 | (p)[3]))
#define read_64(p) (little_endian					    \
		    ? ((uint64_t) (p)[7] << 56 | (uint64_t) (p)[6] << 48    \
		       | (uint64_t) (p)[5] << 40 | (uint64_t) (p)[4] << 32  \
		       | (uint64_t) (p)[3] << 24 | (uint64_t) (p)[2] << 16u \
		       | (uint64_t) (p)[1] << 8 | (uint64_t) (p)[0])	    \
		    : ((uint64_t) (p)[0] << 56 | (uint64_t) (p)[1] << 48    \
		       | (uint64_t) (p)[2] << 40 | (uint64_t) (p)[3] << 32  \
		       | (uint64_t) (p)[4] << 24 | (uint64_t) (p)[5] << 16u \
		       | (uint64_t) (p)[6] << 8 | (uint64_t) (p)[7]))

/* Skip over a LEB128 value (signed or unsigned).  */
static void
skip_leb128 (const uint8_t ** offset, const uint8_t * end)
{
  while (*offset != end && **offset >= 0x80)
    (*offset)++;
  if (*offset != end)
    (*offset)++;
}

/* Read a ULEB128 into a 64-bit word.  Return (uint64_t)-1 on overflow
   or error.  On overflow, skip past the rest of the uleb128.  */
static uint64_t
read_uleb128 (const uint8_t ** offset, const uint8_t * end)
{
  uint64_t result = 0;
  int bit = 0;
  
  do  {
    uint64_t b;
    
    if (*offset == end)
      return (uint64_t) -1;
  
    b = **offset & 0x7f;
    
    if (bit >= 64 || b << bit >> bit != b)
      result = (uint64_t) -1;
    else
      result |= b << bit, bit += 7;
  } while (*(*offset)++ >= 0x80);
  return result;
}

/* Skip over a DWARF attribute of form FORM.  */
static bool
skip_form (const uint8_t ** offset, const uint8_t * end, uint64_t form,
	   uint8_t addr_size, bool dwarf64, bool little_endian)
{
  uint64_t sz;
  
  switch (form)
    {
    case DW_FORM_addr:
      sz = addr_size;
      break;
      
    case DW_FORM_block2:
      if (end - *offset < 2)
	return false;
      sz = 2 + read_16 (*offset);
      break;
      
    case DW_FORM_block4:
      if (end - *offset < 4)
	return false;
      sz = 2 + read_32 (*offset);
      break;
      
    case DW_FORM_data2:
    case DW_FORM_ref2:
      sz = 2;
      break;
      
    case DW_FORM_data4:
    case DW_FORM_ref4:
      sz = 4;
      break;
      
    case DW_FORM_data8:
    case DW_FORM_ref8:
      sz = 8;
      break;
      
    case DW_FORM_string:
      while (*offset != end && **offset)
	++*offset;
    case DW_FORM_data1:
    case DW_FORM_flag:
    case DW_FORM_ref1:
      sz = 1;
      break;
      
    case DW_FORM_block:
      sz = read_uleb128 (offset, end);
      break;
      
    case DW_FORM_block1:
      if (*offset == end)
	return false;
      sz = 1 + **offset;
      break;
      
    case DW_FORM_sdata:
    case DW_FORM_udata:
    case DW_FORM_ref_udata:
      skip_leb128 (offset, end);
      return true;
      
    case DW_FORM_strp:
    case DW_FORM_ref_addr:
      sz = dwarf64 ? 8 : 4;
      break;
      
    default:
      return false;
    }
  if (end - *offset < sz)
    return false;
  *offset += sz;
  return true;
}

/* Given pointers to the DEBUG_INFO and DEBUG_ABBREV sections, and
   their corresponding sizes, and whether the object file is
   LITTLE_ENDIAN or not, look at the compilation unit DIE and
   determine its NAME, compilation directory (in COMP_DIR) and its
   line number information offset (in STMT_LIST).  NAME and COMP_DIR
   may be NULL (especially COMP_DIR) if they are not in the .o file;
   STMT_LIST will be (uint64_t) -1.

   At present this assumes that there's only one compilation unit DIE.  */

int
read_comp_unit (const uint8_t * debug_info,
		size_t debug_info_size,
		const uint8_t * debug_abbrev,
		size_t debug_abbrev_size,
		int little_endian,
		const char ** name,
		const char ** comp_dir,
		uint64_t *stmt_list)
{
  const uint8_t * di = debug_info;
  const uint8_t * da;
  const uint8_t * end;
  const uint8_t * enda;
  uint64_t sz;
  uint16_t vers;
  uint64_t abbrev_base;
  uint64_t abbrev;
  uint8_t address_size;
  bool dwarf64;
  
  *name = NULL;
  *comp_dir = NULL;
  *stmt_list = (uint64_t) -1;

  if (debug_info_size < 12)
    /* Too small to be a real debug_info section.  */
    return false;
  sz = read_32 (di);
  di += 4;
  dwarf64 = sz == 0xffffffff;
  if (dwarf64)
    sz = read_64 (di), di += 8;
  else if (sz > 0xffffff00)
    /* Unknown dwarf format.  */
    return false;
  
  /* Verify claimed size.  */
  if (sz + (di - debug_info) > debug_info_size || sz <= (dwarf64 ? 23 : 11))
    return false;

  vers = read_16 (di);
  if (vers < 2 || vers > 3)
    /* DWARF version wrong for this code.
       Chances are we could continue anyway, but we don't know for sure.  */
    return false;
  di += 2;
  
  /* Find the debug_abbrev section.  */
  abbrev_base = dwarf64 ? read_64 (di) : read_32 (di);
  di += dwarf64 ? 8 : 4;
  
  if (abbrev_base > debug_abbrev_size)
    return false;
  da = debug_abbrev + abbrev_base;
  enda = debug_abbrev + debug_abbrev_size;

  address_size = *di++;

  /* Find the abbrev number we're looking for.  */
  end = di + sz;
  abbrev = read_uleb128 (&di, end);
  if (abbrev == (uint64_t) -1)
    return false;
  
  /* Skip through the debug_abbrev section looking for that abbrev.  */
  for (;;)
    {
      uint64_t this_abbrev = read_uleb128 (&da, enda);
      uint64_t attr;
      
      if (this_abbrev == abbrev)
	/* This is almost always taken.  */
	break;
      skip_leb128 (&da, enda); /* Skip the tag.  */
      if (da == enda)
	return false;
      da++;  /* Skip the DW_CHILDREN_* value.  */
      
      do {
	attr = read_uleb128 (&da, enda);
	skip_leb128 (&da, enda);
      } while (attr != 0 && attr != (uint64_t) -1);
      if (attr != 0)
	return false;
    }

  /* Check that the abbrev is one for a DW_TAG_compile_unit.  */
  if (read_uleb128 (&da, enda) != DW_TAG_compile_unit)
    return false;
  if (da == enda)
    return false;
  da++;  /* Skip the DW_CHILDREN_* value.  */

  /* Now, go through the DIE looking for DW_AT_name,
     DW_AT_comp_dir, and DW_AT_stmt_list.  */
  for (;;)
    {
      uint64_t attr = read_uleb128 (&da, enda);
      uint64_t form = read_uleb128 (&da, enda);

      if (attr == (uint64_t) -1)
	return false;
      else if (attr == 0)
	return true;

      if (form == DW_FORM_indirect)
	form = read_uleb128 (&di, end);
      
      if (attr == DW_AT_name && form == DW_FORM_string)
	*name = (const char *) di;
      else if (attr == DW_AT_comp_dir && form == DW_FORM_string)
	*comp_dir = (const char *) di;
      /* Really we should support DW_FORM_strp here, too, but
	 there's usually no reason for the producer to use that form
         for the DW_AT_name and DW_AT_comp_dir attributes.  */
      else if (attr == DW_AT_stmt_list && form == DW_FORM_data4)
	*stmt_list = read_32 (di);
      else if (attr == DW_AT_stmt_list && form == DW_FORM_data8)
	*stmt_list = read_64 (di);
      if (! skip_form (&di, end, form, address_size, dwarf64, little_endian))
	return false;
    }
}
#endif /* ! KLD */
