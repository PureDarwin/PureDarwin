/* /////////////////////////////////////////////////////////////////////////////
 * File:        b64/b64.h
 *
 * Purpose:     Header file for the b64 library
 *
 * Created:     18th October 2004
 * Updated:     2nd August 2006
 *
 * Thanks:      To Adam McLaurin, for ideas regarding the SecBase64Decode2() and SecBase64Encode2().
 *
 * Home:        http://synesis.com.au/software/
 *
 * Copyright (c) 2004-2006, Matthew Wilson and Synesis Software
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name(s) of Matthew Wilson and Synesis Software nor the names of
 *   any contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * ////////////////////////////////////////////////////////////////////////// */


/** \file b64/b64.h
 *
 * \brief [C/C++] Header file for the b64 library.
 */

#ifndef _SEC_BASE64_H_
#define _SEC_BASE64_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* /////////////////////////////////////////////////////////////////////////////
 * Enumerations
 */

/** \brief Return codes (from SecBase64Encode2() / SecBase64Decode2())
 */
enum
{
    kSecB64_R_OK                   =   0,   /*!< operation was successful. */
    kSecB64_R_INSUFFICIENT_BUFFER  =   1,   /*!< The given translation buffer was not of sufficient size. */
    kSecB64_R_TRUNCATED_INPUT      =   2,   /*!< The input did not represent a fully formed stream of octet couplings. */
    kSecB64_R_DATA_ERROR           =   3   /*!< invalid data. */
};

typedef uint32_t SecBase64Result;

/** \brief Coding behaviour modification flags (for SecBase64Encode2() / SecBase64Decode2())
 */
enum
{
        kSecB64_F_LINE_LEN_USE_PARAM    =   0x0000  /*!< Uses the lineLen parameter to SecBase64Encode2(). Ignored by SecBase64Decode2(). */
    ,   kSecB64_F_LINE_LEN_INFINITE     =   0x0001  /*!< Ignores the lineLen parameter to SecBase64Encode2(). Line length is infinite. Ignored by SecBase64Decode2(). */
    ,   kSecB64_F_LINE_LEN_64           =   0x0002  /*!< Ignores the lineLen parameter to SecBase64Encode2(). Line length is 64. Ignored by SecBase64Decode2(). */
    ,   kSecB64_F_LINE_LEN_76           =   0x0003  /*!< Ignores the lineLen parameter to SecBase64Encode2(). Line length is 76. Ignored by SecBase64Decode2(). */
    ,   kSecB64_F_LINE_LEN_MASK         =   0x000f  /*!< Mask for testing line length flags to SecBase64Encode2(). Ignored by SecBase64Encode2(). */
    ,   kSecB64_F_STOP_ON_NOTHING       =   0x0000  /*!< Decoding ignores all invalid characters in the input data. Ignored by SecBase64Encode2(). */
    ,   kSecB64_F_STOP_ON_UNKNOWN_CHAR  =   0x0100  /*!< Causes decoding to break if any non-Base-64 [a-zA-Z0-9=+/], non-whitespace character is encountered. Ignored by SecBase64Encode2(). */
    ,   kSecB64_F_STOP_ON_UNEXPECTED_WS =   0x0200  /*!< Causes decoding to break if any unexpected whitespace is encountered. Ignored by SecBase64Encode2(). */
    ,   kSecB64_F_STOP_ON_BAD_CHAR      =   0x0300  /*!< Causes decoding to break if any non-Base-64 [a-zA-Z0-9=+/] character is encountered. Ignored by SecBase64Encode2(). */
};

typedef uint32_t SecBase64Flags;

/* /////////////////////////////////////////////////////////////////////////////
 * Functions
 */

#if 0
static inline size_t SecBase64EncodedSize(size_t srcSize, size_t lineLen) {
    size_t total = (((srcSize) + 2) / 3) * 4;
    size_t lineLen = (lineLen);
    if (lineLen > 0) {
        size_t numLines = (total + (lineLen - 1)) / lineLen;
        total += 2 * (numLines - 1);
    }
    return total;
}
#endif

/** \brief Encodes a block of binary data into base64
 *
 * \param src Pointer to the block to be encoded. May not be NULL, except when
 *   \c dest is NULL, in which case it is ignored.
 * \param srcSize Length of block to be encoded
 * \param dest Pointer to the buffer into which the result is to be written. May
 *   be NULL, in which case the function returns the required length
 * \param destLen Length of the buffer into which the result is to be written. Must
 *   be at least as large as that indicated by the return value from
 * \c SecBase64Encode()(NULL, srcSize, NULL, 0).
 *
 * \return 0 if the size of the buffer was insufficient, or the length of the
 * converted buffer was longer than \c destLen
 *
 * \note The function returns the required length if \c dest is NULL
 *
 * \note The function returns the required length if \c dest is NULL. The returned size
 *   might be larger than the actual required size, but will never be smaller.
 *
 * \note Threading: The function is fully re-entrant.
 */
size_t SecBase64Encode(void const *src, size_t srcSize, char *dest, size_t destLen);

/** \brief Encodes a block of binary data into base64
 *
 * \param src Pointer to the block to be encoded. May not be NULL, except when
 *   \c dest is NULL, in which case it is ignored.
 * \param srcSize Length of block to be encoded
 * \param dest Pointer to the buffer into which the result is to be written. May
 *   be NULL, in which case the function returns the required length
 * \param destLen Length of the buffer into which the result is to be written. Must
 *   be at least as large as that indicated by the return value from
 *   \c SecBase64Encode()(NULL, srcSize, NULL, 0).
 * \param flags A combination of the SecBase64Flags enumeration, that moderate the
 *   behaviour of the function
 * \param lineLen If the flags parameter contains kSecB64_F_LINE_LEN_USE_PARAM, then
 *   this parameter represents the length of the lines into which the encoded form is split,
 *   with a hard line break ('\\r\\n'). If this value is 0, then the line is not
 *   split. If it is <0, then the RFC-1113 recommended line length of 64 is used
 * \param rc The return code representing the status of the operation. May be NULL.
 *
 * \return 0 if the size of the buffer was insufficient, or the length of the
 *   converted buffer was longer than \c destLen
 *
 * \note The function returns the required length if \c dest is NULL. The returned size
 *   might be larger than the actual required size, but will never be smaller.
 *
 * \note Threading: The function is fully re-entrant.
 */
size_t SecBase64Encode2( void const  *src
                ,   size_t      srcSize
                ,   char        *dest
                ,   size_t      destLen
                ,   unsigned    flags
                ,   int         lineLen /* = 0 */
                ,   SecBase64Result      *rc     /* = NULL */);

/** \brief Decodes a sequence of base64 into a block of binary data
 *
 * \param src Pointer to the base64 block to be decoded. May not be NULL, except when
 *   \c dest is NULL, in which case it is ignored. If \c dest is NULL, and \c src is
 *   <b>not</b> NULL, then the returned value is calculated exactly, otherwise a value
 *   is returned that is guaranteed to be large enough to hold the decoded block.
 *
 * \param srcLen Length of block to be encoded. Must be an integral of 4, the base64
 *   encoding quantum, otherwise the base64 block is assumed to be invalid
 * \param dest Pointer to the buffer into which the result is to be written. May
 *   be NULL, in which case the function returns the required length
 * \param destSize Length of the buffer into which the result is to be written. Must
 *   be at least as large as that indicated by the return value from
 *   \c SecBase64Decode(src, srcSize, NULL, 0), even in the case where the encoded form
 *   contains a number of characters that will be ignored, resulting in a lower total
 *   length of converted form.
 *
 * \return 0 if the size of the buffer was insufficient, or the length of the
 *   converted buffer was longer than \c destSize
 *
 * \note The function returns the required length if \c dest is NULL. The returned size
 *   might be larger than the actual required size, but will never be smaller.
 *
 * \note \anchor anchor__4_characters The behaviour of both
 * \link b64::SecBase64Encode2 SecBase64Encode2()\endlink
 * and
 * \link b64::SecBase64Decode2 SecBase64Decode2()\endlink
 * are undefined if the line length is not a multiple of 4.
 *
 * \note Threading: The function is fully re-entrant.
 */
size_t SecBase64Decode(char const *src, size_t srcLen, void *dest, size_t destSize);

/** \brief Decodes a sequence of base64 into a block of binary data
 *
 * \param src Pointer to the base64 block to be decoded. May not be NULL, except when
 * \c dest is NULL, in which case it is ignored. If \c dest is NULL, and \c src is
 * <b>not</b> NULL, then the returned value is calculated exactly, otherwise a value
 * is returned that is guaranteed to be large enough to hold the decoded block.
 *
 * \param srcLen Length of block to be encoded. Must be an integral of 4, the base64
 *   encoding quantum, otherwise the base64 block is assumed to be invalid
 * \param dest Pointer to the buffer into which the result is to be written. May
 *   be NULL, in which case the function returns the required length
 * \param destSize Length of the buffer into which the result is to be written. Must
 *   be at least as large as that indicated by the return value from
 *   \c SecBase64Decode(src, srcSize, NULL, 0), even in the case where the encoded form
 *   contains a number of characters that will be ignored, resulting in a lower total
 *   length of converted form.
 * \param flags A combination of the SecBase64Flags enumeration, that moderate the
 *   behaviour of the function.
 * \param rc The return code representing the status of the operation. May be NULL.
 * \param badChar If the flags parameter does not contain kSecB64_F_STOP_ON_NOTHING, this
 *   parameter specifies the address of a pointer that will be set to point to any
 *   character in the sequence that stops the parsing, as dictated by the flags
 *   parameter. May be NULL.
 *
 * \return 0 if the size of the buffer was insufficient, or the length of the
 * converted buffer was longer than \c destSize, or a bad character stopped parsing.
 *
 * \note The function returns the required length if \c dest is NULL. The returned size
 *   might be larger than the actual required size, but will never be smaller.
 *
 * \note The behaviour of both
 * \link b64::SecBase64Encode2 SecBase64Encode2()\endlink
 * and
 * \link b64::SecBase64Decode2 SecBase64Decode2()\endlink
 * are undefined if the line length is not a multiple of 4.
 *
 * \note Threading: The function is fully re-entrant.
 */
size_t SecBase64Decode2( char const  *src
                ,   size_t      srcLen
                ,   void        *dest
                ,   size_t      destSize
                ,   unsigned    flags
                ,   char const  **badChar   /* = NULL */
                ,   SecBase64Result      *rc         /* = NULL */);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SEC_BASE64_H_ */
