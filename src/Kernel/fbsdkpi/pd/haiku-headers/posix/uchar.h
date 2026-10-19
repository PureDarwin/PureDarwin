/*
 * Copyright 2018-2020 Haiku, Inc. All Right Reserved
 * Distributed under the terms of MIT license.
 */
#ifndef _UCHAR_H
#define _UCHAR_H


#include <stdint.h>
#include <wchar_t.h>
#include <_mbstate_t.h>


#if !defined(__cplusplus) || __cplusplus < 201103L
typedef uint_least32_t char32_t;
typedef uint_least16_t char16_t;
#endif


#ifdef __cplusplus
extern "C" {
#endif


#define __STD_UTF_32__ 1
#define __STD_UTF_16__ 1


// TODO implement mbrtoc16


size_t c16rtomb(char *dest, char32_t wc, mbstate_t *mbState);
size_t mbrtoc32(char32_t *dest, const char *src, size_t srcLength, mbstate_t *mbState);
size_t c32rtomb(char *dest, char32_t wc, mbstate_t *mbState);


#ifdef __cplusplus
}
#endif
#endif /* _UCHAR_H */
