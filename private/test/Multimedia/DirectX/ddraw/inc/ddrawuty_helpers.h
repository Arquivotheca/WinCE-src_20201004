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
#pragma once
#if !defined(__DDRAWUTY_HELPERS_H__)
#define __DDRAWUTY_HELPERS_H__

// External Dependencies
// ---------------------
#include <vector>
#include <QATestUty/TestUty.h>

namespace DDrawUty
{
    namespace Surface_Helpers
    {
        enum
        {
            GVCK_SRC = 0x01,
            GVCK_DST = 0x02
        };

        enum
        {
            GBC_HEL = 0x01,
            GBC_HAL = 0x02
        };

        BOOL IsPlanarFourCC(DWORD dwFourCC);
        HRESULT GetValidColorKeys(DWORD dwFlags, DDSURFACEDESC &ddsd, std::vector<DWORD> &vectdwFlags);
        void GetBltCaps(DDSURFACEDESC &ddsdSrc, DDSURFACEDESC &ddsdDst, DWORD dwFlags, DWORD *pdwBltCaps, DWORD *pdwCKeyCaps);
        void GenRandRect(DWORD dwWidth, DWORD dwHeight, RECT *prc);
        void GenRandRect(RECT *prcBound, RECT *prcRand);
        void GenRandNonOverlappingRects(DWORD dwWidth, DWORD dwHeight, RECT *prc1, RECT *prc2);
        void GenRandNonOverlappingRects(RECT *prcBound, RECT *prc1, RECT *prc2);
        eTestResult ComparePixelFormats(DDPIXELFORMAT &ddpfFound, DDPIXELFORMAT &ddpfPassed, eVerbosityLevel Verbosity, bool bOutputData=true);
        eTestResult CompareSurfaceDescs(DDSURFACEDESC &ddsdFound, DDSURFACEDESC &ddsdPassed);
        eTestResult AttachPaletteIfNeeded(IDirectDraw *piDD, IDirectDrawSurface *piDDS);
        HRESULT CALLBACK GetBackBuffer_Callback(LPDIRECTDRAWSURFACE pDDS, LPDDSURFACEDESC pddsd, LPVOID pContext);
        eTestResult VerifyLostSurface(IDirectDraw* pidd, const DDSURFACEDESC& ddsdLostSurface, 
                                      IDirectDrawSurface* piddsLostSurface,
                                      IDirectDrawSurface* piddsPrimarySurface
                                     );        
        eTestResult GetClippingRectangle(IDirectDrawSurface* piDDS, RECT& rcClippingRect);
        bool IsWithinRect(DWORD x, DWORD y, const RECT* rcBound);
        eTestResult ClipToWindow(IDirectDraw* pidd, IDirectDrawSurface* pidds, HWND hwnd);
    }

    namespace OutOfMemory_Helpers
    {
        eTestResult VerifyOOMCondition(HRESULT hr, IDirectDraw *piDD, DDSURFACEDESC ddsd);
    }

    namespace BitMap_Helpers
    {
        HRESULT SaveSurfaceToBMP(IDirectDrawSurface *piDDS, TCHAR *pszName);
    }

    namespace DevMode_Helpers
    {
        // Sets the DEVMODE even if the angle is non-zero.
        // Returns trPass if the new mode was set, else returns trAbort.
        eTestResult SetDevMode(const DEVMODE& dmNewDevMode);

        // Gets the current DEVMODE filling the current display orientation too.
        // Returns trPass if the new mode was set, else returns trAbort.
        eTestResult GetCurrentDevMode(DEVMODE& dmNewDevMode);

        // Converts the DEVMODE angle to the corresponding integer value.
        // e.g. DMD_270 => 270
        unsigned int GetAngle(DWORD dwOrientation);

        // Gets the expected Surface Desc after an orientation change.
        void GetExpectedSurfDesc(
                                CDDSurfaceDesc& cddsdExpected,
                                const CDDSurfaceDesc& cddsdBeforeRotation,
                                DWORD dwCurrentOrientation,
                                DWORD dwPreviousOrientation
                                );
    }
}

#endif // header protection

