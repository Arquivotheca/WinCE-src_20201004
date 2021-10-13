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
#ifndef _UTILITY_DOT_AYCH_
#define _UTILITY_DOT_AYCH_

#include <windows.h>

////////////////////////////////////////////////////////////////////////////////
// Suggested log verbosities
// These are copied from FT.H and stuffed here rather than bringing over the whole file
#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14
#include <kato.h>

typedef enum REGISTRY_OP
{
	Null = 0,
	AddKey = 1, 
	DeleteKey = 2,
	AddValue = 3,
	DeleteValue = 4,
	ChangeValue = 5, 
	Terminus	= 6
};

#define	CE_MAX_PATH			(32767)
#define	DEFAULT_FILE_SIZE	(4096)


DWORD		GetOptionsFromString(const _TCHAR * s);
bool		GetStringFromCmdLine(const WCHAR *, WCHAR *, unsigned, const WCHAR *);
bool		GetULIntFromCmdLine(const WCHAR *, const WCHAR *, unsigned *, unsigned radix = 10);
bool		GetULONGLONGFromCmdLine(const WCHAR *, const WCHAR *, ULONGLONG *);
HKEY		GetRootKeyFromCmdLine(const	wchar_t *, const	wchar_t *);
bool		FlagIsPresentOnCmdLine(const wchar_t * s, const wchar_t * sQry);
wchar_t *	AllocAndFillLongName(void);
void		Zorchage(void);
bool		Zorch(const wchar_t *);
void		Zorchage(CKato * pKato);

#define CHARCOUNT(s)(sizeof(s)/sizeof(*s))

#endif
