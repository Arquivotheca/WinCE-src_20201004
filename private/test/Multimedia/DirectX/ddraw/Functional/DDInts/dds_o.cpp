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

#include "main.h"
#include "globals.h"
#pragma warning(disable : 4995)
#if TEST_IDDS
#if QAHOME_OVERLAY

// Defined in ddbvt.cpp
int LogResult(HRESULT hrActual, const TCHAR *sz, HRESULT hrExpected);

////////////////////////////////////////////////////////////////////////////////
// LogResultOK
//  Simply appends a S_OK for compat. with new LogResult
//
// Return value:
//  pass-through for LogResult
//
int LogResultOK(HRESULT hrActual, const TCHAR *sz)
{
    // Simply pass along to generic function
    return(LogResult(hrActual, sz, S_OK));
}

// Compat. layer.
ITPR ResolveResult(ITPR nTPR, int iArgs)
{
    // Do nothing, simply return for now
    return(nTPR);
}

////////////////////////////////////////////////////////////////////////////////
// EnumOverlayZOrdersCB
//  Callback for the IDirectDrawSurface::EnumOverlayZOrders method test.
//
// Parameters:
//  pDDS            Pointer to the surface being enumerated.
//  pddsd           Pointer to the description of the surface.
//  pvContext       Context passed by the user.
//
// Return value:
//  DDENUMRET_OK to continue enumeration, DDENUMRET_CANCEL to stop.

HRESULT FAR PASCAL EnumOverlayZOrdersCB(
    IDirectDrawSurface  *pDDS,
    DDSURFACEDESC       *pddsd,
    void                *pvContext)
{
    int *pnCallbacks = (int*)pvContext;
    TCHAR szMsg[256];

    // Increment number of overlays found
    (*pnCallbacks)++;
    StringCchPrintf(szMsg, _countof(szMsg),TEXT("EnumOverlayZOrdersCB() Callback function %d"),
       (*pnCallbacks));

    Debug(
        LOG_DETAIL,
        szMsg);

    StringCchPrintf(szMsg,
        _countof(szMsg),
        TEXT("pDDS=%p\n Flags=%x\n Height=%d\n Width=%d\n")
        TEXT("Flags=%x, BPP=%d, R=%x, G=%x, B=%x, A=%x\n\n"),
        pDDS,
        pddsd->dwFlags,
        pddsd->dwHeight,
        pddsd->dwWidth,
        pddsd->ddpfPixelFormat.dwFlags,
        pddsd->ddpfPixelFormat.dwRGBBitCount,
        pddsd->ddpfPixelFormat.dwRBitMask,
        pddsd->ddpfPixelFormat.dwGBitMask,
        pddsd->ddpfPixelFormat.dwBBitMask,
        pddsd->ddpfPixelFormat.dwRGBAlphaBitMask);

    Debug(
        LOG_DETAIL,
        szMsg);

    pDDS->Release();
    return (HRESULT)DDENUMRET_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_AddOverlayDirtyRect ***
//  Tests the IDirectDrawSurface::AddOverlayDirtyRect method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_AddOverlayDirtyRect(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDS    *pIDDS;
    HRESULT hr;
    RECT    rect;
    TCHAR   szError[64];

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    if(!pIDDS->m_bOverlaysSupported)
    {
        Debug(
            LOG_SKIP,
            TEXT("Overlays are not supported by the hardware. Skipping test."));
        return TPR_SKIP;
    }

    // Pick a random rectangle
    rect.left   = rand()%(pIDDS->m_ddsdPrimary.dwWidth - 1);
    rect.right  = rect.left +
                  (rand()%(pIDDS->m_ddsdPrimary.dwWidth - rect.left - 1)) + 1;
    rect.top    = rand()%(pIDDS->m_ddsdPrimary.dwHeight - 1);
    rect.bottom = rect.top +
                  (rand()%(pIDDS->m_ddsdPrimary.dwHeight - rect.top - 1)) + 1;

    hr = pIDDS->m_pDDSPrimary->AddOverlayDirtyRect(&rect);
    
    // AddOverlayDirtyRect is not implemented in DD7 (HPURAID #4362)
    if(hr != DDERR_UNSUPPORTED && FAILED(hr))
    {
        StringCchPrintf(
            szError,
            _countof(szError),
            TEXT("{%d,%d,%d,%d}"),
            rect.left,
            rect.top,
            rect.right,
            rect.bottom);

        Report(
            RESULT_FAILURE,
            c_szIDirectDrawSurface,
            c_szAddOverlayDirtyRect,
            hr,
            szError,
            TEXT("for primary"),
            hr,
            RAID_HPURAID,
            IFKATANAELSE(0, 4362));

        return TPR_FAIL;
    }

    Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szAddOverlayDirtyRect);

    return TPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_EnumOverlayZOrders
//  Tests the IDirectDrawSurface::EnumOverlayZOrders method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_EnumOverlayZOrders(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDS    *pIDDS;
    int     nCallbacks = 0;
    ITPR    nITPR = ITPR_PASS;
    HRESULT hr;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    // a-shende: make sure we can proceed
    if(!pIDDS->m_bOverlaysSupported)
    {
        Debug(
            LOG_SKIP,
            TEXT("Overlays are not supported by the hardware. Skipping test."));
        return TPR_SKIP;
    }

#if QAHOME_OVERLAY
    TCHAR szMsg[256];
    StringCchPrintf(szMsg,
        _countof(szMsg),
        TEXT("Primary=%p, Overlay1=%p, Overlay2=%p\n"),
        pIDDS->m_pDDSPrimary,
        pIDDS->m_pDDSOverlay1,
        pIDDS->m_pDDSOverlay2);
    Debug(LOG_DETAIL, szMsg);
#endif

    hr = pIDDS->m_pDDSPrimary->EnumOverlayZOrders(
        DDENUMOVERLAYZ_FRONTTOBACK,
        (LPVOID)&nCallbacks,
        (LPDDENUMSURFACESCALLBACK)EnumOverlayZOrdersCB);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDrawSurface,
            c_szEnumOverlayZOrders,
            hr);
        nITPR |= ITPR_FAIL;
    }

    if(pIDDS->m_bOverlaysSupported)
    {
        DWORD dwNumOverlays = ((pIDDS->m_pDDSOverlay1)?1:0)+((pIDDS->m_pDDSOverlay2)?1:0);
        if(nCallbacks != long(dwNumOverlays))
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s was expected to enumerate 2")
                TEXT(" overlays (enumerated %d)"),
                c_szIDirectDrawSurface,
                c_szEnumOverlayZOrders,
                nCallbacks);
            nITPR |= ITPR_FAIL;
        }
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szEnumOverlayZOrders);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_GetOverlayPosition
//  Tests the IDirectDrawSurface::GetOverlayPosition method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_GetOverlayPosition(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    LPIDDS  pIDDS;
    RECT    rcSrc = {0, 0, 0, 0};
    RECT    rcDest = {0, 0, 0, 0};
    LONG    lX  = 0,
            lY  = 0,
            lX1 = 0,
            lY1 = 0;
    ITPR    nITPR = ITPR_PASS;
    HRESULT hr;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    if(!pIDDS->m_bOverlaysSupported)
    {
        Debug(
            LOG_SKIP,
            TEXT("Overlays are not supported by the hardware. Skipping test."));
        return TPR_SKIP;
    }

    // Calculate the coordinates to center the overlay
    lX1 = (pIDDS->m_ddsdPrimary.dwWidth / 2) - 
        (pIDDS->m_ddsdOverlay1.dwWidth / 2);
    lY1 = (pIDDS->m_ddsdPrimary.dwHeight / 2) - 
        (pIDDS->m_ddsdOverlay1.dwHeight / 2);

    // Call with Overlay surface
    hr = pIDDS->m_pDDSOverlay1->GetOverlayPosition(&lX, &lY);
    if(hr != DDERR_OVERLAYNOTVISIBLE)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szGetOverlayPosition,
            hr,
            NULL,
            NULL,
            DDERR_OVERLAYNOTVISIBLE,
            RAID_HPURAID,
            2898);
        nITPR |= ITPR_FAIL;
    }

    // a-shende: Specify source coords to use 
    rcSrc.right = pIDDS->m_ddsdOverlay1.dwWidth;
    rcSrc.bottom = pIDDS->m_ddsdOverlay1.dwHeight;
    rcDest.right = pIDDS->m_ddsdOverlay1.dwWidth;
    rcDest.bottom = pIDDS->m_ddsdOverlay1.dwHeight;

    // Setup overlay
    hr = pIDDS->m_pDDSOverlay1->UpdateOverlay(
        &rcSrc,
        pIDDS->m_pDDSPrimary,
        &rcDest,
        DDOVER_SHOW,
        NULL);

    if(FAILED(hr) && (pIDDS->m_ddsdOverlay1.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY))
    {
        // This is OK to fail, if the overlay is in system memory
        Debug(
            LOG_DETAIL,
            TEXT("System memory overlay can't be updated.  Skipping test."));
        nITPR |= ITPR_SKIP;
    }
    else if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szUpdateOverlay,
            hr);
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Call with Overlay surface
        hr = pIDDS->m_pDDSOverlay1->GetOverlayPosition(&lX, &lY);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szGetOverlayPosition,
                hr);
            nITPR |= ITPR_FAIL;
        }

        if(lX != 0 || lY != 0)
        {
            Debug(
                LOG_FAIL,
                TEXT("%s::%s(%d, %d) not reporting same as set (0, 0)")
                TEXT(" before call to %s"),
                c_szIDirectDrawSurface,
                c_szGetOverlayPosition,
                lX,
                lY,
                c_szSetOverlayPosition);
            nITPR |= ITPR_FAIL;
        }

        // Call with Overlay surface
        hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(lX1, lY1);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szSetOverlayPosition,
                hr);
            nITPR |= ITPR_ABORT;
        }
        else
        {
            // Get to check
            hr = pIDDS->m_pDDSOverlay1->GetOverlayPosition(&lX, &lY);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szGetOverlayPosition,
                    hr);
                nITPR |= ITPR_FAIL;
            }

            if(lX != lX1 || lY != lY1)
            {
                Debug(
                    LOG_COMMENT,
                    TEXT("%s::%s(%d, %d) not reporting same as set (%d, %d)")
                    TEXT(" after call to %s"),
                    c_szIDirectDrawSurface,
                    c_szGetOverlayPosition,
                    lX,
                    lY,
                    lX1,
                    lY1,
                    c_szSetOverlayPosition);

                nITPR |= ITPR_FAIL;
            }
        }
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szGetOverlayPosition);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_SetOverlayPosition
//  Tests the IDirectDrawSurface::SetOverlayPosition method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_SetOverlayPosition(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDS    *pIDDS;
    DDCAPS  ddcaps;
    int     nTPR = TPR_PASS;
    RECT    rcSrc = {0, 0, 0, 0};
    RECT    rcDest = {0, 0, 0, 0};
    HRESULT hr;
    LONG    lX  = 0,
            lY  = 0,
            lX1 = 0,
            lY1 = 0,
            lNegativeX=-10,
            lOutOfBoundX=2000,
            lRightEdgeX;
    LONG rcSrcWidth,rcDestWidth;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);
    
    //get ddraw caps
    memset(&ddcaps, 0, sizeof(ddcaps));
    ddcaps.dwSize = sizeof(ddcaps);
    hr = pIDDS->m_pDD->GetCaps(&ddcaps, NULL);
    if(FAILED(hr))
    {
        Debug(
            LOG_FAIL,
            TEXT("GetCaps"));
        return TPR_FAIL;
    }

    if(!pIDDS->m_bOverlaysSupported)
    {
        Debug(
            LOG_SKIP,
            TEXT("Overlays are not supported by the hardware. Skipping test."));
        return TPR_SKIP;
    }

    rcSrcWidth=pIDDS->m_ddsdOverlay1.dwWidth;
    rcDestWidth=rcSrcWidth;

    if(ddcaps.dwAlignSizeSrc)   // adjust source rect size
    {
        if ((rcSrcWidth%(ddcaps.dwAlignSizeSrc)!=0))    //if width is not aligned with the required source alignment size
        {
            // adjust the width of src rect to make it aligned, because src is initialized as overlay size, we can not round it up,
            // we want to change it to be the first smaller number that can be divided by dwAlignSizeSrc.
            rcSrcWidth = ((rcSrcWidth/(ddcaps.dwAlignSizeSrc)))*ddcaps.dwAlignSizeSrc;  
                                                                                        
            if (rcSrcWidth<=0)  //actually the only possible case here is equal to zero, that is when the rcSrcWidth is smaller than ddcaps.dwAlignSizeSrc
            {
                Debug(
                    LOG_SKIP,
                    TEXT("A valid Overlays Src Rect size can not be generated under the AlignSizeSrc limitation defined in the Caps. Skipping test."));
                return TPR_SKIP;
            }
        }
    }
    if(ddcaps.dwAlignSizeDest)  // adjust dest rect size
    {
        if ((rcDestWidth%(ddcaps.dwAlignSizeDest)!=0))    //if rcDestWidth is not aligned with the required destination alignment size
        {
            // the size is round down to make it consistent with rcSrc, although typically primary surface width might be bigger than overlay width
            rcDestWidth = ((rcDestWidth/(ddcaps.dwAlignSizeDest)))*ddcaps.dwAlignSizeDest;    

            if (rcDestWidth <=0)
            {
                Debug(
                    LOG_SKIP,
                    TEXT("A valid Overlays Dest Rect size can not be generated under the AlignSizeDest limitation defined in the Caps. Skipping test."));
                return TPR_SKIP;
            }
        }
    }

    lRightEdgeX=(pIDDS->m_ddsdPrimary.dwWidth - rcDestWidth);   //the starting x coordinate for overlay when the right edge of overlay is on screen boundary

    // Test a source rectangle of the whole overlay
    rcSrc.right = rcSrcWidth;
    rcSrc.bottom = pIDDS->m_ddsdOverlay1.dwHeight;

    // Calculate the coordinates to center the overlay
    lX1 = (pIDDS->m_ddsdPrimary.dwWidth / 2) - 
        (pIDDS->m_ddsdOverlay1.dwWidth / 2);
    lY1 = (pIDDS->m_ddsdPrimary.dwHeight / 2) - 
        (pIDDS->m_ddsdOverlay1.dwHeight / 2);

    if(ddcaps.dwAlignBoundaryDest)  // adjust starting x coordinate of overlay
    {
        if ((lX1%(ddcaps.dwAlignBoundaryDest)!=0))  
        {
            lX1 = ((lX1/(ddcaps.dwAlignBoundaryDest)))*ddcaps.dwAlignBoundaryDest;
        }

        if ((lNegativeX%(ddcaps.dwAlignBoundaryDest)!=0))    // negative case we want it to be negatively round up
        {
            lNegativeX = ((lNegativeX/(ddcaps.dwAlignBoundaryDest))-1)*ddcaps.dwAlignBoundaryDest;
        }

        if ((lOutOfBoundX%(ddcaps.dwAlignBoundaryDest)!=0))  // positive out of bound case we want it to be round up
        {
            lOutOfBoundX = ((lOutOfBoundX/(ddcaps.dwAlignBoundaryDest))+1)*ddcaps.dwAlignBoundaryDest;
        }

        if ((lRightEdgeX%(ddcaps.dwAlignBoundaryDest)!=0))    
        {
            // we want to make sure after the rouding, putting it at new location won't make part of the overlay out of bound
            // and return DDERR_INVALIDPOSITION, therefore we round down both rcDest width and the starting location to make sure it is not happending.
            lRightEdgeX = ((lRightEdgeX/(ddcaps.dwAlignBoundaryDest)))*ddcaps.dwAlignBoundaryDest; 
        }
    }

    rcDest.left = lX1;
    rcDest.top  = lY1;
    rcDest.right  = rcDest.left + rcDestWidth;
    rcDest.bottom = rcDest.top  + pIDDS->m_ddsdOverlay1.dwHeight;

    // Update the overlay centered on the screen
    hr = pIDDS->m_pDDSOverlay1->UpdateOverlay(
        &rcSrc,
        pIDDS->m_pDDSPrimary,
        &rcDest,
        DDOVER_SHOW,
        NULL);
    if(FAILED(hr) && (pIDDS->m_ddsdOverlay1.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY))
    {
        // This is OK to fail, if the overlay is in system memory
        Debug(
            LOG_DETAIL,
            TEXT("System memory overlay can't be updated.  Skipping test."));
        return TPR_SKIP;
    }
    else if(FAILED(hr))
    {
        Debug(
            LOG_FAIL,
            TEXT("UpdateOverlay"));
        return TPR_FAIL;
    }

    // Outside screen boundary
    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(lNegativeX, lY1);
    if(hr != DDERR_INVALIDPOSITION)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition outside left edge of screen"),
            NULL,
            DDERR_INVALIDPOSITION);
        nTPR |= ITPR_FAIL;
    }

    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(lX1, 2000);
    if(hr != DDERR_INVALIDPOSITION)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition outside bottom edge of screen"),
            NULL,
            DDERR_INVALIDPOSITION);
        nTPR |= ITPR_FAIL;
    }

    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(lOutOfBoundX, lY1);
    if(hr != DDERR_INVALIDPOSITION)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition outside right edge of screen"), 
            NULL,
            DDERR_INVALIDPOSITION);
        nTPR |= ITPR_FAIL;
    }

    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(lX1, -10);
    if(hr != DDERR_INVALIDPOSITION)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition outside top edge of screen"),
            NULL,
            DDERR_INVALIDPOSITION);
        nTPR |= ITPR_FAIL;
    }

    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(0, lY1);
    if(hr != S_OK)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition on left edge of screen"),
            NULL,
            S_OK);
        nTPR |= ITPR_FAIL;
    }

    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(lX1,
        (pIDDS->m_ddsdPrimary.dwHeight - pIDDS->m_ddsdOverlay1.dwHeight));
    if(hr != S_OK)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition on bottom edge of screen"), 
            NULL,
            S_OK);
        nTPR |= ITPR_FAIL;
    }

    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(lRightEdgeX, lY1);
    if(hr != S_OK)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition on right edge of screen"), 
            NULL,
            S_OK);
        nTPR |= ITPR_FAIL;
    }

    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(lX1, 0);
    if(hr != S_OK)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition on top edge of screen"),
            NULL,
            S_OK);
        nTPR |= ITPR_FAIL;
    }

    // On Corners
    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(0, 0);
    if(hr != S_OK)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition at top left corner"),
            NULL,
            S_OK);
        nTPR |= ITPR_FAIL;
    }

    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(0,
        (pIDDS->m_ddsdPrimary.dwHeight - pIDDS->m_ddsdOverlay1.dwHeight));
    if(hr != S_OK)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition bottom left corner"),
            NULL,
            S_OK);
        nTPR |= ITPR_FAIL;
    }

    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(
        lRightEdgeX,(pIDDS->m_ddsdPrimary.dwHeight - pIDDS->m_ddsdOverlay1.dwHeight));
    if(hr != S_OK)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition at bottom right corner"),
            NULL,
            S_OK);
        nTPR |= ITPR_FAIL;
    }

    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(
        lRightEdgeX, 0);
    if(hr != S_OK)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition at top right corner"),
            NULL,
            S_OK);
        nTPR |= ITPR_FAIL;
    }

    // Inside screen boundary
    hr = pIDDS->m_pDDSOverlay1->SetOverlayPosition(lX1, lY1); 
    if(hr != S_OK)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szSetOverlayPosition, 
            hr,
            TEXT("IDDS::SetOverlayPosition inside the screen"),
            NULL,
            S_OK);
        nTPR |= ITPR_FAIL;
    }

    // Get position
    hr = pIDDS->m_pDDSOverlay1->GetOverlayPosition(&lX, &lY);
    if(hr != S_OK)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawSurface,
            c_szGetOverlayPosition, 
            hr,
            TEXT("IDDS::GetOverlayPosition "),
            NULL,
            S_OK);
        nTPR |= ITPR_FAIL;
    }
    else
    {
        if(lX != lX1 || lY != lY1)
        {
            Debug(
                LOG_COMMENT,
                TEXT("IDDS::GetOverlayPosition (%d, %d) not reporting same")
                TEXT(" as set (%d, %d)"),
                lX,
                lY,
                lX1,
                lY1);
            nTPR = TPR_FAIL;
        }
        else
        {
            Debug(
                LOG_COMMENT,
                TEXT("IDDS::GetOverlayPosition (%d, %d) reporting same as")
                TEXT(" set (%d, %d)"),
                lX,
                lY,
                lX1,
                lY1);
        }
    }

    return nTPR;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_UpdateOverlay
//  Tests the IDirectDrawSurface::UpdateOverlay method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_UpdateOverlay(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{

    LPIDDS      pIDDS;
    int         nTPR = TPR_PASS;
    RECT        rcSrc = {0, 0, 0, 0};
    RECT        rcDest = {0, 0, 0, 0};
    HRESULT     hr; 
    DDOVERLAYFX ddofx;
    DDCAPS ddcp;


    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    if(!pIDDS->m_bOverlaysSupported)
    {
        Debug(
            LOG_SKIP,
            TEXT("Overlays are not supported by the hardware. Skipping test."));
        return TPR_SKIP;
    }
    
    memset(&ddofx, 0, sizeof(DDOVERLAYFX));
    ddofx.dwSize = sizeof(DDOVERLAYFX);

    // Caps to determine if overlay stretching is supported
    memset(&ddcp, 0x0, sizeof(DDCAPS));
    ddcp.dwSize = sizeof(DDCAPS);
    hr = pIDDS->m_pDD->GetCaps(&ddcp, NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDraw,
            c_szGetCaps,
            hr);
        nTPR |= ITPR_FAIL;
    }
    
    if(pIDDS->m_ddsdOverlay1.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
    {
        // We can't update if the overlay is in system memory
        Debug(
            LOG_DETAIL,
            TEXT("System memory overlay can't be updated.  Skipping test."));
        return TPR_SKIP;
    }

    rcSrc.right   = pIDDS->m_ddsdOverlay1.dwWidth;
    rcSrc.bottom  = pIDDS->m_ddsdOverlay1.dwHeight;
    rcDest.right  = pIDDS->m_ddsdOverlay1.dwWidth;
    rcDest.bottom = pIDDS->m_ddsdOverlay1.dwHeight;

    // If supported, check overlay stretching
    if(ddcp.dwMinOverlayStretch != 1000 || ddcp.dwMaxOverlayStretch != 1000)
    {
        RECT    rcTmpSrc  = {0, 0, 0, 0},
                rcTmpDest = {0, 0, 0, 0};

        // Shrink the overlay - The source is the entire
        // overlay, destination is region shrunk by as
        // much as possible
        rcTmpSrc.right  = rcTmpDest.right  = pIDDS->m_ddsdOverlay1.dwWidth;
        rcTmpSrc.bottom = rcTmpDest.bottom = pIDDS->m_ddsdOverlay1.dwHeight;

        // Nice little method to get the smallest possible
        // Dest that is still larger than or equal to Src
        // multiplied by (Min/1000).
        //rcTmpDest.right  = (LONG)((rcTmpSrc.right  - 1) * ddcp.dwMinOverlayStretch / 1000 + 1);
        //rcTmpDest.bottom = (LONG)((rcTmpSrc.bottom - 1) * ddcp.dwMinOverlayStretch / 1000 + 1);
        if (1000 != ddcp.dwMinOverlayStretch)
        {
            rcTmpDest.right  = (LONG)(rcTmpSrc.right  * ddcp.dwMinOverlayStretch / 1000 + 1);
            rcTmpDest.bottom = (LONG)(rcTmpSrc.bottom * ddcp.dwMinOverlayStretch / 1000 + 1);
        }

        // Aligning with the required Destination alignment size
        if (ddcp.dwAlignSizeDest) 
        {
           if ((rcTmpDest.right % (ddcp.dwAlignSizeDest) != 0)) 
            {
                rcTmpDest.right = ((rcTmpDest.right/(ddcp.dwAlignSizeDest)) + 1) * ddcp.dwAlignSizeDest;    
            }
            if ((rcTmpDest.bottom % (ddcp.dwAlignSizeSrc) != 0)) 
            {
                rcTmpDest.bottom = ((rcTmpDest.bottom/(ddcp.dwAlignSizeDest)) + 1) * ddcp.dwAlignSizeDest;  
            }
        }        
        Debug(
            LOG_COMMENT,
            TEXT("Shrinking from (%d, %d, %d, %d) to (%d, %d, %d, %d)"),
            rcTmpSrc.left, rcTmpSrc.top, rcTmpSrc.right, rcTmpSrc.bottom,
            rcTmpDest.left, rcTmpDest.top, rcTmpDest.right, rcTmpDest.bottom);

        // Test shrink scaling
        hr = pIDDS->m_pDDSOverlay1->UpdateOverlay(
            &rcTmpSrc,
            pIDDS->m_pDDSPrimary,
            &rcTmpDest,
            DDOVER_SHOW,
            &ddofx);
        if(FAILED(hr))
        {
            Report(
                RESULT_UNEXPECTED,
                c_szIDirectDrawSurface,
                c_szUpdateOverlay,
                hr,
                NULL,
                TEXT("using minimum possible stretching"));
            nTPR |= ITPR_FAIL;
        }
        
        // Grow the overlay - The destination is a region
        // the size of the original overlay, but the source
        // is small enough to require the maximum stretch
        // to fill the destination
        rcTmpSrc.right  = rcTmpDest.right  = pIDDS->m_ddsdOverlay1.dwWidth;
        rcTmpSrc.bottom = rcTmpDest.bottom = pIDDS->m_ddsdOverlay1.dwHeight;

        // Nice little method to get the smallest possible
        // Src that when multiplied by (Max/1000) will
        // be larger than or equal to Dest.
        //rcTmpSrc.right  = (LONG)((rcTmpDest.right  - 1) * 1000 / ddcp.dwMaxOverlayStretch + 1);
        //rcTmpSrc.bottom = (LONG)((rcTmpDest.bottom - 1) * 1000 / ddcp.dwMaxOverlayStretch + 1);
        if (1000 != ddcp.dwMaxOverlayStretch)
        {
            rcTmpSrc.right  = (LONG)(rcTmpDest.right  * 1000 / ddcp.dwMaxOverlayStretch + 1);
            rcTmpSrc.bottom = (LONG)(rcTmpDest.bottom * 1000 / ddcp.dwMaxOverlayStretch + 1);
        }

        // Aligning with the required source alignment size
        if (ddcp.dwAlignSizeSrc) 
        {
            if ((rcTmpSrc.right % (ddcp.dwAlignSizeSrc) != 0)) 
            {
                rcTmpSrc.right = ((rcTmpSrc.right/(ddcp.dwAlignSizeSrc)) + 1) * ddcp.dwAlignSizeSrc;    
            }
            if ((rcTmpSrc.bottom % (ddcp.dwAlignSizeSrc) != 0)) 
            {
                rcTmpSrc.bottom = ((rcTmpSrc.bottom/(ddcp.dwAlignSizeSrc)) + 1) * ddcp.dwAlignSizeSrc;  
            }
        }
        Debug(
            LOG_COMMENT,
            TEXT("Growing from (%d, %d, %d, %d) to (%d, %d, %d, %d)"),
            rcTmpSrc.left, rcTmpSrc.top, rcTmpSrc.right, rcTmpSrc.bottom,
            rcTmpDest.left, rcTmpDest.top, rcTmpDest.right, rcTmpDest.bottom);

        // Test grow scaling
        hr = pIDDS->m_pDDSOverlay1->UpdateOverlay(
            &rcTmpSrc,
            pIDDS->m_pDDSPrimary,
            &rcTmpDest,
            DDOVER_SHOW,
            &ddofx);
        if(FAILED(hr))
        {
            Report(
                RESULT_UNEXPECTED,
                c_szIDirectDrawSurface,
                c_szUpdateOverlay,
                hr,
                NULL,
                TEXT("using maximum possible stretching"));
            nTPR |= ITPR_FAIL;
        }
    }


    // Call with Overlay surface on Primary
    nTPR = ResolveResult(
        nTPR,
        LogResultOK(
            pIDDS->m_pDDSOverlay1->UpdateOverlay(
                &rcSrc,
                pIDDS->m_pDDSPrimary,
                &rcDest,
                DDOVER_SHOW,
                &ddofx),
            TEXT("IDDS::UpdateOverlay() with valid Dest and Src rects")));

    // Call with Overlay surface on Primary
    nTPR = ResolveResult(
        nTPR,
        LogResultOK(
            pIDDS->m_pDDSOverlay1->UpdateOverlay(
                &rcSrc,
                pIDDS->m_pDDSPrimary,
                NULL,
                DDOVER_SHOW,
                &ddofx),
            TEXT("IDDS::UpdateOverlay() with Null Dest rect")));

    // Call with Overlay surface on Primary
    nTPR = ResolveResult(
        nTPR,
        LogResultOK(
            pIDDS->m_pDDSOverlay1->UpdateOverlay(
                NULL,
                pIDDS->m_pDDSPrimary,
                &rcDest,
                DDOVER_SHOW,
                &ddofx),
            TEXT("IDDS::UpdateOverlay() with Null Src rect")));

    // Call with Overlay surface on Primary
    nTPR = ResolveResult(
        nTPR,
        LogResultOK(
            pIDDS->m_pDDSOverlay1->UpdateOverlay(
                NULL,
                pIDDS->m_pDDSPrimary,
                NULL,
                DDOVER_SHOW,
                &ddofx),
            TEXT("IDDS::UpdateOverlay() with Null Dest and NULL Src rect")));

    // Call with Overlay surface on Primary
    nTPR = ResolveResult(
        nTPR,
        LogResultOK(
            pIDDS->m_pDDSOverlay1->UpdateOverlay(
                NULL,
                pIDDS->m_pDDSPrimary,
                NULL,
                DDOVER_SHOW,
                NULL),
            TEXT("IDDS::UpdateOverlay() with Null Dest and NULL Src rect")
            TEXT(" and NULL OverlayFX")));

    // How about no flags ?????

    // Call with Overlay surface on Overlay - should return Unsupported
    nTPR = ResolveResult(
        nTPR,
        LogResult(
            pIDDS->m_pDDSOverlay1->UpdateOverlay(
                // &rcSrc,
                NULL,
                pIDDS->m_pDDSOverlay2,
                // &rcDest,
                NULL,
                DDOVER_SHOW,
                &ddofx),
            TEXT("IDDS::UpdateOverlay() on another overlay"),
            DDERR_INVALIDPARAMS));

    return nTPR;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_UpdateOverlayZOrder
//  Tests the IDirectDrawSurface::UpdateOverlayZOrder method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_UpdateOverlayZOrder(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{

    LPIDDS  pIDDS;
    int     nTPR = TPR_PASS;
    int     i;
    DWORD   dwFlag[] = {
        DDOVERZ_INSERTINBACKOF,
        DDOVERZ_INSERTINFRONTOF,
        DDOVERZ_MOVEBACKWARD,
        DDOVERZ_MOVEFORWARD,
        DDOVERZ_SENDTOBACK,
        DDOVERZ_SENDTOFRONT
    };
    TCHAR*  szFlag[] = {
        TEXT("DDOVERZ_INSERTINBACKOF"),
        TEXT("DDOVERZ_INSERTINFRONTOF"),
        TEXT("DDOVERZ_MOVEBACKWARD"),
        TEXT("DDOVERZ_MOVEFORWARD"),
        TEXT("DDOVERZ_SENDTOBACK"),
        TEXT("DDOVERZ_SENDTOFRONT")
    };
    TCHAR   sz[128];

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    if(!pIDDS->m_bOverlaysSupported)
    {
        Debug(
            LOG_SKIP,
            TEXT("Overlays are not supported by the hardware. Skipping test."));
        return TPR_SKIP;
    }


    bool bSkipNULL;
    for(i = 0; i < 6; i++ )
    {
#if QAHOME_INVALIDPARAMS
        // If we're testing invalid params, don't skip anything.
        bSkipNULL = false;
#else // QAHOME_INVALIDPARAMS
        // If we are doing relative movement (in front of
        // or in back of), then skip the NULL test, since
        // it's not a valid usage.
        bSkipNULL = (DDOVERZ_INSERTINBACKOF  == dwFlag[i] ||
                     DDOVERZ_INSERTINFRONTOF == dwFlag[i]);
#endif // QAHOME_INVALIDPARAMS
        if(!bSkipNULL)
        {
            // Call with NULL reference surface
            StringCchPrintf(
                sz,
                _countof(sz),
                TEXT("IDDS::UpdateOverlayZOrder() with %s flag and NULL")
                TEXT(" reference surface"),
                szFlag[i]);
            nTPR = ResolveResult(
                nTPR,
                LogResultOK(
                    pIDDS->m_pDDSOverlay1->UpdateOverlayZOrder(dwFlag[i], NULL),
                    sz));
        }

        // Call with valid reference surface
        StringCchPrintf(
            sz,
            _countof(sz),
            TEXT("IDDS::UpdateOverlayZOrder() with %s flag"),
            szFlag[i]);
        nTPR = ResolveResult(
            nTPR,
            LogResultOK(
                pIDDS->m_pDDSOverlay1->UpdateOverlayZOrder(
                    dwFlag[i],
                    pIDDS->m_pDDSOverlay2),
                sz));
    }

    return nTPR;
}
#endif // QAHOME_OVERLAY
#endif // TEST_IDDS

////////////////////////////////////////////////////////////////////////////////
