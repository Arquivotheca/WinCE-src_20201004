#pragma once
#ifndef __TUXFUNCTIONTABLE_H__
#define __TUXFUNCTIONTABLE_H__

// External Dependencies
// ---------------------

// Test Class Definitions
#include <DDrawUty_Types.h>

#include "DDAutoBlt_Tests.h"

#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define END_FTE { NULL, 0, 0, 0, NULL } };
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, d, e) { TEXT(b), a, 0, d, e },

BEGIN_FTE

    FTH(0,  "Normal (Windowed) Mode:") 
    
    FTH(1,      "Direct Draw Interface Tests:") 
    TUX_FTE(2,      "Enumerate Display Modes",                                  1000,       Test_Normal_Mode::Test_DirectDraw::CTest_EnumDisplayModes)
    TUX_FTE(2,      "Get Caps",                                                 1002,       Test_Normal_Mode::Test_DirectDraw::CTest_GetCaps)
    TUX_FTE(2,      "Create Surface",                                           1004,       Test_Normal_Mode::Test_DirectDraw::CTest_CreateSurface)
    TUX_FTE(2,      "Create Palette",                                           1006,       Test_Normal_Mode::Test_DirectDraw::CTest_CreatePalette)
    TUX_FTE(2,      "Create Clipper",                                           1008,       Test_Normal_Mode::Test_DirectDraw::CTest_CreateClipper)
    
    FTH(1,      "Direct Draw Surface Interface Tests:") 
    TUX_FTE(2,      "AddRef/Release",                                           1100,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_AddRefRelease)
    TUX_FTE(2,      "QueryInterface",                                           1102,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_QueryInterface)
    TUX_FTE(2,      "Get Caps",                                                 1104,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetCaps)
    TUX_FTE(2,      "Get Surface Description",                                  1106,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetSurfaceDescription)
    TUX_FTE(2,      "Get Pixel Format",                                         1108,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetPixelFormat)
    TUX_FTE(2,      "Get/Release Device Context",                               1110,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetReleaseDeviceContext)
//  TUX_FTE(2,      "Get/Set Palette",                                          1112,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetSetPalette)
    TUX_FTE(2,      "Lock/Unlock",                                              1114,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_LockUnlock)
    TUX_FTE(2,      "Get/Set Clipper",                                          1116,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_GetSetClipper)
    TUX_FTE(2,      "Color Filling Blts",                                       1120,       Test_Normal_Mode::Test_DirectDrawSurface::CTest_ColorFillBlts)
    
    FTH(1,      "Same Pixel Format Functionality:") 
    TUX_FTE(2,      "Blt",                                                      1200,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_FillBlt)
//  TUX_FTE(2,      "BltFast",                                                  1202,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_FillBltFast)
    TUX_FTE(2,      "AlphaBlt",                                                 1204,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_FillAlphaBlt)
    TUX_FTE(2,      "ColorKey Blt",                                             1210,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyBlt)
//  TUX_FTE(2,      "ColorKey BltFast",                                         1212,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyBltFast)
    TUX_FTE(2,      "ColorKey AlphaBlt",                                        1214,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyAlphaBlt)
    TUX_FTE(2,      "Overlay Blt",                                              1220,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_OverlayBlt)
//  TUX_FTE(2,      "Overlay BltFast",                                          1222,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_OverlayBltFast)
    TUX_FTE(2,      "Overlay AlphaBlt",                                         1224,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_OverlayAlphaBlt)
    TUX_FTE(2,      "ColorKeyOverlay Blt",                                      1230,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyOverlayBlt)
//  TUX_FTE(2,      "ColorKeyOverlay BltFast",                                  1232,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyOverlayBltFast)
    TUX_FTE(2,      "ColorKeyOverlay AlphaBlt",                                 1234,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyOverlayAlphaBlt)
    
    FTH(1,      "Different Pixel Format Functionality:") 
//  TUX_FTE(2,      "Blt",                                                      1300,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_FillBlt)
//  TUX_FTE(2,      "BltFast",                                                  1302,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_FillBltFast)
    TUX_FTE(2,      "AlphaBlt",                                                 1304,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_FillAlphaBlt)
//  TUX_FTE(2,      "ColorKey Blt",                                             1310,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyBlt)
//  TUX_FTE(2,      "ColorKey BltFast",                                         1312,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyBltFast)
    TUX_FTE(2,      "ColorKey AlphaBlt",                                        1314,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyAlphaBlt)
//  TUX_FTE(2,      "Overlay Blt",                                              1320,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_OverlayBlt)
//  TUX_FTE(2,      "Overlay BltFast",                                          1322,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_OverlayBltFast)
    TUX_FTE(2,      "Overlay AlphaBlt",                                         1324,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_OverlayAlphaBlt)
//  TUX_FTE(2,      "ColorKeyOverlay Blt",                                      1330,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyOverlayBlt)
//  TUX_FTE(2,      "ColorKeyOverlay BltFast",                                  1332,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyOverlayBltFast)
    TUX_FTE(2,      "ColorKeyOverlay AlphaBlt",                                 1334,       Test_Normal_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyOverlayAlphaBlt)
    
//  FTH(1,      "Non-SourceCopy Functionality:") 
    
    FTH(0,  "Exclusive (FullScreen) Mode:") 
    
    FTH(1,      "Direct Draw Interface Tests:") 
    TUX_FTE(2,      "Enumerate Display Modes",                                  2000,       Test_Exclusive_Mode::Test_DirectDraw::CTest_EnumDisplayModes)
    TUX_FTE(2,      "Get Caps",                                                 2002,       Test_Exclusive_Mode::Test_DirectDraw::CTest_GetCaps)
    TUX_FTE(2,      "Create Surface",                                           2004,       Test_Exclusive_Mode::Test_DirectDraw::CTest_CreateSurface)
    TUX_FTE(2,      "Create Palette",                                           2006,       Test_Exclusive_Mode::Test_DirectDraw::CTest_CreatePalette)
    TUX_FTE(2,      "Create Clipper",                                           2008,       Test_Exclusive_Mode::Test_DirectDraw::CTest_CreateClipper)
    
    FTH(1,      "Direct Draw Surface Interface Tests:") 
    TUX_FTE(2,      "AddRef/Release",                                           2100,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_AddRefRelease)
    TUX_FTE(2,      "QueryInterface",                                           2102,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_QueryInterface)
    TUX_FTE(2,      "Get Caps",                                                 2104,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetCaps)
    TUX_FTE(2,      "Get Surface Description",                                  2106,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetSurfaceDescription)
    TUX_FTE(2,      "Get Pixel Format",                                         2108,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetPixelFormat)
    TUX_FTE(2,      "Get/Release Device Context",                               2110,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetReleaseDeviceContext)
//  TUX_FTE(2,      "Get/Set Palette",                                          2112,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetSetPalette)
    TUX_FTE(2,      "Lock/Unlock",                                              2114,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_LockUnlock)
    TUX_FTE(2,      "Get/Set Clipper",                                          2116,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_GetSetClipper)
    TUX_FTE(2,      "Flip",                                                     2118,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_Flip)
    TUX_FTE(2,      "Color Filling Blts",                                       2120,       Test_Exclusive_Mode::Test_DirectDrawSurface::CTest_ColorFillBlts)
    
    FTH(1,      "Same Pixel Format Functionality:") 
    TUX_FTE(2,      "Blt",                                                      2200,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_FillBlt)
//  TUX_FTE(2,      "BltFast",                                                  2202,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_FillBltFast)
    TUX_FTE(2,      "AlphaBlt",                                                 2204,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_FillAlphaBlt)
    TUX_FTE(2,      "ColorKey Blt",                                             2210,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyBlt)
    TUX_FTE(2,      "ColorKey BltFast",                                         2212,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyBltFast)
    TUX_FTE(2,      "ColorKey AlphaBlt",                                        2214,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyAlphaBlt)
    TUX_FTE(2,      "Overlay Blt",                                              2220,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_OverlayBlt)
    TUX_FTE(2,      "Overlay BltFast",                                          2222,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_OverlayBltFast)
    TUX_FTE(2,      "Overlay AlphaBlt",                                         2224,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_OverlayAlphaBlt)
    TUX_FTE(2,      "ColorKeyOverlay Blt",                                      2230,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyOverlayBlt)
    TUX_FTE(2,      "ColorKeyOverlay BltFast",                                  2232,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyOverlayBltFast)
    TUX_FTE(2,      "ColorKeyOverlay AlphaBlt",                                 2234,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_SamePixel::CTest_ColorKeyOverlayAlphaBlt)
    
    FTH(1,      "Different Pixel Format Functionality:") 
    TUX_FTE(2,      "Blt",                                                      2300,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_FillBlt)
//  TUX_FTE(2,      "BltFast",                                                  2302,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_FillBltFast)
    TUX_FTE(2,      "AlphaBlt",                                                 2304,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_FillAlphaBlt)
    TUX_FTE(2,      "ColorKey Blt",                                             2310,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyBlt)
    TUX_FTE(2,      "ColorKey BltFast",                                         2312,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyBltFast)
    TUX_FTE(2,      "ColorKey AlphaBlt",                                        2314,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyAlphaBlt)
    TUX_FTE(2,      "Overlay Blt",                                              2320,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_OverlayBlt)
    TUX_FTE(2,      "Overlay BltFast",                                          2322,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_OverlayBltFast)
    TUX_FTE(2,      "Overlay AlphaBlt",                                         2324,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_OverlayAlphaBlt)
    TUX_FTE(2,      "ColorKeyOverlay Blt",                                      2330,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyOverlayBlt)
    TUX_FTE(2,      "ColorKeyOverlay BltFast",                                  2332,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyOverlayBltFast)
    TUX_FTE(2,      "ColorKeyOverlay AlphaBlt",                                 2334,       Test_Exclusive_Mode::Test_DirectDrawSurface_TWO::Test_DifferentPixel::CTest_ColorKeyOverlayAlphaBlt)
    
//  FTH(1,      "Non-SourceCopy Functionality:") 

END_FTE

#undef BEGIN_FTE
#undef END_FTE
#undef FTE
#undef FTH

#endif 
