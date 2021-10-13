//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "testsuite.h"
#include "bencheng.h"
#include "bitblt.h"
#include "patblt.h"
#include "fillrgn.h"
#include "stretchblt.h"
#include "maskblt.h"
#include "alphablend.h"
#include "transparentblt.h"
#include "getpixel.h"
#include "setpixel.h"
#include "rectangle.h"
#include "ellipse.h"
#include "roundrect.h"
#include "lineto.h"
#include "polyline.h"
#include "polygon.h"
#include "exttextout.h"
#include "drawtext.h"

CTestSuite *
CreateTestSuite(TSTRING tsName, CSection * SectionData)
{
    BOOL bRVal = TRUE;
    CTestSuite *TestHolder = NULL;

    if(tsName == TEXT("BitBlt"))
        TestHolder = new (CBitBltTestSuite)(SectionData);
    else if(tsName == TEXT("StretchBlt"))
        TestHolder = new (CStretchBltTestSuite)(SectionData);
    else if(tsName == TEXT("MaskBlt"))
        TestHolder = new (CMaskBltTestSuite)(SectionData);
    else if(tsName == TEXT("PatBlt"))
        TestHolder = new (CPatBltTestSuite)(SectionData);
    else if(tsName == TEXT("FillRgn"))
        TestHolder = new (CFillRgnTestSuite)(SectionData);
    else if(tsName == TEXT("AlphaBlend"))
        TestHolder = new (CAlphaBlendTestSuite)(SectionData);
    else if(tsName == TEXT("TransparentBlt") || tsName == TEXT("TransparentImage"))
        TestHolder = new (CTransparentBltTestSuite)(SectionData);
    else if(tsName == TEXT("GetPixel"))
        TestHolder = new (CGetPixelTestSuite)(SectionData);
    else if(tsName == TEXT("SetPixel"))
        TestHolder = new (CSetPixelTestSuite)(SectionData);
    else if(tsName == TEXT("Rectangle"))
        TestHolder = new (CRectangleTestSuite)(SectionData);
    else if(tsName == TEXT("Ellipse"))
        TestHolder = new (CEllipseTestSuite)(SectionData);
    else if(tsName == TEXT("RoundRect"))
        TestHolder = new (CRoundRectTestSuite)(SectionData);
    else if(tsName == TEXT("LineTo"))
        TestHolder = new (CLineToTestSuite)(SectionData);
    else if(tsName == TEXT("Polyline"))
        TestHolder = new (CPolylineTestSuite)(SectionData);
    else if(tsName == TEXT("Polygon"))
        TestHolder = new (CPolygonTestSuite)(SectionData);
    else if(tsName == TEXT("ExtTextOut"))
        TestHolder = new (CExtTextOutTestSuite)(SectionData);
    else if(tsName == TEXT("DrawText"))
        TestHolder = new (CDrawTextTestSuite)(SectionData);

    // else it's unknown and the Benchmark Engine will output the apporpriate error.

    return TestHolder;
}

int WINAPI
WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
#ifdef UNDER_CE
  LPWSTR    CommandLine,
#else
  LPSTR     CommandLine,
#endif
  int       ShowCommand
  )
{
    __try
    {
        return RunBenchmark(GetCommandLine());
    } 
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        OutputDebugString(TEXT("Exception in GDIP, application has been killed."));
        MessageBox(NULL, TEXT("Exception in GDIP, application has been killed."), NULL, MB_OK | MB_ICONEXCLAMATION);
        return 0;
    }
}

