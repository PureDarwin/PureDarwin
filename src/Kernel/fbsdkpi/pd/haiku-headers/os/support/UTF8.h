/*
 * Copyright 2007 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _UTF8_H
#define _UTF8_H


#include <BeBuild.h>
#include <InterfaceDefs.h>
#include <SupportDefs.h>


/*  Conversion Flavors */
enum {
	B_ISO1_CONVERSION,				/*  ISO 8859-x */
	B_ISO2_CONVERSION,
	B_ISO3_CONVERSION,
	B_ISO4_CONVERSION,
	B_ISO5_CONVERSION,
	B_ISO6_CONVERSION,
	B_ISO7_CONVERSION,
	B_ISO8_CONVERSION,
	B_ISO9_CONVERSION,
	B_ISO10_CONVERSION,
	B_MAC_ROMAN_CONVERSION,			/*  Macintosh Roman */
	B_SJIS_CONVERSION,				/*  Shift-JIS */
	B_EUC_CONVERSION,				/*  EUC Packed Japanese */
	B_JIS_CONVERSION,				/*  JIS X 0208-1990 */
	B_MS_WINDOWS_CONVERSION,		/*  Windows Latin-1 Codepage 1252 */
	B_UNICODE_CONVERSION,			/*  Unicode 2.0, UCS-2 */
	B_KOI8R_CONVERSION,				/*  KOI8-R */
	B_MS_WINDOWS_1251_CONVERSION,	/*  Windows Cyrillic Codepage 1251 */
	B_MS_DOS_866_CONVERSION,		/*  MS-DOS Codepage 866 */
	B_MS_DOS_CONVERSION,			/*  MS-DOS Codepage 437 */
	B_EUC_KR_CONVERSION,			/*  EUC Korean */
	B_ISO13_CONVERSION,
	B_ISO14_CONVERSION,
	B_ISO15_CONVERSION,
	B_BIG5_CONVERSION,				/*  Chinese Big5 */
	B_GBK_CONVERSION,				/*  Chinese GB18030 */
	B_UTF16_CONVERSION,				/*  Unicode UTF-16 */
	B_MS_WINDOWS_1250_CONVERSION	/*  Windows Central European Codepage */
};


/*  Conversion Functions */

#ifdef __cplusplus

status_t convert_to_utf8(uint32 sourceEncoding, const char* source,
	int32* sourceLength, char* dest, int32* destLength, int32* state,
	char substitute = B_SUBSTITUTE);

status_t convert_from_utf8(uint32 destEncoding, const char* source, 
	int32* sourceLength, char* dest, int32* destLength, int32* state,
	char substitute = B_SUBSTITUTE);

#endif	/* __cplusplus */

#endif	/* _UTF8_H */
