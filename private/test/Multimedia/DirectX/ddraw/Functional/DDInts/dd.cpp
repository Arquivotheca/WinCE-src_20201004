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
//  Module: dd.cpp
//          BVT for the IDirectDraw interface functions.
//
//  Revision History:
//      09/12/96    lblanco     Created for the TUX DLL Wizard.
//      09/26/96    lblanco     Added IDirectDraw BVTs.
//      12/20/96    lblanco     Enhanced tests.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include "xmlParser.h"
#include "string.h"

#if TEST_IDD
////////////////////////////////////////////////////////////////////////////////
// Macros for this module

#define MAX_SURFACE_TYPES   10
#define MAX_ENUM_SURFACES   3
#define SURFACEDATA_GDI     0x12345678
#define SURFACEDATA_NOTGDI  0xabcdef00
#define DUPDWORDCHECKCOUNT  32
#define MAX_FOURCC_CHAR     4

// The following declaration allows the test to build with DX5 headers
#ifndef DDCAPS2_COPYFOURCC
#define DDCAPS2_COPYFOURCC              0x00008000l
#endif

////////////////////////////////////////////////////////////////////////////////
// Types used in this module

struct DDSCREATED
{
    IUnknown    *m_pUnknown;
    bool        m_bSeenBefore;
};

////////////////////////////////////////////////////////////////////////////////
// FourCC Codes Structure

struct FourCCCodeList
{
    DWORD        m_dwFourCCCode;
    UINT         m_nSeen;
    FourCCCodeList* pNext;
}*g_pFourCCCode;


////////////////////////////////////////////////////////////////////////////////
// Local function prototypes

HRESULT CALLBACK Help_IDD_EnumDisplayModes_Callback(
    LPDDSURFACEDESC,
    LPVOID);
HRESULT CALLBACK Help_IDD_EnumSurfaces_Callback(
    IDirectDrawSurface *,
    LPDDSURFACEDESC,
    LPVOID);

ITPR    Help_IDD_CreatePalette(IDirectDraw *, int, DWORD);
ITPR    Help_IDD_CreateSurface(IDirectDraw *, int);
bool    Help_Setup_CreateSurface(IDirectDraw *);
ITPR    Help_DumpDDCaps(DDCAPS &);

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static bool             g_bDDEDMCallbackExpected = false,
                        g_bDDESCallbackExpected = false,
                        g_bDDSDMCallbackExpected = false;
static DDSURFACEDESC    g_rgddsd[MAX_SURFACE_TYPES];
static LPCTSTR          g_pszSurfaceName[MAX_SURFACE_TYPES];
static int              g_nddsd = 0;
static DDSCREATED       g_ddsCreated[MAX_ENUM_SURFACES + 1];
static LONG             g_llistLength = 0;

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_AddRef_Release
//  Tests the IDirectDraw::AddRef and IDirectDraw::Release methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_AddRef_Release(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw *pDD;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

    if(!Test_AddRefRelease(pDD, c_szIDirectDraw))
        return TPR_FAIL;

    Report(RESULT_SUCCESS, c_szIDirectDraw, TEXT("AddRef and Release"));

    return TPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_Compact
//  Tests the IDirectDraw::Compact method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_Compact(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    return TPR_SKIP;
/*
    IDirectDraw *pDD;
    HRESULT     hr;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

    // The IDirectDraw::Compact method is not yet implemented. This general
    // test will catch AVs if and when it is.
    hr = pDD->Compact();
    if(FAILED(hr))
    {
        Report(RESULT_FAILURE, c_szIDirectDraw, c_szCompact, hr);
        return TPR_FAIL;
    }


    Report(RESULT_SUCCESS, c_szIDirectDraw, c_szCompact);

    return TPR_PASS;
*/
}

#if TEST_IDDC
////////////////////////////////////////////////////////////////////////////////
// Test_IDD_CreateClipper
//  Tests the IDirectDraw::CreateClipper method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_CreateClipper(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT hr;
    ITPR    nITPR = ITPR_PASS;

    IDirectDraw         *pDD;
    IDirectDrawClipper  *pDDC;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

    pDDC = (IDirectDrawClipper *)PRESET;
    hr = pDD->CreateClipper(0, &pDDC, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
        pDDC,
        c_szIDirectDraw,
        c_szCreateClipper,
        hr))
    {
        nITPR |= ITPR_FAIL;
    }
    else
    {
        BOOL fChanged = FALSE;
        // Call a method to make sure the pointer is really valid
        hr = pDDC->IsClipListChanged(&fChanged);
        if(FAILED(hr))
        {
            Report(
                RESULT_UNEXPECTED,
                c_szIDirectDrawClipper,
                c_szIsClipListChanged,
                hr,
                NULL,
                NULL,
                S_OK);
            nITPR |= ITPR_FAIL;
        }
        pDDC->Release();
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szCreateClipper);

    return TPRFromITPR(nITPR);
}
#endif // TEST_IDDC

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_CreatePalette
//  Tests the IDirectDraw::CreatePalette method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_CreatePalette(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
//    IDirectDraw *pDD;
//    int         nBpp;
    ITPR        nITPR = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    Report(RESULT_SUCCESS, c_szIDirectDraw, c_szCreatePalette, DDERR_UNSUPPORTED, _T("Palette not supported in this release"));

    return TPRFromITPR(ITPR_SKIP);

//    if(!InitDirectDraw(pDD))
//        return TPRFromITPR(g_tprReturnVal);
//
//    DDCAPS ddcHAL = { 0 };
//    ddcHAL.dwSize = sizeof(ddcHAL);
//    if(FAILED(pDD->GetCaps(&ddcHAL, NULL)))
//    {
//        Report(RESULT_ABORT, c_szIDirectDraw, c_szGetCaps);
//        return TPR_ABORT;
//    }
//
//    nBpp = 8;
//    nITPR |= Help_IDD_CreatePalette(pDD, nBpp, ddcHAL.ddsCaps.dwCaps);
//
//    if(nITPR == ITPR_PASS)
//        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szCreatePalette);
//
//    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_CreateSurface
//  Tests the IDirectDraw::CreateSurface method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_CreateSurface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw *pDD;
    int         nIndex;
    ITPR        nITPR = ITPR_PASS,
                nResult;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

#if NEED_INITDDS
    // Avoid interference with IDirectDrawSurface tests
    FinishDirectDrawSurface();
#endif // NEED_INITDDS

    // Set up the tests
    if(!Help_Setup_CreateSurface(pDD))
        return TPR_ABORT;

#if QAHOME_INVALIDPARAMS
    IDirectDrawSurface  *pDDS;
    HRESULT             hr = S_OK;
    DDSURFACEDESC       ddsd;

    // Pass NULL for the DDSURFACEDESC pointer
    pDDS = (IDirectDrawSurface *)PRESET;
    INVALID_CALL_POINTER_RAID_FLAGS(
        pDD->CreateSurface(NULL, &pDDS, NULL),
        pDDS,
        c_szIDirectDraw,
        c_szCreateSurface,
        TEXT("NULL DDSURFACEDESC pointer"),
        IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
        IFKATANAELSE(2832, 0),
        CRP_ALLOWNONULLOUT);

#if 0
    // Pass bogus pointer for the DDSURFACEDESC pointer
    pDDS = (IDirectDrawSurface *)PRESET;
    INVALID_CALL_POINTER_RAID_FLAGS(
        pDD->CreateSurface((DDSURFACEDESC*)PRESET, &pDDS, NULL),
        pDDS,
        c_szIDirectDraw,
        c_szCreateSurface,
        TEXT("Bogus DDSURFACEDESC pointer")
        RAID_INVALID,
        0,
        CRP_ALLOWNONULLOUT);
#endif

    // Pass NULL for the interface pointer pointer
    pDDS = (IDirectDrawSurface *)PRESET;
    INVALID_CALL_RAID(
        pDD->CreateSurface(g_rgddsd, NULL, NULL),
        c_szIDirectDraw,
        c_szCreateSurface,
        TEXT("NULL interface pointer pointer"),
        IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
        IFKATANAELSE(2832, 0));

#if 0
    // Pass bogus interface pointer pointer
    pDDS = (IDirectDrawSurface *)PRESET;
    INVALID_CALL(
        pDD->CreateSurface(g_rgddsd, (IDirectDrawSurface**)PRESET, NULL),
        c_szIDirectDraw,
        c_szCreateSurface,
        TEXT("bogus interface pointer pointer"));
#endif

    // Ask to create a surface specifying the pixel format, but purposefully
    // leave that field blank
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize         = sizeof(ddsd);
    ddsd.dwFlags        = DDSD_PIXELFORMAT | DDSD_WIDTH |
                          DDSD_HEIGHT;
    ddsd.dwWidth        = 256;
    ddsd.dwHeight       = 256;
    pDDS = (IDirectDrawSurface *)PRESET;
    INVALID_CALL_POINTER_E_RAID(
        hr = pDD->CreateSurface(&ddsd, &pDDS, NULL),
        pDDS,
        c_szIDirectDraw,
        c_szCreateSurface,
        TEXT("DDSD_PIXELFORMAT with empty pixel format field"),
        DDERR_INVALIDPIXELFORMAT,
        IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
        IFKATANAELSE(588, 0));

    // Pass an invalid pixel format (note that the depth is only 16 bits, but
    // the masks suggest a 32-bit format)
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize         = sizeof(ddsd);
    ddsd.dwFlags        = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT |
                          DDSD_PIXELFORMAT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
    ddsd.dwWidth        = 128;
    ddsd.dwHeight       = 128;

    ddsd.ddpfPixelFormat.dwSize             = sizeof(ddsd.ddpfPixelFormat);
    ddsd.ddpfPixelFormat.dwFlags            = DDPF_ALPHAPIXELS | DDPF_RGB;
    ddsd.ddpfPixelFormat.dwRGBBitCount      = 16;
    ddsd.ddpfPixelFormat.dwRBitMask         = 0x00FF0000;
    ddsd.ddpfPixelFormat.dwGBitMask         = 0x0000FF00;
    ddsd.ddpfPixelFormat.dwBBitMask         = 0x000000FF;
    ddsd.ddpfPixelFormat.dwRGBAlphaBitMask  = 0xFF000000;
    pDDS = (IDirectDrawSurface *)PRESET;

    INVALID_CALL_POINTER_E_RAID(
        hr = pDD->CreateSurface(&ddsd, &pDDS, NULL),
        pDDS,
        c_szIDirectDraw,
        c_szCreateSurface,
        TEXT("invalid pixel format"),
        // The call returns different error codes depending on
        // the display driver, but both are acceptable, so
        // allow whatever we got back, and check later.
        hr,
        IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
        IFKATANAELSE(588, 0));

    if(hr != DDERR_INVALIDPIXELFORMAT && hr != DDERR_UNSUPPORTEDFORMAT)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDraw,
            c_szCreateSurface,
            hr,
            NULL,
            TEXT("invalid pixel format"),
            DDERR_INVALIDPIXELFORMAT,
            IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
            IFKATANAELSE(588, 0));
    }

    Debug(LOG_COMMENT, TEXT("Completed invalid param test"));
#endif // QAHOME_INVALIDPARAMS

    nIndex = 0;
    while(1)
    {
        nResult = Help_IDD_CreateSurface(pDD, nIndex++);

        // Was this the last test?
        if(nResult == ITPR_SKIP)
            break;

        nITPR |= nResult;
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szCreateSurface);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_DuplicateSurface
//  Tests the IDirectDraw::DuplicateSurface method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_DuplicateSurface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;
    return TPR_SKIP;

/*
    IDirectDraw *pDD;
    HRESULT     hr;
    ITPR        nITPR = ITPR_PASS;
    int         nIndex;
    DWORD       dwSeed;

    IDirectDrawSurface  *pDDS1 = NULL,
                        *pDDS2 = NULL;
    DDSURFACEDESC       ddsd1,
                        ddsd2;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

#if NEED_INITDDS
    // Avoid interference with IDirectDrawSurface tests
    FinishDirectDrawSurface();
#endif // NEED_INITDDS

#if QAHOME_INVALIDPARAMS
    // Create a simple primary surface. This shouldn't be one that we can
    // duplicate
    memset(&ddsd1, 0, sizeof(ddsd1));
    ddsd1.dwSize = sizeof(ddsd1);
    ddsd1.dwFlags = DDSD_CAPS;
    ddsd1.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    pDDS1 = (IDirectDrawSurface *)PRESET;
    hr = pDD->CreateSurface(&ddsd1, &pDDS1, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
        pDDS1,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        NULL,
        TEXT("for primary")))
    {
        pDDS1 = NULL;
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Attempt to duplicate the primary
        pDDS2 = (IDirectDrawSurface *)PRESET;
        hr = pDD->DuplicateSurface(pDDS1, &pDDS2);
        if(!CheckReturnedPointer(
            CRP_RELEASE | CRP_ALLOWNONULLOUT,
            pDDS2,
            c_szIDirectDraw,
            c_szDuplicateSurface,
            hr,
            TEXT("primary"),
            NULL,
            DDERR_CANTDUPLICATE))
        {
            pDDS2 = NULL;
            nITPR |= ITPR_FAIL;
        }

        pDDS1->Release();
        pDDS1 = NULL;
        pDDS2 = NULL;
    }
#endif // QAHOME_INVALIDPARAMS

    // Create a surface other than the primary, which we should be able to
    // duplicate with no problems.
    memset(&ddsd1, 0, sizeof(ddsd1));
    ddsd1.dwSize = sizeof(ddsd1);
    ddsd1.dwFlags = DDSD_WIDTH | DDSD_HEIGHT;
    ddsd1.dwWidth = 100;
    ddsd1.dwHeight = 100;
    pDDS1 = (IDirectDrawSurface *)PRESET;
    hr = pDD->CreateSurface(&ddsd1, &pDDS1, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
        pDDS1,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        NULL,
        TEXT("for off-screen surface")))
    {
        pDDS1 = NULL;
        nITPR |= ITPR_ABORT;
    }
    else
    {
#if QAHOME_INVALIDPARAMS
        // Pass NULL for the first pointer
        pDDS2 = (IDirectDrawSurface *)PRESET;
        INVALID_CALL_POINTER_E_RAID_FLAGS(
            pDD->DuplicateSurface(NULL, &pDDS2),
            pDDS2,
            c_szIDirectDraw,
            c_szDuplicateSurface,
            TEXT("NULL, non-NULL"),
            // The call returns two different error codes for Katana
            // and DXPak, but both are appropriate, so...
            IFKATANAELSE(DDERR_INVALIDPARAMS, DDERR_INVALIDOBJECT),
            IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
            IFKATANAELSE(2114, 0),
            CRP_ALLOWNONULLOUT);

        // Pass NULL for the second pointer
        INVALID_CALL(
            pDD->DuplicateSurface(pDDS1, NULL),
            c_szIDirectDraw,
            c_szDuplicateSurface,
            TEXT("non-NULL, NULL"));
#endif // QAHOME_INVALIDPARAMS

        // Duplicate this surface
        pDDS2 = (IDirectDrawSurface *)PRESET;
        hr = pDD->DuplicateSurface(pDDS1, &pDDS2);
        if(!CheckReturnedPointer(
            CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
            pDDS2,
            c_szIDirectDraw,
            c_szDuplicateSurface,
            hr,
            TEXT("for off-screen surface")))
        {
            pDDS2 = NULL;
            nITPR |= ITPR_FAIL;
        }
        else if(pDDS2 == pDDS1)
        {
            // We just got the same pointer back -- bad!
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s returned the same pointer as the")
                TEXT(" original surface pointer"),
                c_szIDirectDraw,
                c_szDuplicateSurface);
            nITPR |= ITPR_FAIL;
            pDDS2->Release();
        }
        else
        {
            // Lock the first surface
            memset(&ddsd1, 0, sizeof(ddsd1));
            ddsd1.dwSize = sizeof(ddsd1);
            hr = pDDS1->Lock(NULL, &ddsd1, DDLOCK_SURFACEMEMORYPTR, NULL);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szLock,
                    hr,
                    NULL,
                    TEXT("for original surface"));
                nITPR |= ITPR_ABORT;
            }
            else
            {
                // Write some data to the original surface
                dwSeed = GetTickCount();
                srand(dwSeed);
                for(nIndex = 0; nIndex < DUPDWORDCHECKCOUNT; nIndex++)
                {
                    ((DWORD *)ddsd1.lpSurface)[nIndex] = (DWORD)rand();
                }
                pDDS1->Unlock(ddsd1.lpSurface);

                // Lock the duplicated surface
                memset(&ddsd2, 0, sizeof(ddsd2));
                ddsd2.dwSize = sizeof(ddsd2);
                hr = pDDS2->Lock(NULL, &ddsd2, DDLOCK_SURFACEMEMORYPTR, NULL);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDrawSurface,
                        c_szLock,
                        hr,
                        NULL,
                        TEXT("for duplicate surface"));
                    nITPR |= ITPR_ABORT;
                }
                else
                {
                    // Read data from the duplicate surface; it should be the
                    // same as the data we just wrote to the original surface
                    srand(dwSeed);
                    for(nIndex = 0; nIndex < DUPDWORDCHECKCOUNT; nIndex++)
                    {
                        DWORD dwExpect = (DWORD)rand();
                        if(((DWORD *)ddsd2.lpSurface)[nIndex] != dwExpect)
                        {
                            Debug(
                                LOG_FAIL,
                                FAILURE_HEADER TEXT("The duplicated surface")
                                TEXT(" doesn't contain the data we wrote to")
                                TEXT(" the original surface. ")
                                TEXT(" At index %d, found 0x%08x, expected 0x%08x."),
                                nIndex,
                                ((DWORD *)ddsd2.lpSurface)[nIndex],
                                dwExpect);
                            nITPR |= ITPR_FAIL;
                            break;
                        }
                    }

                    pDDS2->Unlock(ddsd2.lpSurface);

                    // UPDATE:  Perform different operations on the two surfaces
                    //          and verify that operations in one surface don't
                    //          interfere with the other surface.
                }
            }

            // Release the duplicated surface
            pDDS2->Release();
        }

        // Release the original surface
        pDDS1->Release();
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szDuplicateSurface);

    return TPRFromITPR(nITPR);
*/
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_EnumDisplayModes
//  Tests the IDirectDraw::EnumDisplayModes method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_EnumDisplayModes(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw *pDD;
    HRESULT     hr = S_OK;
    ITPR        nITPR = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

#if QAHOME_INVALIDPARAMS
    // NULL function address
    INVALID_CALL(
        pDD->EnumDisplayModes(0, NULL, NULL, NULL),
        c_szIDirectDraw,
        c_szEnumDisplayModes,
        TEXT("0, NULL, NULL, NULL"));
#endif // QAHOME_INVALIDPARAMS

    g_bTestPassed = true;
    g_bDDEDMCallbackExpected = true;
    g_pContext = (void *)PRESET;
    g_nCallbackCount = -1; // No effective limit
    hr = pDD->EnumDisplayModes(
        0,
        NULL,
        g_pContext,
        Help_IDD_EnumDisplayModes_Callback);
    g_bDDEDMCallbackExpected = false;
    if(FAILED(hr))
    {
        Report(RESULT_FAILURE, c_szIDirectDraw, c_szEnumDisplayModes, hr);
        nITPR |= ITPR_FAIL;
    }
    else if(!g_bTestPassed)
    {
        Debug(
            LOG_FAIL,
            TEXT("%s::%s returned success, but the call ")
            TEXT("behaved unexpectedly"),
            c_szIDirectDraw,
            c_szEnumDisplayModes);
        nITPR |= ITPR_FAIL;
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szEnumDisplayModes);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_EnumSurfaces
//  Tests the IDirectDraw::EnumSurfaces method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_EnumSurfaces(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw         *pDD;
    IDirectDrawSurface  *pDDS1,
                        *pDDS2;
#if TEST_ZBUFFER
    IDirectDrawSurface  *pDDS3;
#endif // TEST_ZBUFFER
    HRESULT             hr = S_OK;
    DDSURFACEDESC       ddsd;
    int                 index;
    ITPR                nITPR = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

#if NEED_INITDDS
    // Avoid interference with IDirectDrawSurface tests
    FinishDirectDrawSurface();
#endif // NEED_INITDDS

#if QAHOME_INVALIDPARAMS
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    INVALID_CALL(
        pDD->EnumSurfaces(DDENUMSURFACES_DOESEXIST | DDENUMSURFACES_ALL, &ddsd, NULL, NULL),
        c_szIDirectDraw,
        c_szEnumSurfaces,
        TEXT("NULL callback pointer"));
#endif // QAHOME_INVALIDPARAMS

    // Try the method when no surfaces are present
    memset(g_ddsCreated, 0, sizeof(g_ddsCreated));
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    g_bDDESCallbackExpected = true;
    g_bTestPassed = true;
    g_pContext = (void *)pDD;
    hr = pDD->EnumSurfaces(
        DDENUMSURFACES_DOESEXIST | DDENUMSURFACES_ALL,
        &ddsd,
        g_pContext,
        Help_IDD_EnumSurfaces_Callback);
    g_bDDESCallbackExpected = false;
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDraw,
            c_szEnumSurfaces,
            hr,
            NULL,
            TEXT("when no surfaces existed"));
        nITPR |= ITPR_FAIL;
    }
    else if(!g_bTestPassed)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s::%s returned success, but the call ")
            TEXT("behaved unexpectedly when no surfaces existed"),
            c_szIDirectDraw,
            c_szEnumSurfaces);
        nITPR |= ITPR_FAIL;
    }

    // Create a few surfaces and try again. See if the method can enumerate
    // three surfaces.
    memset(g_ddsCreated, 0, sizeof(g_ddsCreated));

    // Create a primary surface
    ddsd.dwSize         = sizeof(ddsd);
    ddsd.dwFlags        = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    hr = pDD->CreateSurface(&ddsd, &pDDS1, NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDraw,
            c_szCreateSurface,
            hr,
            NULL,
            TEXT("for primary"));
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Get IUnknown interface
        hr = pDDS1->QueryInterface(
            IID_IUnknown,
            (void **)&g_ddsCreated[0].m_pUnknown);
        pDDS1->Release();
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szQueryInterface,
                hr,
                c_szIID_IUnknown,
                TEXT("for primary"));
            nITPR |= ITPR_ABORT;
        }
        else
        {
            // Create an off-screen plain (if surface caps are set, cannot be 0)
            ddsd.dwFlags        = DDSD_WIDTH | DDSD_HEIGHT;
            ddsd.dwWidth        = 100;
            ddsd.dwHeight       = 100;
            hr = pDD->CreateSurface(&ddsd, &pDDS2, NULL);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDraw,
                    c_szCreateSurface,
                    hr,
                    NULL,
                    TEXT("for off-screen surface"));
                nITPR |= ITPR_ABORT;
            }
            else
            {
                // Get IUnknown interface
                hr = pDDS2->QueryInterface(
                    IID_IUnknown,
                    (void **)&g_ddsCreated[1].m_pUnknown);
                pDDS2->Release();
                if(FAILED(hr))
                {
                    Report(
                        RESULT_ABORT,
                        c_szIDirectDrawSurface,
                        c_szQueryInterface,
                        hr,
                        c_szIID_IUnknown,
                        TEXT("for off-screen surface"));
                    nITPR |= ITPR_ABORT;
                }
                else
                {
#if TEST_ZBUFFER
                    // Create a Z-buffer
                    ddsd.dwFlags            = DDSD_CAPS | DDSD_WIDTH |
                                              DDSD_HEIGHT |
                                              DDSD_ZBUFFERBITDEPTH;
                    ddsd.ddsCaps.dwCaps     = DDSCAPS_ZBUFFER;
                    ddsd.dwWidth            = 100;
                    ddsd.dwHeight           = 100;
                    ddsd.dwZBufferBitDepth  = 16;
                    hr = pDD->CreateSurface(&ddsd, &pDDS3, NULL);
                    if(FAILED(hr))
                    {
                        Report(
                            RESULT_ABORT,
                            c_szIDirectDraw,
                            c_szCreateSurface,
                            hr,
                            NULL,
                            TEXT("for Z-buffer"));
                        pDDS1->Release();
                        pDDS2->Release();
                        nITPR |= ITPR_ABORT;
                    }
                    else
                    {
                        // Get IUnknown interface
                        hr = pDDS3->QueryInterface(
                            IID_IUnknown,
                            (void **)&g_ddsCreated[2].m_pUnknown);
                        pDDS3->Release();
                        if(FAILED(hr))
                        {
                            Report(
                                RESULT_ABORT,
                                c_szIDirectDrawSurface,
                                c_szQueryInterface,
                                hr,
                                c_szIID_IUnknown,
                                TEXT("for Z-buffer"));
                            nITPR |= ITPR_ABORT;
                        }
                        else
                        {
#endif // TEST_ZBUFFER
                            // Now, see if we can enumerate these surfaces
                            memset(&ddsd, 0, sizeof(ddsd));
                            ddsd.dwSize = sizeof(ddsd);
                            g_bDDESCallbackExpected = true;
                            g_bTestPassed = true;
                            g_pContext = (void *)pDD;
                            hr = pDD->EnumSurfaces(
                                DDENUMSURFACES_DOESEXIST | DDENUMSURFACES_ALL,
                                &ddsd,
                                g_pContext,
                                Help_IDD_EnumSurfaces_Callback);
                            g_bDDESCallbackExpected = false;

                            // Did the EnumSurfaces call succeed?
                            if(FAILED(hr))
                            {
                                Report(
                                    RESULT_FAILURE,
                                    c_szIDirectDraw,
                                    c_szEnumSurfaces,
                                    hr,
                                    NULL,
                                    TEXT("when three surfaces existed"));
                                nITPR |= ITPR_FAIL;
                            }
                            else if(!g_bTestPassed)
                            {
                                // We saw some surface more than once
                                Debug(
                                    LOG_FAIL,
                                    FAILURE_HEADER TEXT("%s::%s returned")
                                    TEXT(" success, but the call behaved")
                                    TEXT(" unexpectedly when three surfaces")
                                    TEXT(" existed"),
                                    c_szIDirectDraw,
                                    c_szEnumSurfaces);
                                nITPR |= ITPR_FAIL;
                            }
                            else
                            {
                                // Did we not see some surface?
                                for(index = 0;
                                    g_ddsCreated[index].m_pUnknown;
                                    index++)
                                {
                                    if(!g_ddsCreated[index].m_bSeenBefore)
                                    {
                                        Debug(
                                            LOG_FAIL,
                                            FAILURE_HEADER TEXT("%s returned")
                                            TEXT(" success, but some surfaces")
                                            TEXT(" weren't enumerated to us"));
                                        nITPR |= ITPR_FAIL;
                                        break;
                                    }
                                }
                            }

#if TEST_ZBUFFER
                            // Release the IUnknown for the Z buffer
                            if(g_ddsCreated[2].m_pUnknown)
                            {
                                g_ddsCreated[2].m_pUnknown->Release();
                            }
                        }
                    }
#endif // TEST_ZBUFFER

                    // Release the IUnknown for the off-screen surface
                    if(g_ddsCreated[1].m_pUnknown)
                    {
                        g_ddsCreated[1].m_pUnknown->Release();
                    }
                }
            }

            // Release the IUnknown for the primary
            if(g_ddsCreated[0].m_pUnknown)
            {
                g_ddsCreated[0].m_pUnknown->Release();
            }
        }
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szEnumSurfaces);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_FlipToGDISurface
//  Tests the IDirectDraw::FlipToGDISurface method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_FlipToGDISurface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT hr;
    DWORD   dwData;
    ITPR    nITPR = ITPR_PASS;

    IDirectDraw         *pDD;
    IDirectDrawSurface  *pDDS;
    DDSURFACEDESC       ddsd;
    DDCAPS ddcHAL;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    memset(&ddcHAL, 0, sizeof(ddcHAL));
    ddcHAL.dwSize = sizeof(ddcHAL);

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

    hr = pDD->GetCaps(&ddcHAL, NULL);
    if(FAILED(hr))
    {
        Debug(LOG_FAIL,TEXT("Failed to retrieve HAL/HEL caps"));
        nITPR |= TPR_FAIL;
    }

#if NEED_INITDDS
    // Avoid interference with IDirectDrawSurface tests
    FinishDirectDrawSurface();
#endif // NEED_INITDDS

    // Set up a primary surface with a back buffer
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize             = sizeof(ddsd);
    ddsd.dwFlags            = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps     = DDSCAPS_PRIMARYSURFACE |
                              DDSCAPS_FLIP;
    ddsd.dwBackBufferCount  = 1;

    // Create the surface
    pDDS = (IDirectDrawSurface *)PRESET;
    hr = pDD->CreateSurface(&ddsd, &pDDS, NULL);

    if((DDERR_NOFLIPHW == hr || DDERR_INVALIDCAPS == hr) &&
        !(ddcHAL.ddsCaps.dwCaps & DDSCAPS_FLIP))
    {
        Debug(
            LOG_COMMENT,
            TEXT("Flipping unsupported, can't test surface"));
        return TPR_SKIP;
    }

    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        pDDS,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr))
    {
        return TPR_ABORT;
    }

    // Now that we have a surface, we should be able to flip
    hr = pDD->FlipToGDISurface();
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDraw,
            c_szFlipToGDISurface,
            hr,
            NULL,
            TEXT("the first time"));
        pDDS->Release();
        return TPR_FAIL;
    }

    // The current surface is presumably the GDI surface. We only have one
    // back buffer, so flip to it and write some data
    hr = pDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szFlip,
            hr,
            NULL,
            TEXT("after first call to FlipToGDISurface"));
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Lock it and write the data
        ddsd.dwSize = sizeof(ddsd);
        ddsd.lpSurface = (void *)PRESET;
        hr = pDDS->Lock(
            NULL,
            &ddsd,
            DDLOCK_WAITNOTBUSY,
            NULL);
        if(!CheckReturnedPointer(
            CRP_NOTINTERFACE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
            ddsd.lpSurface,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            NULL,
            TEXT("for primary after first call to FlipToGDISurface")))
        {
            pDDS->Release();
            return TPRFromITPR(nITPR | ITPR_ABORT);
        }
        else
        {
            *(DWORD *)ddsd.lpSurface = SURFACEDATA_NOTGDI;
            hr = pDDS->Unlock(NULL);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawSurface,
                    c_szUnlock,
                    hr,
                    NULL,
                    TEXT("for primary after first call to FlipToGDISurface"));
                nITPR |= ITPR_ABORT;
            }

            // Flip back to the GDI surface
            hr = pDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawSurface,
                    c_szFlip,
                    hr,
                    NULL,
                    TEXT("for primary after writing data to the back buffer"));
                return TPRFromITPR(nITPR | ITPR_ABORT);
            }
        }
    }

    // Store data on this surface, which is presumably now the GDI surface
    ddsd.dwSize = sizeof(ddsd);
    ddsd.lpSurface = (void *)PRESET;
    hr = pDDS->Lock(NULL, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
    if(!CheckReturnedPointer(
        CRP_NOTINTERFACE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        ddsd.lpSurface,
        c_szIDirectDrawSurface,
        c_szLock,
        hr,
        NULL,
        TEXT("for primary before second call to FlipToGDISurface")))
    {
        pDDS->Release();
        return TPRFromITPR(nITPR | ITPR_ABORT);
    }
    *(DWORD *)ddsd.lpSurface = SURFACEDATA_GDI;
    hr = pDDS->Unlock(NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szUnlock,
            hr,
            NULL,
            TEXT("for primary before second call to FlipToGDISurface"));
        nITPR |= ITPR_ABORT;
    }

    // Flip again
    hr = pDD->FlipToGDISurface();
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDraw,
            c_szFlipToGDISurface,
            hr,
            NULL,
            TEXT("the second time"));
        pDDS->Release();
        return TPRFromITPR(nITPR | ITPR_FAIL);
    }

    // We should still have the same primary, since the primary was already
    // the GDI surface before we called the method
    ddsd.dwSize = sizeof(ddsd);
    ddsd.lpSurface = (void *)PRESET;
    hr = pDDS->Lock(NULL, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
    if(!CheckReturnedPointer(
        CRP_NOTINTERFACE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        ddsd.lpSurface,
        c_szIDirectDrawSurface,
        c_szLock,
        hr,
        NULL,
        TEXT("for primary after call to FlipToGDISurface")))
    {
        pDDS->Release();
        return TPRFromITPR(nITPR | ITPR_ABORT);
    }
    dwData = *(DWORD *)ddsd.lpSurface;
    hr = pDDS->Unlock(NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szUnlock,
            hr,
            NULL,
            TEXT("for primary after call to FlipToGDISurface"));
        nITPR |= ITPR_ABORT;
    }

    // Are we still in the same place?
    if(dwData != SURFACEDATA_GDI)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s::%s for the primary returned a different")
            TEXT(" surface than expected after second call to %s"),
            c_szIDirectDrawSurface,
            c_szLock,
            c_szFlipToGDISurface);
        nITPR |= ITPR_FAIL;
    }

    // Flip to the back buffer
    hr = pDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
    if(FAILED(hr))
    {
        Report(RESULT_ABORT, c_szIDirectDrawSurface, c_szFlip, hr);
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Flip to GDI surface again
        hr = pDD->FlipToGDISurface();
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDraw,
                c_szFlipToGDISurface,
                hr,
                NULL,
                TEXT("after flipping to the back buffer"));
            pDDS->Release();
            return TPRFromITPR(nITPR |= ITPR_FAIL);
        }

        // You would THINK that you flipped back to the GDI
        // surface, and you have, but pDDS has NOT been updated
        // to point to the GDI surface.  Using FlipToGDISurface
        // will update which surface GDI operations write to,
        // but does NOT keep pDDS in sync.  See HPURAID 5782.
        // TODO: Make a more through testing of this.
  #if 0
        ddsd.dwSize = sizeof(ddsd);
        ddsd.lpSurface = (void *)PRESET;
        hr = pDDS->Lock(
            NULL,
            &ddsd,
            DDLOCK_WAITNOTBUSY | DDLOCK_SURFACEMEMORYPTR,
            NULL);
        if(!CheckReturnedPointer(
            CRP_NOTINTERFACE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
            ddsd.lpSurface,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            NULL,
            TEXT("for primary after flipping to back buffer and back")))
        {
            pDDS->Release();
            return TPRFromITPR(nITPR | ITPR_ABORT);
        }
        dwData = *(DWORD *)ddsd.lpSurface;
        hr = pDDS->Unlock(ddsd.lpSurface);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szUnlock,
                hr,
                NULL,
                TEXT("for primary after flipping to back buffer and back"));
            nITPR |= ITPR_ABORT;
        }

        // Are we pointing back to the GDI surface?
        if(dwData != SURFACEDATA_GDI)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER
#ifdef KATANA
                TEXT("(Keep #2829) ")
#else // not KATANA
                TEXT("(HPURAID #5782) ")
#endif // KATANA
                TEXT("%s::%s for the primary")
                TEXT(" returned a different surface than expected after")
                TEXT(" flipping to back buffer and back"),
                c_szIDirectDrawSurface,
                c_szLock);
            nITPR |= ITPR_FAIL;
        }
#endif // 0
    }

    pDDS->Release();

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szFlipToGDISurface);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_GetCaps
//  Tests the IDirectDraw::GetCaps method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_GetCaps(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT hr = S_OK;
    DDCAPS  ddcHAL1,
            ddcHAL2;
    bool    bHALSuccess;
    ITPR    nITPR = ITPR_PASS;

    IDirectDraw *pDD;

    TCHAR szDriverName[256];
    HKEY hKey;
    ULONG nDriverName = countof(szDriverName);
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                        TEXT("System\\GDI\\Drivers"),
                        0, // Reseved. Must == 0.
                        0, // Must == 0.
                        &hKey) == ERROR_SUCCESS)
    {
       if (RegQueryValueEx(hKey,
                            TEXT("MainDisplay"),
                            NULL,  // Reserved. Must == NULL.
                            NULL,  // Do not need type info.
                            (BYTE *)szDriverName,
                            &nDriverName) == ERROR_SUCCESS)
        {
            Debug(LOG_COMMENT,TEXT("Video driver in use is %s"), szDriverName);
        }

            RegCloseKey(hKey);
    }

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

#if QAHOME_INVALIDPARAMS
    // Both parameters NULL
    INVALID_CALL(
        pDD->GetCaps(NULL, NULL),
        c_szIDirectDraw,
        c_szGetCaps,
        TEXT("NULL, NULL"));
#endif // QAHOME_INVALIDPARAMS

    // HAL only
    memset(&ddcHAL1, 0, sizeof(ddcHAL1));
    ddcHAL1.dwSize = sizeof(ddcHAL1);
    hr = pDD->GetCaps(&ddcHAL1, NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDraw,
            c_szGetCaps,
            hr,
            TEXT("non-NULL, NULL"));
        nITPR |= ITPR_FAIL;
        bHALSuccess = false;
    }
    else
        bHALSuccess = true;

    // Both
    memset(&ddcHAL2, 0, sizeof(ddcHAL2));
    ddcHAL2.dwSize = sizeof(ddcHAL2);
    hr = pDD->GetCaps(&ddcHAL2, NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDraw,
            c_szGetCaps,
            hr,
            TEXT("non-NULL, non-NULL"));
        nITPR |= ITPR_FAIL;
    }
    else
    {
        if(bHALSuccess)
        {
            if(memcmp(&ddcHAL1, &ddcHAL2, sizeof(DDCAPS)))
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%s::%s returned different HAL caps")
                    TEXT(" when  the second parameter was non-NULL from when")
                    TEXT(" it was NULL"),
                    c_szIDirectDraw,
                    c_szGetCaps);
                nITPR |= ITPR_FAIL;
            }
            else
            {
                DebugBeginLevel(0, TEXT("HAL Caps:"));
                nITPR |= Help_DumpDDCaps(ddcHAL1);
                DebugEndLevel(TEXT(""));
            }
        }
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szGetCaps);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_GetDisplayMode
//  Tests the IDirectDraw::GetDisplayMode method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_GetDisplayMode(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT     hr = S_OK;
    ITPR        nITPR = ITPR_PASS;
    IDirectDraw *pDD;
    int         nIndex;

    DDSURFACEDESC   ddsd;

    // The array of structures below contains the list of valid display modes.
    // The test will set the display mode to each of those, and then get it
    // back to make sure that the returned value matches the mode we set. To
    // add or remove modes, just edit the array.
    static struct
    {
        DWORD   m_dwWidth,
                m_dwHeight,
                m_dwDepth,
                m_dwFrequency,
                m_fSuperVGA;
        LPCTSTR m_pszName;
    } s_rgModes[] = {
#ifdef KATANA
        { 640, 240, 16, 60, FALSE, TEXT("for 640x240x16") },
        { 320, 240, 16, 60, TRUE,  TEXT("for 320x240x16") }
#else // KATANA
        { 640, 480, 16, 60, TRUE,  TEXT("for 640x480x16") }
#endif // KATANA
    };

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

#if QAHOME_INVALIDPARAMS
    // NULL pointer
#ifdef KATANA
#define BUGID   0
#else // KATANA
#define BUGID   1546
#endif // KATANA
    INVALID_CALL_RAID(
        pDD->GetDisplayMode(NULL),
        c_szIDirectDraw,
        c_szGetDisplayMode,
        TEXT("NULL"),
        RAID_HPURAID,
        BUGID);
#undef BUGID
#endif // QAHOME_INVALIDPARAMS

    // Set the display mode to each valid mode and get it back
    for(nIndex = 0; nIndex < countof(s_rgModes); nIndex++)
    {
        // Set the display mode
        hr = pDD->SetDisplayMode(
            s_rgModes[nIndex].m_dwWidth,
            s_rgModes[nIndex].m_dwHeight,
            s_rgModes[nIndex].m_dwDepth,
            s_rgModes[nIndex].m_dwFrequency,
            s_rgModes[nIndex].m_fSuperVGA);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE | RESULT_COMMENTFAILURES,
                c_szIDirectDraw,
                c_szSetDisplayMode,
                hr,
                s_rgModes[nIndex].m_pszName + 4);
        }
        else
        {
            // Get the display mode
            ddsd.dwSize = sizeof(ddsd);
            hr = pDD->GetDisplayMode(&ddsd);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDraw,
                    c_szGetDisplayMode,
                    hr,
                    NULL,
                    s_rgModes[nIndex].m_pszName);
                nITPR |= ITPR_FAIL;
            }
            else if(ddsd.dwWidth != s_rgModes[nIndex].m_dwWidth ||
                    ddsd.dwHeight != s_rgModes[nIndex].m_dwHeight ||
                    ddsd.ddpfPixelFormat.dwRGBBitCount !=
                        s_rgModes[nIndex].m_dwDepth ||
                    ddsd.dwRefreshRate != s_rgModes[nIndex].m_dwFrequency)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER
#ifndef KATANA
                    TEXT("(HPURAID #1054) ")
#endif // KATANA
                    TEXT("%s::%s was expected to return %s")
                    TEXT(" (returned %dx%dx%d at %d Hz)"),
                    c_szIDirectDraw,
                    c_szGetDisplayMode,
                    s_rgModes[nIndex].m_pszName + 4,
                    ddsd.dwWidth,
                    ddsd.dwHeight,
                    ddsd.ddpfPixelFormat.dwRGBBitCount,
                    ddsd.dwRefreshRate);
                nITPR |= ITPR_FAIL;
            }
        }
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szGetDisplayMode);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// GetSupportedFourCCCodes
// Get the supported FourCC codes from the config file
//
// Return value: S_OK to continue enumerating, S_FAIL to stop.

HRESULT GetSupportedFourCCCodes()
{
    HRESULT hr = S_OK;
    BSTR FourCCCodeQueryTerm;
    VARIANT variant;   
    TCHAR szRetCode[MAX_PATH];

    CComPtr<IXMLDOMNodeList> pDOMNodeList, pDOMSubNodeList;
    CComPtr<IXMLDOMNode>     pTempNode, pFourccNode;
    CComPtr<IXMLDOMNode>     cpXMLRootNode;
    CXMLParser* xmlParser = new CXMLParser(NULL);
    
    static const TCHAR *szCodeNode = TEXT("//SupportedFourCCCodes/Code");
    static const TCHAR *szNodeList[] = {TEXT("//Code/code1"),
        TEXT("//Code/code2"),
        TEXT("//Code/code3"),
        TEXT("//Code/code4")};  

    hr = xmlParser->LoadXMLFile( TEXT("\\Release\\SupportedFourCCCodes.xml") );
    if (FAILED(hr) || hr == S_FALSE)
    {      
        Debug(LOG_DETAIL,
             TEXT(" Failed to load the xml file \\Release\\SupportedFourCCCodes.xml \n"));
        return hr;
    }   

    hr = xmlParser->GetCurrentNode( &cpXMLRootNode );
    if ((FAILED(hr)) ||  cpXMLRootNode == NULL)
    {   
        Debug(LOG_DETAIL,
             TEXT(" Failed to get the Root node from xml file\n"));
        return hr;
    }

    BSTR searchTag = SysAllocString(szCodeNode);
    // Find all nodes which match the search string (in this case, Codec)
    hr = cpXMLRootNode->selectNodes(searchTag, &pDOMNodeList );
    if ((FAILED(hr)) || pDOMNodeList == NULL)
    {
        Debug(LOG_DETAIL,
             TEXT(" Failed to get the node list from xml file \n"));
       return hr;
    }
    
    //Get the number of supported FourCC codes from xml
    pDOMNodeList->get_length( &g_llistLength );  

    int nIndex, nCodeIndex;
    for(nIndex = 0; nIndex < g_llistLength; nIndex++)
    {       
        char szCode[MAX_FOURCC_CHAR];
        pDOMNodeList->get_item( (long) nIndex, &pFourccNode );

        //Get the FourCC codes and pass it to array
        for(nCodeIndex = 0; nCodeIndex < MAX_FOURCC_CHAR; nCodeIndex++)
        {
            FourCCCodeQueryTerm = SysAllocString(szNodeList[nCodeIndex]);  // create BSTR for the search query on Code node
            pFourccNode->selectNodes(FourCCCodeQueryTerm, &pDOMSubNodeList); 
            pDOMSubNodeList->get_item( (long) nIndex, &pTempNode );        
            xmlParser->GetNodeValue( pTempNode, &variant);  // copy field value into variant    
            _tcscpy_s(szRetCode, MAX_PATH, OLE2T(variant.bstrVal));
            szCode[nCodeIndex] = (CHAR)*szRetCode; 

            // free the BSTR so that it can be re-alloc'ed later
            SysFreeString( FourCCCodeQueryTerm );
            
            // Release TempNode so that it can be re-used
            pTempNode.Release();
            pDOMSubNodeList.Release();
        }
        pFourccNode.Release();

        //Pass the FourCC codes to structure    
        if(g_pFourCCCode != NULL)
        {
            FourCCCodeList* p , *pTemp;
            for( p = g_pFourCCCode; p->pNext != NULL; p = p->pNext);
            pTemp = new FourCCCodeList();
            if(!pTemp)
            {
                hr = E_FAIL;
                return hr;
            }
            pTemp->m_dwFourCCCode = MAKEFOURCC(szCode[0],szCode[1],szCode[2],szCode[3]);          
            pTemp-> m_nSeen = 0;
            pTemp->pNext = NULL;
            p->pNext = pTemp;
        }
        else
        {
            g_pFourCCCode = new FourCCCodeList();   
            if(!g_pFourCCCode)
            {
                hr = E_FAIL;
                return hr;
            }
            g_pFourCCCode->m_dwFourCCCode = MAKEFOURCC(szCode[0],szCode[1],szCode[2],szCode[3]);
            g_pFourCCCode->m_nSeen = 0;
            g_pFourCCCode->pNext = NULL;
        }
    }

    // free the BSTR so that it can be re-alloc'ed later
    if (searchTag != NULL)
    {
        SysFreeString( searchTag );
    }
    if (cpXMLRootNode)
    {
        cpXMLRootNode.Release();
    }
    if (pDOMNodeList)
    {
        pDOMNodeList.Release();
    }
    if (xmlParser != NULL)
    {
        // destructore for xmlParser deinitializes COM
        delete xmlParser;
    }
    return hr;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_GetFourCCCodes
//  Tests the IDirectDraw::GetFourCCCodes method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_GetFourCCCodes(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT hr = S_OK;
    DWORD   dwNumCodes,
            dwQueriedNumCodes = 0,
            dwCodes[512];
    UINT    nIndex;
    ITPR    nITPR = ITPR_PASS;
    bool    bQueried;

    IDirectDraw *pDD;   

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

    g_pFourCCCode = NULL;
    hr = GetSupportedFourCCCodes(); 
    if(FAILED(hr) || g_pFourCCCode == NULL)
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDraw,
            c_szGetFourCCCodes,
            hr,
            TEXT("NULL"));
        nITPR |= ITPR_FAIL;
        return TPRFromITPR(nITPR);
    }

#if QAHOME_INVALIDPARAMS
    // NULL first parameter
    INVALID_CALL(
        pDD->GetFourCCCodes(NULL, NULL),
        c_szIDirectDraw,
        c_szGetFourCCCodes,
        TEXT("NULL first parameter"));
#endif // QAHOME_INVALIDPARAMS

    // NULL array pointer test
    dwNumCodes = countof(dwCodes);
    hr = pDD->GetFourCCCodes(&dwNumCodes, NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDraw,
            c_szGetFourCCCodes,
            hr,
            TEXT("NULL"));
        nITPR |= ITPR_FAIL;
        bQueried = false;
    }
    else
    {
        bQueried = true;
        dwQueriedNumCodes = dwNumCodes;
        Debug(
            LOG_DETAIL,
            TEXT("%s::%s reports there %s %d entr%s in the FourCC codes array"),
            c_szIDirectDraw,
            c_szGetFourCCCodes,
            dwQueriedNumCodes == 1 ? TEXT("is") : TEXT("are"),
            dwQueriedNumCodes,
            dwQueriedNumCodes == 1 ? TEXT("y") : TEXT("ies"));

        // Make sure we don't have overflow
        for(nIndex = 0; nIndex < countof(dwCodes); nIndex++)
        {
            dwCodes[nIndex] = (nIndex + 1)*(nIndex + 2);
        }

        if(dwQueriedNumCodes > 1)
        {
            // Array large enough to test
            dwNumCodes = 1;

            __try
            {
                hr = pDD->GetFourCCCodes(&dwNumCodes, dwCodes);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDraw,
                        c_szGetFourCCCodes,
                        hr,
                        NULL,
                        TEXT("with small array"));
                    nITPR |= ITPR_FAIL;
                }
                else
                {
                    // Was anything at all written to the array?
                    if(dwCodes[0] == 2)
                    {
                        Debug(
                            LOG_FAIL,
                            FAILURE_HEADER TEXT("%s::%s did not fill the array"),
                            c_szIDirectDraw,
                            c_szGetFourCCCodes);
                        nITPR |= ITPR_FAIL;
                    }

                    // Make sure the array was not overflown
                    for(nIndex = 1; nIndex < countof(dwCodes); nIndex++)
                    {
                        if(dwCodes[nIndex] != (DWORD)((nIndex + 1)*(nIndex + 2)))
                        {
                            Debug(
                                LOG_FAIL,
                                FAILURE_HEADER TEXT("%s::%s overflowed the array")
                                TEXT(" when the array we passed in was too small"),
                                c_szIDirectDraw,
                                c_szGetFourCCCodes);
                            nITPR |= ITPR_FAIL;
                            break;
                        }
                    }

                    // Make sure we got the right number returned in dwNumCodes
                    /*
                    // According to the resolution of bug #6140, this behavior
                    // is by design. The fix was in the docs, which should
                    // now state that the first parameter to GetFourCCCodes
                    // will return the number of codes copied.

                    if(dwNumCodes != dwQueriedNumCodes)
                    {
                        Debug(
                            LOG_FAIL,
                            FAILURE_HEADER
#ifdef KATANA
                            TEXT("(Keep #6140) ")
#endif // KATANA
                            TEXt("%s::%s was")
                            TEXT(" expected to return a code count of %d")
                            TEXT(" when the array we passed in was too small")
                            TEXT(" (returned %d)"),
                            c_szIDirectDraw,
                            c_szGetFourCCCodes,
                            dwQueriedNumCodes,
                            dwNumCodes);
                        nITPR |= ITPR_FAIL;
                    }
                    */
                    if(dwNumCodes != 1)
                    {
                        Debug(
                            LOG_FAIL,
                            FAILURE_HEADER TEXT("%s::%s was expected to")
                            TEXT(" return a code count of 1 when the array")
                            TEXT(" we passed in was too small (returned %d)"),
                            c_szIDirectDraw,
                            c_szGetFourCCCodes,
                            dwNumCodes);
                        nITPR |= ITPR_FAIL;
                    }
                }
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                Debug(
                    LOG_EXCEPTION,
                    FAILURE_HEADER
                    TEXT("%s::%s threw an")
                    TEXT(" exception when the array passed in was too small"),
                    c_szIDirectDraw,
                    c_szGetFourCCCodes);
                nITPR |= ITPR_FAIL;
            }
        }
    }

    // Valid size array test
    __try
    {
        dwNumCodes = countof(dwCodes);
        hr = pDD->GetFourCCCodes(&dwNumCodes, dwCodes);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDraw,
                c_szGetFourCCCodes,
                hr,
                TEXT("with big array"));
            nITPR |= ITPR_FAIL;
        }
        else
        {
            // If we successfully determined how many FourCC codes there are, we
            // should make sure that the same value was returned this time
            if(bQueried)
            {
                if(dwNumCodes != dwQueriedNumCodes)
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER TEXT("%s::%s was expected to return a")
                        TEXT(" code count of %d (returned %d)"),
                        c_szIDirectDraw,
                        c_szGetFourCCCodes,
                        dwQueriedNumCodes,
                        dwNumCodes);
                    nITPR |= ITPR_FAIL;
                }
            }

            // Make sure the array was not overflown
            for(nIndex = dwNumCodes; nIndex < countof(dwCodes); nIndex++)
            {
                if(dwCodes[nIndex] != (DWORD)((nIndex + 1)*(nIndex + 2)))
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER TEXT("%s::%s overflowed the array"),
                        c_szIDirectDraw,
                        c_szGetFourCCCodes);
                    nITPR |= ITPR_FAIL;
                    break;
                }
            }

            FourCCCodeList* pTempFourCCCode;

            // Read off the codes and make sure that we see each code we expect
            // to see. Make sure we see each one of those exactly once. First
            // we must clear the "seen" flag for the known codes

            for( pTempFourCCCode = g_pFourCCCode; pTempFourCCCode->pNext != NULL; pTempFourCCCode = pTempFourCCCode->pNext)
            {
                pTempFourCCCode->m_nSeen = 0;
            }

            // Then, iterate through the returned codes and match with the
            // known ones
            for(nIndex = 0; nIndex < dwNumCodes; nIndex++)
            {
                int nIndex2;        
                for( pTempFourCCCode = g_pFourCCCode, nIndex2=0; pTempFourCCCode->pNext != NULL; pTempFourCCCode = pTempFourCCCode->pNext, nIndex2=nIndex2+1)                   
                {
                    // Is this the code?
                    if(dwCodes[nIndex] == pTempFourCCCode->m_dwFourCCCode)
                    {                
                        // Yes. Have we seen it before? Only log the bug the
                        // first time we see this particular code repeated
                        if(pTempFourCCCode->m_nSeen++ == 1)
                        {
                            Debug(
                                LOG_FAIL,
                                FAILURE_HEADER TEXT("%s::%s returned the")
                                TEXT(" FourCC code \"%c%c%c%c\" multiple")
                                TEXT(" times"),
                                c_szIDirectDraw,
                                c_szGetFourCCCodes,
                                dwCodes[nIndex] & 0XFF,
                                (dwCodes[nIndex] >> 8) & 0XFF,
                                (dwCodes[nIndex] >> 16) & 0XFF,
                                (dwCodes[nIndex] >> 24) & 0XFF);
                            nITPR |= ITPR_FAIL;
                        }
                        break;
                    }
                }

                // Was this code unknown?
                if(nIndex2 == (g_llistLength-1))
                {
                    // The code was not found in the "known codes" table
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER TEXT("%s::%s returned the unrecognized")
                        TEXT(" FourCC code \"%c%c%c%c\""),
                        c_szIDirectDraw,
                        c_szGetFourCCCodes,
                        dwCodes[nIndex] & 0XFF,
                        (dwCodes[nIndex] >> 8) & 0XFF,
                        (dwCodes[nIndex] >> 16) & 0XFF,
                        (dwCodes[nIndex] >> 24) & 0XFF);
                    nITPR |= ITPR_FAIL;
                }
            }

#if 0
            // This technique is unmaintainable.  Just print the codes that we found
            // Finally, make sure that all known codes were listed

            for( pTempFourCCCode = g_pFourCCCode; pTempFourCCCode->pNext != NULL; pTempFourCCCode = p->pNext)           
            {
                if(!p->m_nSeen)
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER TEXT("%s::%s did not enumerate the")
                        TEXT(" FourCC code \"%c%c%c%c\""),
                        c_szIDirectDraw,
                        c_szGetFourCCCodes,
                        p->m_dwFourCCCode & 0XFF,
                        (p->m_dwFourCCCode >> 8) & 0XFF,
                        (p->m_dwFourCCCode >> 16) & 0XFF,
                        (p->m_dwFourCCCode >> 24) & 0XFF);
                    nITPR |= ITPR_FAIL;
                }
            }
#else
            Debug(LOG_DETAIL, TEXT("Returned Codes:"));
            for(nIndex = 0; nIndex < dwNumCodes; nIndex++)
            {
                Debug(
                    LOG_DETAIL,
                    TEXT("    \"%c%c%c%c\""),
                    dwCodes[nIndex] & 0XFF,
                    (dwCodes[nIndex] >> 8) & 0XFF,
                    (dwCodes[nIndex] >> 16) & 0XFF,
                    (dwCodes[nIndex] >> 24) & 0XFF);
            }
#endif
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        Debug(
            LOG_EXCEPTION,
            FAILURE_HEADER
            TEXT("%s::%s threw an exception")
            TEXT(" when the array passed in was large enough"));
        nITPR |= ITPR_FAIL;
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szGetFourCCCodes);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_GetGDISurface
//  Tests the IDirectDraw::GetGDISurface method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_GetGDISurface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw *pDD;
    HRESULT     hr = S_OK;
    DWORD       dwGDIData = 0;
    ITPR        nITPR = ITPR_PASS;

    IDirectDrawSurface  *pDDSPrimary = NULL,
                        *pDDSOffScreen = NULL,
                        *pDDSGDI;
    DDSURFACEDESC       ddsd;
    DDCAPS ddcHAL;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

    memset(&ddcHAL, 0, sizeof(ddcHAL));
    ddcHAL.dwSize = sizeof(ddcHAL);
    pDD->GetCaps(&ddcHAL, NULL);

#if NEED_INITDDS
    // Avoid interference with IDirectDrawSurface tests
    FinishDirectDrawSurface();
#endif // NEED_INITDDS

#if QAHOME_INVALIDPARAMS
    // NULL parameter
    INVALID_CALL(
        pDD->GetGDISurface(NULL),
        c_szIDirectDraw,
        c_szGetGDISurface,
        TEXT("NULL"));
#endif // QAHOME_INVALIDPARAMS

    // Call the method when there are no surfaces
    pDDSGDI = (IDirectDrawSurface *)PRESET;
    __try
    {
        hr = pDD->GetGDISurface(&pDDSGDI);
        if(!CheckReturnedPointer(
            CRP_RELEASE | CRP_ALLOWNONULLOUT,
            pDDSGDI,
            c_szIDirectDraw,
            c_szGetGDISurface,
            hr,
            NULL,
            TEXT("with no existing surfaces"),
            DDERR_NOTFOUND
#ifndef KATANA
            ,
            RAID_HPURAID,
            1052
#endif // KATANA
            ))
        {
            nITPR |= ITPR_FAIL;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        Report(
            RESULT_EXCEPTION,
            c_szIDirectDraw,
            c_szGetGDISurface,
            hr,
            NULL,
            TEXT("with no existing surfaces"),
            DDERR_NOTFOUND
#ifndef KATANA
            ,
            RAID_HPURAID,
            1052
#endif // KATANA
            );
        nITPR |= ITPR_FAIL;
    }
    pDDSGDI = NULL;

    // Create an off-screen surface (obviously not the GDI surface)
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize         = sizeof(ddsd);
    ddsd.dwFlags        = DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.dwWidth        = 100;
    ddsd.dwHeight       = 100;
    pDDSOffScreen = (IDirectDrawSurface *)PRESET;
    hr = pDD->CreateSurface(&ddsd, &pDDSOffScreen, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        pDDSOffScreen,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        NULL,
        TEXT("for off-screen surface")))
    {
        pDDSOffScreen = NULL;
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Call the method when there is only an off-screen surface
        pDDSGDI = (IDirectDrawSurface *)PRESET;
        hr = pDD->GetGDISurface(&pDDSGDI);
        if(!CheckReturnedPointer(
            CRP_RELEASE | CRP_ALLOWNONULLOUT,
            pDDSGDI,
            c_szIDirectDraw,
            c_szGetGDISurface,
            hr,
            NULL,
            TEXT("with only an off-screen surface present"),
            DDERR_NOTFOUND
#ifndef KATANA
            ,
            RAID_HPURAID,
            1052
#endif // KATANA
            ))
        {
            nITPR |= ITPR_FAIL;
        }
    }

    // Create a primary and back buffer
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize             = sizeof(ddsd);
    ddsd.dwFlags            = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps     = DDSCAPS_PRIMARYSURFACE |
                              DDSCAPS_FLIP;
    ddsd.dwBackBufferCount  = 1;
    pDDSPrimary = (IDirectDrawSurface *)PRESET;

    if(!(ddcHAL.ddsCaps.dwCaps & DDSCAPS_FLIP))
    {
        Debug(LOG_COMMENT, TEXT("Flipping unsupported."));
        ddsd.dwFlags &= ~DDSD_BACKBUFFERCOUNT;
        ddsd.ddsCaps.dwCaps &= ~DDSCAPS_FLIP;
        ddsd.dwBackBufferCount  = 0;
    }

    hr = pDD->CreateSurface(&ddsd, &pDDSPrimary, NULL);

    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        pDDSPrimary,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        TEXT("primary")))
    {
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Write data to the primary
        ddsd.dwSize = sizeof(ddsd);
        ddsd.lpSurface = (void *)PRESET;
        hr = pDDSPrimary->Lock(NULL, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
        if(!CheckReturnedPointer(
            CRP_NOTINTERFACE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
            ddsd.lpSurface,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            NULL,
            TEXT("for primary")))
        {
            nITPR |= ITPR_ABORT;
        }
        else
        {
            *(DWORD *)ddsd.lpSurface = SURFACEDATA_GDI;
            hr = pDDSPrimary->Unlock(NULL);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawSurface,
                    c_szUnlock,
                    hr,
                    NULL,
                    TEXT("for primary"));
                nITPR |= ITPR_ABORT;
            }
        }

        // Flip and write data to the back buffer
        hr = pDDSPrimary->Flip(NULL, DDFLIP_WAITNOTBUSY);
        if(DDERR_NOTFLIPPABLE == hr && !(ddsd.ddsCaps.dwCaps & DDSCAPS_FLIP))
        {
            Debug(LOG_COMMENT, TEXT("Flipping unsupported, skipping a portion of the test."));
        }
        else if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szFlip,
                hr,
                NULL,
                TEXT("before call to GetGDISurface"));
            nITPR |= ITPR_ABORT;
        }
        else
        {
            ddsd.dwSize = sizeof(ddsd);
            ddsd.lpSurface = (void *)PRESET;
            hr = pDDSPrimary->Lock(NULL, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
            if(!CheckReturnedPointer(
                CRP_NOTINTERFACE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
                ddsd.lpSurface,
                c_szIDirectDrawSurface,
                c_szLock,
                hr,
                NULL,
                TEXT("for primary")))
            {
                nITPR |= ITPR_ABORT;
            }
            else
            {
                *(DWORD *)ddsd.lpSurface = SURFACEDATA_NOTGDI;
                hr = pDDSPrimary->Unlock(NULL);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_ABORT,
                        c_szIDirectDrawSurface,
                        c_szUnlock,
                        hr,
                        NULL,
                        TEXT("for primary"));
                    nITPR |= ITPR_ABORT;
                }
            }
        }

        // Get the GDI surface when there is one
        pDDSGDI = (IDirectDrawSurface *)PRESET;
        hr = pDD->GetGDISurface(&pDDSGDI);
        if(!CheckReturnedPointer(
            CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
            pDDSGDI,
            c_szIDirectDraw,
            c_szGetGDISurface,
            hr,
            NULL,
            TEXT("before flipping")))
        {
            nITPR |= ITPR_FAIL;
        }
        else
        {
            // Get the bits for the GDI surface
            hr = pDDSGDI->Lock(NULL, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
            if(!CheckReturnedPointer(
                CRP_NOTINTERFACE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
                ddsd.lpSurface,
                c_szIDirectDrawSurface,
                c_szLock,
                hr,
                NULL,
                TEXT("for GDI surface before flipping")))
            {
                nITPR |= ITPR_ABORT;
            }
            else
            {
                // Store the value in the surface
                dwGDIData = *(DWORD *)ddsd.lpSurface;

                pDDSGDI->Unlock(NULL);
            }

            pDDSGDI->Release();

            // Flip to the back buffer
            hr = pDDSPrimary->Flip(NULL, DDFLIP_WAITNOTBUSY);
            if(DDERR_NOTFLIPPABLE == hr && !(ddsd.ddsCaps.dwCaps & DDSCAPS_FLIP))
            {
                Debug(LOG_COMMENT, TEXT("Flipping unsupported, skipping a portion of the test."));
            }
            else if(FAILED(hr))
            {
                Report(RESULT_ABORT, c_szIDirectDrawSurface, c_szFlip, hr);
                nITPR |= TPR_ABORT;
            }

            // Get the GDI surface again
            pDDSGDI = (IDirectDrawSurface *)PRESET;
            hr = pDD->GetGDISurface(&pDDSGDI);
            if(!CheckReturnedPointer(
                CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
                pDDSGDI,
                c_szIDirectDraw,
                c_szGetGDISurface,
                hr,
                NULL,
                TEXT("after flipping surfaces")))
            {
                nITPR |= ITPR_FAIL;
            }
            else
            {
                // Get the bits for the GDI surface again
                hr = pDDSGDI->Lock(NULL, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
                if(!CheckReturnedPointer(
                    CRP_NOTINTERFACE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
                    ddsd.lpSurface,
                    c_szIDirectDrawSurface,
                    c_szLock,
                    hr,
                    NULL,
                    TEXT("for GDI surface before flipping")))
                {
                    nITPR |= ITPR_ABORT;
                }
                else
                {
                    // Was this the surface that we were expecting?
                    if(*(DWORD *)ddsd.lpSurface != dwGDIData)
                    {
                        if (ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE &&
                            ddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
                        {
                            Debug(
                                LOG_DETAIL,
                                TEXT("%s::%s returned the")
                                TEXT(" wrong surface after flipping (%x,%x) on sysmem primary.")
                                TEXT("  OK, not failing."),
                                c_szIDirectDraw,
                                c_szGetGDISurface,
                                *(DWORD *)ddsd.lpSurface,
                                dwGDIData);
                        }
                        else
                        {
                            Debug(
                                LOG_FAIL,
                                FAILURE_HEADER TEXT("%s::%s returned the")
                                TEXT(" wrong surface after flipping (%x,%x)"),
                                c_szIDirectDraw,
                                c_szGetGDISurface,
                                *(DWORD *)ddsd.lpSurface,
                                dwGDIData);
                            nITPR |= ITPR_FAIL;
                        }
                    }

                    pDDSGDI->Unlock(NULL);
                }

                pDDSGDI->Release();
            }
        }

        // Release the primary
        pDDSPrimary->Release();
    }

    if(pDDSOffScreen)
    {
        // Release the off-screen surface
        pDDSOffScreen->Release();
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szGetGDISurface);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_GetMonitorFrequency
//  Tests the IDirectDraw::GetMonitorFrequency method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_GetMonitorFrequency(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw *pDD;
    HRESULT     hr = S_OK;
    DWORD       dwFrequency;
    ITPR        nITPR = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

#if QAHOME_INVALIDPARAMS
    // NULL parameter
    INVALID_CALL(
        pDD->GetMonitorFrequency(NULL),
        c_szIDirectDraw,
        c_szGetMonitorFrequency,
        TEXT("NULL"));
#endif // QAHOME_INVALIDPARAMS

    dwFrequency = (DWORD)PRESET;
    hr = pDD->GetMonitorFrequency(&dwFrequency);
    if(!CheckReturnedPointer(
        CRP_NOTPOINTER | CRP_ALLOWNONULLOUT,
        dwFrequency,
        c_szIDirectDraw,
        c_szGetMonitorFrequency,
        hr,
        NULL,
        TEXT("refresh rate")))
    {
        nITPR |= ITPR_FAIL;
    }
    else
    {
        Debug(
            LOG_DETAIL,
            TEXT("%s::%s reports that the refresh rate is %dHz"),
            c_szIDirectDraw,
            c_szGetMonitorFrequency,
            dwFrequency);
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szGetMonitorFrequency);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_GetScanLine
//  Tests the IDirectDraw::GetScanLine method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_GetScanLine(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw *pDD;
    HRESULT     hr = S_OK;
    DWORD       dwScanLine;
    ITPR        nITPR = ITPR_PASS;
    int         nZero = 0,
                nIndex;
    DDCAPS      ddcp;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);
    PREFAST_ASSUME(pDD);

#if QAHOME_INVALIDPARAMS
#ifdef KATANA
#define BUGNUMBER   0
#else // KATANA
#define BUGNUMBER   1609
#endif // KATANA
    // NULL parameter
    INVALID_CALL_RAID(
        pDD->GetScanLine(NULL),
        c_szIDirectDraw,
        c_szGetScanLine,
        TEXT("NULL"),
        RAID_HPURAID,
        BUGNUMBER);
#undef BUGNUMBER
#endif // QAHOME_INVALIDPARAMS


    // Store caps
    memset(&ddcp, 0x0, sizeof(DDCAPS));
    ddcp.dwSize = sizeof(DDCAPS);
    if(pDD)
    {
        pDD->GetCaps(&ddcp, NULL);
    }

    // The following constant determines how many zero results in a row we
    // can tolerate before we call it a bug
#define MAXGETSCANLINEZERORESULTS   10

    // The following constant determines how many times we'll call the method,
    // hoping to catch it out of range (if such a bug is present)
#define MAXGETSCANLINEITERATIONS    100

    // The following constants determine the valid range for scan line values
#define MINSCANLINEVALUE            0
#define MAXSCANLINEVALUE            639



    // Check for hardware support
    if(ddcp.dwMiscCaps & DDMISCCAPS_READSCANLINE)
    {
        // Function supported, continue testing

        // This loop runs for MAXGETSCANLINEITERATIONS iterations. If we get
        // MAXGETSCANLINEZERORESULTS in a row at any time, we log a bug and quit
        // the loop
        for(nIndex = 0; nIndex < MAXGETSCANLINEITERATIONS; nIndex++)
        {
            dwScanLine = PRESET;
            hr = pDD->GetScanLine(&dwScanLine);

#ifdef KATANA
#define BUGID   0
#else // KATANA
#define BUGID   1614
#endif // KATANA
            if(!CheckReturnedPointer(
                CRP_NOTPOINTER | CRP_NULLNOTFAILURE | CRP_ALLOWNONULLOUT,
                dwScanLine,
                c_szIDirectDraw,
                c_szGetScanLine,
                hr,
                NULL,
                TEXT("scan line"),
                0,
                RAID_HPURAID,
                BUGID))
#undef BUGID
            {
                nITPR |= ITPR_FAIL;
                break;
            }
            else
            {
                Debug(
                    LOG_DETAIL,
                    TEXT("%s::%s returned scan line %d"),
                    c_szIDirectDraw,
                    c_szGetScanLine,
                    dwScanLine);
                if(!dwScanLine)
                {
                    if(++nZero == MAXGETSCANLINEZERORESULTS)
                    {
                        nIndex -= MAXGETSCANLINEZERORESULTS - 1;
                        Debug(
                            LOG_FAIL,
                            FAILURE_HEADER
#ifdef KATANA
                            TEXT("(Keep #5976) ")
#endif // KATANA
                            TEXT("%s::%s returned zero")
                            TEXT(" %d conscecutive times after %d iteration%s with")
                            TEXT(" non-zero returns"),
                            c_szIDirectDraw,
                            c_szGetScanLine,
                            MAXGETSCANLINEZERORESULTS,
                            nIndex,
                            ((nIndex == 1) ? TEXT("") : TEXT("s")));
                        nITPR |= ITPR_FAIL;
                        break;
                    }
                }
                else
                {
                    // This was obviously not a zero return
                    nZero = 0;

                    // Check to see if we are within range
                    if(dwScanLine < MINSCANLINEVALUE ||
                       dwScanLine > MAXSCANLINEVALUE)
                    {
                        Debug(
                            LOG_FAIL,
                            FAILURE_HEADER
#ifndef KATANA
                            TEXT("(HPURAID #1053) ")
#endif // KATANA
                            TEXT("%s::%s was expected to return")
                            TEXT(" values in the range %d-%d, inclusive")
                            TEXT(" (returned %d after %d iteration%s)"),
                            c_szIDirectDraw,
                            c_szGetScanLine,
                            MINSCANLINEVALUE,
                            MAXSCANLINEVALUE,
                            dwScanLine,
                            nIndex,
                            ((nIndex == 1) ? TEXT("") : TEXT("s")));
                        nITPR |= ITPR_FAIL;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        // Function not supported, make sure it returns that way
        Debug(LOG_COMMENT, TEXT("Reading scan line not supported by driver."));

        dwScanLine = PRESET;
        hr = pDD->GetScanLine(&dwScanLine);
        if(DDERR_UNSUPPORTED != hr)
        {
            Report(
                RESULT_UNEXPECTED,
                c_szIDirectDraw,
                c_szGetScanLine,
                hr,
                NULL,
                NULL,
                DDERR_UNSUPPORTED);
            nITPR |= ITPR_FAIL;
        }
        else
        {
            Debug(LOG_SKIP, TEXT("Skipping test."));
            nITPR |= ITPR_SKIP;
        }
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szGetScanLine);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_GetVerticalBlankStatus_WaitForVerticalBlank
//  Tests the IDirectDraw::GetVerticalBlankStatus and
//  IDirectDraw::WaitForVerticalBlank methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_GetVerticalBlankStatus_WaitForVerticalBlank(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    // Test configuration numbers
    const int MAXGETVBLANKSTATUSFALSERESULTS = 10;
    const int MAXGETVBLANKSTATUSTRUERESULTS  = 6;
    const int NUMGVBTESTS                    = 20;
    IDirectDraw *pDD;
    HRESULT     hr = S_OK;
    BOOL        bVBInterval;
    ITPR        nITPR = ITPR_PASS;
    int         nFailures,
                nIndex;

    DDCAPS ddCaps;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

#if QAHOME_INVALIDPARAMS
    INVALID_CALL_RAID(
        pDD->GetVerticalBlankStatus(NULL),
        c_szIDirectDraw,
        c_szGetVerticalBlankStatus,
        TEXT("NULL"),
        RAID_HPURAID,
        1611);
    INVALID_CALL_E(
        pDD->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, (HANDLE)PRESET),
        c_szIDirectDraw,
        TEXT("WaitForVerticalBlank"),
        TEXT("DDWAITVB_BLOCKBEGINEVENT, non-null"), DDERR_INVALIDPARAMS);
#endif // QAHOME_INVALIDPARAMS


    memset(&ddCaps, 0x00, sizeof(ddCaps));
    ddCaps.dwSize = sizeof(ddCaps);
    pDD->GetCaps(&ddCaps, NULL);

    nFailures = 0;
    for(nIndex = 0; nIndex < NUMGVBTESTS; nIndex++)
    {
        bVBInterval = PRESET;
        Sleep(30);
        // Wait for beginning of vertical blank
        hr = pDD->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, NULL);
        if (DDERR_UNSUPPORTED == hr && !(ddCaps.dwMiscCaps & DDMISCCAPS_READVBLANKSTATUS))
        {
            Debug(LOG_COMMENT, TEXT("WaitForVerticalBlank(DDWAITVP_BLOCKBEGIN) unsupported"));
            nITPR|=ITPR_SKIP;
            break;
        }
        else if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDraw,
                c_szWaitForVerticalBlank,
                hr,
                TEXT("DDWAITVB_BLOCKBEGIN"));
            nITPR |= ITPR_FAIL;
        }
        else
        {
            // Since the interval just begun, we should be in it right now
            hr = pDD->GetVerticalBlankStatus(&bVBInterval);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDraw,
                    c_szGetVerticalBlankStatus,
                    hr,
                    NULL,
                    TEXT("after DDWAITVB_BLOCKBEGIN"));
                nITPR |= ITPR_FAIL;
            }
            else if(bVBInterval == PRESET)
            {
                Report(
                    RESULT_INVALIDNOTPOINTER,
                    c_szIDirectDraw,
                    c_szGetVerticalBlankStatus,
                    0,
                    NULL,
                    TEXT("boolean value after DDWAITVB_BLOCKBEGIN"));
                nITPR |= ITPR_FAIL;
            }
            else if(!bVBInterval)
            {
                Debug(
                    LOG_FAIL,
                    TEXT("%s::%s was expected to return true")
                    TEXT(" after %s(DDWAITVB_BLOCKBEGIN) (returned false)"),
                    c_szIDirectDraw,
                    c_szGetVerticalBlankStatus,
                    c_szWaitForVerticalBlank);
                nFailures++;
            }
        }
    }

    // allow a few misses b4 reporting error
    if(MAXGETVBLANKSTATUSFALSERESULTS < nFailures && nFailures != NUMGVBTESTS/2)
    {
        nITPR |= ITPR_FAIL;
        // false was returned a number of times; log the failure
        Debug(
            LOG_FAIL,
            FAILURE_HEADER
            TEXT("(HPURAID #1612) ")
            TEXT("%s::%s returned false after")
            TEXT(" DDWAITVB_BLOCKBEGIN %d out of %d times"),
            c_szIDirectDraw,
            c_szGetVerticalBlankStatus,
            nFailures,
            NUMGVBTESTS);
    }

    nFailures = 0;
    for(nIndex = 0; nIndex < NUMGVBTESTS; nIndex++)
    {
        // Wait for end of vertical blank
        hr = pDD->WaitForVerticalBlank(DDWAITVB_BLOCKEND, NULL);
        // if we're unsupported, no point in continuing through the loop
        if (DDERR_UNSUPPORTED == hr && !(ddCaps.dwMiscCaps & DDMISCCAPS_READVBLANKSTATUS))
        {
            Debug(LOG_COMMENT, TEXT("WaitForVerticalBlank(DDWAITVP_BLOCKEND) unsupported"));
            nITPR|=ITPR_SKIP;
            break;
        }
        else if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDraw,
                c_szWaitForVerticalBlank,
                hr,
                TEXT("DDWAITVB_BLOCKEND"));
            nITPR |= ITPR_FAIL;
        }
        else
        {
            // Since the interval just ended, we should not be in it right now
            bVBInterval = PRESET;
            hr = pDD->GetVerticalBlankStatus(&bVBInterval);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDraw,
                    c_szGetVerticalBlankStatus,
                    hr,
                    NULL,
                    TEXT("after DDWAITVB_BLOCKEND"));
                nITPR |= ITPR_FAIL;
            }
            else if(bVBInterval == PRESET)
            {
                Report(
                    RESULT_INVALIDNOTPOINTER,
                    c_szIDirectDraw,
                    c_szGetVerticalBlankStatus,
                    0,
                    NULL,
                    TEXT("boolean value after DDWAITVB_BLOCKEND"));
                nITPR |= ITPR_FAIL;
            }
            else if(bVBInterval)
            {
                Debug(
                    LOG_FAIL,
                    TEXT("%s::%s was expected to return false")
                    TEXT(" after %s(DDWAITVB_BLOCKEND) (returned true)"),
                    c_szIDirectDraw,
                    c_szGetVerticalBlankStatus,
                    c_szWaitForVerticalBlank);
                nFailures++;
            }
        }
    }

    // allow a few misses b4 reporting error
    if(MAXGETVBLANKSTATUSTRUERESULTS < nFailures)
    {
        nITPR |= ITPR_FAIL;

        // true was returned a number of times; log the failure
        Debug(
            LOG_FAIL,
            FAILURE_HEADER
            TEXT("(HPURAID #2756) ")
            TEXT("%s::%s returned true after")
            TEXT(" DDWAITVB_BLOCKEND %d out of %d times"),
            c_szIDirectDraw,
            c_szGetVerticalBlankStatus,
            nFailures,
            NUMGVBTESTS);
    }

    if(nITPR == ITPR_PASS)
    {
        Report(
            RESULT_SUCCESS,
            c_szIDirectDraw,
            TEXT("GetVerticalBlankStatus and WaitForVerticalBlank"));
    }

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_Initialize
//  Tests the IDirectDraw::Initialize method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_Initialize(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    return TPR_SKIP;

/*
    IDirectDraw *pDD;
    HRESULT     hr;


    if(!InitDirectDraw(pDD))
    {
        if(g_tprReturnVal == ITPR_ABORT)
            return TPR_FAIL;
        else return TPRFromITPR(g_tprReturnVal);
    }

    hr = pDD->Initialize(NULL);
    if(hr != DDERR_ALREADYINITIALIZED)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDraw,
            c_szInitialize,
            hr,
            NULL,
            NULL,
            DDERR_ALREADYINITIALIZED);
        return TPR_FAIL;
    }

    Report(RESULT_SUCCESS, c_szIDirectDraw, c_szInitialize);

    return TPR_PASS;
*/
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_QueryInterface
//  Tests the IDirectDraw::QueryInterface method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_QueryInterface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw *pDD;
    INDEX_IID   iidValid[] = {
        INDEX_IID_IDirectDraw
    };

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

    if(!Test_QueryInterface(
        pDD,
        c_szIDirectDraw,
        iidValid,
        countof(iidValid),
        SPECIAL_IDD_QI))
    {
        return TPR_FAIL;
    }

    Report(RESULT_SUCCESS, c_szIDirectDraw, c_szQueryInterface);

    return TPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_RestoreDisplayMode
//  Tests the IDirectDraw::RestoreDisplayMode method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_RestoreDisplayMode(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw *pDD;
    HRESULT     hr;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

    // UPDATE: We have multiple display modes again, so this needs to be
    //         tested again.
    hr = pDD->RestoreDisplayMode();
    if(FAILED(hr))
    {
        Report(RESULT_FAILURE, c_szIDirectDraw, c_szRestoreDisplayMode, hr);
        return TPR_FAIL;
    }

    Report(RESULT_SUCCESS, c_szIDirectDraw, c_szRestoreDisplayMode);

    return TPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDD_SetCooperativeLevel
//  Tests the IDirectDraw::SetCooperativeLevel method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDD_SetCooperativeLevel(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw *pDD;
    HRESULT     hr = S_OK;
    DDSTATE     ddsOldState;
    ITPR        nITPR = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    // When we initialize the test, make sure that the cooperative level is
    // not set.
    ddsOldState = ResetDirectDraw(DDOnly);

    // Reinitialize
    if(!InitDirectDraw(pDD))
    {
        ResetDirectDraw(ddsOldState);
        return TPRFromITPR(g_tprReturnVal);
    }

#ifdef UNDER_CE
#if QAHOME_INVALIDPARAMS
    // Pass a NULL HWND
    INVALID_CALL(
        pDD->SetCooperativeLevel(NULL, DDSCL_FULLSCREEN),
        c_szIDirectDraw,
        c_szSetCooperativeLevel,
        TEXT("NULL HWND"));
#endif // QAHOME_INVALIDPARAMS
#endif // UNDER_CE

    // On the Katana, only full-screen exclusive mode is supported. There is
    // nothing special to test, so just call the method and verify correctness.
    hr = pDD->SetCooperativeLevel(
        g_hMainWnd,
        DDSCL_FULLSCREEN);
    if(FAILED(hr))
    {
        Report(RESULT_FAILURE, c_szIDirectDraw, c_szSetCooperativeLevel, hr);
        nITPR |= ITPR_FAIL;
    }

    // Restore the original state of the testing object
    ResetDirectDraw(ddsOldState);

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDraw, c_szSetCooperativeLevel);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Help_IDD_EnumDisplayModes_Callback
//  Helper callback for the IDirectDraw::EnumDisplayModes test function.
//
// Parameters:
//  pDDSD           Description of a surface for a display mode.
//  pContext        Context (we always pass NULL).
//
// Return value:
//  DDENUMRET_OK to continue enumerating, DDENUMRET_CANCEL to stop.

HRESULT CALLBACK Help_IDD_EnumDisplayModes_Callback(
    LPDDSURFACEDESC pDDSD,
    LPVOID          pContext)
{
    // Were we expecting to be called back at this time?
    if(!g_bDDEDMCallbackExpected)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("Unexpected EnumDisplayModes callback"));
        g_bTestPassed = false;
        return (HRESULT)DDENUMRET_CANCEL;
    }

    if(!pDDSD)
    {
        Report(
            RESULT_NULLNOTPOINTER,
            c_szIDirectDraw,
            c_szEnumDisplayModes,
            0,
            NULL,
            TEXT("DDMODEDESC"));
        g_bTestPassed = false;
        return (HRESULT)DDENUMRET_OK;
    }

    Debug(
        LOG_DETAIL,
        TEXT("Found mode %dx%dx%d"),
        pDDSD->dwWidth,
        pDDSD->dwHeight,
        pDDSD->ddpfPixelFormat.dwRGBBitCount);

    // Is the context value correct?
    if(pContext != g_pContext)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("Context mismatch in IDirectDraw::")
            TEXT("EnumDisplayModes callback (expected 0x%08x, got 0x%08x)."),
            g_pContext,
            pContext);
        g_bTestPassed = false;
    }

#ifndef KATANA
    // Does the display mode seem right?
    // TODO: Come up with a better way to make this determination
    if(pDDSD->dwWidth < 240 || pDDSD->dwWidth > 1600 ||
       pDDSD->dwHeight < 240 || pDDSD->dwHeight > 1200 ||
       (pDDSD->ddpfPixelFormat.dwRGBBitCount != 16   &&
        pDDSD->ddpfPixelFormat.dwRGBBitCount != 32))
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("(HPURAID #1051) Mode %dx%dx%d seems bogus"),
            pDDSD->dwWidth,
            pDDSD->dwHeight,
            pDDSD->ddpfPixelFormat.dwRGBBitCount);
        g_bTestPassed = false;
    }
#endif // KATANA

    // Are we done with callbacks?
    if(!--g_nCallbackCount)
    {
        g_bDDEDMCallbackExpected = false;
        return (HRESULT)DDENUMRET_CANCEL;
    }

    return (HRESULT)DDENUMRET_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Help_IDD_CreatePalette
//  Helper function to the IDirectDraw::CreatePalette test function. Creates
//  and tests one palette.
//
// Parameters:
//  pDD             Pointer to the DirectDraw object.
//  nBpp            Bits per pixel for the palette to be created.
//
// Return value:
//  ITPR_PASS if successful, ITPR_FAIL if an error occurs, or possibly another
//  special condition code.

ITPR Help_IDD_CreatePalette(
    IDirectDraw *pDD,
    int         nBpp,
    DWORD       dwHALCaps)
{
    HRESULT hr = S_OK,
            hrExpected = S_OK;
    int     index,
            nEntries = 0;
    DWORD   dwCreateFlags = 0;
    TCHAR   szConfig[32];
    ITPR    nITPR = ITPR_PASS;

    IDirectDrawPalette  *pDDP;
    PALETTEENTRY        pe[400],
                        pe2[400];

    // Define the valid and invalid color entries
#define RIGHT_R ((index + 1)%256)
#define RIGHT_G ((2*(index + 2))%256)
#define RIGHT_B ((3*(index + 3))%256)
#define WRONG_R ((index + 2)%256)
#define WRONG_G ((2*index + 3)%256)
#define WRONG_B ((3*index + 4)%256)

    // Fill out our palette entry array
    for(index = 0; index < countof(pe); index++)
    {
        pe[index].peRed     = RIGHT_R;
        pe[index].peGreen   = RIGHT_G;
        pe[index].peBlue    = RIGHT_B;
        pe[index].peFlags   = 0;
    }

    switch(nBpp)
    {
    case 8:
        dwCreateFlags = DDSCAPS_PALETTE/* | DDPCAPS_ALLOW256*/;
        nEntries = 256;
        hrExpected = S_OK;
        break;
    }

    if(!(dwHALCaps & dwCreateFlags))
    {
        Debug(LOG_COMMENT, TEXT("Palette not supported... Skipping."));
        return nITPR;
    }

    DebugBeginLevel(
        0,
        TEXT("Testing creation of %d-bpp palette..."),
        nBpp);

#if QAHOME_INVALIDPARAMS
    // Pass NULL as the second parameter
    pDDP = (IDirectDrawPalette *)PRESET;

    Debug(LOG_COMMENT, TEXT("Invalid Test: PaletteEntry NULL"));
    INVALID_CALL_POINTER(
        pDD->CreatePalette(dwCreateFlags, NULL, &pDDP, NULL),
        pDDP,
        c_szIDirectDraw,
        c_szCreatePalette,
        TEXT("NULL palette entry pointer"));

    // Pass NULL as the third parameter
    Debug(LOG_COMMENT, TEXT("Invalid Test: PalettePointer NULL"));
    INVALID_CALL(
        pDD->CreatePalette(dwCreateFlags, pe, NULL, NULL),
        c_szIDirectDraw,
        c_szCreatePalette,
        TEXT("NULL interface pointer pointer"));
#endif // QAHOME_INVALIDPARAMS

    // Create the palette
    pDDP = (IDirectDrawPalette *)PRESET;
    hr = pDD->CreatePalette(dwCreateFlags, pe, &pDDP, NULL);
    _stprintf_s(szConfig, TEXT("for %d-bit palette"), nBpp);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
        pDDP,
        c_szIDirectDraw,
        c_szCreatePalette,
        hr,
        NULL,
        szConfig,
        hrExpected,
        IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
        IFKATANAELSE(2867, 0)))
    {
        nITPR = ITPR_FAIL;
    }
    else if(SUCCEEDED(hrExpected))
    {
        // Check that the palette entry array hasn't been modified
        for(index = 0; index < countof(pe); index++)
        {
            if(pe[index].peRed != RIGHT_R ||
               pe[index].peGreen != RIGHT_G ||
               pe[index].peBlue != RIGHT_B ||
               pe[index].peFlags != 0)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%s::%s garbled the palette entry array we")
                    TEXT(" passed in"),
                    c_szIDirectDraw,
                    c_szCreatePalette);
                nITPR |= ITPR_FAIL;
            }
        }

        // Verify that the entries have been stored properly
        for(index = 0; index < countof(pe2); index++)
        {
            pe2[index].peRed    = WRONG_R;
            pe2[index].peGreen  = WRONG_G;
            pe2[index].peBlue   = WRONG_B;
            pe2[index].peFlags  = 0 + 1;
        }

        hr = pDDP->GetEntries(0, 0, nEntries, pe2);

        // We don't need the palette anymore
        pDDP->Release();

        // Did we get the palette entries?
        if(FAILED(hr))
        {
            Report(RESULT_ABORT, c_szIDirectDrawPalette, c_szGetEntries, hr);
            nITPR |= ITPR_ABORT;
        }
        else
        {
            bool fCheck;
            // Verify that we got the right data
            for(index = 0; index < nEntries; index++)
            {
                fCheck = true;
                if(pe2[index].peRed != RIGHT_R ||
                   pe2[index].peGreen != RIGHT_G ||
                   pe2[index].peBlue != RIGHT_B ||
                   pe2[index].peFlags != 0)
                {
                    if(fCheck)
                    {
                        Debug(
                           LOG_FAIL,
                           FAILURE_HEADER TEXT("Palette wasn't stored or retrieved")
                           TEXT(" properly for entry %d"),
                           index);
                        nITPR |= ITPR_FAIL;
                    }
                }
            }

            // Verify that there was no overflow
            for(index = nEntries; index < countof(pe2); index++)
            {
                if(pe2[index].peRed != WRONG_R ||
                   pe2[index].peGreen != WRONG_G ||
                   pe2[index].peBlue != WRONG_B ||
                   pe2[index].peFlags != 0 + 1)
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER TEXT("Too many palette entries were stored")
                        TEXT(" or retrieved"));
                    nITPR |= ITPR_FAIL;
                    break;
                }
            }
        }
    }

    DebugEndLevel(TEXT("Done with %d-bpp palette creation"), nBpp);

    // Delete some definitions that we no longer need
#undef RIGHT_R
#undef RIGHT_G
#undef RIGHT_B
#undef WRONG_R
#undef WRONG_G
#undef WRONG_B

    return nITPR;
}

////////////////////////////////////////////////////////////////////////////////
// Help_IDD_CreateSurface
//  Helper function to the IDirectDraw::CreateSurface test function. Creates
//  and tests one surface.
//
// Parameters:
//  pDD             Pointer to the DirectDraw object.
//  nTest           Test code.
//
// Return value:
//  ITPR_PASS if the test passes, ITPR_FAIL if the test fails, and ITPR_SKIP if
//  the test code is out of range.

ITPR Help_IDD_CreateSurface(
    IDirectDraw *pDD,
    int         nTest)
{
    HRESULT hr;
    DWORD   dwTemp;
    ITPR    nITPR = ITPR_PASS;

    IDirectDrawSurface  *pDDS;
    DDSURFACEDESC       ddsd;

    DDCAPS ddcHAL;

    memset(&ddcHAL, 0, sizeof(ddcHAL));
    ddcHAL.dwSize = sizeof(ddcHAL);

    hr = pDD->GetCaps(&ddcHAL, NULL);
    if(FAILED(hr))
    {
        Debug(LOG_FAIL,TEXT("Failed to retrieve HAL/HEL caps"));
        nITPR |= ITPR_FAIL;
    }

    if(nTest < 0 || nTest >= g_nddsd)
        return ITPR_SKIP;

    // Log a comment
    Debug(
        LOG_COMMENT,
        TEXT("Testing creation of %s"),
        g_pszSurfaceName[nTest]);

    // Create the surface
    pDDS = (IDirectDrawSurface *)PRESET;
    hr = pDD->CreateSurface(g_rgddsd + nTest, &pDDS, NULL);

    if((DDERR_NOFLIPHW == hr || DDERR_INVALIDCAPS == hr) &&
        (((LPDDSURFACEDESC) (g_rgddsd + nTest))->ddsCaps.dwCaps & DDSCAPS_FLIP) &&
        (!(ddcHAL.ddsCaps.dwCaps & DDSCAPS_FLIP)||
        !(((LPDDSURFACEDESC) (g_rgddsd + nTest))->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)))
    {
        Debug(
            LOG_COMMENT,
            TEXT("Flipping unsupported, can't test surface"));
        return ITPR_PASS;
    }
    if(FAILED(hr) &&
       ((LPDDSURFACEDESC) (g_rgddsd + nTest))->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY &&
       !(ddcHAL.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY))
    {
        Debug(
            LOG_COMMENT,
            TEXT("Driver doesn't support video memory surfaces"));
        return ITPR_PASS;
    }
    if (DDERR_OUTOFVIDEOMEMORY == hr || DDERR_OUTOFMEMORY == hr)
    {
        Debug(
            LOG_COMMENT,
            TEXT("Out of memory returned"));
        return ITPR_PASS;
    }
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
        pDDS,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr))
    {
        return ITPR_FAIL;
    }

    // Verify that the surface object was correctly created
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    hr = pDDS->GetSurfaceDesc(&ddsd);
    if(FAILED(hr))
    {
        Report(RESULT_FAILURE, c_szIDirectDrawSurface, c_szGetSurfaceDesc, hr);
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // SPECIAL BUG HANDLING: Did we create an off-screen surface in system
        // memory when we requested video memory?
        if(nTest == 4)
        {
            dwTemp = ddsd.ddsCaps.dwCaps;
            dwTemp &= (DDSCAPS_VIDEOMEMORY | DDSCAPS_SYSTEMMEMORY);
            if(dwTemp == DDSCAPS_SYSTEMMEMORY)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER
#ifdef KATANA
                    TEXT("(Keep #654) ")
#endif // KATANA
                    TEXT("%s::%s created the")
                    TEXT(" surface in system memory even though we explicitly")
                    TEXT(" requested video memory"),
                    c_szIDirectDraw,
                    c_szCreateSurface);
                nITPR |= ITPR_FAIL;

                // Hide this bug so we don't catch it again
                ddsd.ddsCaps.dwCaps = (ddsd.ddsCaps.dwCaps &
                                       ~(DDSCAPS_VIDEOMEMORY |
                                         DDSCAPS_SYSTEMMEMORY)) |
                                       DDSCAPS_VIDEOMEMORY;
            }
        }

        // SPECIAL BUG HANDLING: Did we create a Z buffer in video memory when
        // we requested system memory?
        else if(nTest == 8)
        {
            dwTemp = ddsd.ddsCaps.dwCaps;
            dwTemp &= (DDSCAPS_VIDEOMEMORY | DDSCAPS_SYSTEMMEMORY);
            if(dwTemp == DDSCAPS_VIDEOMEMORY)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER
#ifdef KATANA
                    TEXT("(Keep #654) ")
#endif // KATANA
                    TEXT("%s::%s created the")
                    TEXT(" surface in video memory even though we explicitly")
                    TEXT(" requested system memory"),
                    c_szIDirectDraw,
                    c_szCreateSurface);
                nITPR |= ITPR_FAIL;

                // Hide this bug so we don't catch it again
                ddsd.ddsCaps.dwCaps = (ddsd.ddsCaps.dwCaps &
                                       ~(DDSCAPS_VIDEOMEMORY |
                                         DDSCAPS_SYSTEMMEMORY)) |
                                       DDSCAPS_SYSTEMMEMORY;
            }
        }

        // Do the caps agree with what we passed in?
        if((ddsd.ddsCaps.dwCaps & g_rgddsd[nTest].ddsCaps.dwCaps) !=
           g_rgddsd[nTest].ddsCaps.dwCaps)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s created a surface with caps that")
                TEXT(" don't match what we asked for. We asked for 0x%08x,")
                TEXT(" but got 0x%08x"),
                c_szIDirectDraw,
                c_szCreateSurface,
                g_rgddsd[nTest].ddsCaps.dwCaps,
                ddsd.ddsCaps.dwCaps);
            nITPR |= ITPR_FAIL;
        }
    }

    // We don't need to hold on to this pointer anymore
    pDDS->Release();

    return nITPR;
}

////////////////////////////////////////////////////////////////////////////////
// Help_Setup_CreateSurface
//  Determines which types of surfaces we should be able to create. This depends
//  on the current cooperative mode, among other things.
//
// Parameters:
//  pDD            Pointer to the DirectDraw object.
//
// Return value:
//  true if successful, false otherwise.

bool Help_Setup_CreateSurface(IDirectDraw *pDD)
{
    DDSURFACEDESC   *pddsd = g_rgddsd;
    LPCTSTR         *ppszName = g_pszSurfaceName;

    // UPDATE: Instead of a static array initialization, query the DirectDraw
    //         object for capabilities.

    memset(g_rgddsd, 0, sizeof(g_rgddsd));

    // The macro below ensures that we are not on drugs. If we are about to
    // overflow the global array, this macro will throw up, which should clue us in
    // that it is time to increase MAX_SURFACE_TYPES.
#ifdef DEBUG
#define CHECK_OVERFLOW \
    do\
    {\
        if(pddsd-g_rgddsd >= MAX_SURFACE_TYPES)\
        {\
            Debug(\
                LOG_FAIL,\
                FAILURE_HEADER\
                TEXT("Help_Setup_CreateSurface was about to overflow g_rgddsd")\
                TEXT(" array. Increase the value of MAX_SURFACE_TYPES"));\
            return false;\
        }\
    } while(0)
#else // DEBUG
#define CHECK_OVERFLOW
#endif // DEBUG

    // Simple primary
    *ppszName++             = TEXT("simple primary");
    pddsd->dwSize           = sizeof(DDSURFACEDESC);
    pddsd->dwFlags          = DDSD_CAPS;
    pddsd->ddsCaps.dwCaps   = DDSCAPS_PRIMARYSURFACE;
    pddsd++;

    // Primary with backbuffer
    CHECK_OVERFLOW;
    *ppszName++             = TEXT("primary with back buffer");
    pddsd->dwSize           = sizeof(DDSURFACEDESC);
    pddsd->dwFlags          = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    pddsd->ddsCaps.dwCaps   = DDSCAPS_PRIMARYSURFACE |
                              DDSCAPS_FLIP;
    pddsd->dwBackBufferCount= 1;
    pddsd++;

    // Primary with two backbuffers
    CHECK_OVERFLOW;
    *ppszName++             = TEXT("primary with two back buffers");
    pddsd->dwSize           = sizeof(DDSURFACEDESC);
    pddsd->dwFlags          = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    pddsd->ddsCaps.dwCaps   = DDSCAPS_PRIMARYSURFACE |
                              DDSCAPS_FLIP;
    pddsd->dwBackBufferCount= 2;
    pddsd++;

    // Offscreen plain on system memory
    CHECK_OVERFLOW;
    *ppszName++             = TEXT("off-screen surface on system memory");
    pddsd->dwSize           = sizeof(DDSURFACEDESC);
    pddsd->dwFlags          = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    pddsd->ddsCaps.dwCaps   = DDSCAPS_SYSTEMMEMORY;
    pddsd->dwWidth          = 100;
    pddsd->dwHeight         = 100;
    pddsd++;

    // Offscreen plain on video memory
    CHECK_OVERFLOW;
    *ppszName++             = TEXT("off-screen surface on video memory");
    pddsd->dwSize           = sizeof(DDSURFACEDESC);
    pddsd->dwFlags          = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    pddsd->ddsCaps.dwCaps   = DDSCAPS_VIDEOMEMORY;
    pddsd->dwWidth          = 100;
    pddsd->dwHeight         = 100;
    pddsd++;

    // Primary explicitly on video memory
    CHECK_OVERFLOW;
    *ppszName++             = TEXT("primary explicitly on video memory");
    pddsd->dwSize           = sizeof(DDSURFACEDESC);
    pddsd->dwFlags          = DDSD_CAPS;
    pddsd->ddsCaps.dwCaps   = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;
    pddsd++;

    // Primary explicitly on system memory
    CHECK_OVERFLOW;
    *ppszName++             = TEXT("primary explicitly on system memory");
    pddsd->dwSize           = sizeof(DDSURFACEDESC);
    pddsd->dwFlags          = DDSD_CAPS;
    pddsd->ddsCaps.dwCaps   = DDSCAPS_PRIMARYSURFACE | DDSCAPS_SYSTEMMEMORY;
    pddsd++;

    // UPDATE: add more cases
    g_nddsd = (int)(pddsd - g_rgddsd);

    // As we don't need that macro anymore, we'll undefine it
#undef CHECK_OVERFLOW

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Help_IDD_EnumSurfaces_Callback
//  Helper callback for the IDirectDraw::EnumSurfaces test function.
//
// Parameters:
//  pDDS            Pointer to a surface object.
//  pDDSD           Description of a surface for a display mode.
//  pContext        Context (we pass a pointer to the IDirectDraw interface).
//
// Return value:
//  DDENUMRET_OK to continue enumerating, DDENUMRET_CANCEL to stop.

HRESULT CALLBACK Help_IDD_EnumSurfaces_Callback(
    IDirectDrawSurface  *pDDS,
    LPDDSURFACEDESC     pDDSD,
    LPVOID              pContext)
{
    IUnknown    *pUnknown;
    IDirectDraw *pDD;
    int         index;
    HRESULT     hr;

    // Were we expecting to be called?
    if(!g_bDDESCallbackExpected)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("Unexpected call to")
            TEXT(" Help_IDD_EnumSurfaces_Callback"));
        g_bTestPassed = false;
        return (HRESULT)DDENUMRET_CANCEL;
    }

    if(pContext != g_pContext)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s::%s passed an incorrect context value.")
            TEXT(" Expected 0x%08x, got 0x%08x"),
            c_szIDirectDraw,
            c_szEnumSurfaces,
            g_pContext,
            pContext);
        g_bTestPassed = false;
    }

    pDD = (IDirectDraw *)g_pContext;

    if(!pDDS)
    {
        Report(
            RESULT_NULLNOTPOINTER,
            c_szIDirectDraw,
            c_szEnumSurfaces,
            0,
            NULL,
            TEXT("interface pointer"));
        g_bTestPassed = false;
    }

    if(!pDDSD)
    {
        Report(
            RESULT_NULL,
            c_szIDirectDraw,
            c_szEnumSurfaces,
            0,
            NULL,
            TEXT("surface description pointer"));
        g_bTestPassed = false;
    }

    // Is this one of the surfaces that we created?
    if(pDDS)
    {
        hr = pDDS->QueryInterface(IID_IUnknown, (void **)&pUnknown);
        pDDS->Release();
        if(!CheckReturnedPointer(
            CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
            pUnknown,
            c_szIDirectDrawSurface,
            c_szQueryInterface,
            hr,
            c_szIID_IUnknown))
        {
            g_bTestPassed = false;
            return (HRESULT)DDENUMRET_OK;
        }

        // Check all created surfaces
        for(index = 0; g_ddsCreated[index].m_pUnknown; index++)
        {
            if(g_ddsCreated[index].m_pUnknown == pUnknown)
            {
                if(g_ddsCreated[index].m_bSeenBefore)
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER TEXT("%s enumerated surface pointer")
                        TEXT(" 0x%08x more than once"),
                        pUnknown);
                    g_bTestPassed = false;
                }
                else
                {
                    g_ddsCreated[index].m_bSeenBefore = true;
                }
                break;
            }
        }

        // If we didn't find this surface, it shouldn't have been enumerated
        if(!g_ddsCreated[index].m_pUnknown)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s enumerated a surface we didn't")
                TEXT(" create (0x%08x)"),
                c_szIDirectDraw,
                c_szEnumSurfaces,
                pDDS);
            g_bTestPassed = false;
        }

        pUnknown->Release();
    }

    return (HRESULT)DDENUMRET_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Help_DumpDDCaps
//  Sends a complete description of the given DDCAPS structure to Kato.
//
// Parameters:
//  ddcaps          Structure to dump.
//
// Return value:
//  ITPR_PASS if all caps are valid, ITPR_FAIL if seemingly invalid caps are
//  found.

ITPR Help_DumpDDCaps(DDCAPS &ddcaps)
{
    DWORD   dwFlags,
            dwValid,
            dwRop,
            dwBit;
    int     nIdx;
    DWORD   *pdwRops;

    ITPR    nITPR = ITPR_PASS;

    // We'll log all flags using "flag maps". These maps are constructed with
    // macros, and have the following format:
    //  BEGIN_CHECK(dwStructMember)
    //      CHECK_FLAG(DDCAPS_FLAG1)
    //      CHECK_FLAG(DDCAPS_FLAG2)
    //      ...
    //      CHECK_FLAG(DDCAPS_FLAGn)
    //  END_CHECK()
    //
    // This causes a hierarchical logging in Kato that also checks for the
    // presence of any invalid flags.

    // Before checking flags, initialize the macro-based system with the macro
    // below. The parameter is the name of the structure member that we are
    // checking.
#define BEGIN_CHECK(x)\
    dwFlags = ddcaps.x;\
    dwValid = 0;\
    DebugBeginLevel(0, TEXT(#x) TEXT(":"));\
    if(!dwFlags)\
        Debug(LOG_DETAIL, TEXT("(None)"));\
    else\
    {

    // The macro below is a general-purpose flag-checker. It checks the flags
    // in dwFlags. If the specified flag is found, it is logged to Kato.
#define CHECK_FLAG(x)\
        dwValid |= x;\
        if(dwFlags & x)\
            Debug(LOG_DETAIL, TEXT(#x));

    // This other macro checks for invalid flags. It first masks out valid
    // bits. Then, anything left is invalid. Finally, it closes out the logging
    // level in Kato.
#define END_CHECK()\
    }\
    dwFlags &= ~dwValid;\
    if(dwFlags)\
    {\
        Debug(\
            LOG_FAIL,\
            FAILURE_HEADER TEXT("Other unknown flags: 0x%08x"), dwFlags);\
        nITPR |= ITPR_FAIL;\
    }\
    DebugEndLevel(TEXT(""));

    // Before checking ROP flags, initialize the macro-based system with the
    // macro below. The parameter is the name of the ROP array structure member
    // that we are checking.
#define BEGIN_ROP_CHECK(x)\
    pdwRops = ddcaps.x;\
    dwFlags = 0;\
    DebugBeginLevel(0, TEXT(#x) TEXT(":"));

    // This is a general ROP checker.  It checks whether the bit coresponding
    // to the given raster operation code (see SDK documentation on BitBlt)
    // is set in the ROP array ititialized by BEGIN_ROP_CHECK.  If the
    // bit is found, it logs the name to Kato, and marks that we found a ROP.
#define CHECK_ROP(x)\
    dwRop = (DWORD)LOBYTE(HIWORD(x));\
    nIdx = dwRop/32;\
    dwBit = 1 << (dwRop % 32);\
    if(pdwRops[nIdx] & dwBit)\
    {\
        Debug(LOG_DETAIL, TEXT(#x));\
        dwFlags = 1;\
    }

    // Macro to end the ROP checking block.  If we haven't found any matching
    // ROP codes, logs "(None)".  Closes out the loging level in Kato.
#define END_ROP_CHECK()\
    if(!dwFlags)\
        Debug(LOG_DETAIL, TEXT("(None)"));\
    DebugEndLevel(TEXT(""));

    // Macro to check for the most common ROP codes.  There are actually 256
    // different ROP codes, but these are the most common (and the only named
    // ones).  See the BitBlt documention for more information.
#define CHECK_ROP_ARRAY(x)\
    BEGIN_ROP_CHECK(x)\
        CHECK_ROP(BLACKNESS)\
        CHECK_ROP(DSTINVERT)\
        CHECK_ROP(MERGECOPY)\
        CHECK_ROP(MERGEPAINT)\
        CHECK_ROP(NOTSRCCOPY)\
        CHECK_ROP(NOTSRCERASE)\
        CHECK_ROP(PATCOPY)\
        CHECK_ROP(PATINVERT)\
        CHECK_ROP(PATPAINT)\
        CHECK_ROP(SRCAND)\
        CHECK_ROP(SRCCOPY)\
        CHECK_ROP(SRCERASE)\
        CHECK_ROP(SRCINVERT)\
        CHECK_ROP(SRCPAINT)\
        CHECK_ROP(WHITENESS)\
    END_ROP_CHECK()

    // Other members don't contain flags, but numerical values. For those, we
    // use the macro below, which simply logs the appropriate value:
#define CHECK_VALUE(x)\
    Debug(LOG_DETAIL, TEXT(#x) TEXT(" = %d"), ddcaps.x);

    // Flags
    BEGIN_CHECK(dwPalCaps)
        CHECK_FLAG(DDPCAPS_PRIMARYSURFACE)
        CHECK_FLAG(DDPCAPS_ALPHA)
    END_CHECK()

    BEGIN_CHECK(dwBltCaps)
        CHECK_FLAG(DDBLTCAPS_READSYSMEM)
        CHECK_FLAG(DDBLTCAPS_WRITESYSMEM)
        CHECK_FLAG(DDBLTCAPS_FOURCCTORGB)
        CHECK_FLAG(DDBLTCAPS_COPYFOURCC)
        CHECK_FLAG(DDBLTCAPS_FILLFOURCC)
        CHECK_FLAG(DDBLTCAPS_ROTATION90)
    END_CHECK()

    BEGIN_CHECK(dwCKeyCaps)
        CHECK_FLAG(DDCKEYCAPS_DESTBLT)
        CHECK_FLAG(DDCKEYCAPS_DESTBLTCLRSPACE)
        CHECK_FLAG(DDCKEYCAPS_DESTBLTCLRSPACEYUV)
        CHECK_FLAG(DDCKEYCAPS_SRCBLT)
        CHECK_FLAG(DDCKEYCAPS_SRCBLTCLRSPACE)
        CHECK_FLAG(DDCKEYCAPS_SRCBLTCLRSPACEYUV)
        CHECK_FLAG(DDCKEYCAPS_BOTHBLT)
    END_CHECK()

    BEGIN_CHECK(dwAlphaCaps)
        CHECK_FLAG(DDALPHACAPS_ALPHAPIXELS)
        CHECK_FLAG(DDALPHACAPS_ALPHASURFACE)
        CHECK_FLAG(DDALPHACAPS_ALPHAPALETTE)
        CHECK_FLAG(DDALPHACAPS_ALPHACONSTANT)
        CHECK_FLAG(DDALPHACAPS_ARGBSCALE)
        CHECK_FLAG(DDALPHACAPS_SATURATE)
        CHECK_FLAG(DDALPHACAPS_PREMULT)
        CHECK_FLAG(DDALPHACAPS_NONPREMULT)
        CHECK_FLAG(DDALPHACAPS_ALPHAFILL)
        CHECK_FLAG(DDALPHACAPS_ALPHANEG)
    END_CHECK()

    BEGIN_CHECK(dwOverlayCaps)
        CHECK_FLAG(DDOVERLAYCAPS_FLIP)
        CHECK_FLAG(DDOVERLAYCAPS_AUTOFLIP)
        CHECK_FLAG(DDOVERLAYCAPS_FOURCC)
        CHECK_FLAG(DDOVERLAYCAPS_ZORDER)
        CHECK_FLAG(DDOVERLAYCAPS_MIRRORLEFTRIGHT)
        CHECK_FLAG(DDOVERLAYCAPS_MIRRORUPDOWN)
        CHECK_FLAG(DDOVERLAYCAPS_CKEYSRC)
        CHECK_FLAG(DDOVERLAYCAPS_CKEYSRCCLRSPACE)
        CHECK_FLAG(DDOVERLAYCAPS_CKEYSRCCLRSPACEYUV)
        CHECK_FLAG(DDOVERLAYCAPS_CKEYDEST)
        CHECK_FLAG(DDOVERLAYCAPS_CKEYDESTCLRSPACE)
        CHECK_FLAG(DDOVERLAYCAPS_CKEYDESTCLRSPACEYUV)
        CHECK_FLAG(DDOVERLAYCAPS_CKEYBOTH)
        CHECK_FLAG(DDOVERLAYCAPS_ALPHADEST)
        CHECK_FLAG(DDOVERLAYCAPS_ALPHASRC)
        CHECK_FLAG(DDOVERLAYCAPS_ALPHADESTNEG)
        CHECK_FLAG(DDOVERLAYCAPS_ALPHASRCNEG)
        CHECK_FLAG(DDOVERLAYCAPS_ALPHACONSTANT)
        CHECK_FLAG(DDOVERLAYCAPS_ALPHAPREMULT)
        CHECK_FLAG(DDOVERLAYCAPS_ALPHANONPREMULT)
        CHECK_FLAG(DDOVERLAYCAPS_ALPHAANDKEYDEST)
        CHECK_FLAG(DDOVERLAYCAPS_OVERLAYSUPPORT)
    END_CHECK()

    BEGIN_CHECK(dwMiscCaps)
        CHECK_FLAG(DDMISCCAPS_READSCANLINE)
        CHECK_FLAG(DDMISCCAPS_READMONITORFREQ)
        CHECK_FLAG(DDMISCCAPS_READVBLANKSTATUS)
        CHECK_FLAG(DDMISCCAPS_FLIPINTERVAL)
        CHECK_FLAG(DDMISCCAPS_FLIPODDEVEN)
        CHECK_FLAG(DDMISCCAPS_FLIPVSYNCWITHVBI)
        CHECK_FLAG(DDMISCCAPS_COLORCONTROLOVERLAY)
        CHECK_FLAG(DDMISCCAPS_COLORCONTROLPRIMARY)
        CHECK_FLAG(DDMISCCAPS_GAMMACONTROLOVERLAY)
        CHECK_FLAG(DDMISCCAPS_GAMMACONTROLPRIMARY)
        CHECK_FLAG(DDMISCCAPS_AUTOFLIPOVERLAY)
        CHECK_FLAG(DDMISCCAPS_VIDEOPORT)
        CHECK_FLAG(DDMISCCAPS_BOBINTERLEAVED)
        CHECK_FLAG(DDMISCCAPS_BOBNONINTERLEAVED)
        CHECK_FLAG(DDMISCCAPS_BOBHARDWARE)
    END_CHECK()

    CHECK_VALUE(dwVidMemTotal)
    CHECK_VALUE(dwVidMemFree)
    CHECK_VALUE(dwVidMemStride)
    CHECK_VALUE(dwMaxVisibleOverlays)
    CHECK_VALUE(dwCurrVisibleOverlays)
    CHECK_VALUE(dwNumFourCCCodes)
    CHECK_VALUE(dwAlignBoundarySrc)
    CHECK_VALUE(dwAlignSizeSrc)
    CHECK_VALUE(dwAlignBoundaryDest)
    CHECK_VALUE(dwAlignSizeDest)

    CHECK_ROP_ARRAY(dwRops)

    BEGIN_CHECK(ddsCaps.dwCaps)
        CHECK_FLAG(DDSCAPS_ALPHA)
        CHECK_FLAG(DDSCAPS_BACKBUFFER)
        CHECK_FLAG(DDSCAPS_FLIP)
        CHECK_FLAG(DDSCAPS_FRONTBUFFER)
        CHECK_FLAG(DDSCAPS_OVERLAY)
        CHECK_FLAG(DDSCAPS_PALETTE)
        CHECK_FLAG(DDSCAPS_PRIMARYSURFACE)
        CHECK_FLAG(DDSCAPS_SYSTEMMEMORY)
        CHECK_FLAG(DDSCAPS_VIDEOMEMORY)
        CHECK_FLAG(DDSCAPS_WRITEONLY)
        CHECK_FLAG(DDSCAPS_VIDEOPORT)
        CHECK_FLAG(DDSCAPS_READONLY)
        CHECK_FLAG(DDSCAPS_HARDWAREDEINTERLACE)
        CHECK_FLAG(DDSCAPS_NOTUSERLOCKABLE)
        CHECK_FLAG(DDSCAPS_DYNAMIC)
        CHECK_FLAG(DDSCAPS_OWNDC)
    END_CHECK()

    CHECK_VALUE(dwMinOverlayStretch)
    CHECK_VALUE(dwMaxOverlayStretch)

    CHECK_VALUE(dwMinVideoStretch)
    CHECK_VALUE(dwMaxVideoStretch)

    CHECK_VALUE(dwMaxVideoPorts)
    CHECK_VALUE(dwCurrVideoPorts)

    // We don't need these macros anymore
#undef BEGIN_CHECK
#undef CHECK_FLAG
#undef END_CHECK
#undef CHECK_VALUE

    return nITPR;
}
#endif // TEST_IDD

////////////////////////////////////////////////////////////////////////////////
