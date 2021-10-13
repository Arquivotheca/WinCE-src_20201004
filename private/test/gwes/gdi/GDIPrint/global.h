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

#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include "ft.h"
#include "printhandler.h"

#ifndef UNDER_CE
#include <stdio.h>
#endif

#define countof(a) (sizeof(a)/sizeof(*(a)))

#define  NO_MESSAGES       if (uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;

extern CKato *g_pKato;

extern PrintHandler gPrintHandler;

// universal logging
enum infoType {
   FAIL,
   ECHO,
   DETAIL,
   ABORT,
   SKIP,
};


// global GDI defines
typedef BOOL (WINAPI * PFNALPHABLEND)(HDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, HDC tdcSrc, int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, BLENDFUNCTION blendFunction);
typedef int (WINAPI * PFNGRADIENTFILL)(HDC hdc, PTRIVERTEX pVertex,ULONG nVertex,PVOID pMesh, ULONG nCount, ULONG ulMode);

typedef union tagMYRGBQUAD { 
    struct {
      BYTE rgbBlue;
      BYTE rgbGreen;
      BYTE rgbRed;
      BYTE rgbReserved;
    };
    UINT rgbMask;
} MYRGBQUAD;

struct MYBITMAPINFO {
    BITMAPINFOHEADER bmih;
    MYRGBQUAD ct[256];
};

enum PixelFormats {
    // 16bpp
    RGB16 = 1,
    RGB4444,
    RGB565,
    RGB555,
    RGB1555,

    BGR4444,
    BGR565,
    BGR555,
    BGR1555,

    // 24bpp
    RGB24,

    // 32bpp
    RGB32,
    RGB8888,
    RGB888,
    BGR8888,
    BGR888,

    // sys pal colors
    PAL1,
    PAL2,
    PAL4,
    PAL8,

    // user palette colors
    RGB1,
    RGB2,
    RGB4,
    RGB8
};

HBITMAP myCreateDIBSection(HDC hdc, int w, int h, int nIdentifier, VOID ** ppvBits, struct MYBITMAPINFO * UserBMI, int *nUsage);

/***********************************************************************************
***
***   Log Definitions
***
************************************************************************************/

#define MY_EXCEPTION          0
#define MY_FAIL               2
#define MY_ABORT              4
#define MY_SKIP               6
#define MY_NOT_IMPLEMENTED    8
#define MY_PASS              10

// in Global.cpp
extern void info(infoType, LPCTSTR,...);
extern TESTPROCAPI getCode(void);
extern HINSTANCE  globalInst;

/***********************************************************************************
***
***   Error checking macro's
***
************************************************************************************/
#define NOERRORFOUND TEXT("Detailed error information not found")

void InternalCheckNotRESULT(DWORD dwExpectedResult, DWORD dwResult, const TCHAR *FuncText, const TCHAR *File, int LineNum);
void InternalCheckForRESULT(DWORD dwExpectedResult, DWORD dwResult, const TCHAR *FuncText, const TCHAR *File, int LineNum);
void InternalCheckForLastError(DWORD dwExpectedResult, const TCHAR *File, int LineNum);


// This is for verifying that functions return something other than the failure code.
// If there was a failure, reset the last error so info and FormatMessage don't interfere with it.
#define CheckNotRESULT(__RESULT, __FUNC) \
	do{ \
		InternalCheckNotRESULT((DWORD) __RESULT, (DWORD) (__FUNC), TEXT(#__FUNC), TEXT(__FILE__), __LINE__); \
	}while(0)

// this verifies that the function returns what it's expected to return.
// If there was a failure, reset the last error so info and FormatMessage don't interfere with it.
#define CheckForRESULT(__RESULT, __FUNC) \
	do{ \
		InternalCheckForRESULT((DWORD) __RESULT, (DWORD) (__FUNC), TEXT(#__FUNC), TEXT(__FILE__), __LINE__); \
	}while(0)

// don't check the last error for printer DC's.  Metafiles don't set last errors.
#ifdef UNDER_CE
#define CheckForLastError(__RESULT) \
	do{ \
		if(!isPrinterDC(NULL)) \
			InternalCheckForLastError((DWORD) __RESULT, TEXT(__FILE__), __LINE__); \
	}while(0)
#else
#define CheckForLastError(__RESULT) 0
#endif

#endif
