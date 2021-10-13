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
//  Module: dd4.cpp
//          BVT for the IDirectDraw interface functions.
//
//  Revision History:
//      10/08/96    lblanco     Created.
//      12/22/98    a-rampar    Modified to test IDirectDraw interface
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

#if TEST_IDD4
////////////////////////////////////////////////////////////////////////////////
// Local types

struct DDSCAPSPAIR
{
    DWORD   m_dwCaps;
    LPCTSTR m_pszName;
};

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

void    Help_IDD4_GetDeviceIdentifier(IDirectDraw *, DDDEVICEIDENTIFIER *);

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static DDSCAPSPAIR g_ddscaps[] =
{
#if QAHOME_OVERLAY
    { DDSCAPS_OVERLAY,            TEXT("DDSCAPS_OVERLAY") },
#endif // QAHOME_OVERLAY
    { DDSCAPS_PRIMARYSURFACE,     TEXT("DDSCAPS_PRIMARYSURFACE") },
};

static IDirectDraw     *g_pDD4 = NULL;
static CFinishManager   *g_pfmDD4 = NULL;

////////////////////////////////////////////////////////////////////////////////
// Test_IDD4_TestCooperativeLevel
//  Tests the IDirectDraw::TestCooperativeLevel method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD4_TestCooperativeLevel(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT hr;

    IDirectDraw    *pDD4;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD4))
        return TPRFromITPR(g_tprReturnVal);

    // Simple smoke test. Since InitDirectDraw4 sets the cooperative level,
    // we should get S_OK in the following call
    __try
    {
        hr = pDD4->TestCooperativeLevel();
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDraw,
                c_szTestCooperativeLevel,
                hr);
            return TPR_FAIL;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
#ifdef KATANA
#define BUGID   5113
#else // KATANA
#define BUGID   0
#endif // KATANA
        Report(
            RESULT_EXCEPTION,
            c_szIDirectDraw,
            c_szTestCooperativeLevel,
            0,
            NULL,
            NULL,
            0,
            RAID_HPURAID,
            BUGID);
#undef BUGID
    }

    Report(RESULT_SUCCESS, c_szIDirectDraw, c_szTestCooperativeLevel);

    return TPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD4_GetDeviceIdentifier
//  Tests the IDirectDraw::GetDeviceIdentifier method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD4_GetDeviceIdentifier(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT hr = S_OK;
    ITPR    nITPR = ITPR_PASS;

    IDirectDraw        *pDD4;
    IDirectDraw        *tmp_p = NULL;
    DDDEVICEIDENTIFIER  dddid;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD4))
        return TPRFromITPR(g_tprReturnVal);


    // a-shende(07.09.1999): zero out ident struct
    memset(&dddid, 0x0, sizeof(DDDEVICEIDENTIFIER));

#if QAHOME_INVALIDPARAMS
#ifdef KATANA
    INVALID_CALL(
        pDD4->GetDeviceIdentifier(NULL, 0),
        c_szIDirectDraw,
        c_szGetDeviceIdentifier,
        TEXT("NULL, 0"));
    INVALID_CALL(
        pDD4->GetDeviceIdentifier(&dddid, PRESET),
        c_szIDirectDraw,
        c_szGetDeviceIdentifier,
        TEXT("non-NULL, invalid flags"));
#else // not KATANA
    // a-shende(07.15.1999) -- all DXPAX work is unsupported, use of tmp_p
    // variable allows the use of existing macros: ignore it.
    INVALID_CALL_POINTER_E(
        pDD4->GetDeviceIdentifier(NULL, 0),
        tmp_p,
        c_szIDirectDraw,
        c_szGetDeviceIdentifier,
        TEXT("NULL, 0"),
        DDERR_UNSUPPORTED);

    INVALID_CALL_POINTER_E(
        pDD4->GetDeviceIdentifier(&dddid, PRESET),
        tmp_p,
        c_szIDirectDraw,
        c_szGetDeviceIdentifier,
        TEXT("non-NULL, invalid flags"),
        DDERR_INVALIDPARAMS);
#endif // not KATANA
#endif // QAHOME_INVALIDPARAMS

    // Now try with flag 0.
    __try
    {
        hr = pDD4->GetDeviceIdentifier(&dddid, 0);
#ifdef KATANA
#define BUGID   0
#else // no KATANA
#define BUGID   1574
#endif // not KATANA
        if(DDERR_UNSUPPORTED != hr)
        {
            // a-shende(07.09.1999): flag this bug
            Report(
                RESULT_UNEXPECTED,
                c_szIDirectDraw,
                c_szGetDeviceIdentifier,
                hr,
                TEXT("0"),
                NULL,
                DDERR_UNSUPPORTED,
                RAID_HPURAID,
                BUGID);
#undef BUGID
            nITPR |= ITPR_FAIL;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
#ifdef KATANA
#define BUGID   5110
#else // KATANA
#define BUGID   0
#endif // KATANA
        Report(
            RESULT_EXCEPTION,
            c_szIDirectDraw,
            c_szGetDeviceIdentifier,
            0,
            TEXT("0"),
            NULL,
            0,
            RAID_HPURAID,
            BUGID);
#undef BUGID
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szGetDeviceIdentifier);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Help_IDD4_GetDeviceIdentifier()
//  Displays the properties of Driver
//
// Parameters:
//  pDD4           IDirectDraw pointer.
//  pddscaps        Entry representing the DDSCAPS value we should use.
//  pdwTotal        Pointer for the second argument.
//  pdwFree         Pointer for the third argument.
//
// Return value:
//  true if the test succeeds, false otherwise.

void Help_IDD4_GetDeviceIdentifier(
    IDirectDraw        *pDD4,
    DDDEVICEIDENTIFIER  *pdddid)
{
    // Driver Name and Description.
    Debug(LOG_COMMENT, TEXT("Driver Name = %s"), pdddid->szDriver);
    Debug(LOG_COMMENT, TEXT("Driver Description = %s"), pdddid->szDescription);

    // Driver Version.
    Debug(
        LOG_COMMENT,
        TEXT("Product = %d, Version = %d, SubVersion = %d, Build = %d"),
        HIWORD(pdddid->liDriverVersion.HighPart),
        LOWORD(pdddid->liDriverVersion.HighPart),
        HIWORD(pdddid->liDriverVersion.LowPart),
        LOWORD(pdddid->liDriverVersion.LowPart));

    // Vender ID (Manufacturer)
    if(pdddid->dwVendorId)
    {
        Debug(LOG_COMMENT, TEXT("Vendor ID = %d"), pdddid->dwVendorId);
    }
    else
    {
        Debug(LOG_COMMENT, TEXT("Unknown Manufacturer"));
    }

    // Device ID (Chipset)
    if(pdddid->dwDeviceId)
    {
        Debug(LOG_COMMENT, TEXT("Device ID = %d"), pdddid->dwDeviceId);
    }
    else
    {
        Debug(LOG_COMMENT, TEXT("Unknown Chipset"));
    }

    // SubSystem (Board).
    if(pdddid->dwSubSysId)
    {
        Debug(LOG_COMMENT, TEXT("SubSystem ID = %d"), pdddid->dwSubSysId);
    }
    else
    {
        Debug(LOG_COMMENT, TEXT("Unknown Board"));
    }

    // Revision level of chipset
    if(pdddid->dwRevision)
    {
        Debug(
            LOG_COMMENT,
            TEXT("Chipset Revision Level = %d"),
            pdddid->dwRevision);
    }
    else
    {
        Debug(LOG_COMMENT, TEXT("Unknown Chipset Revision Level"));
    }

    // GUID Identifier of the Driver
    Debug(
        LOG_COMMENT,
        TEXT("GUID ID = { %08x-%08x-%08x-%02x%02x-%02x%02x%02x%02x%02x%02x }"),
        pdddid->guidDeviceIdentifier.Data1,
        pdddid->guidDeviceIdentifier.Data2,
        pdddid->guidDeviceIdentifier.Data3,
        pdddid->guidDeviceIdentifier.Data4[0],
        pdddid->guidDeviceIdentifier.Data4[1],
        pdddid->guidDeviceIdentifier.Data4[2],
        pdddid->guidDeviceIdentifier.Data4[3],
        pdddid->guidDeviceIdentifier.Data4[4],
        pdddid->guidDeviceIdentifier.Data4[5],
        pdddid->guidDeviceIdentifier.Data4[6],
        pdddid->guidDeviceIdentifier.Data4[7]);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD4_GetSurfaceFromDC
//  Tests the IDirectDraw::GetSurfaceFromDC method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD4_GetSurfaceFromDC(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    ITPR    nITPR = ITPR_PASS;

    IDirectDraw    *pDD4;
    IDDS            *pIDDS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD4))
        return TPRFromITPR(g_tprReturnVal);

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

#if QAHOME_INVALIDCALLS
    HRESULT hr;
    INVALID_CALL(
        pDD4->GetSurfaceFromDC(NULL, NULL),
        c_szIDirectDraw,
        c_szGetSurfaceFromDC,
        TEXT("NULL, NULL"));
#endif // QAHOME_INVALIDCALLS

    // Get the DC for each surface

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szGetSurfaceFromDC);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD4_RestoreAllSurfaces
//  Tests the IDirectDraw::RestoreAllSurfaces method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD4_RestoreAllSurfaces(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw    *pDD4;
    HRESULT         hr;
    DDSURFACEDESC  ddsd2;
    LPDIRECTDRAWSURFACE pDDSPrimary4;
    LPDIRECTDRAWSURFACE pDDSBackBuffer4;

    ITPR            nITPR = ITPR_PASS;
    DDCAPS ddcHAL;

    memset(&ddcHAL, 0, sizeof(ddcHAL));
    ddcHAL.dwSize = sizeof(ddcHAL);

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    FinishDirectDrawSurface();
    FinishDirectDraw();

    if(!InitDirectDraw(pDD4))
        return TPRFromITPR(g_tprReturnVal);

    hr = pDD4->GetCaps(&ddcHAL, NULL);
    if(FAILED(hr))
    {
        Debug(LOG_FAIL,TEXT("Failed to retrieve HAL/HEL caps"));
    }

    // On the Katana, only full-screen exclusive mode is supported. There is
    // nothing special to test, so just call the method and verify correctness.
    hr = pDD4->SetCooperativeLevel(
        g_hMainWnd,
        DDSCL_FULLSCREEN);
    if(FAILED(hr))
    {
        nITPR |= ITPR_FAIL;
    }

    // Create a primary and back buffer
    memset(&ddsd2, 0, sizeof(ddsd2));
    ddsd2.dwSize             = sizeof(ddsd2);
    ddsd2.dwFlags            = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd2.ddsCaps.dwCaps     = DDSCAPS_PRIMARYSURFACE |
                               DDSCAPS_FLIP;
    ddsd2.dwBackBufferCount  = 1;
    pDDSPrimary4 = (IDirectDrawSurface *)PRESET;
    hr = pDD4->CreateSurface(&ddsd2, &pDDSPrimary4, NULL);
    if((DDERR_NOFLIPHW == hr || DDERR_INVALIDCAPS == hr) &&
        !(ddcHAL.ddsCaps.dwCaps & DDSCAPS_FLIP))
    {
        Debug(LOG_COMMENT, TEXT("Flipping unsupported, can't test surface"));
    }
    else if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        pDDSPrimary4,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        TEXT("primary")))
    {
        nITPR |= ITPR_ABORT;
    }
    else
    {
        DDSCAPS ddscaps2;

        // Back Buffer surface
        memset(&ddscaps2, 0, sizeof(DDSCAPS));
        ddscaps2.dwCaps = DDSCAPS_BACKBUFFER;
        pDDSBackBuffer4 = (IDirectDrawSurface *)PRESET;
        hr = pDDSPrimary4->EnumAttachedSurfaces(&pDDSBackBuffer4, Help_GetBackBuffer_Callback);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetAttachedSurface,
                hr,
                TEXT("DDSCAPS_BACKBUFFER"));
            pDDSBackBuffer4 = NULL;
            nITPR |= ITPR_ABORT;
        }

        hr = pDDSPrimary4->GetSurfaceDesc(&ddsd2);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetSurfaceDesc,
                hr);
            nITPR |= ITPR_ABORT;
        }

        hr = pDD4->SetDisplayMode(ddsd2.dwWidth, ddsd2.dwHeight, ddsd2.ddpfPixelFormat.dwRGBBitCount, 60, 0);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDraw,
                c_szSetDisplayMode,
                hr);
            nITPR |= ITPR_ABORT;
        }

        if(pDDSPrimary4->IsLost() != S_OK)
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szIsLost,
                hr,
                NULL,
                TEXT("for primary"));
            nITPR |= ITPR_ABORT;
        }
        else
        {

            if(pDDSBackBuffer4 && (pDDSBackBuffer4->IsLost() != S_OK))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawSurface,
                    c_szIsLost,
                    hr,
                    NULL,
                    TEXT("for back buffer"));
                nITPR |= ITPR_ABORT;
            }
            else
            {
                // Both the surface has "lost".
                // Try restoring them.
                hr = pDD4->RestoreAllSurfaces();
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDraw,
                        c_szRestoreAllSurfaces,
                        hr);
                    nITPR |= ITPR_FAIL;
                }
                else
                {
                    // It should have restored both primary and back buffer surface.
                    if(pDDSPrimary4->IsLost() != S_OK)
                    {
                        Report(
                            RESULT_UNEXPECTED,
                            c_szIDirectDrawSurface,
                            c_szIsLost,
                            hr,
                            NULL,
                            TEXT("for primary after RestoreAllSurfaces"),
                            S_OK);
                        nITPR |= ITPR_FAIL;
                    }
                    else
                    {
                        if(pDDSBackBuffer4 && (pDDSBackBuffer4->IsLost() != S_OK))
                        {
                            Report(
                                RESULT_UNEXPECTED,
                                c_szIDirectDrawSurface,
                                c_szIsLost,
                                hr,
                                NULL,
                                TEXT("for back buffer after RestoreAllSurfaces"),
                                S_OK);
                            nITPR |= ITPR_FAIL;
                        }
                        else
                        {
                            // We are done with test Restore the display mode.
                            hr = pDD4->RestoreDisplayMode();

                            // a-shende(07.21.1999): adding return check
                            if(FAILED(hr))
                            {
                                Report(
                                    RESULT_FAILURE,
                                    c_szIDirectDraw,
                                    c_szRestoreDisplayMode,
                                    hr);
                                nITPR |= ITPR_FAIL;
                            }
                        }
                    }
                }
            }
        }

        // Release the back buffer, if we got it
        if(pDDSBackBuffer4)
        {
            pDDSBackBuffer4->Release();
        }

        // Release the primary we created
        pDDSPrimary4->Release();
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szRestoreAllSurfaces);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD4_CreateSurface
//  Tests the IDirectDraw::CreateSurface method (client surfaces).
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD4_CreateSurface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT hr;
    BYTE    *pMemory = NULL;
    int     nIndex;
    ITPR    nITPR = ITPR_PASS;

    IDirectDraw        *pDD4;
    IDirectDrawSurface *pDDS4;
    DDSURFACEDESC      ddsd2;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD4))
        return TPRFromITPR(g_tprReturnVal);

    // Allocate memory for a surface
    pMemory = new BYTE[64*64*3];
    if(!pMemory)
    {
        Debug(
            LOG_ABORT,
            TEXT("Unable to allocate memory for client surface"));
        return TPR_ABORT;
    }

    // Write some data that we can read later
    for(nIndex = 0; nIndex < 10; nIndex++)
    {
        ((DWORD *)pMemory)[nIndex] = (DWORD)(PRESET*nIndex);
    }

    // Create an off-screen surface in system memory
    memset(&ddsd2, 0, sizeof(ddsd2));
    ddsd2.dwSize            = sizeof(ddsd2);
    ddsd2.dwFlags           = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_LPSURFACE | DDSD_PITCH | DDSD_CAPS;
    ddsd2.ddsCaps.dwCaps    = DDSCAPS_SYSTEMMEMORY;
    ddsd2.dwWidth           = 64;
    ddsd2.dwHeight          = 64;
    ddsd2.lPitch            = ddsd2.dwWidth*3;
    ddsd2.lpSurface         = pMemory;
    ddsd2.ddpfPixelFormat.dwSize        = sizeof(ddsd2.ddpfPixelFormat);
    ddsd2.ddpfPixelFormat.dwFlags       = DDPF_RGB;
    ddsd2.ddpfPixelFormat.dwRGBBitCount = 24;
    ddsd2.ddpfPixelFormat.dwRBitMask    = 0x00FF0000;
    ddsd2.ddpfPixelFormat.dwGBitMask    = 0x0000FF00;
    ddsd2.ddpfPixelFormat.dwBBitMask    = 0x000000FF;

    pDDS4 = (IDirectDrawSurface *)PRESET;
    hr = pDD4->CreateSurface(&ddsd2, &pDDS4, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
        pDDS4,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr))
    {
        delete[] pMemory;
        return TPR_FAIL;
    }

    // Verify that the surface is pointing to the new
    // memory
    memset(&ddsd2, 0, sizeof(ddsd2));
    ddsd2.dwSize = sizeof(ddsd2);
    hr = pDDS4->Lock(
        NULL,
        &ddsd2,
        DDLOCK_WAITNOTBUSY,
        NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            NULL,
            TEXT("for off-screen surface after call to")
            TEXT(" CreateSurface"));
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Read off the data
        for(nIndex = 0; nIndex < 10; nIndex++)
        {
            if(((DWORD *)ddsd2.lpSurface)[nIndex] !=
               (DWORD)(PRESET*nIndex))
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("The data in the")
                    TEXT(" surface after call to %s is")
                    TEXT(" incorrect"),
                    c_szCreateSurface);
                nITPR |= ITPR_FAIL;
                break;
            }
        }
        if(nIndex == 10)
        {
            Debug(
                LOG_PASS,
                TEXT("The data in the surface")
                TEXT(" after call to %s is correct"),
                c_szCreateSurface);
        }

        pDDS4->Unlock(NULL);
    }

    if(pDDS4)
        pDDS4->Release();

    if(pMemory)
        delete[] pMemory;

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szCreateSurface);

    return TPRFromITPR(nITPR);
}

#endif // TEST_IDD4

////////////////////////////////////////////////////////////////////////////////
