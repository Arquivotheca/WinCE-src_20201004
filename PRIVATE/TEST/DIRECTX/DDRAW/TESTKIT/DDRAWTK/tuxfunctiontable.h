//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//

#pragma once
#ifndef __TUXFUNCTIONTABLE_H__
#define __TUXFUNCTIONTABLE_H__

// External Dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

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

    FTH(0,  "Normal (Windowed) Mode:")
    TUX_FTE(2,      "Blt",                                      200,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::CTest_FillBlt)
    TUX_FTE(2,      "ColorKey Blt",                             210,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::CTest_ColorKeyBlt)
    TUX_FTE(2,      "Color Filling Blts",                       220,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_ColorFillBlts)

    FTH(0,  "Exclusive Mode:")
    TUX_FTE(2,      "Blt",                                      300,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_FillBlt)
    TUX_FTE(2,      "ColorKey Blt",                             310,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_ColorKeyBlt)
    TUX_FTE(2,      "Color Filling Blts",                       320,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_ColorFillBlts)
    TUX_FTE(2,      "Flip",                                     330,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_Flip)

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

    FTH(0,  "Interactive Exclusive Mode:")
    TUX_FTE(2,      "Blt",                                     1300,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_FillInteractiveBlt)
    TUX_FTE(2,      "Overlay Blt",                             1340,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_OverlayInteractiveOverlayBlt)
    TUX_FTE(2,      "ColorKeyOverlay Blt",                     1350,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_ColorKeyOverlayInteractiveOverlayBlt)
    TUX_FTE(2,      "ColorFill Overlay Blt",                   1360,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::CTest_OverlayInteractiveColorFillOverlayBlts)


END_FTE

#undef BEGIN_FTE
#undef END_FTE
#undef FTE
#undef FTH

#endif 
