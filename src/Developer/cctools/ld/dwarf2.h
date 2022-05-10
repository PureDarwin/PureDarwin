/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
/* These constants were taken from version 3 of the DWARF standard,
   which is Copyright (c) 2005 Free Standards Group, and
   Copyright (c) 1992, 1993 UNIX International, Inc.  */

/* This is not a complete list.  */
enum {
  DW_TAG_compile_unit = 17,
  DW_TAG_partial_unit = 60
};

/* This is not a complete list.  */
enum {
  DW_AT_sibling = 1,
  DW_AT_name = 3,
  DW_AT_stmt_list = 16,
  DW_AT_comp_dir = 27
};

enum {
  DW_FORM_addr = 1,
  DW_FORM_block2 = 3,
  DW_FORM_block4,
  DW_FORM_data2,
  DW_FORM_data4,
  DW_FORM_data8,
  DW_FORM_string,
  DW_FORM_block,
  DW_FORM_block1,
  DW_FORM_data1,
  DW_FORM_flag,
  DW_FORM_sdata,
  DW_FORM_strp,
  DW_FORM_udata,
  DW_FORM_ref_addr,
  DW_FORM_ref1,
  DW_FORM_ref2,
  DW_FORM_ref4,
  DW_FORM_ref8,
  DW_FORM_ref_udata,
  DW_FORM_indirect /* 22 */
};

enum {
  DW_LNS_extended_op = 0,
  DW_LNS_copy,
  DW_LNS_advance_pc,
  DW_LNS_advance_line,
  DW_LNS_set_file,
  DW_LNS_set_column,
  DW_LNS_negate_stmt,
  DW_LNS_set_basic_block,
  DW_LNS_const_add_pc,
  DW_LNS_fixed_advance_pc,
  DW_LNS_set_prologue_end,
  DW_LNS_set_epilogue_begin,
  DW_LNS_set_isa
};

enum {
  DW_LNE_end_sequence = 1,
  DW_LNE_set_address,
  DW_LNE_define_file
};
