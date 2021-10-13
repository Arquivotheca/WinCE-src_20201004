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
#ifndef __MAIN_H__
#define __MAIN_H__

// ***************** utility ****************
TESTPROCAPI LoopPrimarySurface_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// ***************** APIs *****************
// pal
TESTPROCAPI GetNearestPaletteIndex_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetNearestColor_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetSystemPaletteEntries_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetPaletteEntries_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetPaletteEntries_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreatePalette_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SelectPalette_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RealizePalette_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

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
TESTPROCAPI SetBitmapBits_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI DrawFocusRect_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetROP2_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI AlphaBlend_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI MapWindowPoints_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetROP2_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Gdi
TESTPROCAPI GdiFlush_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GdiSetBatchLimit_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ChangeDisplaySettingsEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Region
TESTPROCAPI CombineRgn_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateRectRgn_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateRectRgnIndirect_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI FillRgn_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetRegionData_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetRgnBox_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI OffsetRgn_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI PtInRegion_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RectInRegion_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetRectRgn_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EqualRgn_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI FrameRgn_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ExtCreateRegion_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// brush
TESTPROCAPI CreatePatternBrush_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateSolidBrush_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetBrushOrgEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetBrushOrgEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateDIBPatternBrushPt_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetSysColorBrush_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreatePenIndirect_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetSystemStockBrush_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreatePen_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Device Attributes
TESTPROCAPI GetBkColor_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetBkMode_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetTextColor_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetBkColor_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetBkMode_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetTextColor_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetViewportOrgEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetStretchBltMode_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetStretchBltMode_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetTextCharacterExtra_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetTextCharacterExtra_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetLayout_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetLayout_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetViewportOrgEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetViewportExtEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetWindowOrgEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetWindowOrgEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI OffsetViewportOrgEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetWindowExtEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Device Context
TESTPROCAPI CreateCompatibleDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI DeleteDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetDCOrgEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetDeviceCaps_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RestoreDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SaveDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ScrollDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ExtEscape_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BitmapEscape_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Device Objects
TESTPROCAPI DeleteObject_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetCurrentObject_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetObject_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetObjectType_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetStockObject_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SelectObject_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Fonts
TESTPROCAPI AddFontResource_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateFontIndirect_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EnumFontFamilies_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EnumFontFamProc_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetCharABCWidths_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetCharWidth_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetCharWidth32_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RemoveFontResource_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EnumFonts_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EnableEUDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TranslateCharsetInfo_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EnumFontFamiliesEx_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

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

// Print
TESTPROCAPI StartDoc_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EndDoc_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI StartPage_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EndPage_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI AbortDoc_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetAbortProc_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Manual
TESTPROCAPI ManualFont_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ManualFontClipped_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// TestChk
TESTPROCAPI CheckAllWhite_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CheckScreenHalves_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetReleaseDC_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Thrash_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CheckDriverVerify_T(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

/*----------------------------------------------*
| Test Registration Function Table
*-----------------------------------------------*/
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    TEXT("GDI API Groups"), 0, 0, 0, NULL,

    TEXT("Clip"), 0, 0, 100, NULL,
    TEXT("ExcludeClipRect"), 1, (unsigned long) ExcludeClipRect_T, EExcludeClipRect, LoopPrimarySurface_T,
    TEXT("GetClipBox"), 1, (unsigned long) GetClipBox_T, EGetClipBox, LoopPrimarySurface_T,
    TEXT("GetClipRgn"), 1, (unsigned long) GetClipRgn_T, EGetClipRgn, LoopPrimarySurface_T,
    TEXT("IntersectClipRect"), 1, (unsigned long) IntersectClipRect_T, EIntersectClipRect, LoopPrimarySurface_T,
    TEXT("SelectClipRgn"), 1, (unsigned long) SelectClipRgn_T, ESelectClipRgn, LoopPrimarySurface_T,

    TEXT("Draw"), 0, 0, 200, NULL,
    TEXT("BitBlt"), 1, (unsigned long) BitBlt_T, EBitBlt, LoopPrimarySurface_T,
    TEXT("ClientToScreen"), 1, (unsigned long) ClientToScreen_T, EClientToScreen, LoopPrimarySurface_T,
    TEXT("CreateBitmap"), 1, (unsigned long) CreateBitmap_T, ECreateBitmap, LoopPrimarySurface_T,
    TEXT("CreateCompatibleBitmap"), 1, (unsigned long) CreateCompatibleBitmap_T, ECreateCompatibleBitmap, LoopPrimarySurface_T,
    TEXT("CreateDIBSection"), 1, (unsigned long) CreateDIBSection_T, ECreateDIBSection, LoopPrimarySurface_T,
    TEXT("Ellipse"), 1, (unsigned long) Ellipse_T, EEllipse, LoopPrimarySurface_T,
    TEXT("DrawEdge"), 1, (unsigned long) DrawEdge_T, EDrawEdge, LoopPrimarySurface_T,
    TEXT("GetPixel"), 1, (unsigned long) GetPixel_T, EGetPixel, LoopPrimarySurface_T,
    TEXT("MaskBlt"), 1, (unsigned long) MaskBlt_T, EMaskBlt, LoopPrimarySurface_T,
    TEXT("PatBlt"), 1, (unsigned long) PatBlt_T, EPatBlt, LoopPrimarySurface_T,
    TEXT("Polygon"), 1, (unsigned long) Polygon_T, EPolygon, LoopPrimarySurface_T,
    TEXT("Polyline"), 1, (unsigned long) Polyline_T, EPolyline, LoopPrimarySurface_T,
    TEXT("Rectangle"), 1, (unsigned long) Rectangle_T, ERectangle, LoopPrimarySurface_T,
    TEXT("FillRect"), 1, (unsigned long) FillRect_T, EFillRect, LoopPrimarySurface_T,
    TEXT("RectVisible"), 1, (unsigned long) RectVisible_T, ERectVisible, LoopPrimarySurface_T,
    TEXT("RoundRect"), 1, (unsigned long) RoundRect_T, ERoundRect, LoopPrimarySurface_T,
    TEXT("ScreenToClient"), 1, (unsigned long) ScreenToClient_T, EScreenToClient, LoopPrimarySurface_T,
    TEXT("SetPixel"), 1, (unsigned long) SetPixel_T, ESetPixel, LoopPrimarySurface_T,
    TEXT("StretchBlt"), 1, (unsigned long) StretchBlt_T, EStretchBlt, LoopPrimarySurface_T,
    TEXT("TransparentImage"), 1, (unsigned long) TransparentImage_T, ETransparentImage, LoopPrimarySurface_T,
    TEXT("StretchDIBits"), 1, (unsigned long) StretchDIBits_T, EStretchDIBits, LoopPrimarySurface_T,
    TEXT("SetDIBitsToDevice"), 1, (unsigned long) SetDIBitsToDevice_T, ESetDIBitsToDevice, LoopPrimarySurface_T,
    TEXT("GradientFill"), 1, (unsigned long) GradientFill_T, EGradientFill, LoopPrimarySurface_T,
    TEXT("InvertRect"), 1, (unsigned long) InvertRect_T, EInvertRect, LoopPrimarySurface_T,
    TEXT("MoveToEx"), 1, (unsigned long) MoveToEx_T, EMoveToEx, LoopPrimarySurface_T,
    TEXT("LineTo"), 1, (unsigned long) LineTo_T, ELineTo, LoopPrimarySurface_T,
    TEXT("GetDIBColorTable"), 1, (unsigned long) GetDIBColorTable_T, EGetDIBColorTable, LoopPrimarySurface_T,
    TEXT("SetDIBColorTable"), 1, (unsigned long) SetDIBColorTable_T, ESetDIBColorTable, LoopPrimarySurface_T,
    TEXT("SetBitmapBits"), 1, (unsigned long) SetBitmapBits_T, ESetBitmapBits, LoopPrimarySurface_T,
    TEXT("DrawFocusRect"), 1, (unsigned long) DrawFocusRect_T, EDrawFocusRect, LoopPrimarySurface_T,
    TEXT("SetROP2"), 1, (unsigned long) SetROP2_T, ESetROP2, LoopPrimarySurface_T,
    TEXT("AlphaBlend"), 1, (unsigned long) AlphaBlend_T, EAlphaBlend, LoopPrimarySurface_T,
    TEXT("MapWindowPoints"), 1, (unsigned long) MapWindowPoints_T, EMapWindowPoints, LoopPrimarySurface_T,
    TEXT("GetROP2"), 1, (unsigned long) GetROP2_T, EGetROP2, LoopPrimarySurface_T,

    TEXT("Palette"), 0, 0, 300, NULL,
    TEXT("GetNearestColor"), 1, (unsigned long) GetNearestColor_T, EGetNearestColor, LoopPrimarySurface_T,
    TEXT("GetNearestPaletteIndex"), 1, (unsigned long) GetNearestPaletteIndex_T, EGetNearestPaletteIndex, LoopPrimarySurface_T,
    TEXT("GetSystemPaletteEntries"), 1, (unsigned long) GetSystemPaletteEntries_T, EGetSystemPaletteEntries, LoopPrimarySurface_T,
    TEXT("GetPaletteEntries"), 1, (unsigned long) GetPaletteEntries_T, EGetPaletteEntries, LoopPrimarySurface_T,
    TEXT("SetPaletteEntries"), 1, (unsigned long) SetPaletteEntries_T, ESetPaletteEntries, LoopPrimarySurface_T,
    TEXT("CreatePalette"), 1, (unsigned long) CreatePalette_T, ECreatePalette, LoopPrimarySurface_T,
    TEXT("SelectPalette"), 1, (unsigned long) SelectPalette_T, ESelectPalette, LoopPrimarySurface_T,
    TEXT("RealizePalette"), 1, (unsigned long) RealizePalette_T, ERealizePalette, LoopPrimarySurface_T,

    TEXT("GDI"), 0, 0, 400, NULL,
    TEXT("GdiFlush"), 1, (unsigned long) GdiFlush_T, EGdiFlush, LoopPrimarySurface_T,
    TEXT("GdiSetBatchLimit"), 1, (unsigned long) GdiSetBatchLimit_T, EGdiSetBatchLimit, LoopPrimarySurface_T,
    TEXT("ChangeDisplaySettingsEx"), 1, (unsigned long) ChangeDisplaySettingsEx_T, EChangeDisplaySettingsEx, LoopPrimarySurface_T,

    TEXT("Region"), 0, 0, 500, NULL,
    TEXT("CombineRgn"), 1, (unsigned long) CombineRgn_T, ECombineRgn, LoopPrimarySurface_T,
    TEXT("CreateRectRgn"), 1, (unsigned long) CreateRectRgn_T, ECreateRectRgn, LoopPrimarySurface_T,
    TEXT("CreateRectRgnIndirect"), 1, (unsigned long) CreateRectRgnIndirect_T, ECreateRectRgnIndirect, LoopPrimarySurface_T,
    TEXT("EqualRgn"), 1, (unsigned long) EqualRgn_T, EEqualRgn, LoopPrimarySurface_T,
    TEXT("FillRgn"), 1, (unsigned long) FillRgn_T, EFillRgn, LoopPrimarySurface_T,
    TEXT("FrameRgn"), 1, (unsigned long) FrameRgn_T, EFrameRgn, LoopPrimarySurface_T,
    TEXT("GetRegionData"), 1, (unsigned long) GetRegionData_T, EGetRegionData, LoopPrimarySurface_T,
    TEXT("GetRgnBox"), 1, (unsigned long) GetRgnBox_T, EGetRgnBox, LoopPrimarySurface_T,
    TEXT("OffsetRgn"), 1, (unsigned long) OffsetRgn_T, EOffsetRgn, LoopPrimarySurface_T,
    TEXT("PtInRegion"), 1, (unsigned long) PtInRegion_T, EPtInRegion, LoopPrimarySurface_T,
    TEXT("RectInRegion"), 1, (unsigned long) RectInRegion_T, ERectInRegion, LoopPrimarySurface_T,
    TEXT("SetRectRgn"), 1, (unsigned long) SetRectRgn_T, ESetRectRgn, LoopPrimarySurface_T,
    TEXT("ExtCreateRegion"), 1, (unsigned long) ExtCreateRegion_T, EExtCreateRegion, LoopPrimarySurface_T,

    TEXT("Brush and Pens"), 0, 0, 600, NULL,
    TEXT("CreateDIBPatternBrushPt"), 1, (unsigned long) CreateDIBPatternBrushPt_T, ECreateDIBPatternBrushPt, LoopPrimarySurface_T,
    TEXT("CreatePatternBrush"), 1, (unsigned long) CreatePatternBrush_T, ECreatePatternBrush, LoopPrimarySurface_T,
    TEXT("CreateSolidBrush"), 1, (unsigned long) CreateSolidBrush_T, ECreateSolidBrush, LoopPrimarySurface_T,
    TEXT("GetBrushOrgEx"), 1, (unsigned long) GetBrushOrgEx_T, EGetBrushOrgEx, LoopPrimarySurface_T,
    TEXT("GetSysColorBrush"), 1, (unsigned long) GetSysColorBrush_T, EGetSysColorBrush, LoopPrimarySurface_T,
    TEXT("SetBrushOrgEx"), 1, (unsigned long) SetBrushOrgEx_T, ESetBrushOrgEx, LoopPrimarySurface_T,
    TEXT("CreatePenIndirect"), 1, (unsigned long) CreatePenIndirect_T, ECreatePenIndirect, LoopPrimarySurface_T,
    TEXT("GetSystemStockBrush"), 1, (unsigned long) GetSystemStockBrush_T, ESystemStockBrush, LoopPrimarySurface_T,
    TEXT("CreatePen"), 1, (unsigned long) CreatePen_T, ECreatePen, LoopPrimarySurface_T,

    TEXT("Device Attribute"), 0, 0, 700, NULL,
    TEXT("GetBkColor"), 1, (unsigned long) GetBkColor_T, EGetBkColor, LoopPrimarySurface_T,
    TEXT("GetBkMode"), 1, (unsigned long) GetBkMode_T, EGetBkMode, LoopPrimarySurface_T,
    TEXT("GetTextColor"), 1, (unsigned long) GetTextColor_T, EGetTextColor, LoopPrimarySurface_T,
    TEXT("SetBkColor"), 1, (unsigned long) SetBkColor_T, ESetBkColor, LoopPrimarySurface_T,
    TEXT("SetBkMode"), 1, (unsigned long) SetBkMode_T, ESetBkMode, LoopPrimarySurface_T,
    TEXT("SetTextColor"), 1, (unsigned long) SetTextColor_T, ESetTextColor, LoopPrimarySurface_T,
    TEXT("SetViewportOrgEx"), 1, (unsigned long) SetViewportOrgEx_T, ESetViewportOrgEx, LoopPrimarySurface_T,
    TEXT("SetStretchBltMode"), 1, (unsigned long) SetStretchBltMode_T, ESetStretchBltMode, LoopPrimarySurface_T,
    TEXT("GetStretchBltMode"), 1, (unsigned long) GetStretchBltMode_T, EGetStretchBltMode, LoopPrimarySurface_T,
    TEXT("SetTextCharacterExtra"), 1, (unsigned long) SetTextCharacterExtra_T, ESetTextCharacterExtra, LoopPrimarySurface_T,
    TEXT("GetTextCharacterExtra"), 1, (unsigned long) GetTextCharacterExtra_T, EGetTextCharacterExtra, LoopPrimarySurface_T,
    TEXT("SetLayout"), 1, (unsigned long) SetLayout_T, ESetLayout, LoopPrimarySurface_T,
    TEXT("GetLayout"), 1, (unsigned long) GetLayout_T, EGetLayout, LoopPrimarySurface_T,
    TEXT("GetViewportOrgEx"), 1, (unsigned long) GetViewportOrgEx_T, EGetViewportOrgEx, LoopPrimarySurface_T,
    TEXT("GetViewportExtEx"), 1, (unsigned long) GetViewportExtEx_T, EGetViewportExtEx, LoopPrimarySurface_T,
    TEXT("SetWindowOrgEx"), 1, (unsigned long) SetWindowOrgEx_T, ESetWindowOrgEx, LoopPrimarySurface_T,
    TEXT("GetWindowOrgEx"), 1, (unsigned long) GetWindowOrgEx_T, EGetWindowOrgEx, LoopPrimarySurface_T,
    TEXT("OffsetViewportOrgEx"), 1, (unsigned long) OffsetViewportOrgEx_T, EOffsetViewportOrgEx, LoopPrimarySurface_T,
    TEXT("GetWindowExtEx"), 1, (unsigned long) GetWindowExtEx_T, EGetWindowExtEx, LoopPrimarySurface_T,

    TEXT("Device Context"), 0, 0, 800, NULL,
    TEXT("CreateCompatibleDC"), 1, (unsigned long) CreateCompatibleDC_T, ECreateCompatibleDC, LoopPrimarySurface_T,
    TEXT("DeleteDC"), 1, (unsigned long) DeleteDC_T, EDeleteDC, LoopPrimarySurface_T,
    TEXT("GetDCOrgEx"), 1, (unsigned long) GetDCOrgEx_T, EGetDCOrgEx, LoopPrimarySurface_T,
    TEXT("GetDeviceCaps"), 1, (unsigned long) GetDeviceCaps_T, EGetDeviceCaps, LoopPrimarySurface_T,
    TEXT("RestoreDC"), 1, (unsigned long) RestoreDC_T, ERestoreDC, LoopPrimarySurface_T,
    TEXT("SaveDC"), 1, (unsigned long) SaveDC_T, ESaveDC, LoopPrimarySurface_T,
    TEXT("ScrollDC"), 1, (unsigned long) ScrollDC_T, EScrollDC, LoopPrimarySurface_T,
    TEXT("CreateDC"), 1, (unsigned long) CreateDC_T, ECreateDC, LoopPrimarySurface_T,
    TEXT("ExtEscape"), 1, (unsigned long) ExtEscape_T, EExtEscape, LoopPrimarySurface_T,
    TEXT("BitmapEscape"), 1, (unsigned long) BitmapEscape_T, EBitmapEscape, LoopPrimarySurface_T,

    TEXT("Device Object"), 0, 0, 900, NULL,
    TEXT("DeleteObject"), 1, (unsigned long) DeleteObject_T, EDeleteObject, LoopPrimarySurface_T,
    TEXT("GetCurrentObject"), 1, (unsigned long) GetCurrentObject_T, EGetCurrentObject, LoopPrimarySurface_T,
    TEXT("GetObject"), 1, (unsigned long) GetObject_T, EGetObject, LoopPrimarySurface_T,
    TEXT("GetObjectType"), 1, (unsigned long) GetObjectType_T, EGetObjectType, LoopPrimarySurface_T,
    TEXT("GetStockObject"), 1, (unsigned long) GetStockObject_T, EGetStockObject, LoopPrimarySurface_T,
    TEXT("SelectObject"), 1, (unsigned long) SelectObject_T, ESelectObject, LoopPrimarySurface_T,

    TEXT("Font"), 0, 0, 1000, NULL,
    TEXT("AddFontResource"), 1, (unsigned long) AddFontResource_T, EAddFontResource, LoopPrimarySurface_T,
    TEXT("CreateFontIndirect"), 1, (unsigned long) CreateFontIndirect_T, ECreateFontIndirect, LoopPrimarySurface_T,
    TEXT("EnumFonts"), 1, (unsigned long) EnumFonts_T, EEnumFonts, LoopPrimarySurface_T,
    TEXT("EnumFontFamilies"), 1, (unsigned long) EnumFontFamilies_T, EEnumFontFamilies, LoopPrimarySurface_T,
    TEXT("EnumFontFamProc"), 1, (unsigned long) EnumFontFamProc_T, EEnumFontFamProc, LoopPrimarySurface_T,
    TEXT("GetCharABCWidths"), 1, (unsigned long) GetCharABCWidths_T, EGetCharABCWidths, LoopPrimarySurface_T,
    TEXT("GetCharWidth"), 1, (unsigned long) GetCharWidth_T, EGetCharWidth, LoopPrimarySurface_T,
    TEXT("GetCharWidth32"), 1, (unsigned long) GetCharWidth32_T, EGetCharWidth32, LoopPrimarySurface_T,
    TEXT("RemoveFontResource"), 1, (unsigned long) RemoveFontResource_T, ERemoveFontResource, LoopPrimarySurface_T,
    TEXT("EnableEUDC"), 1, (unsigned long) EnableEUDC_T, EEnableEUDC, LoopPrimarySurface_T,
    TEXT("TranslateCharsetInfo"), 1, (unsigned long) TranslateCharsetInfo_T, ETranslateCharsetInfo, LoopPrimarySurface_T,
    TEXT("EnumFontFamiliesEx"), 1, (unsigned long) EnumFontFamiliesEx_T, EEnumFontFamiliesEx, LoopPrimarySurface_T,

    TEXT("Text"), 0, 0, 1100, NULL,
    TEXT("DrawTextW"), 1, (unsigned long) DrawTextW_T, EDrawTextW, LoopPrimarySurface_T,
    TEXT("ExtTextOut"), 1, (unsigned long) ExtTextOut_T, EExtTextOut, LoopPrimarySurface_T,
    TEXT("GetTextAlign"), 1, (unsigned long) GetTextAlign_T, EGetTextAlign, LoopPrimarySurface_T,
    TEXT("GetTextExtentPoint"), 1, (unsigned long) GetTextExtentPoint_T, EGetTextExtentPoint, LoopPrimarySurface_T,
    TEXT("GetTextExtentPoint32"), 1, (unsigned long) GetTextExtentPoint32_T, EGetTextExtentPoint32, LoopPrimarySurface_T,
    TEXT("GetTextExtentExPoint"), 1, (unsigned long) GetTextExtentExPoint_T, EGetTextExtentExPoint, LoopPrimarySurface_T,
    TEXT("GetTextFace"), 1, (unsigned long) GetTextFace_T, EGetTextFace, LoopPrimarySurface_T,
    TEXT("GetTextMetrics"), 1, (unsigned long) GetTextMetrics_T, EGetTextMetrics, LoopPrimarySurface_T,
    TEXT("SetTextAlign"), 1, (unsigned long) SetTextAlign_T, ESetTextAlign, LoopPrimarySurface_T,

    TEXT("Print"), 0, 0, 1200, NULL,
    TEXT("StartDoc"), 1, (unsigned long) StartDoc_T, EStartDoc, LoopPrimarySurface_T,
    TEXT("EndDoc"), 1, (unsigned long) EndDoc_T, EEndDoc, LoopPrimarySurface_T,
    TEXT("StartPage"), 1, (unsigned long) StartPage_T, EStartPage, LoopPrimarySurface_T,
    TEXT("EndPage"), 1, (unsigned long) EndPage_T, EEndPage, LoopPrimarySurface_T,
    TEXT("AbortDoc"), 1, (unsigned long) AbortDoc_T, EAbortDoc, LoopPrimarySurface_T,
    TEXT("SetAbortProc"), 1, (unsigned long) SetAbortProc_T, ESetAbortProc, LoopPrimarySurface_T,

    TEXT("Verify Test"), 0, 0, 1300, NULL,
    TEXT("CheckAllWhite"), 1, (unsigned long) CheckAllWhite_T, ECheckAllWhite, LoopPrimarySurface_T,
    TEXT("CheckScreenHalves"), 1, (unsigned long) CheckScreenHalves_T, ECheckScreenHalves, LoopPrimarySurface_T,
    TEXT("GetReleaseDC"), 1, (unsigned long) GetReleaseDC_T, EGetReleaseDC, LoopPrimarySurface_T,
    TEXT("Thrash"), 1, (unsigned long) Thrash_T, EThrash, LoopPrimarySurface_T,
    TEXT("CheckDriverVerify"), 1, (unsigned long) CheckDriverVerify_T, ECheckDriverVerify, LoopPrimarySurface_T,

    TEXT("Manual"), 0, 0, 1400, NULL,
    TEXT("Manual Font Tests"), 1, (unsigned long) ManualFont_T, EManualFont, LoopPrimarySurface_T,
    TEXT("Manual Font Clip Tests"), 1, (unsigned long) ManualFontClipped_T, EManualFontClip, LoopPrimarySurface_T,

    NULL, 0, 0, 0, NULL
};

#endif // header protection
