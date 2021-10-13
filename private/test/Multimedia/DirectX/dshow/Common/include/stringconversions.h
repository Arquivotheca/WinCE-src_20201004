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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _STRING_CONVERSIONS_H
#define _STRING_CONVERSIONS_H

#include <windows.h>
#include <tchar.h>

#ifndef countof
#define countof(x) (sizeof(x)/sizeof(x[0]))
#endif

inline WCHAR* TCharToWChar(const TCHAR* from, WCHAR* to, int dstchars)
{
#ifdef _UNICODE
	// TCHAR is WCHAR - no conversion required
	if (dstchars <= 0)
		return NULL;
	return wcsncpy(to, from, dstchars);
#else
	// TCHAR is char - conversion required
	// ANSI or UTF-8 should be used as the encoding
	if (!MultiByteToWideChar(CP_UTF8, 0, from, -1, to, dstchars))
		return NULL;
	else return to;
#endif
}

inline WCHAR* CharToWChar(const char* from, WCHAR* to, int len)
{
	if (!from || !to)
		return NULL;
	// ANSI or UTF-8 should be used as the encoding
	return (!MultiByteToWideChar(CP_UTF8, 0, from, -1, to, len)) ? NULL : to;
}


inline TCHAR* CharToTChar(const char* from, TCHAR* to, int len)
{
	if (!from || !to)
		return NULL;
#ifdef _UNICODE
	// TCHAR is WCHAR - conversion required
	// ANSI or UTF-8 should be used as the encoding
	return (!MultiByteToWideChar(CP_UTF8, 0, from, -1, to, len)) ? NULL : to;
#else
	// TCHAR is char - no conversion required
	return strncpy(to, from, len);
#endif
}

inline TCHAR* WCharToTChar(const WCHAR* from, TCHAR* to, int len)
{
	if (!from || !to)
		return NULL;
#ifdef _UNICODE
	// WCHAR is TCHAR - no conversion required
	return _tcsncpy(to, from, len);
#else
	// TCHAR is char - conversion required from wchar to char
	if (wcstombs(to, from, len))
		return to;
	else return NULL;
#endif
}


inline char* TCharToChar(const TCHAR* from, char* to, int len)
{
#ifdef _UNICODE
	// TCHAR is WCHAR - convert to char
	if (wcstombs(to, from, len))
		return to;
	else return NULL;
#else
	// TCHAR is char - no conversion required
	strncpy(to, from, len);
#endif
	return to;
}


#endif
