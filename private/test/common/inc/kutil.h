//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __KUTIL_H__
#define __KUTIL_H__

#include <windows.h>
#include <tux.h>
#include <katoex.h>

// short cuts
LPCTSTR RsltString (INT nRslt);		// pass, fail, etc.
void	Log (INT nVrbLvl, LPCTSTR pszFmt, ...);	
void	LogV (INT nVrbLvl, LPCTSTR pszFmt, va_list pArgs);
void	LogPass (LPCTSTR pszFmt, ...);
void	LogFail (LPCTSTR pszFmt, ...);
void	LogSkip (LPCTSTR pszFmt, ...);
void	LogStep (LPCTSTR pszFmt, ...);
void	LogAbort (LPCTSTR pszFmt, ...);
void	LogExcp (LPCTSTR pszFmt, ...);
void	LogNotice (LPCTSTR pszFmt, ...);
void	LogMemoryStatus (LPCTSTR pszTxt);
BOOL	IsPrevTestFailed ();

BOOL	AdjustMemory (INT nKbytes);

// default shell proc -- MUST HANDLE SPM_REGISTER 
INT DefaultShellProc (UINT uMsg, SPPARAM spParam);

// global data
extern	CKato *__g_pKato;
extern	LPSPS_SHELL_INFO g_pSsi;
extern	SYSTEM_INFO g_si;

// macros
//countof is defined earlier as well. undeffing it to use this file's definition.
#undef countof
#define countof(x)		(sizeof(x) / sizeof(x[0]))
#define	FILTER(x) \
    if ((x) != TPM_EXECUTE)	{ \
        return TPR_NOT_HANDLED;	\
    }

#define	FILTEREX(uMsg, tpParam, lpFTE)	\
    if (uMsg == TPM_QUERY_THREAD_COUNT) {	\
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = \
            lpFTE->dwUserData; \
        return SPR_HANDLED; \
    } else  { FILTER(uMsg) }

#define	MEMCLR(x)		memset (&x, 0, sizeof (x))

#define	LOG(x) 	LogStep (TEXT("%s ==> 0x%x"), TEXT(#x), x);

#define	CHK(x, r) \
{ \
    DWORD _dw = (DWORD) (x); \
    if (!_dw) {	\
        LogFail (TEXT("FAIL: line %d, %s ==> 0x%x (GLE: %u)"), __LINE__, TEXT(#x), _dw, GetLastError ());	\
        return (r);	\
    } \
}
#define	CHKF(x, r)	\
{ \
    DWORD _dw = (DWORD) (x); \
    if (_dw) {	\
        LogFail (TEXT("FAIL: line %d, %s ==> 0x%x"), __LINE__, TEXT(#x), _dw);	\
        return (r);	\
    } \
}

typedef struct _PARAMENTRY {
    UINT uParam;
    LPCTSTR pszParamName;
} PARAMENTRY, *PPARAMENTRY;

#define	PARAM(x)	{ x, TEXT(#x) }

// Force use of our random number generator for results that are
// consistent across all platforms.
//
// Random Object
//
typedef struct _RND {
    int     table[55];
    int     j;
    int     k;
    int     init;
} RND, *PRND;

extern int   RND_rand(PRND prnd);
extern void  RND_srand(PRND prnd, unsigned int seed);
extern int 	 RND_int(PRND prnd, int high, int low);

extern DWORD  SetMaxMem (void);

__inline int RandInt( int high, int low )
{
    return RND_int(NULL, high, low);
}

#define WIN_MOBILE 1

int CheckConfig();
void ClickOKOnErrorDialogue();

__inline BOOL UnderULDR()
{
    return (BOOL)(0xFFFFFFFF == GetFileAttributes(L"\\Windows\\Registry\\system.hv"));
}

HRESULT UnregisterTestHarness(LPCWSTR lpszTestHarnessName);

VOID ReadOOMRegKeys(DWORD *cpSystem, DWORD *cpForeground, DWORD *cpAppLow, DWORD *cpHealthy);

#endif	// __KUTIL_H__
