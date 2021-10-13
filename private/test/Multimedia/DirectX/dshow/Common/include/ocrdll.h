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
#include <windows.h>
#include <tchar.h>
#include "ocr.h"

typedef HRESULT (*PFNINITOCR)( TCHAR * tszTemplateBitmap, float Scaling );
typedef HRESULT (*PFNCLEANUPOCR)( );
typedef HRESULT (*PFNRECOGNIZEOCRCHARACTERS)(BYTE * pBits, int width, int height, int stride, int x, int y, BITMAPINFO * pSrcBMI, GUID MediaSubtype, TCHAR * String, int Length, CharacterType ct);
typedef HRESULT (*PFNRECOGNIZEOCRCHARACTERSWITHCONFIDENCE)(BYTE * pBits, int width, int height, int stride, int x, int y, BITMAPINFO * pSrcBMI, GUID MediaSubtype, TCHAR * String, double * Confidence, int StringAndConfidenceCount, double * MinConfidence, CharacterType ct);
typedef HRESULT (*PFNGETOCRFONT)( HDC hdc );
typedef HRESULT (*PFNFREEOCRFONT)( HDC hdc );
typedef HRESULT (*PFNSAVEOCRFONTASTEMPLATE)( TCHAR *tszFileName );


