#pragma once
#ifndef __TUXFUNCTIONTABLE_H__
#define __TUXFUNCTIONTABLE_H__

// External Dependencies
// ---------------------

// Test Class Definitions
#include <DDrawUty_Types.h>

#include "DDFunc_Tests.h"

#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define END_FTE { NULL, 0, 0, 0, NULL } };
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, d, e) { TEXT(b), a, 0, d, e },

BEGIN_FTE

#if CFG_TEST_DirectDrawAPI 
    FTH(0,  "DirectDraw APIs Tests:") 
    TUX_FTE(1,      "DirectDrawCreate",                                         100,        Test_DirectDrawAPI::CTest_DirectDrawCreate) 
#if CFG_TEST_IDirectDrawClipper
    TUX_FTE(1,      "DirectDrawCreateClipper",                                  102,        Test_DirectDrawAPI::CTest_DirectDrawCreateClipper) 
#endif // CFG_TEST_IDirectDrawClipper
    TUX_FTE(1,      "DirectDrawEnumerate",                                      104,        Test_DirectDrawAPI::CTest_DirectDrawEnumerate) 
#endif // CFG_TEST_DirectDrawAPI

#if CFG_TEST_IDirectDraw
    FTH(0,  "DirectDraw Interface Tests:") 
    
    FTH(1,      "Interface : IDirectDraw")

    FTH(2,          "No Cooperative Level")
#if CFG_TEST_IDirectDrawClipper
    TUX_FTE(3,          "DirectDraw IDirectDraw::CreateClipper",                    200,       Test_No_Mode::Test_DirectDraw::CTest_CreateClipper)
#endif // CFG_TEST_IDirectDrawClipper
    TUX_FTE(3,          "DirectDraw IDirectDraw::GetCaps",                          202,       Test_No_Mode::Test_DirectDraw::CTest_GetCaps)
    TUX_FTE(3,          "DirectDraw IDirectDraw::SetCooperativeLevel",              204,       Test_No_Mode::Test_DirectDraw::CTest_SetCooperativeLevel)
    TUX_FTE(3,          "DirectDraw IDirectDraw::CTest_ReceiveWMUnderFSE",          206,       Test_No_Mode::Test_DirectDraw::CTest_ReceiveWMUnderFSE)
    
    FTH(2,          "Normal Cooperative Level")
//  TUX_FTE(3,          "DirectDraw IDirectDraw::Compact",                          300,       Test_Normal_Mode::Test_DirectDraw::CTest_Compact)
    TUX_FTE(3,          "DirectDraw IDirectDraw::CreatePalette",                    302,       Test_Normal_Mode::Test_DirectDraw::CTest_CreatePalette)
    TUX_FTE(3,          "DirectDraw IDirectDraw::CreateSurface",                    304,       Test_Normal_Mode::Test_DirectDraw::CTest_CreateSurface)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::DuplicateSurface",                 306,       Test_Normal_Mode::Test_DirectDraw::CTest_DuplicateSurface)
    TUX_FTE(3,          "DirectDraw IDirectDraw::EnumDisplayModes",                 308,       Test_Normal_Mode::Test_DirectDraw::CTest_EnumDisplayModes)
    TUX_FTE(3,          "DirectDraw IDirectDraw::GetFourCCCodes",                   314,       Test_Normal_Mode::Test_DirectDraw::CTest_GetFourCCCodes)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::GetGDISurface",                    316,       Test_Normal_Mode::Test_DirectDraw::CTest_GetGDISurface)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::GetMonitorFrequency",              318,       Test_Normal_Mode::Test_DirectDraw::CTest_GetMonitorFrequency)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::GetScanLine",                      320,       Test_Normal_Mode::Test_DirectDraw::CTest_GetScanLine)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::GetWaitForVerticalBlank",          322,       Test_Normal_Mode::Test_DirectDraw::CTest_GetWaitForVerticalBlank)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::Initialize",                       324,       Test_Normal_Mode::Test_DirectDraw::CTest_Initialize)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::GetSurfaceFromDC",                 328,       Test_Normal_Mode::Test_DirectDraw::CTest_GetSurfaceFromDC)

    FTH(2,          "Exclusive Cooperative Level")
//  TUX_FTE(3,          "DirectDraw IDirectDraw::Compact",                          400,       Test_Exclusive_Mode::Test_DirectDraw::CTest_Compact)
    TUX_FTE(3,          "DirectDraw IDirectDraw::CreatePalette",                    402,       Test_Exclusive_Mode::Test_DirectDraw::CTest_CreatePalette)
    TUX_FTE(3,          "DirectDraw IDirectDraw::CreateSurface",                    404,       Test_Exclusive_Mode::Test_DirectDraw::CTest_CreateSurface)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::DuplicateSurface",                 406,       Test_Exclusive_Mode::Test_DirectDraw::CTest_DuplicateSurface)
    TUX_FTE(3,          "DirectDraw IDirectDraw::EnumDisplayModes",                 408,       Test_Exclusive_Mode::Test_DirectDraw::CTest_EnumDisplayModes)
    TUX_FTE(3,          "DirectDraw IDirectDraw::FlipToGDISurface",                 410,       Test_Exclusive_Mode::Test_DirectDraw::CTest_FlipToGDISurface)
    TUX_FTE(3,          "DirectDraw IDirectDraw::GetSetDisplayMode",                412,       Test_Exclusive_Mode::Test_DirectDraw::CTest_GetSetDisplayMode)
    TUX_FTE(3,          "DirectDraw IDirectDraw::GetFourCCCodes",                   414,       Test_Exclusive_Mode::Test_DirectDraw::CTest_GetFourCCCodes)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::GetGDISurface",                    416,       Test_Exclusive_Mode::Test_DirectDraw::CTest_GetGDISurface)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::GetMonitorFrequency",              418,       Test_Exclusive_Mode::Test_DirectDraw::CTest_GetMonitorFrequency)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::GetScanLine",                      420,       Test_Exclusive_Mode::Test_DirectDraw::CTest_GetScanLine)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::GetWaitForVerticalBlank",          422,       Test_Exclusive_Mode::Test_DirectDraw::CTest_GetWaitForVerticalBlank)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::Initialize",                       424,       Test_Exclusive_Mode::Test_DirectDraw::CTest_Initialize)
    TUX_FTE(3,          "DirectDraw IDirectDraw::RestoreDisplayMode",               426,       Test_Exclusive_Mode::Test_DirectDraw::CTest_RestoreDisplayMode)
//  TUX_FTE(3,          "DirectDraw IDirectDraw::GetSurfaceFromDC",                 428,       Test_Exclusive_Mode::Test_DirectDraw::CTest_GetSurfaceFromDC)

#endif // CFG_TEST_IDirectDraw

#if CFG_TEST_IDirectDrawSurface

    FTH(0,  "DirectDrawSurface Interface Tests:") 

    FTH(1,      "Normal Cooperative Level")
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::AddDeleteAttachedSurface",  500,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_AddDeleteAttachedSurface)
#if CFG_DirectDraw_Overlay    
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::AddOverlayDirtyRect",       502,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_AddOverlayDirtyRect)
#endif
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::AlphaBlt",                  503,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_AlphaBlt)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::Blt",                       504,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_Blt)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::BltBatch",                  506,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_BltBatch)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::BltFast",                   508,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_BltFast)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::EnumAttachedSurfaces",      510,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_EnumAttachedSurfaces)
#if CFG_DirectDraw_Overlay    
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::EnumOverlayZOrders",        512,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_EnumOverlayZOrders)
#endif
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::Flip",                      514,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_Flip)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetAttachedSurface",        516,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetAttachedSurface)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetBltStatus",              518,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetBltStatus)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetCaps",                   520,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetCaps)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetSetClipper",             522,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetSetClipper)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetSetColorKey",            524,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetSetColorKey)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetDC",                     526,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetDC)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetFlipStatus",             528,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetFlipStatus)
#if CFG_DirectDraw_Overlay    
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetOverlayPosition",        530,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetOverlayPosition)
#endif
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetSetPalette",             532,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetSetPalette)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetPixelFormat",            534,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetPixelFormat)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetSetFreePrivateData",     535,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetSetFreePrivateData)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetSurfaceDesc",            536,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetSurfaceDesc)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetChangeUniquenessValue",  537,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetChangeUniquenessValue)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::Initialize",                538,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_Initialize)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::IsLostRestore",             540,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_IsLostRestore)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::IsAccessibleDuringLost",    541,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_IsAccessibleDuringLost)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::LockUnlock",                542,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_LockUnlock)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::LockUnlockAfterWindowsOP",  543,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_LockUnlockAfterWindowsOP)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::ReleaseDC",                 544,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_ReleaseDC)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::Restore",                   546,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_Restore)
#if CFG_DirectDraw_Overlay    
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::SetOverlayPosition",        548,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_SetOverlayPosition)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::UpdateOverlay",             550,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_UpdateOverlay)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::UpdateOverlayDisplay",      552,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_UpdateOverlayDisplay)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::UpdateOverlayZOrder",       554,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_UpdateOverlayZOrder)
#endif
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::SetHWnd",                   556,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_SetHWnd)

    FTH(1,      "Exclusive Cooperative Level")
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::AddDeleteAttachedSurface",  600,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_AddDeleteAttachedSurface)
#if CFG_DirectDraw_Overlay    
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::AddOverlayDirtyRect",       602,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_AddOverlayDirtyRect)
#endif
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::AlphaBlt",                  603,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_AlphaBlt)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::Blt",                       604,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_Blt)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::BltBatch",                  606,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_BltBatch)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::BltFast",                   608,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_BltFast)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::EnumAttachedSurfaces",      610,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_EnumAttachedSurfaces)
#if CFG_DirectDraw_Overlay    
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::EnumOverlayZOrders",        612,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_EnumOverlayZOrders)
#endif
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::Flip",                      614,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_Flip)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetAttachedSurface",        616,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetAttachedSurface)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetBltStatus",              618,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetBltStatus)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetCaps",                   620,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetCaps)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetSetClipper",             622,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetSetClipper)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetSetColorKey",            624,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetSetColorKey)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetDC",                     626,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetDC)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetFlipStatus",             628,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetFlipStatus)
#if CFG_DirectDraw_Overlay    
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetOverlayPosition",        630,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetOverlayPosition)
#endif
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetSetPalette",             632,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetSetPalette)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetPixelFormat",            634,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetPixelFormat)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetSetFreePrivateData",     635,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetSetFreePrivateData)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetSurfaceDesc",            636,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetSurfaceDesc)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::GetChangeUniquenessValue",  637,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetChangeUniquenessValue)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::Initialize",                638,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_Initialize)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::IsLostRestore",             640,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_IsLostRestore)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::IsAccessibleDuringLost",    641,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_IsAccessibleDuringLost)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::LockUnlock",                642,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_LockUnlock)
    TUX_FTE(2,      "DirectDraw IDirectDrawSurface::ReleaseDC",                 644,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_ReleaseDC)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::Restore",                   646,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_Restore)
#if CFG_DirectDraw_Overlay    
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::SetOverlayPosition",        648,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_SetOverlayPosition)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::UpdateOverlay",             650,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_UpdateOverlay)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::UpdateOverlayDisplay",      652,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_UpdateOverlayDisplay)
//  TUX_FTE(2,      "DirectDraw IDirectDrawSurface::UpdateOverlayZOrder",       654,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_UpdateOverlayZOrder)
#endif

#endif // CFG_TEST_IDirectDrawSurface

END_FTE

#undef BEGIN_FTE
#undef END_FTE
#undef FTE
#undef FTH

#endif 
