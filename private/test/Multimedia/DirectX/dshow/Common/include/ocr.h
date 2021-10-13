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
#include "ocrutil.h"
#include <windows.h>
#include "mmimaging.h"

#ifndef OCR_H
#define OCR_H

#define WIDTH_THRESHOLD (int) 4
#define HEIGHT_THRESHOLD (int) 4

// if our initial comparison is below this threshold, then we're obviously mismatching so
// there's no point in continuing the comparison.
#define EARLYEXITTHRESHOLD .2

// horizontal partial mistmatches are less severe than vertical mismatches.
// if a pixel is lit and has a lit pixel to the left or right then it might be on a long vertical line that's stretched a little
// so it's mostly harmless. if the pixel above or below it are lit, then it's a littlemore severe because character
// structure is very important along the vertical. Either mistmatch is less severe than a pixel floating
// in the middle of nowhere
#define VERTICALPARTIALMISMATCH 9
#define HORIZONTALPARTIALMISMATCH 5
#define STANDARDPENALTY 10
#define STANDARDMATCH 10

// any spacing between single quotes that is at or less than 3 pixels, then
// it'll be interpreted as a double quote
#define DOUBLEQUOTESPACING 3


typedef class CDShowOCR
{
    private:
    OCRData *m_pTemplate;
    OCRData *m_pTemplateRowData;

    BYTE *m_pTemplateBitmap;
    BITMAPINFO m_bmiTemplate;

    int m_nSpaceWidth;
    float m_fScalingFactor;
    LOGFONT m_lfOCRFont;

    HRESULT OCRCharacterString(OCRData *pCharacters, CharacterType ct = PUNCTDIGITALPHA);
    HRESULT ConvolveOCRCharacter(OCRData *pTemplate, OCRData *pCharacter);
    HRESULT OCRCharacter(OCRData *pTemplate, int tx, int ty, OCRData *pCharacter, int cx, int cy);
    HRESULT InsertSpacing(OCRData *pCharacters);
    HRESULT FindCharacters( OCRData *pRowData, OCRData **ppCharacters);
    HRESULT ConstrainCharacter(OCRData *pCharacter);
    HRESULT FindRows(BYTE *pBits, int width, int height, int stride, int LeftEdge, int TopEdge, GUID MediaSubtype, OCRData **ppRows);
    HRESULT SetupTemplateFromFile(TCHAR *tszBitmapName, float Scaling);
    HRESULT SetupTemplateFromHFont(HFONT hfontTemplate, float Scaling);
    HRESULT SetupTemplateOCRData();
    HRESULT ScaleOCRFont();


    public:

    CDShowOCR();
    ~CDShowOCR();

    HRESULT Init(TCHAR *tszTemplateBitmap, float Scaling = 1.0);
    HRESULT InitFromLogfont(LOGFONT *lfTemplate, float Scaling = 1.0);

    HRESULT Cleanup();
    HRESULT RecognizeCharacters(BYTE *pBits, int width, int height, int stride, int x, int y, BITMAPINFO *pSrcBMI, GUID MediaSubtype, TCHAR *String, int Length, CharacterType ct = PUNCTDIGITALPHA);
    HRESULT RecognizeCharacters(BYTE *pBits, int width, int height, int stride, int x, int y, BITMAPINFO *pSrcBMI, GUID MediaSubtype, TCHAR *String, double *Confidence, int StringAndConfidenceCount, double *MinConfidence, CharacterType ct = PUNCTDIGITALPHA);

} DShowOCR, *pDShowOCR;

#endif

