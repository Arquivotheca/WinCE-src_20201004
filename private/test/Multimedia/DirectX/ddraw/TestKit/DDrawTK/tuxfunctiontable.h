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
//

#pragma once
#ifndef __TUXFUNCTIONTABLE_H__
#define __TUXFUNCTIONTABLE_H__

// External Dependencies
// ---------------------

// Test Class Definitions
#include <DDrawUty_Types.h>

#include "DDrawTK.h"

#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define END_FTE { NULL, 0, 0, 0, NULL } };
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, d, e) { TEXT(b), a, 0, d, e },

BEGIN_FTE
    FTH(0,  "Driver Capabilities:")
    TUX_FTE(2,      "Get Caps",                                 100,       Test_Normal_Mode::Test_DirectDraw::CTest_GetCaps)
    TUX_FTE(2,      "Enumerate Display Modes",                  101,       Test_Normal_Mode::Test_DirectDraw::CTest_EnumDisplayModes)
    TUX_FTE(2,      "Verify Surface Creation",                  102,       Test_Normal_Mode::Test_DirectDraw::CTest_CreateSurfaceVerification)
    TUX_FTE(2,      "VBID Surface Creation",                    110,       Test_Normal_Mode::Test_DirectDraw::CTest_VBIDSurfaceCreation)
    TUX_FTE(2,      "VBID Surface Data Handling",               120,       Test_Normal_Mode::Test_DirectDraw::CTest_VBIDSurfaceData)

    FTH(0,  "Normal (Windowed) Mode:")
    TUX_FTE(2,      "Blt",                                      200,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::CTest_FillBlt)
    TUX_FTE(2,      "ColorKey Blt",                             210,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::CTest_ColorKeyBlt)
    TUX_FTE(2,      "Color Filling Blts",                       220,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_ColorFillBlts)
    TUX_FTE(2,      "Set/GetPixel Verification",                230,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_SetGetPixel)
    TUX_FTE(2,      "Flip",                                     240,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_Flip)
    TUX_FTE(2,      "Flip Interlaced",                          250,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_FlipInterlaced)
    TUX_FTE(2,      "Get Surface Desc",                         260,       Test_Normal_Mode::Test_DevModeChange::CTest_GetSurfaceDesc)

    FTH(0,  "Exclusive Mode:")
    TUX_FTE(2,      "Blt",                                      300,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_FillBlt)
    TUX_FTE(2,      "ColorKey Blt",                             310,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_ColorKeyBlt)
    TUX_FTE(2,      "Color Filling Blts",                       320,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_ColorFillBlts)
    TUX_FTE(2,      "Flip",                                     330,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_Flip)
    TUX_FTE(2,      "Set/GetPixel Verification",                340,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_SetGetPixel)
    TUX_FTE(2,      "Flip Interlaced",                          350,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_FlipInterlaced)
    TUX_FTE(2,      "Get Surface Desc",                         360,       Test_Exclusive_Mode::Test_DevModeChange::CTest_GetSurfaceDesc)

    FTH(0,  "Video Port Container Tests:")
    TUX_FTE(2,      "Create Video Port",                        400,       Test_DDVideoPortContainer::CTest_CreateVideoPort)
    TUX_FTE(2,      "Video Port Enumeration",                   410,       Test_DDVideoPortContainer::CTest_EnumVideoPorts)
    TUX_FTE(2,      "GetVideoPortConnectInfo",                  420,       Test_DDVideoPortContainer::CTest_GetVideoPortConnectInfo)
    TUX_FTE(2,      "QueryVideoPortStatus",                     430,       Test_DDVideoPortContainer::CTest_QueryVideoPortStatus)

    FTH(0,  "Video Port Tests:")
    TUX_FTE(2,      "GetBandwidthInfo",                         500,       Test_DirectDrawVideoPort::Test_StateIterators::CTest_GetBandwidthInfo)
    TUX_FTE(2,      "GetSetColorControls",                      502,       Test_DirectDrawVideoPort::Test_StateIterators::CTest_GetSetColorControls)
    TUX_FTE(2,      "GetInputOutputFormats",                    504,       Test_DirectDrawVideoPort::Test_StateIterators::CTest_GetInputOutputFormats)
    TUX_FTE(2,      "GetFieldPolarity",                         506,       Test_DirectDrawVideoPort::Test_StateIterators::CTest_GetFieldPolarity)
    TUX_FTE(2,      "GetVideoLine",                             508,       Test_DirectDrawVideoPort::Test_StateIterators::CTest_GetVideoLine)
    TUX_FTE(2,      "GetVideoSignalStatus",                     510,       Test_DirectDrawVideoPort::Test_StateIterators::CTest_GetVideoSignalStatus)
    TUX_FTE(2,      "SetTargetSurface",                         512,       Test_DirectDrawVideoPort::Test_StateIterators::CTest_SetTargetSurface)
    TUX_FTE(2,      "StartVideo",                               514,       Test_DirectDrawVideoPort::Test_StateIterators::CTest_StartVideo)
    TUX_FTE(2,      "StopVideo",                                516,       Test_DirectDrawVideoPort::Test_StateIterators::CTest_StopVideo)
    TUX_FTE(2,      "UpdateVideo",                              518,       Test_DirectDrawVideoPort::Test_StateIterators::CTest_UpdateVideo)
    TUX_FTE(2,      "WaitForSync",                              520,       Test_DirectDrawVideoPort::Test_StateIterators::CTest_WaitForSync)

    FTH(0,  "Interactive Normal (Windowed) Mode:")
    TUX_FTE(2,      "Blt",                                     1200,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::CTest_FillInteractiveBlt)
    TUX_FTE(2,      "Overlay Blt",                             1240,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::CTest_OverlayInteractiveOverlayBlt)
    TUX_FTE(2,      "ColorKeyOverlay Blt",                     1250,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::CTest_ColorKeyOverlayInteractiveOverlayBlt)
    TUX_FTE(2,      "ColorFill Overlay Blt",                   1260,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::CTest_OverlayInteractiveColorFillOverlayBlts)
    TUX_FTE(2,      "Interlaced Overlay Flip",                 1270,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::CTest_OverlayInteractiveInterlacedOverlayFlip)

    FTH(0,  "Interactive Exclusive Mode:")
    TUX_FTE(2,      "Blt",                                     1300,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_FillInteractiveBlt)
    TUX_FTE(2,      "Overlay Blt",                             1340,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_OverlayInteractiveOverlayBlt)
    TUX_FTE(2,      "ColorKeyOverlay Blt",                     1350,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_ColorKeyOverlayInteractiveOverlayBlt)
    TUX_FTE(2,      "ColorFill Overlay Blt",                   1360,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_OverlayInteractiveColorFillOverlayBlts)
    TUX_FTE(2,      "Interlaced Overlay Flip",                 1370,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_OverlayInteractiveInterlacedOverlayFlip)


END_FTE

#undef BEGIN_FTE
#undef END_FTE
#undef FTE
#undef FTH

#endif
