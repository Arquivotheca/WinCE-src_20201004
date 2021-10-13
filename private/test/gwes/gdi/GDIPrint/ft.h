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

#ifndef __FT_H__
#define __FT_H__

#include <katoex.h>
#include <tux.h>

// Test function prototypes (TestProc's)
TESTPROCAPI BitBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI MaskBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI StretchBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI PatBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI TransparentBlt_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI AlphaBlend_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI StretchDIBits_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI SetDIBitsToDevice_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI GradientFill_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI FillRect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI PolyLine_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Polygon_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI LineTo_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Rectangle_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI RoundRect_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI Ellipse_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI DrawText_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI ExtTextOut_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
   TEXT("GDIPrint tests"), 0, 0, 0, NULL,
   TEXT("BitBlt_T"), 1, 0, 100, BitBlt_T,
   TEXT("MaskBlt_T"), 1, 0, 101, MaskBlt_T,
   TEXT("StretchBlt_T"), 1, 0, 102, StretchBlt_T,
   TEXT("PatBlt_T"), 1, 0, 103, PatBlt_T,
   TEXT("TransparentBlt_T"), 1, 0, 104, TransparentBlt_T,
   TEXT("AlphaBlend_T"), 1, 0, 105, AlphaBlend_T,
   TEXT("StretchDIBits_T"), 1, 0, 106, StretchDIBits_T,
   TEXT("SetDIBitsToDevice_T"), 1, 0, 107, SetDIBitsToDevice_T,
   TEXT("GradientFill_T"), 1, 0, 108, GradientFill_T,
   TEXT("FillRect_T"), 1, 0, 109, FillRect_T,
   TEXT("PolyLine_T"), 1, 0, 110, PolyLine_T,
   TEXT("Polygon_T"), 1, 0, 111, Polygon_T,
   TEXT("LineTo_T"), 1, 0, 112, LineTo_T,
   TEXT("Rectangle_T"), 1, 0, 113, Rectangle_T,
   TEXT("RoundRect_T"), 1, 0, 114, RoundRect_T,
   TEXT("Ellipse_T"), 1, 0, 115, Ellipse_T,
   TEXT("DrawText_T"), 1, 0, 116, DrawText_T,
   TEXT("ExtTextOut_T"), 1, 0, 117, ExtTextOut_T,
   NULL, 0, 0, 0, NULL  // marks end of list
};

#endif // __TUXDEMO_H__
