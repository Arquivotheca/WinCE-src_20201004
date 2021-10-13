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
//  Module: ddc.cpp
//          BVT for the IDirectDrawClipper interface functions.
//
//  Revision History:
//      10/21/96    mattbron    Added tests for DirectDrawClipper
//      11/23/96    mattbron    Changed UNDER_NK to UNDER_CE for preprocessor
//                              definitions
//      12/09/96    mattbron    Added conditional support for clipper tests
//      07/23/97    lblanco     Fixed code style (hey, if I'm going to be
//                              maintaining this code, it's only fair! :)).
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#pragma warning(disable : 4995)
#if TEST_IDDC

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static IDirectDrawClipper   *g_pDDC = NULL;
static CFinishManager       *g_pfmDDC = NULL;

////////////////////////////////////////////////////////////////////////////////
// InitDirectDrawClipper
//  Creates a DirectDrawClipper object.
//
// Parameters:
//  pDDC            Receives a pointer to the interface.
//  pfnFinish       Pointer to a FinishXXX function. If this parameter is not
//                  NULL, the system understands that whenever this object is
//                  destroyed, the FinishXXX function must be called as well.
//
// Return value:
//  true if successful, false otherwise.

bool InitDirectDrawClipper(IDirectDrawClipper *&pDDC, FNFINISH pfnFinish)
{
    IDirectDraw *pDD;
    HRESULT     hr;

    // Clean out the pointer
    pDDC = NULL;

    // Did we create the clipper object?
    if(!g_pDDC)
    {
        if(!InitDirectDraw(pDD, FinishDirectDrawClipper))
            return false;

        Debug(LOG_COMMENT, TEXT("Running InitDirectDrawClipper..."));
        g_pDDC = (IDirectDrawClipper *)PRESET;
        hr = pDD->CreateClipper(0, &g_pDDC, NULL);
        if(!CheckReturnedPointer(
            CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
            g_pDDC,
            c_szIDirectDraw,
            c_szCreateClipper,
            hr))
        {
            g_pDDC = NULL;
            return false;
        }

        Debug(LOG_COMMENT, TEXT("Done with InitDirectDrawClipper"));
    }

    // Do we have a CFinishManager object?
    if(!g_pfmDDC)
    {
        g_pfmDDC = new CFinishManager;
        if(!g_pfmDDC)
        {
            FinishDirectDrawClipper();
            return false;
        }
    }

    // Add the FinishXXX function to the chain.
    g_pfmDDC->AddFunction(pfnFinish);
    pDDC = g_pDDC;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// FinishDirectDrawClipper
//  Cleans up after InitDirectDrawClipper
//
// Parameters:
//  None.
//
// Return value:
//  None.

void FinishDirectDrawClipper(void)
{
    // Terminate any other higher levels
    if(g_pfmDDC)
    {
        g_pfmDDC->Finish();
        delete g_pfmDDC;
        g_pfmDDC = NULL;
    }

    if(g_pDDC)
    {
        g_pDDC->Release();
        g_pDDC = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDC_AddRef_Release
//  Tests the IDirectDrawClipper::AddRef and IDirectDrawClipper::Release
//  methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDC_AddRef_Release(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDrawClipper  *pDDC;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawClipper(pDDC))
        return TPRFromITPR(g_tprReturnVal);

    if(!Test_AddRefRelease(pDDC, c_szIDirectDrawClipper))
        return TPR_FAIL;

    Report(RESULT_SUCCESS, c_szIDirectDrawClipper, TEXT("AddRef and Release"));

    return TPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDC_Get_SetClipList
//  Tests the IDirectDrawClipper::GetClipList and
//  IDirectDrawClipper::SetClipList methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDC_Get_SetClipList(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT hr = S_OK;
    ITPR    nITPR = ITPR_PASS;
    RECT    rectClip;
    int     nRects = 1;
    RGNDATA *pRgnData1,
            *pRgnData2;
    DWORD   dwSize;
    HRGN    hRgn1;
#ifndef UNDER_CE
    HRGN    hRgn2;
#endif // UNDER_CE
    DWORD   dwWidth,
            dwHeight;

    DDSURFACEDESC       ddsd;
    IDirectDraw         *pDD;
    IDirectDrawClipper  *pDDC;


    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

    if(!InitDirectDrawClipper(pDDC))
        return TPRFromITPR(g_tprReturnVal);

#if QAHOME_INVALIDPARAMS
#ifdef KATANA
#define BUGID   0
#else // KATANA
#define BUGID   1597
#endif // KATANA
    INVALID_CALL_RAID(
        pDDC->GetClipList(NULL, NULL, NULL),
        c_szIDirectDrawClipper,
        c_szGetClipList,
        TEXT("NULL, NULL, NULL"),
        RAID_HPURAID,
        BUGID);
#undef BUGID

#endif // QAHOME_INVALIDPARAMS

    // Clear the clip list
    hr = pDDC->SetClipList(NULL, 0);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDrawClipper,
            c_szSetClipList,
            hr,
            TEXT("NULL"),
            TEXT("the first time"));
        nITPR |= ITPR_FAIL;
    }

    // Get the clip list, which is currently empty
    hr = pDDC->GetClipList(NULL, NULL, &dwSize);
    if(hr != DDERR_NOCLIPLIST)
    {
        Report(
            RESULT_UNEXPECTED,
            c_szIDirectDrawClipper,
            c_szGetClipList,
            hr,
            NULL,
            TEXT("when there was no clip list yet"));
        nITPR |= ITPR_FAIL;
    }

    // Get the display mode to calculate random clip lists
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(DDSURFACEDESC);
    hr = pDD->GetDisplayMode(&ddsd);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDraw,
            c_szGetDisplayMode,
            hr);
        nITPR |= ITPR_ABORT;

        // Go with some canned values
        dwWidth = 200;
        dwHeight = 150;
    }
    else
    {
        dwWidth = ddsd.dwWidth;
        dwHeight = ddsd.dwHeight;
    }

    // Generate a random clip list
    // Force the upper left of our rectangle to be at least one pixel
    // from the botom left of the screen.
    rectClip.left   = rand() % (dwWidth-1);
    rectClip.top    = rand() % (dwHeight-1);
    rectClip.right  = rectClip.left + (rand()%(dwWidth - 1 - rectClip.left)) + 1;
    rectClip.bottom = rectClip.top + (rand()%(dwHeight - 1 - rectClip.top)) + 1;

    hRgn1 = CreateRectRgnIndirect(&rectClip);
    if(!CheckReturnedPointer(
        CRP_NOTPOINTER | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        hRgn1,
        TEXT("CreateRectRgnIndirect"),
        NULL,
        0))
    {
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Get size of RGNDATA
        dwSize = GetRegionData(hRgn1, 0, NULL);

        // Allocate a structure to hold the data
        BYTE*  data1 = new BYTE[dwSize];
        if(!data1)
        {
            Debug(
                LOG_ABORT,
                FAILURE_HEADER TEXT("Couldn't allocate memory for source")
                TEXT(" region data structure!"));
            nITPR |= ITPR_ABORT;
        }
        else
        {
            // Cast the data
            pRgnData1 = (RGNDATA *)(BYTE *)data1;

            // Get the region data
            if(!GetRegionData(hRgn1, dwSize, pRgnData1))
            {
                Debug(LOG_ABORT, TEXT("GetRegionData() failed"));
                nITPR |= ITPR_ABORT;
            }
            else
            {
                // Set the clip list to this region
                hr = pDDC->SetClipList(pRgnData1, 0);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDrawClipper,
                        c_szSetClipList,
                        hr);
                    nITPR |= ITPR_FAIL;
                }
                else
                {
                    // Get the clip list back
                    dwSize = PRESET;
                    hr = pDDC->GetClipList(NULL, NULL, &dwSize);
                    if(!CheckReturnedPointer(
                        CRP_NOTPOINTER | CRP_ALLOWNONULLOUT,
                        dwSize,
                        c_szIDirectDrawClipper,
                        c_szGetClipList,
                        hr,
                        TEXT("NULL, NULL, non-NULL"),
                        TEXT("size")))
                    {
                        nITPR |= ITPR_FAIL;
                    }
                    else
                    {
                        // Allocate a structure to hold the region data
                        BYTE* data2 = new BYTE[dwSize];
                        if(!data2)
                        {
                            Debug(
                                LOG_ABORT,
                                TEXT("Couldn't allocate memory for returned")
                                TEXT(" region data structure!"));
                            nITPR |= ITPR_ABORT;
                        }
                        else
                        {
                            pRgnData2 = (RGNDATA *)(BYTE *)data2;

                            hr = pDDC->GetClipList(NULL, pRgnData2, &dwSize);
                            if(FAILED(hr))
                            {
                                Report(
                                    RESULT_FAILURE,
                                    c_szIDirectDrawClipper,
                                    c_szGetClipList,
                                    hr,
                                    TEXT("non-NULL parameters"));
                                nITPR |= ITPR_FAIL;
                            }
                            delete[] data2;
                        }
                    }
                }
            }
            delete[] data1;
        }

        DeleteObject(hRgn1);
    }

    // Clear the clip list
    hr = pDDC->SetClipList(NULL, 0);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDrawClipper,
            c_szSetClipList,
            hr,
            NULL,
            TEXT("the last time"));
        nITPR |= ITPR_FAIL;
    }
    else
    {
        // Get the clip list (now empty) again
        hr = pDDC->GetClipList(NULL, NULL, &dwSize);
        if(hr != DDERR_NOCLIPLIST)
        {
            Report(
                RESULT_UNEXPECTED,
                c_szIDirectDrawClipper,
                c_szGetClipList,
                hr,
                NULL,
                TEXT("after the clip list was removed"));
            nITPR |= ITPR_FAIL;
        }
    }

    if(nITPR == ITPR_PASS)
    {
        Report(
            RESULT_SUCCESS,
            c_szIDirectDrawClipper,
            TEXT("GetClipList and SetClipList"));
    }

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDC_Get_SetHWnd
//  Tests the IDirectDrawClipper::GetHWnd and IDirectDrawClipper::SetHWnd
//  methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDC_Get_SetHWnd(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HWND    hWnd;
    HRESULT hr = S_OK;
    ITPR    nITPR = ITPR_PASS;

    IDirectDrawClipper  *pDDC;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawClipper(pDDC))
        return TPRFromITPR(g_tprReturnVal);

#if QAHOME_INVALIDPARAMS
    INVALID_CALL(
        pDDC->GetHWnd(NULL),
        c_szIDirectDrawClipper,
        c_szGetHWnd,
        TEXT("NULL"));
#endif // QAHOME_INVALIDPARAMS

/*#ifdef KATANA
#define BUGID   0
#else // KATANA
#define BUGID   1610
#endif // KATANA
    INVALID_CALL_RAID(
        pDDC->SetHWnd(0, NULL),
        c_szIDirectDrawClipper,
        c_szSetHWnd,
        TEXT("NULL HWND"),
        RAID_HPURAID,
        BUGID);
#undef BUGID*/

    // HPURAID #1610 : Set the clipping window to NULL (clip to the screen)
    hr = pDDC->SetHWnd(0, NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDrawClipper,
            c_szSetHWnd,
            hr);
        nITPR |= ITPR_FAIL;
    }
    else
    {
        // Get the window handle back
        hWnd = (HWND)PRESET;
        hr = pDDC->GetHWnd(&hWnd);
        if(!CheckReturnedPointer(
            CRP_NOTPOINTER | CRP_NULLNOTFAILURE | CRP_ALLOWNONULLOUT,
            hWnd,
            c_szIDirectDrawClipper,
            c_szGetHWnd,
            hr,
            NULL,
            TEXT("window handle")))
        {
            nITPR |= ITPR_FAIL;
        }
        else if(hWnd != NULL)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s returned window handle 0x%08X")
                TEXT(" (expected 0x%08X)"),
                c_szIDirectDrawClipper,
                c_szGetHWnd,
                hWnd,
                NULL);
            nITPR |= ITPR_FAIL;
        }
    }

    // Set the window handle to the real window
    hr = pDDC->SetHWnd(0, g_hMainWnd);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDrawClipper,
            c_szSetHWnd,
            hr);
        nITPR |= ITPR_FAIL;
    }
    else
    {
        // Get the window handle back
        hWnd = (HWND)PRESET;
        hr = pDDC->GetHWnd(&hWnd);
        if(!CheckReturnedPointer(
            CRP_NOTPOINTER | CRP_ALLOWNONULLOUT,
            hWnd,
            c_szIDirectDrawClipper,
            c_szGetHWnd,
            hr,
            NULL,
            TEXT("window handle")))
        {
            nITPR |= ITPR_FAIL;
        }
        else if(hWnd != g_hMainWnd)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s returned window handle 0x%08X")
                TEXT(" (expected 0x%08X)"),
                c_szIDirectDrawClipper,
                c_szGetHWnd,
                hWnd,
                g_hMainWnd);
            nITPR |= ITPR_FAIL;
        }
    }

    if(nITPR == ITPR_PASS)
    {
        Report(
            RESULT_SUCCESS,
            c_szIDirectDrawClipper,
            TEXT("GetHWnd and SetHWnd"));
    }

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDC_IsClipListChanged
//  Tests the IDirectDrawClipper::IsClipListChanged method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDC_IsClipListChanged(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT hr = S_OK;
    BOOL    bChanged;
    ITPR    nITPR = ITPR_PASS;

    IDirectDrawClipper  *pDDC;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawClipper(pDDC))
        return TPRFromITPR(g_tprReturnVal);

#if QAHOME_INVALIDPARAMS
    INVALID_CALL(
        pDDC->IsClipListChanged(NULL),
        c_szIDirectDrawClipper,
        c_szIsClipListChanged,
        TEXT("NULL"));
#endif // QAHOME_INVALIDPARAMS

    hr = pDDC->IsClipListChanged(&bChanged);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDrawClipper,
            c_szIsClipListChanged,
            hr);
        nITPR |= ITPR_FAIL;
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDrawClipper, c_szIsClipListChanged);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDC_QueryInterface
//  Tests the IDirectDrawClipper::QueryInterface method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDC_QueryInterface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDrawClipper  *pDDC;

    INDEX_IID   iidValid[] = {
        INDEX_IID_IDirectDrawClipper
    };

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawClipper(pDDC))
        return TPRFromITPR(g_tprReturnVal);

    if(!Test_QueryInterface(
        pDDC,
        c_szIDirectDrawClipper,
        iidValid,
        countof(iidValid)))
    {
        return TPR_FAIL;
    }

    Report(RESULT_SUCCESS, c_szIDirectDrawClipper, c_szQueryInterface);

    return TPR_PASS;
}
#endif // TEST_IDDC

////////////////////////////////////////////////////////////////////////////////
