////////////////////////////////////////////////////////////////////////////////
//
//  DDInts TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: ft.h
//          Declares the TUX function table and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES the function
//          table.
//
//  Revision History:
//      09/12/96    lblanco     Created for the TUX DLL Wizard.
//
////////////////////////////////////////////////////////////////////////////////

#if (!defined(__FT_H__) || defined(__GLOBALS_CPP__))
#ifndef __FT_H__
#define __FT_H__
#endif

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __DEFINE_FTE__
#undef BEGIN_FTE
#undef FTE
#undef FTH
#undef END_FTE
#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define FTH(a, b) { TEXT(a), b, 0, 0, NULL },
#define FTE(a, b, c, d, e) { TEXT(a), b, c, d, e },
#define END_FTE { NULL, 0, 0, 0, NULL } };
#else // __DEFINE_FTE__
#ifdef __GLOBALS_CPP__
#define BEGIN_FTE
#else // __GLOBALS_CPP__
#define BEGIN_FTE extern FUNCTION_TABLE_ENTRY g_lpFTE[];
#endif // __GLOBALS_CPP__
#define FTH(a, b)
#define FTE(a, b, c, d, e) TESTPROCAPI e(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
#define END_FTE
#endif // __DEFINE_FTE__

////////////////////////////////////////////////////////////////////////////////
// TUX Function table

BEGIN_FTE
#if TEST_APIS
    FTH("DirectDraw API Tests",                                 0)
    FTE(    "DirectDrawCreate",                                 1, 0, 0x0100, Test_DirectDrawCreate)
    FTE(    "DirectDrawEnumerate",                              1, 0, 0x0101, Test_DirectDrawEnumerate)
#if TEST_IDDC
    FTE(    "DirectDrawCreateClipper",                          1, 0, 0x0102, Test_DirectDrawCreateClipper)
#endif // TEST_IDDC
#endif // TEST_APIS
#if TEST_IDD
    FTH("IDirectDraw Interface Tests",                          0)
    FTE(    "IDirectDraw::AddRef/Release",                      1, 0, 0x0200, Test_IDD_AddRef_Release)
    FTE(    "IDirectDraw::Compact",                             1, 0, 0x0201, Test_IDD_Compact)
#if TEST_IDDC
    FTE(    "IDirectDraw::CreateClipper",                       1, 0, 0x0202, Test_IDD_CreateClipper)
#endif // TEST_IDDC
    FTE(    "IDirectDraw::CreatePalette",                       1, 0, 0x0203, Test_IDD_CreatePalette)
    FTE(    "IDirectDraw::CreateSurface",                       1, 0, 0x0204, Test_IDD_CreateSurface)
    FTE(    "IDirectDraw::DuplicateSurface",                    1, 0, 0x0205, Test_IDD_DuplicateSurface)
    FTE(    "IDirectDraw::EnumDisplayModes",                    1, 0, 0x0206, Test_IDD_EnumDisplayModes)
    FTE(    "IDirectDraw::EnumSurfaces",                        1, 0, 0x0207, Test_IDD_EnumSurfaces)
    FTE(    "IDirectDraw::FlipToGDISurface",                    1, 0, 0x0208, Test_IDD_FlipToGDISurface)
    FTE(    "IDirectDraw::GetCaps",                             1, 0, 0x0209, Test_IDD_GetCaps)
    FTE(    "IDirectDraw::GetDisplayMode",                      1, 0, 0x020a, Test_IDD_GetDisplayMode)
    FTE(    "IDirectDraw::GetFourCCCodes",                      1, 0, 0x020b, Test_IDD_GetFourCCCodes)
    FTE(    "IDirectDraw::GetGDISurface",                       1, 0, 0x020c, Test_IDD_GetGDISurface)
    FTE(    "IDirectDraw::GetMonitorFrequency",                 1, 0, 0x020d, Test_IDD_GetMonitorFrequency)
    FTE(    "IDirectDraw::GetScanLine",                         1, 0, 0x020e, Test_IDD_GetScanLine)
    FTE(    "IDirectDraw::GetVerticalBlankStatus/WaitForVerticalBlank",
                                                                1, 0, 0x020f, Test_IDD_GetVerticalBlankStatus_WaitForVerticalBlank)
//  FTE(    "IDirectDraw::Initialize",                          1, 0, 0x0210, Test_IDD_Initialize)
    FTE(    "IDirectDraw::QueryInterface",                      1, 0, 0x0211, Test_IDD_QueryInterface)
    FTE(    "IDirectDraw::RestoreDisplayMode",                  1, 0, 0x0212, Test_IDD_RestoreDisplayMode)
    FTE(    "IDirectDraw::SetCooperativeLevel",                 1, 0, 0x0213, Test_IDD_SetCooperativeLevel)
//  FTE(    "IDirectDraw::SetDisplayMode",                      1, 0, 0x0214, Test_IDD_SetDisplayMode)
#endif // TEST_IDD
#if TEST_IDD2
    FTH("IDirectDraw2 Interface Tests",                         0)
//  FTE(    "IDirectDraw2::AddRef/Release",                     1, 0, 0x0300, Test_IDD2_AddRef_Release)
    FTE(    "IDirectDraw2::GetAvailableVidMem",                 1, 0, 0x0301, Test_IDD2_GetAvailableVidMem)
//  FTE(    "IDirectDraw2::QueryInterface",                     1, 0, 0x0302, Test_IDD2_QueryInterface)
    FTE(    "IDirectDraw2::SetDisplayMode",                     1, 0, 0x0303, Test_IDD2_SetDisplayMode)
#endif // TEST_IDD2
#if TEST_IDD4
    FTH("IDirectDraw4 Interface Tests",                         0)
//  FTE(    "IDirectDraw4::AddRef/Release",                     1, 0, 0x0c00, Test_IDD4_AddRef_Release)
    FTE(    "IDirectDraw4::GetDeviceIdentifier",                1, 0, 0x0c01, Test_IDD4_GetDeviceIdentifier)
    FTE(    "IDirectDraw4::GetSurfaceFromDC",                   1, 0, 0x0c02, Test_IDD4_GetSurfaceFromDC)
//  FTE(    "IDirectDraw4::QueryInterface",                     1, 0, 0x0c03, Test_IDD4_QueryInterface)
    FTE(    "IDirectDraw4::RestoreAllSurfaces",                 1, 0, 0x0c04, Test_IDD4_RestoreAllSurfaces)
    FTE(    "IDirectDraw4::TestCooperativeLevel",               1, 0, 0x0c05, Test_IDD4_TestCooperativeLevel)
#ifndef KATANA
    FTE(    "IDirectDraw4::CreateSurface (client)",             1, 0, 0x0c06, Test_IDD4_CreateSurface)
#endif // KATANA
#endif // TEST_IDD4
#if TEST_IDDC
#if TEST_IDDC
    FTH("IDirectDrawClipper Interface Tests",                   0)
    FTE(    "IDirectDrawClipper::AddRef/Release",               1, 0, 0x0400, Test_IDDC_AddRef_Release)
    FTE(    "IDirectDrawClipper::Get/SetClipList",              1, 0, 0x0401, Test_IDDC_Get_SetClipList)
    FTE(    "IDirectDrawClipper::Get/SetHWnd",                  1, 0, 0x0402, Test_IDDC_Get_SetHWnd)
//  FTE(    "IDirectDrawClipper::Initialize",                   1, 0, 0x0403, Test_IDDC_Initialize)
    FTE(    "IDirectDrawClipper::IsClipListChanged",            1, 0, 0x0404, Test_IDDC_IsClipListChanged)
    FTE(    "IDirectDrawClipper::QueryInterface",               1, 0, 0x0405, Test_IDDC_QueryInterface)
#endif // TEST_IDDC
#endif // TEST_IDDC
#if TEST_IDDCC
    FTH("IDirectDrawColorControl Interface Tests",              0)
    FTE(    "IDirectDrawColorControl::AddRef/Release",          1, 0, 0x0500, Test_IDDCC_AddRef_Release)
    FTE(    "IDirectDrawColorControl::Get/SetColorControls",    1, 0, 0x0501, Test_IDDCC_Get_SetColorControls)
    FTE(    "IDirectDrawColorControl::QueryInterface",          1, 0, 0x0502, Test_IDDCC_QueryInterface)
#endif // TEST_IDDCC
#if TEST_IDDP
    FTH("IDirectDrawPalette Interface Tests",                   0)
    FTE(    "IDirectDrawPalette::AddRef/Release",               1, 0, 0x0600, Test_IDDP_AddRef_Release)
    FTE(    "IDirectDrawPalette::GetCaps",                      1, 0, 0x0601, Test_IDDP_GetCaps)
    FTE(    "IDirectDrawPalette::Get/SetEntries",               1, 0, 0x0602, Test_IDDP_Get_SetEntries)
//  FTE(    "IDirectDrawPalette::Initialize",                   1, 0, 0x0603, Test_IDDP_Initialize)
    FTE(    "IDirectDrawPalette::QueryInterface",               1, 0, 0x0604, Test_IDDP_QueryInterface)
#endif // TEST_IDDP
#if TEST_IDDS
    FTH("IDirectDrawSurface Interface Tests",                   0)
//  FTE(    "IDirectDrawSurface::Add/DeleteAttachedSurface",    1, 0, 0x0700, Test_IDDS_Add_DeleteAttachedSurface)
#if QAHOME_OVERLAY
    FTE(    "IDirectDrawSurface::AddOverlayDirtyRect",          1, 0, 0x0701, Test_IDDS_AddOverlayDirtyRect)
#endif // QAHOME_OVERLAY
    FTE(    "IDirectDrawSurface::AddRef/Release",               1, 0, 0x0702, Test_IDDS_AddRef_Release)
    FTE(    "IDirectDrawSurface::Blt",                          1, 0, 0x0703, Test_IDDS_Blt)
//  FTE(    "IDirectDrawSurface::BltBatch",                     1, 0, 0x0704, Test_IDDS_BltBatch)
//  FTE(    "IDirectDrawSurface::BltFast",                      1, 0, 0x0705, Test_IDDS_BltFast)
    FTE(    "IDirectDrawSurface::EnumAttachedSurfaces",         1, 0, 0x0707, Test_IDDS_EnumAttachedSurfaces)
#if QAHOME_OVERLAY
    FTE(    "IDirectDrawSurface::EnumOverlayZOrders",           1, 0, 0x0708, Test_IDDS_EnumOverlayZOrders)
#endif // QAHOME_OVERLAY
    FTE(    "IDirectDrawSurface::Flip",                         1, 0, 0x0709, Test_IDDS_Flip)
//  FTE(    "IDirectDrawSurface::GetAttachedSurface",           1, 0, 0x070a, Test_IDDS_GetAttachedSurface)
    FTE(    "IDirectDrawSurface::GetBltStatus",                 1, 0, 0x070b, Test_IDDS_GetBltStatus)
    FTE(    "IDirectDrawSurface::GetCaps",                      1, 0, 0x070c, Test_IDDS_GetCaps)
#if TEST_IDDC
    FTE(    "IDirectDrawSurface::Get/SetClipper",               1, 0, 0x070d, Test_IDDS_Get_SetClipper)
#endif // TEST_IDDC
    FTE(    "IDirectDrawSurface::Get/SetColorKey",              1, 0, 0x070e, Test_IDDS_Get_SetColorKey)
    FTE(    "IDirectDrawSurface::GetDC",                        1, 0, 0x070f, Test_IDDS_GetDC)
    FTE(    "IDirectDrawSurface::GetFlipStatus",                1, 0, 0x0710, Test_IDDS_GetFlipStatus)
#if QAHOME_OVERLAY
    FTE(    "IDirectDrawSurface::GetOverlayPosition",           1, 0, 0x0711, Test_IDDS_GetOverlayPosition)
#endif // QAHOME_OVERLAY
    FTE(    "IDirectDrawSurface::Get/SetPalette",               1, 0, 0x0712, Test_IDDS_Get_SetPalette)
    FTE(    "IDirectDrawSurface::GetPixelFormat",               1, 0, 0x0713, Test_IDDS_GetPixelFormat)
    FTE(    "IDirectDrawSurface::GetSurfaceDesc",               1, 0, 0x0714, Test_IDDS_GetSurfaceDesc)
//  FTE(    "IDirectDrawSurface::Initialize",                   1, 0, 0x0715, Test_IDDS_Initialize)
    FTE(    "IDirectDrawSurface::IsLost",                       1, 0, 0x0716, Test_IDDS_IsLost)
    FTE(    "IDirectDrawSurface::Lock/Unlock",                  1, 0, 0x0717, Test_IDDS_Lock_Unlock)
    FTE(    "IDirectDrawSurface::QueryInterface",               1, 0, 0x0718, Test_IDDS_QueryInterface)
    FTE(    "IDirectDrawSurface::ReleaseDC",                    1, 0, 0x0719, Test_IDDS_ReleaseDC)
    FTE(    "IDirectDrawSurface::Restore",                      1, 0, 0x071a, Test_IDDS_Restore)
#if QAHOME_OVERLAY
    FTE(    "IDirectDrawSurface::SetOverlayPosition",           1, 0, 0x071c, Test_IDDS_SetOverlayPosition)
    FTE(    "IDirectDrawSurface::UpdateOverlay",                1, 0, 0x071d, Test_IDDS_UpdateOverlay)
//  FTE(    "IDirectDrawSurface::UpdateOverlayDisplay",         1, 0, 0x071e, Test_IDDS_UpdateOverlayDisplay)
    FTE(    "IDirectDrawSurface::UpdateOverlayZOrder",          1, 0, 0x071f, Test_IDDS_UpdateOverlayZOrder)
#endif // QAHOME_OVERLAY
#endif // TEST_IDDS
#if TEST_IDDS2
    FTH("IDirectDrawSurface2 Interface Tests",                  0)
//  FTE(    "IDirectDrawSurface2::AddRef/Release",              1, 0, 0x0800, Test_IDDS2_AddRef_Release)
    FTE(    "IDirectDrawSurface2::GetDDInterface",              1, 0, 0x0801, Test_IDDS2_GetDDInterface)
//  FTE(    "IDirectDrawSurface2::PageLock/Unlock",             1, 0, 0x0802, Test_IDDS2_PageLock_Unlock)
//  FTE(    "IDirectDrawSurface2::QueryInterface",              1, 0, 0x0803, Test_IDDS2_QueryInterface)
#endif // TEST_IDDS2
#if TEST_IDDS3
    FTH("IDirectDrawSurface3 Interface Tests",                  0)
//  FTE(    "IDirectDrawSurface3::AddRef/Release",              1, 0, 0x0900, Test_IDDS3_AddRef_Release)
//  FTE(    "IDirectDrawSurface3::QueryInterface",              1, 0, 0x0901, Test_IDDS3_QueryInterface)
    FTE(    "IDirectDrawSurface3::SetSurfaceDesc",              1, 0, 0x0902, Test_IDDS3_SetSurfaceDesc)
#endif // TEST_IDDS3
#if TEST_IDDS4
    FTH("IDirectDrawSurface4 Interface Tests",                  0)
//  FTE(    "IDirectDrawSurface4::AddRef/Release",              1, 0, 0x0d00, Test_IDDS4_AddRef_Release)
//  FTE(    "IDirectDrawSurface4::QueryInterface",              1, 0, 0x0d01, Test_IDDS4_QueryInterface)
#endif // TEST_IDDS4
#if TEST_IDDS5
    FTH("IDirectDrawSurface5 Interface Tests",                  0)
//  FTE(    "IDirectDrawSurface5::AddRef/Release",              1, 0, 0x0e00, Test_IDDS5_AddRef_Release)
    FTE(    "IDirectDrawSurface5::AlphaBlt",                    1, 0, 0x0e01, Test_IDDS5_AlphaBlt)
//  FTE(    "IDirectDrawSurface5::QueryInterface",              1, 0, 0x0e02, Test_IDDS5_QueryInterface)
#endif // TEST_IDDS5
#if TEST_IDDP
    FTH("High-level Palette Tests",                             0)
    FTE(    "Blt from palletized surface to 16-bit surface",    1, 0, 0x0a00, Test_Palette_PaletteTo16Bpp)
#endif // TEST_IDDP
#if TEST_BLT
    FTH("High-level Surface Blt Tests",                         0)
    FTE(    "Asynchronous Blt's",                               1, 0, 0x0b00, Test_Blt_Async)
//  FTE(    "Color Fills",                                      1, 0, 0x0b01, Test_Blt_ColorFill)
#endif // TEST_BLT
END_FTE

////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__
