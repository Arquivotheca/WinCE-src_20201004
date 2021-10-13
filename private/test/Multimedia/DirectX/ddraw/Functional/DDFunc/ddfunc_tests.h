#pragma once
#if !defined(__DDFUNC_TESTS_H__)
#define __DDFUNC_TESTS_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>
#include <DDrawUty_Config.h>
#include <DDTest_Base.h>
#include <DDTest_Iterators.h>
#include <DDTest_Modes.h>
#include <DDTest_Modifiers.h>

#include "DDFunc_API.h"
#include "DDFunc_DDraw.h"
#include "DDFunc_DDSurface.h"
#include "DDFunc_Sets.h"

namespace Test_No_Mode
{
    namespace Test_DirectDraw
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : \
            virtual public Test_DDraw_Modes::CTest_NoMode, \
            virtual public Test_IDirectDraw::CTest_##METHODNAME { }
            
#if CFG_TEST_IDirectDrawClipper
        DECLARE_TEST(CreateClipper);
#endif // CFG_TEST_IDirectDrawClipper
        DECLARE_TEST(GetCaps);
        DECLARE_TEST(SetCooperativeLevel);
        DECLARE_TEST(ReceiveWMUnderFSE);
        #undef DECLARE_TEST
    };
};

namespace Test_Normal_Mode
{
    namespace Test_DirectDraw
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : \
            virtual public Test_DDraw_Modes::CTest_NormalMode, \
            virtual public Test_IDirectDraw::CTest_##METHODNAME { }

    //    DECLARE_TEST(Compact);
        DECLARE_TEST(CreatePalette);
        DECLARE_TEST(CreateSurface);
    //    DECLARE_TEST(DuplicateSurface);
        DECLARE_TEST(EnumDisplayModes);
        DECLARE_TEST(GetFourCCCodes);
    //    DECLARE_TEST(GetGDISurface);
    //    DECLARE_TEST(GetMonitorFrequency);
    //    DECLARE_TEST(GetScanLine);
    //    DECLARE_TEST(GetWaitForVerticalBlank);
    //    DECLARE_TEST(Initialize);
    //    DECLARE_TEST(GetSurfaceFromDC);

        #undef DECLARE_TEST
    };
    
    namespace Test_DirectDrawSurface
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : \
            virtual public Test_DDraw_Modes::CTest_NormalMode, \
            virtual public Test_Surface_Sets::CTest_All, \
            virtual public Test_Surface_Iterators::CIterateSurfaces, \
            virtual public Test_IDirectDrawSurface::CTest_##METHODNAME { }

        DECLARE_TEST(AddDeleteAttachedSurface);
        #if CFG_DirectDraw_Overlay    
            DECLARE_TEST(AddOverlayDirtyRect);
        #endif
        DECLARE_TEST(AlphaBlt);
        DECLARE_TEST(Blt);
        DECLARE_TEST(BltBatch);
        DECLARE_TEST(BltFast);
        DECLARE_TEST(EnumAttachedSurfaces);
        #if CFG_DirectDraw_Overlay    
        //    DECLARE_TEST(EnumOverlayZOrders);
        #endif
        DECLARE_TEST(Flip);
        DECLARE_TEST(GetAttachedSurface);
        DECLARE_TEST(GetBltStatus);
        DECLARE_TEST(GetCaps);
        DECLARE_TEST(GetSetClipper);
        DECLARE_TEST(GetSetColorKey);
        DECLARE_TEST(GetDC);
        DECLARE_TEST(GetFlipStatus);
        #if CFG_DirectDraw_Overlay    
        //    DECLARE_TEST(GetOverlayPosition);
        #endif
        DECLARE_TEST(GetSetPalette);
        DECLARE_TEST(GetPixelFormat);
        DECLARE_TEST(GetSetFreePrivateData);
        DECLARE_TEST(GetSurfaceDesc);
        DECLARE_TEST(GetChangeUniquenessValue);
        //DECLARE_TEST(Initialize);
        DECLARE_TEST(IsLostRestore);
        DECLARE_TEST(IsAccessibleDuringLost);
        DECLARE_TEST(LockUnlock);
        DECLARE_TEST(LockUnlockAfterWindowsOP);
        DECLARE_TEST(ReleaseDC);
        //DECLARE_TEST(Restore);
        #if CFG_DirectDraw_Overlay
        //    DECLARE_TEST(SetOverlayPosition);
        //    DECLARE_TEST(UpdateOverlay);
        //    DECLARE_TEST(UpdateOverlayDisplay);
        //    DECLARE_TEST(UpdateOverlayZOrder);
        #endif
        DECLARE_TEST(SetHWnd);

        #undef DECLARE_TEST
    };
};

namespace Test_Exclusive_Mode
{
    namespace Test_DirectDraw
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : \
            virtual public Test_DDraw_Modes::CTest_ExclusiveMode, \
            virtual public Test_Mode_Sets::CTest_All, \
            virtual public Test_IDirectDraw::CTest_##METHODNAME { }

    //    DECLARE_TEST(Compact);
        DECLARE_TEST(CreatePalette);
        DECLARE_TEST(CreateSurface);
    //    DECLARE_TEST(DuplicateSurface);
        DECLARE_TEST(EnumDisplayModes);
        DECLARE_TEST(FlipToGDISurface);
        DECLARE_TEST(GetSetDisplayMode);
        DECLARE_TEST(GetFourCCCodes);
    //    DECLARE_TEST(GetGDISurface);
    //    DECLARE_TEST(GetMonitorFrequency);
    //    DECLARE_TEST(GetScanLine);
    //    DECLARE_TEST(GetWaitForVerticalBlank);
    //    DECLARE_TEST(Initialize);
        DECLARE_TEST(RestoreDisplayMode);
    //    DECLARE_TEST(GetSurfaceFromDC);

        #undef DECLARE_TEST
    };
    
    namespace Test_DirectDrawSurface
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : \
            virtual public Test_DDraw_Modes::CTest_ExclusiveMode, \
            virtual public Test_Mode_Sets::CTest_All, \
            virtual public Test_Surface_Sets::CTest_All, \
            virtual public Test_Surface_Iterators::CIterateSurfaces, \
            virtual public Test_IDirectDrawSurface::CTest_##METHODNAME { }

        DECLARE_TEST(AddDeleteAttachedSurface);
        #if CFG_DirectDraw_Overlay    
            DECLARE_TEST(AddOverlayDirtyRect);
        #endif
        DECLARE_TEST(AlphaBlt);
        DECLARE_TEST(Blt);
        DECLARE_TEST(BltBatch);
        DECLARE_TEST(BltFast);
        DECLARE_TEST(EnumAttachedSurfaces);
        #if CFG_DirectDraw_Overlay    
        //    DECLARE_TEST(EnumOverlayZOrders);
        #endif
        DECLARE_TEST(Flip);
        DECLARE_TEST(GetAttachedSurface);
        DECLARE_TEST(GetBltStatus);
        DECLARE_TEST(GetCaps);
        DECLARE_TEST(GetSetClipper);
        DECLARE_TEST(GetSetColorKey);
        DECLARE_TEST(GetDC);
        DECLARE_TEST(GetFlipStatus);
        #if CFG_DirectDraw_Overlay    
        //    DECLARE_TEST(GetOverlayPosition);
        #endif
        DECLARE_TEST(GetSetPalette);
        DECLARE_TEST(GetPixelFormat);
        DECLARE_TEST(GetSetFreePrivateData);
        DECLARE_TEST(GetSurfaceDesc);
        DECLARE_TEST(GetChangeUniquenessValue);
        //DECLARE_TEST(Initialize);
        DECLARE_TEST(IsLostRestore);
        DECLARE_TEST(IsAccessibleDuringLost);
        DECLARE_TEST(LockUnlock);
        DECLARE_TEST(ReleaseDC);
        //DECLARE_TEST(Restore);
        #if CFG_DirectDraw_Overlay
        //    DECLARE_TEST(SetOverlayPosition);
        //    DECLARE_TEST(UpdateOverlay);
        //    DECLARE_TEST(UpdateOverlayDisplay);
        //    DECLARE_TEST(UpdateOverlayZOrder);
        #endif

        #undef DECLARE_TEST
    };
};

#endif // header protection

