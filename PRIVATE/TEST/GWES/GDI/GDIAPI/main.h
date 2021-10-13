//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __MAIN_H__
#define __MAIN_H__

// SpecDC
TESTPROCAPI Info_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI DumpDir_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// clip 
TESTPROCAPI ExcludeClipRect_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetClipBox_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetClipRgn_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI IntersectClipRect_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SelectClipRgn_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Draw
TESTPROCAPI Rectangle_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI FillRect_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RectVisible_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BitBlt_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateCompatibleBitmap_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateDIBSection_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetPixel_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PatBlt_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetPixel_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GdiFlush_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GdiSetBatchLimit_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ClientToScreen_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateBitmap_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Ellipse_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI DrawEdge_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI MaskBlt_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Polygon_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Polyline_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RoundRect_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ScreenToClient_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI StretchBlt_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TransparentImage_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI StretchDIBits_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetDIBitsToDevice_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GradientFill_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI InvertRect_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI MoveToEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI LineTo_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetDIBColorTable_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetDIBColorTable_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// brush
TESTPROCAPI CreatePatternBrush_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateSolidBrush_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetBrushOrgEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateDIBPatternBrushPt_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetSysColorBrush_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreatePenIndirect_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetSystemStockBrush_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Device Context
TESTPROCAPI CreateCompatibleDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI DeleteDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RestoreDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SaveDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ScrollDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Device Objects
TESTPROCAPI DeleteObject_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetCurrentObject_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetObject_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetObjectType_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetStockObject_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SelectObject_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Text
TESTPROCAPI ExtTextOut_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetTextAlign_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetTextExtentPoint32_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetTextFace_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetTextMetrics_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetTextAlign_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI DrawTextW_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetTextExtentPoint_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetTextExtentExPoint_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// TestChk
TESTPROCAPI CheckAllWhite_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CheckScreenHalves_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetReleaseDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Thrash_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

/*----------------------------------------------*
| Test Registration Function Table
*-----------------------------------------------*/
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    TEXT("Information"), 0, 0, 0, NULL,
    TEXT("Configuration"), 1, 0, EInfo, Info_T,

    TEXT("GDI API Groups"), 0, 0, 0, NULL,

    TEXT("Clip"), 1, 0, 0, NULL,
    TEXT("ExcludeClipRect"), 2, 0, EExcludeClipRect, ExcludeClipRect_T,
    TEXT("GetClipBox"), 2, 0, EGetClipBox, GetClipBox_T,
    TEXT("GetClipRgn"), 2, 0, EGetClipRgn, GetClipRgn_T,
    TEXT("IntersectClipRect"), 2, 0, EIntersectClipRect, IntersectClipRect_T,
    TEXT("SelectClipRgn"), 2, 0, ESelectClipRgn, SelectClipRgn_T,

    TEXT("Draw"), 1, 0, 0, NULL,
    TEXT("BitBlt"), 2, 0, EBitBlt, BitBlt_T,
    TEXT("ClientToScreen"), 2, 0, EClientToScreen, ClientToScreen_T,
    TEXT("CreateBitmap"), 2, 0, ECreateBitmap, CreateBitmap_T,
    TEXT("CreateCompatibleBitmap"), 2, 0, ECreateCompatibleBitmap, CreateCompatibleBitmap_T,
    TEXT("CreateDIBSection"), 2, 0, ECreateDIBSection, CreateDIBSection_T,
    TEXT("Ellipse"), 2, 0, EEllipse, Ellipse_T,
    TEXT("DrawEdge"), 2, 0, EDrawEdge, DrawEdge_T,
    TEXT("GetPixel"), 2, 0, EGetPixel, GetPixel_T,
    TEXT("MaskBlt"), 2, 0, EMaskBlt, MaskBlt_T,
    TEXT("PatBlt"), 2, 0, EPatBlt, PatBlt_T,
    TEXT("Polygon"), 2, 0, EPolygon, Polygon_T,
    TEXT("Polyline"), 2, 0, EPolyline, Polyline_T,
    TEXT("Rectangle"), 2, 0, ERectangle, Rectangle_T,
    TEXT("FillRect"), 2, 0, EFillRect, FillRect_T,
    TEXT("RectVisible"), 2, 0, ERectVisible, RectVisible_T,
    TEXT("RoundRect"), 2, 0, ERoundRect, RoundRect_T,
    TEXT("ScreenToClient"), 2, 0, EScreenToClient, ScreenToClient_T,
    TEXT("SetPixel"), 2, 0, ESetPixel, SetPixel_T,
    TEXT("StretchBlt"), 2, 0, EStretchBlt, StretchBlt_T,
    TEXT("TransparentImage"), 2, 0, ETransparentImage, TransparentImage_T,
    TEXT("StretchDIBit"), 2, 0, EStretchDIBits, StretchDIBits_T,
    TEXT("SetDIBitsToDevice"), 2, 0, ESetDIBitsToDevice, SetDIBitsToDevice_T,
    TEXT("GradientFill"), 2, 0, EGradientFill, GradientFill_T,
    TEXT("InvertRect"), 2, 0, EInvertRect, InvertRect_T,
    TEXT("MoveToEx"), 2, 0, EMoveToEx, MoveToEx_T,
    TEXT("LineTo"), 2, 0, ELineTo, LineTo_T,
    TEXT("GetDIBColorTable"), 2, 0, EGetDIBColorTable, GetDIBColorTable_T,
    TEXT("SetDIBColorTable"), 2, 0, ESetDIBColorTable, SetDIBColorTable_T,

    TEXT("Brush & Pens"), 1, 0, 0, NULL,
    TEXT("CreateDIBPatternBrushPt"), 2, 0, ECreateDIBPatternBrushPt, CreateDIBPatternBrushPt_T,
    TEXT("CreatePatternBrush"), 2, 0, ECreatePatternBrush, CreatePatternBrush_T,
    TEXT("CreateSolidBrush"), 2, 0, ECreateSolidBrush, CreateSolidBrush_T,
    TEXT("GetSysColorBrush"), 2, 0, EGetSysColorBrush, GetSysColorBrush_T,
    TEXT("SetBrushOrgEx"), 2, 0, ESetBrushOrgEx, SetBrushOrgEx_T,
    TEXT("CreatePenIndirect"), 2, 0, ECreatePenIndirect, CreatePenIndirect_T,
    TEXT("GetStockBrush"), 2, 0, ESystemStockBrush, GetSystemStockBrush_T,

    TEXT("Device Context"), 1, 0, 0, NULL,
    TEXT("CreateCompatibleDC"), 2, 0, ECreateCompatibleDC, CreateCompatibleDC_T,
    TEXT("DeleteDC"), 2, 0, EDeleteDC, DeleteDC_T,
    TEXT("RestoreDC"), 2, 0, ERestoreDC, RestoreDC_T,
    TEXT("SaveDC"), 2, 0, ESaveDC, SaveDC_T,
    TEXT("ScrollDC"), 2, 0, EScrollDC, ScrollDC_T,


    TEXT("Device Object"), 1, 0, 0, NULL,
    TEXT("DeleteObject"), 2, 0, EDeleteObject, DeleteObject_T,
    TEXT("GetCurrentObject"), 2, 0, EGetCurrentObject, GetCurrentObject_T,
    TEXT("GetObject"), 2, 0, EGetObject, GetObject_T,
    TEXT("GetObjectType"), 2, 0, EGetObjectType, GetObjectType_T,
    TEXT("GetStockObject"), 2, 0, EGetStockObject, GetStockObject_T,
    TEXT("SelectObject"), 2, 0, ESelectObject, SelectObject_T,

    TEXT("Text"), 1, 0, 0, NULL,
    TEXT("DrawTextW"), 2, 0, EDrawTextW, DrawTextW_T,
    TEXT("ExtTextOut"), 2, 0, EExtTextOut, ExtTextOut_T,
    TEXT("GetTextAlign"), 2, 0, EGetTextAlign, GetTextAlign_T,
    TEXT("GetTextExtentPoint"), 2, 0, EGetTextExtentPoint, GetTextExtentPoint_T,
    TEXT("GetTextExtentPoint32"), 2, 0, EGetTextExtentPoint32, GetTextExtentPoint32_T,
    TEXT("GetTextExtentExPoint"), 2, 0, EGetTextExtentExPoint, GetTextExtentExPoint_T,
    TEXT("GetTextFace"), 2, 0, EGetTextFace, GetTextFace_T,
    TEXT("GetTextMetrics"), 2, 0, EGetTextMetrics, GetTextMetrics_T,
    TEXT("SetTextAlign"), 2, 0, ESetTextAlign, SetTextAlign_T,

    TEXT("Verify Test"), 0, 0, 0, NULL,
    TEXT("CheckAllWhite"), 1, 0, ECheckAllWhite, CheckAllWhite_T,
    TEXT("CheckScreenHalves"), 1, 0, ECheckScreenHalves, CheckScreenHalves_T,
    TEXT("GetReleaseDC"), 1, 0, EGetReleaseDC, GetReleaseDC_T,
    TEXT("Thrash"), 1, 0, EThrash, Thrash_T,

    NULL, 0, 0, 0, NULL
};

#endif // header protection
