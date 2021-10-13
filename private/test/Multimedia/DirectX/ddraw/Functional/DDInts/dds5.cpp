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
//  Module: dds5.cpp
//          BVTs for the IDirectDrawSurface interface functions.
//
//  Revision History:
//      08/06/99    lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

#if TEST_IDDS5
#include "dds5.h"

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static IDDS            g_idds5 = { 0 };
static CFinishManager   *g_pfmDDS5 = NULL;

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS5_AlphaBlt
//  Tests the IDirectDrawSurface::AlphaBlt method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS5_AlphaBlt(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO     *pDDS5TI;
    INT             nResult = ITPR_PASS;
    HRESULT         hr =S_OK;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDS5TI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDS5TI)
    {
        nResult = Help_DDSIterate(0, Test_IDDS5_AlphaBlt, lpFTE);
        if(nResult == TPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szAlphaBlt);
    }
    else if(pDDS5TI->m_pDDS)
    {
        DDALPHABLTFX    ddabfx;

        memset (&ddabfx, 0, sizeof(DDALPHABLTFX));
        ddabfx.dwSize = sizeof(DDALPHABLTFX);

        // Perform a color fill
        ddabfx.dwFillColor = 0;
        __try
        {
            hr = pDDS5TI->m_pDDS->AlphaBlt(
                NULL,
                NULL,
                NULL,
                DDABLT_WAITNOTBUSY | DDABLT_COLORFILL,
                &ddabfx);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szAlphaBlt,
                    hr);
                nResult |= ITPR_FAIL;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            Report(
                RESULT_EXCEPTION,
                c_szIDirectDrawSurface,
                c_szAlphaBlt,
                hr);
            nResult |= ITPR_FAIL;
        }
    }

    return nResult;
}

#endif // TEST_IDDS5

////////////////////////////////////////////////////////////////////////////////
