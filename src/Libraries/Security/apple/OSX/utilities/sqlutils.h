/*
 * Copyright (c) 2011-2012,2014 Apple Inc. All Rights Reserved.
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


/*
 * sqlutils.h - some wrapper for sql3lite
 */
#ifndef _SECURITY_UTILITIES_SQLUTILS_H_
#define _SECURITY_UTILITIES_SQLUTILS_H_

#include <sqlite3.h>

/* Those are just wrapper around the sqlite3 functions, but they have size_t for some len parameters,
   and checks for overflow before casting to int */
static inline int sqlite3_bind_blob_wrapper(sqlite3_stmt* pStmt, int i, const void* zData, size_t n, void(*xDel)(void*))
{
    if(n>INT_MAX) return SQLITE_TOOBIG;
    return sqlite3_bind_blob(pStmt, i, zData, (int)n, xDel);
}

static inline int sqlite3_bind_text_wrapper(sqlite3_stmt* pStmt, int i, const void* zData, size_t n, void(*xDel)(void*))
{
    if(n>INT_MAX) return SQLITE_TOOBIG;
    return sqlite3_bind_text(pStmt, i, zData, (int)n, xDel);
}

static inline int sqlite3_prepare_wrapper(sqlite3 *db, const char *zSql, size_t nByte, sqlite3_stmt **ppStmt, const char **pzTail)
{
    if(nByte>INT_MAX) return SQLITE_TOOBIG;
    return sqlite3_prepare(db, zSql, (int)nByte, ppStmt, pzTail);
}

#endif
