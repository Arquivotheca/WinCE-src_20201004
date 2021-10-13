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
/*****************************************************************************\
 *
 *  file:           windowsqa.h
 *  date:           12/01/98
 *  author:         johndog
 *
 *  Purpose:        Try to bridge the gaps between the multiple desktop and CE platform SDKs
 *
 *
\*****************************************************************************/

#pragma once

#pragma warning(disable:4786)


////////////////////////////////////////////////
// Phase 1
////////////////////////////////////////////////

#ifdef UNDER_CE

////////////////////////////////////////////////
// Phase 1A - Device only
////////////////////////////////////////////////


#include <windows.h>

#include <ossvcsqa.h>

#include <bldver.h>



#define _TRY_       __try
#define _CATCH_     __except (EXCEPTION_EXECUTE_HANDLER )

//#ifndef	VK_ACTION
//#define VK_ACTION   0x86
//#endif



#if CE_MAJOR_VER >= 0x0004
#define U_rclose(x) (CloseHandle((HANDLE)x))
#else
// LMEM is not supported under Wince 3.0
#undef LocalAlloc
#endif


#ifdef NO_WINDOWING_SUPPORT
static 
int
WINAPI
MessageBox(
    HWND hWnd ,
    LPCTSTR lpText,
    LPCTSTR lpCaption,
    UINT uType)
{
    return 0;
}

#define MB_OK  0
#endif





#include <ossvcsqa.h>


#else // UNDER_CE ^ / v DESKTOP

////////////////////////////////////////////////
// Phase 1B - Desktop only
////////////////////////////////////////////////


// Propagate DEBUG to _DEBUG
#ifdef DEBUG
#ifndef NDEBUG
#ifndef _DEBUG
#define _DEBUG 1
#endif //!_DEBUG
#endif // !NDEBUG
#endif //!DEBUG

// Propagate _DEBUG to DEBUG
#ifdef _DEBUG
#ifndef DEBUG
#define DEBUG
#endif // !DEBUG
#endif //_DEBUG


#ifdef MFC
#include <afx.h>
#include <afxdisp.h>
#include <new>
#include <memory>
#else
#include <windows.h>
#include <wtypes.h>
#endif

#include <stdio.h>

#include <tchar.h>


#if defined(_DEBUG) && defined(MFC)
#define new DEBUG_NEW
#endif



#define RETAILMSG(cond,printf_exp)   \
   ((cond)?(LSTrace printf_exp),1:0)


#define NotifyWinUserSystem(dw);
#define NWUS_DOUBLE_TAP_CHANGED


// Don't use qa macros
#define QACTYPE_h

typedef HANDLE  HTHREAD;
typedef HANDLE  HPROCESS;

typedef DWORD CEOID;

typedef unsigned long ulong;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef unsigned int uint;


#define _TRY_       try
#define _CATCH_     catch (...)


#define QASetWindowsJournalHook(a,b,c)
#define QAUnhookWindowsJournalHook(a)

//#undef sndPlaySound
//#undef SND_ASYNC
//#define sndPlaySound(a,b)
//#define SND_ASYNC 0x0001 




#ifndef COMPILETIME_ASSERT
#define COMPILETIME_ASSERT(f) switch (0) case 0: case f:
#endif



//////////////////////////////
// Virtual key codes
//////////////////////////////

#define VK_SEMICOLON        0xBA
#define VK_EQUAL            0xBB
#define VK_COMMA            0xBC
#define VK_HYPHEN           0xBD
#define VK_PERIOD           0xBE
#define VK_SLASH            0xBF
#define VK_BACKQUOTE        0xC0

#define VK_LBRACKET         0xDB
#define VK_BACKSLASH        0xDC
#define VK_RBRACKET         0xDD
#define VK_APOSTROPHE       0xDE

#define VK_NOCONVERT        VK_NONCONVERT

#define VK_OFF          0xDF

#endif // DESKTOP


////////////////////////////////////////////////
// Phase 2 - Both Desktop and Device
////////////////////////////////////////////////

#undef MyDebugBreak
#define MyDebugBreak()   DebugBreak()

#define LOGMSG(cond,printf_exp)   \
   ((cond)?(NKDbgPrintfW printf_exp),1:0)

#ifdef DEBUG


#undef GUTSTEXT
#ifdef _UNICODE
#define __GUTSTEXT(quote) L##quote
#define GUTSTEXT(quote) __GUTSTEXT(quote)
#else
#define GUTSTEXT(quote) quote
#endif

#undef ASSERT
#define ASSERT(x) \
    if (!(x)) {    \
        RETAILMSG(1, (GUTSTEXT("ASSERT FAILURE at %s line %d\r\n"), GUTSTEXT(__FILE__), __LINE__)); \
        MyDebugBreak(); \
    } else {}
#else

#undef ASSERT
#define ASSERT(x)

#endif

#ifdef __cplusplus
extern "C" {
#endif
extern BOOL     LSTrace( LPCTSTR lpszFormat, ... );
#ifdef __cplusplus
}
#endif



#ifdef DEBUG
#undef TRACE
#define TRACE LSTrace
#elif !defined(TRACE)
#define TRACE
#endif


#define CountOf( x )        ( sizeof( x ) / sizeof( *x ) )
#define ZeroObject( x )        ( ZeroMemory( &x, sizeof( x ) ))

#ifdef DEBUG
#define DBG_INIT(dw) (((DWORD&)dw) = 0xBAADF00D)
#else
#define DBG_INIT(dw) 
#endif

#pragma warning(disable:4786)

#define WIN32_FROM_HRESULT(x) \
    ((HRESULT_FACILITY(x) == FACILITY_WIN32) ? ((DWORD)((x) & 0x0000FFFF)) : (x))

