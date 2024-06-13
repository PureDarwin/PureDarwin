/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Portions Copyright (c) 2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef EFI_BOOT_DEVICE_TREE_H_INCLUDED
#define EFI_BOOT_DEVICE_TREE_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

typedef struct _Property {
    char *name;
    uint32_t length;
    void *value;
    struct _Property *next;
} DT_Property;

typedef struct _Node {
    DT_Property *properties;
    DT_Property *last_prop;
    struct _Node *children;
    struct _Node *next;
} DT_Node;

extern DT_Property *DT_AddProperty(DT_Node *node, char *name, uint32_t length, void *value);
extern DT_Node *DT_AddChild(DT_Node *parent, char *name);
extern DT_Node *DT_FindNode(char *path, bool createIfMissing);
extern void DT_FreeProperty(DT_Property *property);
extern void DT_FreeNode(DT_Node *node);
extern char *DT_GetName(DT_Node *node);

void DT_Initialize(void);
void DT_Finalize(void);

void DT_FlattenDeviceTree(void **result, uint32_t *length);

#endif
