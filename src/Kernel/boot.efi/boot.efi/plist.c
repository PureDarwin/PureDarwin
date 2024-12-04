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

#include "plist.h"
#include "util.h"

#define kXMLTagPlist        "plist " // The trailing space here is intentional.
#define kXMLTagDict         "dict"
#define kXMLTagArray        "array"
#define kXMLTagKey          "key"
#define kXMLTagString       "string"
#define kXMLTagInteger      "integer"
#define kXMLTagData         "data"
#define kXMLTagDate         "date"
#define kXMLTagTrue         "true/"
#define kXMLTagFalse        "false/"

static long ParseListTag(char *buffer, PlistElementPtr *element, long type, long empty);
static long ParseKeyTag(char *buffer, PlistElementPtr *element);
static long ParseStringTag(char *buffer, PlistElementPtr *element);
static long ParseIntegerTag(char *buffer, PlistElementPtr *element);
static long ParseDataTag(char *buffer, PlistElementPtr *element);
static long ParseDateTag(char *buffer, PlistElementPtr *element);
static long ParseBooleanTag(char *buffer, PlistElementPtr *element, long type);
static long GetNextTag(char *buffer, char **tag, long *start);
static long FixDataMatchingTag(char *buffer, char *tag);
static PlistElementPtr NewElement(void);
static char *NewSymbol(char *string);
static void FreeSymbol(char *string);

PlistElementPtr PlistFindValue(PlistElementPtr dictionary, char *key)
{
    PlistElementPtr elementList, element;

    if (dictionary->type != kPlistTypeDictionary) return NULL;

    element = NULL;
    elementList = dictionary->element;

    while (elementList != NULL)
    {
        element = elementList;
        elementList = element->nextElement;

        if (element->type != kPlistTypeKey || element->string == NULL)
            continue;

        if (!strcmp(element->string, key))
            return element->element;
    }

    return NULL;
}

long PlistParseNextElement(char *buffer, PlistElementPtr *elementPtr)
{
    long length, position;
    char *tagName;

    length = GetNextTag(buffer, &tagName, NULL);
    if (length == -1) return -1;

    position = length;

    if (!strncmp(tagName, kXMLTagPlist, 6)) {
        length = 0;
    }
    else if (!strcmp(tagName, kXMLTagDict)) {
        length = ParseListTag(buffer + position, elementPtr, kPlistTypeDictionary, 0);
    }
    else if (!strcmp(tagName, kXMLTagDict "/")) {
        length = ParseListTag(buffer + position, elementPtr, kPlistTypeDictionary, 1);
    }
    else if (!strcmp(tagName, kXMLTagArray)) {
        length = ParseListTag(buffer + position, elementPtr, kPlistTypeArray, 0);
    }
    else if (!strcmp(tagName, kXMLTagArray "/")) {
        length = ParseListTag(buffer + position, elementPtr, kPlistTypeArray, 1);
    }
    else if (!strcmp(tagName, kXMLTagKey)) {
        length = ParseKeyTag(buffer + position, elementPtr);
    }
    else if (!strcmp(tagName, kXMLTagString)) {
        length = ParseStringTag(buffer + position, elementPtr);
    }
    else if (!strcmp(tagName, kXMLTagData)) {
        length = ParseDataTag(buffer + position, elementPtr);
    }
    else if (!strcmp(tagName, kXMLTagDate)) {
        length = ParseDateTag(buffer + position, elementPtr);
    }
    else if (!strcmp(tagName, kXMLTagTrue)) {
        length = ParseBooleanTag(buffer + position, elementPtr, kPlistTypeBooleanTrue);
    }
    else if (!strcmp(tagName, kXMLTagFalse)) {
        length = ParseBooleanTag(buffer + position, elementPtr, kPlistTypeBooleanFalse);
    } else {
        *elementPtr = NULL;
        length = 0;
    }

    if (length == -1) return -1;
    return position + length;
}

static long ParseListTag(char *buffer, PlistElementPtr *elementPtr, long type, long empty)
{
    long length;
    long position = 0;
    PlistElementPtr element;
    PlistElementPtr tagList = NULL;

    if (!empty) {
        while (1) {
            length = PlistParseNextElement(buffer + position, &element);
            if (length == -1) break;

            position += length;

            if (element == NULL) break;
            element->nextElement = tagList;
            tagList = element;
        }

        if (length == -1) {
            PlistFreeElement(tagList);
            return -1;
        }
    }

    element = NewElement();
    if (element == NULL)
    {
        PlistFreeElement(tagList);
        return -1;
    }

    element->type = type;
    element->string = NULL;
    element->element = tagList;
    element->nextElement = NULL;

    *elementPtr = element;
    return position;
}

static long ParseKeyTag(char *buffer, PlistElementPtr *elementPtr) {
    long length, length2;
    char *string;
    PlistElementPtr element, subelement;

    length = FixDataMatchingTag(buffer, kXMLTagKey);
    if (length == -1) return -1;

    length2 = PlistParseNextElement(buffer + length, &subelement);
    if (length2 == -1) return -1;

    element = NewElement();
    if (element == NULL) {
        PlistFreeElement(subelement);
        return -1;
    }

    string = NewSymbol(buffer);
    if (string == NULL) {
        PlistFreeElement(element);
        PlistFreeElement(subelement);
        return -1;
    }

    element->type = kPlistTypeKey;
    element->string = string;
    element->element = subelement;
    element->nextElement = NULL;

    *elementPtr = element;
    return length + length2;
}

static long ParseStringTag(char *buffer, PlistElementPtr *elementPtr) {
    long length;
    char *string;
    PlistElementPtr element;

    length = FixDataMatchingTag(buffer, kXMLTagString);
    if (length == -1) return -1;

    element = NewElement();
    if (element == NULL) return -1;

    string = NewSymbol(buffer);
    if (string == NULL) {
        PlistFreeElement(element);
        return -1;
    }

    element->type = kPlistTypeString;
    element->string = string;
    element->element = NULL;
    element->nextElement = NULL;

    *elementPtr = element;
    return length;
}

static long ParseIntegerTag(char *buffer, PlistElementPtr *elementPtr) {
    long length, integer;
    PlistElementPtr element;

    length = FixDataMatchingTag(buffer, kXMLTagInteger);
    if (length == -1) return -1;

    element = NewElement();
    if (element == NULL) return -1;

    // Integers in property lists are not parsed correctly, and are always zero.
    // However, since there should not be any integers in the Info.plist files and
    // com.apple.Boot.plist file, this is unlikely to be a problem.
    integer = 0;

    element->type = kPlistTypeInteger;
    element->string = (char *)integer;
    element->element = NULL;
    element->nextElement = NULL;

    *elementPtr = element;
    return length;
}

static long ParseDataTag(char *buffer, PlistElementPtr *elementPtr) {
    long length;
    PlistElementPtr element;

    length = FixDataMatchingTag(buffer, kXMLTagData);
    if (length == -1) return -1;

    // The contents of <data> tags will be skipped. Again, since the
    // plist files we are processing should not have <data> tags,
    // this is unlikely to be a problem.
    element = NewElement();
    element->type = kPlistTypeData;
    element->string = NULL;
    element->element = NULL;
    element->nextElement = NULL;

    *elementPtr = element;
    return length;
}

static long ParseDateTag(char *buffer, PlistElementPtr *elementPtr) {
    long length;
    PlistElementPtr element;

    length = FixDataMatchingTag(buffer, kXMLTagDate);
    if (length == -1) return -1;

    element = NewElement();
    if (element == NULL) return -1;

    // Same thing goes for <date> tags.
    element->type = kPlistTypeDate;
    element->string = NULL;
    element->element = NULL;
    element->nextElement = NULL;

    *elementPtr = element;
    return length;
}

static long ParseBooleanTag(char *buffer, PlistElementPtr *elementPtr, long type) {
    PlistElementPtr element = NewElement();
    if (element == NULL) return -1;

    element->type = type;
    element->string = NULL;
    element->element = NULL;
    element->nextElement = NULL;

    *elementPtr = element;
    return 0;
}

static long GetNextTag(char *buffer, char **tag, long *start) {
    if (tag == NULL) return -1;

    long count = 0;
    long count2;

    // Find the start of the tag.
    while (buffer[count] != '\0' && buffer[count] != '<') {
        count++;
    }

    if (buffer[count] == '\0') return -1;

    // Find the end of the tag.
    count2 = count + 1;
    while (buffer[count2] != '\0' && buffer[count2] != '>') {
        count2++;
    }

    // Fix the tag data.
    *tag = buffer + count + 1;
    buffer[count2] = '\0';

    if (start != NULL) *start = count;
    return count2 + 1;
}

static long FixDataMatchingTag(char *buffer, char *tag) {
    long length, start, stop;
    char *endTag;

    start = 0;
    while (1) {
        length = GetNextTag(buffer + start, &endTag, &stop);
        if (length == -1) return -1;

        if (*endTag == '/' && !strcmp(endTag + 1, tag)) break;
        start += length;
    }

    buffer[start + stop] = '\0';
    return start + length;
}

#define kElementsPerBlock           0x1000

static PlistElementPtr gElementsFree = NULL;

static PlistElementPtr NewElement() {
    PlistElementPtr element;

    if (gElementsFree == NULL) {
        element = malloc(kElementsPerBlock * sizeof(PlistElement));
        if (element == NULL) return NULL;

        for (long count = 0; count < kElementsPerBlock; count++) {
            element[count].type = kPlistTypeNone;
            element[count].string = NULL;
            element[count].element = NULL;
            element[count].nextElement = element + count + 1;
        }

        element[kElementsPerBlock - 1].nextElement = NULL;
        gElementsFree = element;
    }

    element = gElementsFree;
    gElementsFree = element->nextElement;
    return element;
}

void PlistFreeElement(PlistElementPtr element) {
    if (element == NULL) return;
    if (element->string != NULL) FreeSymbol(element->string);

    PlistFreeElement(element->element);
    PlistFreeElement(element->nextElement);

    element->type = kPlistTypeNone;
    element->string = NULL;
    element->element = NULL;
    element->nextElement = gElementsFree;
    gElementsFree = element;
}

struct Symbol {
    long refCount;
    struct Symbol *next;
    char string[];
};
typedef struct Symbol Symbol, *SymbolPtr;

static SymbolPtr gSymbolsHead = NULL;

static SymbolPtr FindSymbol(char *string, SymbolPtr *previousSymbolPtr) {
    SymbolPtr symbol = gSymbolsHead;
    SymbolPtr previous = NULL;

    while (symbol != 0) {
        if (!strcmp(symbol->string, string)) break;

        previous = symbol;
        symbol = symbol->next;
    }

    if (symbol != NULL && previousSymbolPtr != NULL) {
        *previousSymbolPtr = previous;
    }

    return symbol;
}

static char *NewSymbol(char *string) {
    static SymbolPtr lastGuy = NULL;

    SymbolPtr symbol = FindSymbol(string, NULL);

    if (symbol == NULL) {
        // Symbol doesn't exist, add a new one.
        symbol = (SymbolPtr)malloc(sizeof(Symbol) + 1 + strlen(string));

        if (symbol == NULL) panic("NULL symbol!");

        symbol->refCount = 0;
        strcpy(symbol->string, string);

        symbol->next = gSymbolsHead;
        gSymbolsHead = symbol;
    }

    // Update the refCount and return the string
    symbol->refCount++;
    return symbol->string;
}

static void FreeSymbol(char *string) {
    SymbolPtr previous;
    SymbolPtr symbol = FindSymbol(string, &previous);
    if (symbol == NULL) return;

    symbol->refCount--;
    if (symbol->refCount != 0) return;

    if (previous != NULL) previous->next = symbol->next;
    else gSymbolsHead = symbol->next;

    free(symbol);
}
