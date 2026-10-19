/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef __MBSTATE_T_H
#define __MBSTATE_T_H


#include <stddef.h>


typedef struct {
	void* converter;
	char charset[64];
	unsigned int count;
	char data[1024 + 8];	/* 1024 bytes for data, 8 for alignment space */
} mbstate_t;


#endif /* __MBSTATE_T_H */
