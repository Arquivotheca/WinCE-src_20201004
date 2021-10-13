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

