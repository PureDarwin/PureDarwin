/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
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

#ifndef EFI_BOOT_UTIL_H
#define EFI_BOOT_UTIL_H

#ifndef _EFI_DEF_H
#define NULL 0
#endif

typedef __SIZE_TYPE__ size_t;
extern int strcmp(char *left, char *right);
extern int strncmp(char *left, char *right, size_t size);
extern int strlen(char *string);
extern char *strcpy(char *destination, const char *source);
extern void bzero(void *s, size_t n);
extern void bcopy(const void *source, void *dest, size_t len);

extern void *malloc(size_t size);
extern void *calloc(size_t count, size_t size);
extern void free(void *ptr);

// In main.c
extern void EfiPanicBoot(char *message, char *file, int line) __attribute__((noreturn));
#define panic(msg) EfiPanicBoot(msg, __FILE__, __LINE__)

#endif
