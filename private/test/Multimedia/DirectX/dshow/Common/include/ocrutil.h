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
#include <windows.h>
#include "mmimaging.h"

#ifndef OCRUTIL_H
#define OCRUTIL_H

static TCHAR g_tszTrainingString[] = TEXT("0123456789Aa Bb CcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz!.@#$%^&*(),'\"");

#define COLORTHRESHOLD 90

// if the gray threshold is too low, then scattered gray pixels in the background will be detected
// as additional rows of periods.
#define GRAYTHRESHOLD 130

#define FONTHEIGHT 16
#define FONTWEIGHT FW_BOLD

#define FONTCHARSET ANSI_CHARSET
#define FONTQUALITY NONANTIALIASED_QUALITY
#define FONTPITCHANDFAMILY FF_MODERN
#define FONTFACENAME TEXT("Tahoma")
#ifdef UNDER_CE
#define EXTRA_EXTRA 1
#else
#define EXTRA_EXTRA 0
#endif

#define EXTRASPACING 4

#define BACKGROUNDCOLOR RGB(0,0,0)
#define TEXTCOLOR RGB(255,255,255)

enum CharacterType {
    ALPHA=0x1,
    DIGIT=0x2,
    DIGITALPHA=0x3,
    PUNCT=0x4,
    PUNCTALPHA=0x5,
    PUNCTDIGIT=0x6,
    PUNCTDIGITALPHA=0x7
};

typedef struct _OCRDATA {
    DWORD PositiveWeighting;
    DWORD NegativeWeighting;
    double Confidence;

    TCHAR Character;

    BYTE *pBits;
    int stride;
    GUID MediaSubType;
    int Width;
    int Height;

    int LeftEdge;
    int TopEdge;
    
    struct _OCRDATA *pNext;
    struct _OCRDATA *pPrev;

    struct _OCRDATA *pParentSurface;
    struct _OCRDATA *pCurrentMatch;
} OCRData;

HRESULT ThresholdImage(BYTE *pBits, int width, int height, int stride, int x, int y, GUID MediaSubtype);
HRESULT Blur(BYTE *pBits, int width, int height, int stride, int x, int y, GUID MediaSubtype);
HRESULT NoiseFilter(BYTE *pBits, int width, int height, int stride, int x, int y, GUID MediaSubtype);
HRESULT InsertOCRData(OCRData *pData, OCRData *pInsert);
HRESULT AppendOCRData(OCRData **pData, OCRData *pAppend);
HRESULT FreeOCRData(OCRData *pData);

int CalculateCharacterExtra(HFONT hfnt);
HRESULT SetupHDCForOCR(HDC hdc);
HRESULT GetOCRLogfont(LOGFONT *lf);
HRESULT GetOCRFontFromLogfont(HDC hdc, const LOGFONT *lf);
HRESULT GetOCRFont(HDC hdc);
HRESULT FreeOCRFont(HDC hdc);
HRESULT SaveOCRFontAsTemplate(TCHAR *tszFileName, LOGFONT *lfFont = NULL);


#endif

