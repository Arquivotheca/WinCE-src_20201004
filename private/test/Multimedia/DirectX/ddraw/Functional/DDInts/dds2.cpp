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
//  Module: dds2.cpp
//          BVTs for the IDirectDrawSurface interface functions.
//
//  Revision History:
//      02/06/97    mattbron    DirectDrawSurface2 Interface tests
//      02/13/97    mattbron    Modified code for PageLock()/PageUnlock()
//      02/21/97    mattbron    Added to Flip
//      06/22/97    lblanco     Fixed code style (hey, if I'm going to be
//                              maintaining this code, it's only fair! :)).
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include "dds.h"

#if TEST_IDDS2

////////////////////////////////////////////////////////////////////////////////
// Local types
#if 0
struct DDS2TESTINFO
{
    IDDS               *m_pIDDS2;
    IDirectDrawSurface *m_pDDS2;
    LPCTSTR             m_pszName;
};
#endif

INT Help_DDSIterate(
    DWORD                   dwFlags,
    DDSTESTPROC             pfnTestProc,
    LPFUNCTION_TABLE_ENTRY  pFTE,
    DWORD                   dwTestData);

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static IDDS            g_idds2 = { 0 };
static CFinishManager   *g_pfmDDS2 = NULL;

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS2_GetDDInterface
//  Tests the IDirectDrawSurface::GetDDInterface method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS2_GetDDInterface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO    *pDDS2TI;
    INT             nResult = ITPR_PASS;
    HRESULT         hr;
    IDirectDraw     *pDD;
    TCHAR           szName[64];

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDS2TI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDS2TI)
    {
        nResult = Help_DDSIterate(0, Test_IDDS2_GetDDInterface, lpFTE, 0);
        if(nResult == TPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szGetDDInterface);
    }
    else if(pDDS2TI->m_pDDS)
    {
        _stprintf_s(szName, TEXT("for %s"), pDDS2TI->m_pszName);
        pDD = (IDirectDraw *)PRESET;
        hr = pDDS2TI->m_pDDS->GetDDInterface(&pDD);

        if(!CheckReturnedPointer(
            CRP_RELEASE | CRP_ALLOWNONULLOUT,
            pDD,
            c_szIDirectDrawSurface,
            c_szGetDDInterface,
            hr,
            szName))
        {
            nResult = ITPR_FAIL;
        }
        else if(pDD != pDDS2TI->m_pDD)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s %s was expected to return")
                TEXT(" 0x%08x for the %s pointer (returned 0x%08x)"),
                c_szIDirectDrawSurface,
                c_szGetDDInterface,
                szName,
                pDDS2TI->m_pDD,
                c_szIDirectDraw,
                pDD);

            DDCAPS ddcaps1 = {0};
            DDCAPS ddcaps2 = {0};
            ddcaps1.dwSize = sizeof(DDCAPS);
            pDD->GetCaps(&ddcaps1, NULL);
            ddcaps2.dwSize = sizeof(DDCAPS);
            pDDS2TI->m_pDD->GetCaps(&ddcaps2, NULL);
            nResult = ITPR_FAIL;
        }
        else
        {
            nResult = ITPR_PASS;
        }
    }

    return nResult;
}

#endif // TEST_IDDS2

////////////////////////////////////////////////////////////////////////////////
