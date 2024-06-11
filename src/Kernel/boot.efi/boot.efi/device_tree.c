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

#include <x86_64/efibind.h>
#include <lib.h>
#include "device_tree.h"
#include "util.h"

#define kPropertyNameLength             32

typedef struct DeviceTreeNodeProperty {
    char            name[kPropertyNameLength];
    unsigned long   length;
    // Property data follows of variable length
} DT_DeviceTreeNodeProperty;

typedef struct OpaqueDTEntry {
    unsigned long nProperties;
    unsigned long nChildren;

    // DT_DeviceTreeNodeProperty    properties[nProperties];
    // DT_DeviceTreeNode            childrenn[nChildren];
} DT_DeviceTreeNode;

typedef char DT_PropertyNameBuffer[kPropertyNameLength];
enum {
    // Max length of a C-string entry name (trailing NUL not included).
    kDT_MaxEntryNameLength = kPropertyNameLength - 1
};

#if DEBUG_BUILD
#define DPRINTF(args...) Print(args)
#else
#define DPRINTF(args...) /* nothing */
#endif

void DT_PrintTree(DT_Node *node);

#define RoundToLong(x)      (((x) + 3) & ~3)

static struct _DTSizeInfo {
    uint32_t numNodes;
    uint32_t numProperties;
    uint32_t totalPropertySize;
} DT_Info;

#define kAllocSize          4096

static DT_Node *rootNode;
static DT_Node *freeNodes, *allocatedNodes;
static DT_Property *freeProperties, *allocatedProperties;

DT_Property *DT_AddProperty(DT_Node *node, char *name, uint32_t length, void *value) {
    DT_Property *prop;

    DPRINTF(L"DT_AddProperty([Node '%s'], '%s', %d, 0x%x)\n", DT_GetName(node), name, length, value);
    if (freeProperties == NULL) {
        void *buf = malloc(kAllocSize);
        if (buf == NULL) return NULL;

        DPRINTF(L"Allocating more free properties\n");
        bzero(buf, kAllocSize);

        prop = (DT_Property *)buf;
        prop->next = allocatedProperties;
        allocatedProperties = prop;
        prop->value = buf;
        prop++;

        for (int i = 1; i < (kAllocSize / sizeof(DT_Property)); i++) {
            prop->next = freeProperties;
            freeProperties = prop;
            prop++;
        }
    }

    prop = freeProperties;
    freeProperties = prop->next;

    prop->name = name;
    prop->length = length;
    prop->value = value;

    // Always add to end of list
    if (node->properties == NULL) {
        node->properties = prop;
    } else {
        node->last_prop->next = prop;
    }

    node->last_prop = prop;
    prop->next = NULL;

    DPRINTF(L"Done [0x%x]\n", prop);

    DT_Info.numProperties++;
    DT_Info.totalPropertySize += RoundToLong(length);

    return prop;
}

DT_Node *DT_AddChild(DT_Node *parent, char *name)
{
    DT_Node *node;

    if (freeNodes == NULL) {
        DPRINTF(L"Allocating more free properties\n");

        void *buf = malloc(kAllocSize);
        if (buf == NULL) return NULL;

        bzero(buf, kAllocSize);
        node = (DT_Node *)buf;
        node->next = allocatedNodes;
        allocatedNodes = node;
        node->children = (DT_Node *)buf;
        node++;

        for (int i = 1; i < (kAllocSize / sizeof(DT_Node)); i++) {
            node->next = freeNodes;
            freeNodes = node;
            node++;
        }
    }

    DPRINTF(L"DT_AddChild(0x%x, '%s')\n", parent, name);
    node = freeNodes;
    freeNodes = node->next;

    DPRINTF(L"Got free node 0x%x\n", node);
    DPRINTF(L"prop = 0x%x, children = 0x%x, next = 0x%x", node->properties, node->children, node->next);

    if (parent == NULL) {
        rootNode = node;
        node->next = NULL;
    } else {
        node->next = parent->children;
        parent->children = node;
    }

    DT_Info.numNodes++;
    DT_AddProperty(node, "name", strlen(name) + 1, name);

    return node;
}

void DT_FreeProperty(DT_Property *property) {
    property->next = freeProperties;
    freeProperties = property;
}

void DT_Initialize(void) {
    DPRINTF(L"DT_Initialize()\n");

    freeNodes = NULL;
    allocatedNodes = NULL;
    freeProperties = NULL;
    allocatedProperties = NULL;

    DT_Info.numNodes = 0;
    DT_Info.numProperties = 0;
    DT_Info.totalPropertySize = 0;

    rootNode = DT_AddChild(NULL, "/");
    DPRINTF(L"DT_Initialize() - done\n");
}

void DT_Finalize(void) {
    DPRINTF(L"DT_Finalize()\n");

    for (DT_Property *prop = allocatedProperties; prop != NULL; prop = prop->next) {
        free(prop->value);
    }

    for (DT_Node *node = allocatedNodes; node != NULL; node = node->next) {
        free((void *) node->children);
    }

    allocatedNodes = NULL;
    allocatedProperties = NULL;
    rootNode = NULL;

    // XXX: Leaks any allocated strings
    DT_Info.numNodes = 0;
    DT_Info.numProperties = 0;
    DT_Info.totalPropertySize = 0;
}

static void *FlattenNodes(DT_Node *node, void *buffer) {
    DT_Property *prop;
    DT_DeviceTreeNode *flatNode;
    DT_DeviceTreeNodeProperty *flatProp;

    if (node == NULL) return buffer;

    flatNode = (DT_DeviceTreeNode *)buffer;
    buffer += sizeof(DT_DeviceTreeNode);

    int count;
    for (count = 0, prop = node->properties; prop != NULL; count++, prop = prop->next) {
        flatProp = (DT_DeviceTreeNodeProperty *)buffer;
        strcpy(flatProp->name, prop->name);

        flatProp->length = prop->length;
        buffer += sizeof(DT_DeviceTreeNodeProperty);
        bcopy(prop->value, buffer, prop->length);
        buffer += RoundToLong(prop->length);
    }

    flatNode->nProperties = count;

    for (count = 0, node = node->children; node != NULL; count++, node = node->next) {
        buffer = FlattenNodes(node, buffer);
    }

    flatNode->nChildren = count;

    return buffer;
}

void DT_FlattenDeviceTree(void **result, uint32_t *length) {
    uint32_t totalSize;
    void *buf;

    DPRINTF(L"DT_FlattenDeviceTree(0x%x, 0x%x)\n", result, length);
    if (result != NULL) DT_PrintTree(rootNode);

    totalSize = DT_Info.numNodes * sizeof(DT_DeviceTreeNode) +
        DT_Info.numProperties * sizeof(DT_DeviceTreeNodeProperty) +
        DT_Info.totalPropertySize;
    DPRINTF(L"Total size 0x%x\n", totalSize);

    if (result != NULL) {
        if (totalSize == 0) {
            buf = NULL;
        } else {
            if (*result == NULL) {
                buf = malloc(totalSize);
            } else {
                buf = *result;
            }

            bzero(buf, totalSize);
            FlattenNodes(rootNode, buf);
        }

        *result = buf;
    }

    if (length != NULL) *length = totalSize;
}

char *DT_GetName(DT_Node *node) {
    for (DT_Property *prop = node->properties; prop != NULL; prop = prop->next) {
        if (strcmp(prop->name, "name")) return prop->value;
    }

    return "(null)";
}

DT_Node *DT_FindNode(char *path, bool createIfMissing) {
    DT_Node *node, *child;
    DT_PropertyNameBuffer nameBuf;
    char *bp;

    DPRINTF(L"DT_FindNode('%s', %s)\n", path, createIfMissing ? "TRUE" : "FALSE");

    node = rootNode;
    DPRINTF(L"root = 0x%x\n", node);

    while (node != NULL) {
        // Skip leading path
        while (*path == '/') path++;

        int i;
        for (i = 0, bp = nameBuf; ++i < kDT_MaxEntryNameLength && *path && *path != '/'; bp++, path++) {
            *bp = *path;
        }

        if (nameBuf[0] == '\0') {
            // last path entry
            break;
        }

        DPRINTF(L"Node '%s'\n", nameBuf);

        for (child = node->children; child != NULL; child++) {
            DPRINTF(L"Child 0x%x\n", child);

            if (strcmp(DT_GetName(child), nameBuf) == 0)
                break;
        }

        if (child == NULL && createIfMissing) {
            DPRINTF(L"Creating node\n");

            char *str = malloc(strlen(nameBuf) + 1);
            strcpy(str, nameBuf);

            child = DT_AddChild(node, str);
        }

        node = child;
    }

    return node;
}

void DT_PrintNode(DT_Node *node, int level) {
    char spaces[10], *cp = spaces;

    if (level > 9) level = 9;
    while (level--) *cp++ = ' ';
    *cp = '\0';

    Print(L"%s=== Node ===\n", spaces);
    for (DT_Property *prop = node->properties; prop != NULL; prop = prop->next) {
        char c = *((char *)prop->value);
        if (prop->length < 64 && (
            strcmp(prop->name, "name") == 0 ||
            (c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_'))
        {
            Print(L"%sProperty '%s' [%d] = '%s'\n", spaces, prop->name, prop->length, (char *)prop->value);
        } else {
            Print(L"%sProperty '%s' [%d] = (data)\n", spaces, prop->name, prop->length);
        }
    }

    Print(L"%s==========\n", spaces);
}

static void _PrintTree(DT_Node *node, int level) {
    DT_PrintNode(node, level);
    level++;

    for (node = node->children; node != NULL; node = node->next) {
        _PrintTree(node, level);
    }
}

void DT_PrintTree(DT_Node *node) {
    if (node == NULL) node = rootNode;
    _PrintTree(node, 0);
}
