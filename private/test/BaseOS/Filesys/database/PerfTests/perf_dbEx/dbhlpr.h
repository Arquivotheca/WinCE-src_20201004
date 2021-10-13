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
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __TCSP_HLP__
#define __TCSP_HLP__

#include <bldver.h>
#include <windbase.h>
#include "tuxmain.h"

//  =========================================
//  HELPER MACROS
//  =========================================

#define CHECK_ALLOC(X)      { \
	if (NULL == X) \
		GENERIC_FAIL(LocalAlloc); \
}

#define DUMP_LOCN   {TRACE(TEXT("File %s, line %d\r\n"), _T(__FILE__), __LINE__);}

#define GENERIC_FAIL(X) { \
	TRACE(_T("%s error in %s line %u 0x%x "), _T(#X), _T(__FILE__), __LINE__, GetLastError()); \
	TRACE(_T("\n")); \
	goto ErrorReturn; \
}

#define FREE(X) { \
	if (X) \
	{ \
		if (LocalFree(X)) \
			TRACE(_T(">>>WARNING: LocalFree Failed. 0x%x\n"), GetLastError()); \
		 X=NULL; \
	} \
}   

#define GET_PROPID(X)                   X & 0xFF

#define CLOSE_HANDLE(X) \
{ \
	if (INVALID_HANDLE_VALUE!=X) \
	{ \
		if (!CloseHandle(X)) \
			TRACE(TEXT("CloseHandle failed 0x%x\n"), GetLastError()); \
		else \
			X=INVALID_HANDLE_VALUE; \
	} \
}


//  =========================================
//  HELPER CONSTANTS
//  =========================================
#define TST_FLAG_READABLE		0x0001  //  used by Hlp_GenStringData
#define TST_FLAG_ALPHA		0x0002
#define TST_FLAG_ALPHA_NUM	0x0003
#define TST_FLAG_NUMERIC		0x0004

#define HLP_FILL_RANDOM		1
#define HLP_FILL_SEQUENTIAL	2
#define HLP_FILL_DONT_CARE	0


//  =========================================
//  HELPER FUNCTIONS
//  =========================================
LPTSTR  Hlp_GenStringData(__out_ecount(cChars) LPTSTR pszString, DWORD cChars, DWORD dwFlags);
void Hlp_GenRandomFileTime(FILETIME *myFileTime);
BOOL Hlp_FillBuffer(__out_ecount(cbBuffer) PBYTE pbBuffer, DWORD cbBuffer, DWORD dwFlags);
BOOL Hlp_GetSysTimeAsFileTime(FILETIME *retFileTime);
BOOL Hlp_GetRecordCount(HANDLE hDB, DWORD *pdwRecordCount);
BOOL Hlp_GetRecordCount_FromDBOidEx(CEGUID& VolGUID, CEOID DBOid, DWORD *pdwRecordCount);
void Hlp_DisplayMemoryInfo(void);
DWORD Hlp_GetFreeStorePages(void);
DWORD Hlp_GetFreePrgmPages(void);
BOOL Hlp_EatAll_Prgm_MemEx(DWORD dwNumPagesToLeave);
BOOL Hlp_SetMemPercentage(DWORD dwStorePcent, DWORD dwRamPcent);

#endif


