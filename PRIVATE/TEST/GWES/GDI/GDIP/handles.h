//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <string>
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
HDC myCreateSurface(TSTRING tsDeviceName, TSTRING tsSurfaceType, UINT *nSurfaceBitDepth);
BOOL myDeleteSurface(HDC hSurface, TSTRING tsSurfaceType);
HBRUSH myCreateBrush(TSTRING tsBrushName);
HPEN myCreatePen(TSTRING tsPenName);
HFONT myCreateFont(DWORD dwHeight, DWORD dwWidth, DWORD dwEscapement, 
                    TSTRING tsWeight, BOOL bItalic, BOOL bUnderline, BOOL bStrikeOut, 
                    TSTRING tsCharSet, TSTRING tsOutPrecision, TSTRING tsClipPrecision, 
                    TSTRING tsQuality, TSTRING tsPitch, TSTRING tsFamily, TSTRING tsFaceName);

#endif
