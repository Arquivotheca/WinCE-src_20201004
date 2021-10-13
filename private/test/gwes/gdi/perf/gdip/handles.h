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
#include <string>
#include "tuxmain.h"
#include "testsuite.h"
#include "helpers.h"

#ifndef HANDLES_H
#define HANDLES_H

#define BGROFFSET 10000
#define RGBEMPTY 0
#define MAXRGBDESCRIPTORLENGTH 4

typedef union tagMYRGBQUAD { 
    struct {
      BYTE rgbBlue;
      BYTE rgbGreen;
      BYTE rgbRed;
      BYTE rgbReserved;
    };
    int rgbMask;
} MYRGBQUAD;

struct MYBITMAPINFO {
    BITMAPINFOHEADER bmih;
    MYRGBQUAD ct[256];
};

void setRGBQUAD(MYRGBQUAD *rgbq, int r, int g, int b);
HDC myCreateSurface(TSTRING tsDeviceName, TSTRING tsSurfaceType, UINT *nSurfaceBitDepth, HWND *hWnd);
BOOL myDeleteSurface(HDC hSurface, TSTRING tsSurfaceType, HWND hWnd);
HBRUSH myCreateBrush(TSTRING tsBrushName);
HPEN myCreatePen(TSTRING tsPenName);
HFONT myCreateFont(DWORD dwHeight, DWORD dwWidth, DWORD dwEscapement, 
                    TSTRING tsWeight, BOOL bItalic, BOOL bUnderline, BOOL bStrikeOut, 
                    TSTRING tsCharSet, TSTRING tsOutPrecision, TSTRING tsClipPrecision, 
                    TSTRING tsQuality, TSTRING tsPitch, TSTRING tsFamily, TSTRING tsFaceName);

#endif
