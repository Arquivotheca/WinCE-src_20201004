//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//--------------------------------------------------------------------------------------------
//
//	THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//	ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//	THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//	PARTICULAR PURPOSE.
//
//
//--------------------------------------------------------------------------------------------
//
// June 2004 - Michael Rockhold (michrock)
//
//--------------------------------------------------------------------------------------------


#ifndef __PRIV_CETKCONNLIB_H__
#define __PRIV_CETKCONNLIB_H__

#include "windows.h"
#include "tchar.h"
#include <stdarg.h>
#include <wtypes.h>
#include <strsafe.h>

#ifdef UNICODE
#define UNICODE_T_STR_FORMAT _T("%s")
#define ASCII_T_STR_FORMAT _T("%S")
#else
#define UNICODE_T_STR_FORMAT _T("%S")
#define ASCII_T_STR_FORMAT _T("%s")
#endif

#define GUIDFORMAT _T("{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}")

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH( array )	( sizeof( array ) / sizeof( array[ 0 ] ) )
#endif //#ifndef ARRAY_LENGTH

#define DIE(e,g) { errnum = e; goto g; }
#define ERROR_EXIT(c,e,l)	if (c) DIE(e,l)

#define RELEASE(i) {if (NULL != i) {i->Release(); i=NULL; }}

void MyMessageBox(const TCHAR* caption, int style, const TCHAR* fmt, ...);
bool GetCurrentProcessInfo(DWORD* pdwCurrentProceessID, LPWSTR* ppModuleFileName);

void
tcstombs(
	char *mbstr,
	const TCHAR *tcstr,
	size_t count 
	);

template <class I>
bool
HexStrToUInt(LPCTSTR psz, size_t len, OUT I* pRet)
{
	TCHAR temp[13];
	if (len>12) len = 12;
	_tcsncpy(temp, psz, len); temp[len] = _T('\0');
	LPTSTR pszStop;
	*pRet = (I)_tcstoul(temp, &pszStop, 16);
	return ( pszStop == &temp[len] );
};

template<class T> class Matcher {
public:
	typedef T match_t;
	Matcher() {}
	virtual ~Matcher() {}
	virtual bool Match(match_t* thing) = 0;
};

const int GUID_STRING_LEN = 38;
bool StringToGUID(IN LPCTSTR psz, OUT GUID* guid);
LPTSTR GUIDToString(IN const GUID& guid, OUT TCHAR sz[GUID_STRING_LEN]);

#endif

