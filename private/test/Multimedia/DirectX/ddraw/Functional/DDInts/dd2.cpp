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
////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: dd2.cpp
//          BVT for the IDirectDraw interface functions.
//
//  Revision History:
//      10/08/96    lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

#if TEST_IDD2

////////////////////////////////////////////////////////////////////////////////
// Local types

struct DDSCAPSPAIR
{
    DWORD   m_dwCaps;
    LPCTSTR m_pszName;
};

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

bool    Help_IDD2_GAVM(IDirectDraw *, DDSCAPSPAIR *, DWORD *, DWORD *);
HRESULT CALLBACK Help_IDD2_SetDisplayMode_Callback(
    LPDDSURFACEDESC,
    LPVOID);

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static bool g_bDD2SDMCallbackExpected = false;

static DDSCAPSPAIR g_ddscaps[] =
{
#if QAHOME_OVERLAY
    { DDSCAPS_OVERLAY,            TEXT("DDSCAPS_OVERLAY") },
#endif // QAHOME_OVERLAY
    { DDSCAPS_PRIMARYSURFACE,     TEXT("DDSCAPS_PRIMARYSURFACE") },
};

static IDirectDraw     *g_pDD2 = NULL;
static CFinishManager   *g_pfmDD2 = NULL;

////////////////////////////////////////////////////////////////////////////////
// Test_IDD2_GetAvailableVidMem
//  Tests the IDirectDraw::GetAvailableVidMem method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD2_GetAvailableVidMem(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    UINT    nIndex;
    DWORD   dwTotal1,
            dwTotal2,
            dwFree1,
            dwFree2;
    const TCHAR   *pszBugID;
    ITPR    nITPR = ITPR_PASS;

    IDirectDraw    *pDD2;
    KATOVERBOSITY   dwVerbosity;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD2))
        return TPRFromITPR(g_tprReturnVal);
    
    for(nIndex = 0; nIndex < countof(g_ddscaps); nIndex++)
    {
        if(Help_IDD2_GAVM(pDD2, &g_ddscaps[nIndex], NULL, &dwFree1) &&
           Help_IDD2_GAVM(pDD2, &g_ddscaps[nIndex], &dwTotal1, NULL) &&
           Help_IDD2_GAVM(pDD2, &g_ddscaps[nIndex], &dwTotal2, &dwFree2))
        {
            if(dwTotal1 != dwTotal2)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%s::%s(%s) returned different values")
                    TEXT(" for total memory when the pointer for free memory")
                    TEXT(" was NULL (%u) and non-NULL (%u)"),
                    c_szIDirectDraw,
                    c_szGetAvailableVidMem,
                    g_ddscaps[nIndex].m_pszName,
                    dwTotal1,
                    dwTotal2);
                nITPR |= ITPR_FAIL;
            }

            if(dwFree1 != dwFree2)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%s::%s(%s) returned different values")
                    TEXT(" for free memory when the pointer for total memory")
                    TEXT(" was NULL (%u) and non-NULL (%u)"),
                    c_szIDirectDraw,
                    c_szGetAvailableVidMem,
                    g_ddscaps[nIndex].m_pszName,
                    dwFree1,
                    dwFree2);
                nITPR |= ITPR_FAIL;
            }

            if(dwTotal1 == dwTotal2 && dwFree1 == dwFree2)
            {
                if(dwTotal1 < dwFree1)
                {
                    dwVerbosity = LOG_FAIL;
#ifdef KATANA
                    pszBugID = FAILURE_HEADER TEXT("(Keep #337) ");
#else // KATANA
                    pszBugID = FAILURE_HEADER;
#endif // KATANA
                    nITPR |= ITPR_FAIL;
                }
                else
                {
                    dwVerbosity = LOG_DETAIL;
                    pszBugID = TEXT("");
                }

                Debug(
                    dwVerbosity,
                    TEXT("%s%s::%s reports total memory for %s is %u")
                    TEXT(" (%u free)"),
                    pszBugID,
                    c_szIDirectDraw,
                    c_szGetAvailableVidMem,
                    g_ddscaps[nIndex].m_pszName,
                    dwTotal1,
                    dwFree1);
            }
        }
        else
        {
            nITPR |= ITPR_FAIL;
        }
    }

    // UPDATE: See how total/free memory changes when we allocate additional
    //         objects. Also see whether the reported value appears to be
    //         correct or not.

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szGetAvailableVidMem);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD2_SetDisplayMode
//  Tests the IDirectDraw::SetDisplayMode method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD2_SetDisplayMode(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw    *pDD2;
    HRESULT         hr = S_OK;
    ITPR            nITPR = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD2))
        return TPRFromITPR(g_tprReturnVal);

#ifdef UNDER_CE
#if QAHOME_INVALIDPARAMS
    // Try invalid resolutions and color depths
    INVALID_CALL_E(
        pDD2->SetDisplayMode(0, 0, 0, 0, 0),
        c_szIDirectDraw,
        c_szSetDisplayMode,
        TEXT("0x0x0, 0Hz"),
        DDERR_INVALIDMODE);
    INVALID_CALL_E_RAID(
        pDD2->SetDisplayMode(640, 480, 16, 1, 0),
        c_szIDirectDraw,
        c_szSetDisplayMode,
        TEXT("640x480x16, 1Hz"),
        DDERR_INVALIDMODE,
        IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
        IFKATANAELSE(2833, 0));
    INVALID_CALL_E(
        pDD2->SetDisplayMode(640, 480, 8, 60, 0),
        c_szIDirectDraw,
        c_szSetDisplayMode,
        TEXT("640x480x8, 60Hz"),
        DDERR_INVALIDMODE);
#endif // QAHOME_INVALIDPARAMS
#endif // UNDER_CE

    g_pContext = (void *)pDD2;
    g_bTestPassed = true;
    g_bDD2SDMCallbackExpected = true;
    hr = pDD2->EnumDisplayModes(
        0,
        NULL,
        g_pContext,
        Help_IDD2_SetDisplayMode_Callback);
    g_bDD2SDMCallbackExpected = false;
    if(FAILED(hr))
    {
        Report(RESULT_FAILURE, c_szIDirectDraw, c_szEnumDisplayModes, hr);
        nITPR |= ITPR_FAIL;
    }
    else if(!g_bTestPassed)
    {
        nITPR |= ITPR_FAIL;
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szSetDisplayMode);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Help_IDD2_GAVM
//  Calls the IDirectDraw::GetAvailableVidMem method once, with the specified
//  parameters, and reports any errors.
//
// Parameters:
//  pDD2           IDirectDraw pointer.
//  pddscaps        Entry representing the DDSCAPS value we should use.
//  pdwTotal        Pointer for the second argument.
//  pdwFree         Pointer for the third argument.
//
// Return value:
//  true if the test succeeds, false otherwise.

bool Help_IDD2_GAVM(
    IDirectDraw    *pDD2,
    DDSCAPSPAIR     *pddscaps,
    DWORD           *pdwTotal,
    DWORD           *pdwFree)
{
    HRESULT hr;
    LPCTSTR szNon = TEXT("non-"),
            szEmpty = TEXT("");
    DDSCAPS ddsCaps;
    TCHAR   szConfig[128];
    bool    bSuccess = true;
    UINT    nBugID;

    if(pdwTotal)
        *pdwTotal = PRESET;
    if(pdwFree)
        *pdwFree = PRESET;
    ddsCaps.dwCaps = pddscaps->m_dwCaps;

    _stprintf_s(
        szConfig,
        TEXT("%s, %sNULL, %sNULL"),
        pddscaps->m_pszName,
        pdwTotal ? szNon : szEmpty,
        pdwFree ? szNon : szEmpty);
    hr = pDD2->GetAvailableVidMem(
        &ddsCaps,
        pdwTotal,
        pdwFree);
    if(DDERR_UNSUPPORTED == hr)
    {
        Debug(LOG_PASS, _T("Driver does not support GetAvailableVideoMemory"));
        return true;
    }
    else if(FAILED(hr))
    {
#ifdef KATANA
#define DB      RAID_KEEP
        nBugID = 0;
#else // KATANA
#define DB      RAID_HPURAID
        if(!pdwTotal && pdwFree)
        {
            nBugID = 10182;
        }
        else
        {
            nBugID = 0;
        }
#endif // KATANA
        Report(
            RESULT_FAILURE,
            c_szIDirectDraw,
            c_szGetAvailableVidMem,
            hr,
            szConfig,
            NULL,
            S_OK,
            DB,
            nBugID);
#undef DB
        return false;
    }

    // Did the method return meaningful information?
    if(pdwTotal && *pdwTotal == PRESET)
    {
        Report(
            RESULT_INVALIDNOTPOINTER,
            c_szIDirectDraw,
            c_szGetAvailableVidMem,
            0,
            szConfig,
            TEXT("total memory value"));
        bSuccess = false;
    }

    if(pdwFree && *pdwFree == PRESET)
    {
        Report(
            RESULT_INVALIDNOTPOINTER,
            c_szIDirectDraw,
            c_szGetAvailableVidMem,
            0,
            szConfig,
            TEXT("free memory value"));
        bSuccess = false;
    }

    return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////
// Help_IDD2_SetDisplayMode_Callback
//  Helper callback for the IDirectDraw::EnumDisplayModes test function, when
//  called by the IDirectDraw::SetDisplayMode test function.
//
// Parameters:
//  pDDSD           Description of a surface for a display mode.
//  pContext        Context (we pass a pointer to a HSDM structure).
//
// Return value:
//  DDENUMRET_OK to continue enumerating, DDENUMRET_CANCEL to stop.

HRESULT CALLBACK Help_IDD2_SetDisplayMode_Callback(
    LPDDSURFACEDESC pDDSD,
    LPVOID          pContext)
{
    HRESULT         hr;
    IDirectDraw    *pDD2;
    DDSURFACEDESC   ddsd;

    // Were we expecting to be called?
    if(!g_bDD2SDMCallbackExpected)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("Unexpected call to")
            TEXT(" Help_IDD2_SetDisplayMode_Callback"));
        g_bTestPassed = false;
        return (HRESULT)DDENUMRET_CANCEL;
    }

    if(!pDDSD)
    {
        Report(
            RESULT_NULL,
            c_szIDirectDraw,
            c_szEnumDisplayModes,
            0,
            NULL,
            TEXT("for the surface description"));
        g_bTestPassed = false;
        return (HRESULT)DDENUMRET_OK;
    }

    Debug(
        LOG_DETAIL,
        TEXT("Setting display to mode %dx%dx%d, %dHz"),
        pDDSD->dwWidth,
        pDDSD->dwHeight,
        pDDSD->ddpfPixelFormat.dwRGBBitCount,
        pDDSD->dwRefreshRate);

    if(pContext != g_pContext)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s::%s callback was passed an incorrect")
            TEXT(" context value. Expected 0x%08x, got 0x%08x.")
            TEXT(" Not setting display mode."),
            c_szIDirectDraw,
            c_szEnumDisplayModes,
            g_pContext,
            pContext);
        g_bTestPassed = false;
    }

    pDD2 = (IDirectDraw *)g_pContext;

    hr = pDD2->SetDisplayMode(
        pDDSD->dwWidth,
        pDDSD->dwHeight,
        pDDSD->ddpfPixelFormat.dwRGBBitCount,
        pDDSD->dwRefreshRate,
        0);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDraw,
            c_szSetDisplayMode,
            hr,
            NULL,
            NULL,
            hr,
            IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
            IFKATANAELSE(0, 4419));
        g_bTestPassed = false;
    }
    else
    {
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        hr = pDD2->GetDisplayMode(&ddsd);
        if(FAILED(hr))
        {
            Report(RESULT_FAILURE, c_szIDirectDraw, c_szGetDisplayMode, hr);
            g_bTestPassed = false;
        }
        else
        {
            if(ddsd.dwWidth != pDDSD->dwWidth ||
               ddsd.dwHeight != pDDSD->dwHeight ||
               ddsd.ddpfPixelFormat.dwRGBBitCount !=
                    pDDSD->ddpfPixelFormat.dwRGBBitCount ||
               // Special case if the refresh rates are not
               // equal but the "enumed" refresh rate is 0
               // (which indicates "adapter default").
               // See HPURAID 4359.
               (ddsd.dwRefreshRate != pDDSD->dwRefreshRate &&
                    pDDSD->dwRefreshRate != 0))
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER
#ifdef KATANA
                    TEXT("(Keep #3082) ")
#else // not KATANA
                    TEXT("(HPURAID #4359) ")
#endif // not KATANA
                    TEXT("%s::%s apparently set")
                    TEXT(" the display mode to %dx%dx%d, %dHz instead"),
                    c_szIDirectDraw,
                    c_szSetDisplayMode,
                    ddsd.dwWidth,
                    ddsd.dwHeight,
                    ddsd.ddpfPixelFormat.dwRGBBitCount,
                    ddsd.dwRefreshRate);
                g_bTestPassed = false;
            }
        }

        pDD2->RestoreDisplayMode();
    }

    return (HRESULT)DDENUMRET_OK;
}
#endif // TEST_IDD2

////////////////////////////////////////////////////////////////////////////////
