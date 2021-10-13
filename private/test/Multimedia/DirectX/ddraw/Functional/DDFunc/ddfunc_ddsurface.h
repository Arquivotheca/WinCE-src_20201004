#pragma once
#if !defined(__DDFUNC_DDSURFACE_H__) && CFG_TEST_IDirectDrawSurface
#define __DDFUNC_DDSURFACE_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>
#include <DDTest_Base.h>

namespace Test_IDirectDrawSurface
//template <class CFG>
//class Test_IDirectDrawSurface
{
    #define DECLARE_TEST(METHODNAME, SURFACES) \
    class CTest_##METHODNAME : virtual public DDrawBaseClasses::CTest_IDirectDrawSurface \
    { \
    public: \
        CTest_##METHODNAME() { m_dwSurfCategories = SURFACES; } \
        virtual eTestResult TestIDirectDrawSurface(); \
    }

    DECLARE_TEST(AddDeleteAttachedSurface, scOffScrVid | scOffScrSys);
    #if CFG_DirectDraw_Overlay    
        DECLARE_TEST(AddOverlayDirtyRect, scOverlay);
    #endif
    DECLARE_TEST(AlphaBlt, scAllSurfaces);
    DECLARE_TEST(Blt, scAllSurfaces);
    DECLARE_TEST(BltBatch, scAllSurfaces);
    DECLARE_TEST(BltFast, scAllSurfaces);
    DECLARE_TEST(EnumAttachedSurfaces, scNull | scAllSurfaces);
    #if CFG_DirectDraw_Overlay    
    //    DECLARE_TEST(EnumOverlayZOrders, scOverlay);
    #endif
    DECLARE_TEST(Flip, scAllSurfaces);
    DECLARE_TEST(GetAttachedSurface, scAllSurfaces);
    DECLARE_TEST(GetBltStatus, scAllSurfaces);
    DECLARE_TEST(GetCaps, scAllSurfaces);
    DECLARE_TEST(GetSetClipper, scAllSurfaces);
    DECLARE_TEST(GetSetColorKey, scAllSurfaces);
    DECLARE_TEST(GetDC, scAllSurfaces);
    DECLARE_TEST(GetFlipStatus, scPrimary);
    #if CFG_DirectDraw_Overlay    
    //    DECLARE_TEST(GetOverlayPosition, scAllSurfaces);
    #endif
    DECLARE_TEST(GetSetPalette, scAllSurfaces);
    DECLARE_TEST(GetPixelFormat, scAllSurfaces);
    DECLARE_TEST(GetSetFreePrivateData, scAllSurfaces);
    DECLARE_TEST(GetSurfaceDesc, scAllSurfaces);
    DECLARE_TEST(GetChangeUniquenessValue, scAllSurfaces);
    //DECLARE_TEST(Initialize);
    DECLARE_TEST(IsLostRestore, scAllSurfaces);
    DECLARE_TEST(IsAccessibleDuringLost, scPrimary);
    DECLARE_TEST(LockUnlock, scAllSurfaces);
    DECLARE_TEST(LockUnlockAfterWindowsOP, scPrimary);
    DECLARE_TEST(ReleaseDC, scAllSurfaces);
    //DECLARE_TEST(Restore);
    #if CFG_DirectDraw_Overlay
    //    DECLARE_TEST(SetOverlayPosition);
    //    DECLARE_TEST(UpdateOverlay);
    //    DECLARE_TEST(UpdateOverlayDisplay);
    //    DECLARE_TEST(UpdateOverlayZOrder);
    #endif
    DECLARE_TEST(SetHWnd, scAllSurfaces);

    #undef DECLARE_TEST
};

#endif // header protection

