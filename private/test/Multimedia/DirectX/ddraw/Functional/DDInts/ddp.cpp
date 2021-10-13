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
//  Module: ddp.cpp
//          BVT for the IDirectDrawPalette interface functions.
//
//  Revision History:
//      07/07/97    lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

#if TEST_IDDP

////////////////////////////////////////////////////////////////////////////////
// Local types

typedef INT (WINAPI *DDPTESTPROC)(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
struct DDPTESTINFO
{
    IDirectDrawPalette  *m_pDDP;
    DWORD               m_dwCaps;
    LPCTSTR             m_pszName;
};

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

LPCTSTR GetDDPCaps(DWORD);
INT     Help_DDPIterate(DDPTESTPROC, LPFUNCTION_TABLE_ENTRY);
ITPR    Help_DDPGetSetEntries(
            IDirectDrawPalette *,
            DWORD,
            DWORD,
            DWORD,
            LPCTSTR,
            LPCTSTR pszAdditional = NULL);

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

DWORD   g_adwPaletteCaps[] =
{
    DDSCAPS_PALETTE
};

////////////////////////////////////////////////////////////////////////////////
// GetDDPCaps
//  Returns a string representing the given caps.
//
// Parameters:
//  dwCaps          Caps.
//
// Return value:
//  A string with the textual representation of the caps.

LPCTSTR GetDDPCaps(DWORD dwCaps)
{
    switch(dwCaps)
    {
    case DDSCAPS_PALETTE:
        return TEXT("Paletted");
    }

    // If we didn't catch it, it's unknown
    return TEXT("<unknown>");
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDP_AddRef_Release
//  Tests the IDirectDrawPalette::AddRef and IDirectDrawPalette::Release
//  methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDP_AddRef_Release(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;
    
    return TPR_SKIP;

}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDP_GetCaps
//  Tests the IDirectDrawPalette::GetCaps method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDP_GetCaps(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
   
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    return TPR_SKIP;

}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDP_Get_SetEntries
//  Tests the IDirectDrawPalette::GetEntries and IDirectDrawPalette::SetEntries
//  methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDP_Get_SetEntries(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{

#if QAHOME_INVALIDPARAMS
    HRESULT     hr = S_OK;
#endif // QAHOME_INVALIDPARAMS

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    return TPR_SKIP;

}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDP_QueryInterface
//  Tests the IDirectDrawPalette::QueryInterface method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDP_QueryInterface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    INDEX_IID   iidValid[] = {
        INDEX_IID_IDirectDrawPalette
    };

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    return TPR_SKIP;

}

#endif // TEST_IDDP
#if NEED_CREATEDDP
////////////////////////////////////////////////////////////////////////////////
// Help_CreateDDPalette
//  Creates a DirectDraw Palette object.
//
// Parameters:
//  dwCaps          Flags indicating the type of palette to create.
//  ppDDP           Pointer where the interface will be returned.
//
// Return value:
//  true if successful, false otherwise.

bool Help_CreateDDPalette(DWORD dwCaps, IDirectDrawPalette **ppDDP)
{
    IDirectDraw         *pDD;
    IDirectDrawPalette  *pDDP = (IDirectDrawPalette*) PRESET;
    HRESULT             hr;
    PALETTEENTRY        pe[256];

    // Sanity check
    if(!ppDDP)
    {
        Debug(
            LOG_ABORT,
            FAILURE_HEADER TEXT("Help_CreateDDPalette was passed NULL"));
        return false;
    }
    *ppDDP = NULL;

    if(!InitDirectDraw(pDD))
        return false;

    memset(&pe[0], 0, sizeof(pe));
    hr = pDD->CreatePalette(dwCaps, pe, &pDDP, NULL);
#ifdef KATANA
#define DB      RAID_KEEP
#define BUGID   2867
#else // KATANA
#define DB      RAID_HPURAID
#define BUGID   0
#endif // KATANA
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        pDDP,
        c_szIDirectDraw,
        c_szCreatePalette,
        hr,
        NULL,
        NULL,
        0,
        DB,
        BUGID))
#undef DB
#undef BUGID
    {
        return false;
    }

    *ppDDP = pDDP;

    return true;
}
#endif // NEED_CREATEDDP
#if TEST_IDDP

////////////////////////////////////////////////////////////////////////////////
// Help_DDPIterate
//  Iterates through all palette types and runs the specified test for each
//  type.
//
// Parameters:
//  pfnTestProc     Address of the test function. This should be the same as the
//                  test function in the test function table. If the user data
//                  parameter is not NULL, the call originated from this
//                  function. Otherwise, it originated from the Tux shell.
//
// Return value:
//  The code returned by the test function, or TPR_FAIL if some other error
//  condition arises.

INT Help_DDPIterate(DDPTESTPROC pfnTestProc, LPFUNCTION_TABLE_ENTRY pFTE)
{
    int     index;
    ITPR    nITPR = ITPR_PASS;

    IDirectDrawPalette      *pDDP;
    DDPTESTINFO             ddpti;
    FUNCTION_TABLE_ENTRY    fte;

    if(!pfnTestProc || !pFTE)
    {
        Debug(
            LOG_ABORT,
            FAILURE_HEADER TEXT("Help_DDPIterate was passed NULL"));
        return TPR_ABORT;
    }

    fte = *pFTE;
    fte.dwUserData  = (DWORD)&ddpti;
    for(index = 0; index < countof(g_adwPaletteCaps); index++)
    {
        ddpti.m_pszName = GetDDPCaps(g_adwPaletteCaps[index]);
        ddpti.m_dwCaps  = g_adwPaletteCaps[index];
        DebugBeginLevel(0, TEXT("Testing %s palette..."), ddpti.m_pszName);

        if(!Help_CreateDDPalette(g_adwPaletteCaps[index], &ddpti.m_pDDP))
        {
            nITPR=g_tprReturnVal;
        }
        else
        {
            pDDP = ddpti.m_pDDP;

            __try
            {
                nITPR |= pfnTestProc(TPM_EXECUTE, 0, &fte);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                Debug(
                    LOG_EXCEPTION,
                    FAILURE_HEADER TEXT("An exception occurred during the")
                    TEXT(" execution of this test"));
                nITPR |= ITPR_FAIL;
            }

            pDDP->Release();
        }

        DebugEndLevel(TEXT("Done with %s palette"), ddpti.m_pszName);
    }

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Help_DDPGetSetEntries
//  Calls IDirectDrawPalette::GetEntries and IDirectDrawPalette::SetEntries and
//  verifies that the result is correct and that the appropriate values were
//  modified. It also verifies that the array we pass in is not modified beyond
//  the appropriate boundaries.
//
// Parameters:
//  pDDP            Pointer to the palette object.
//  dwFlags         Flags used to create the palette.
//  dwBase          Starting index.
//  dwNumEntries    Number of entries to modify.
//  pszQualifier    Additional logging info.
//  pszAdditional   Additional logging info.
//
// Return value:
//  ITPR_PASS if the test passes, ITPR_FAIL if it fails, or ITPR_ABORT if the
//  test can't be run to completion.

ITPR Help_DDPGetSetEntries(
    IDirectDrawPalette  *pDDP,
    DWORD               dwFlags,
    DWORD               dwBase,
    DWORD               dwNumEntries,
    LPCTSTR             pszQualifier,
    LPCTSTR             pszAdditional)
{
    PALETTEENTRY    pe[256];

    HRESULT hr;
    char    *pData = (char *)pe;
    int     index,
            nFirstChange,
            nLastChange,
            nFirst,
            nLast,
            nArraySize,
            nPitch,
            nTableSize,
            nBias;
    LPCTSTR pszOpen,
            pszClose,
            pszSpace,
            pszEmpty = TEXT("");
    bool    bValid,
            bDontAllow256;
    ITPR    nITPR = ITPR_PASS;

    // The following macros will help to keep us straight in terms of what we
    // expect to see in the arrays we pass in, and in the palette entries we
    // get back
#define PRESET_LOCAL_VALUE      ((char)((5 + index)%256))
#define PRESET_PALETTE_ENTRY    ((char)((7 + index)%256))
#define TEST_PALETTE_ENTRY      ((char)((9 + index)%256))

    // Initial setup
    if(pszQualifier)
    {
        pszOpen = TEXT("(");
        pszClose = TEXT(")");
    }
    else
    {
        pszOpen = pszEmpty;
        pszQualifier = pszEmpty;
        pszClose = pszEmpty;
    }

    if(pszAdditional)
    {
        pszSpace = TEXT(" ");
    }
    else
    {
        pszSpace = pszEmpty;
        pszAdditional = pszEmpty;
    }

    // Determine if we are we going to modify valid locations in the palette
    nTableSize = 256;

    // Determine what things will be valid in this test
    bDontAllow256 = FALSE;
    bValid = dwNumEntries > 0 && (dwBase + dwNumEntries) <= (DWORD)nTableSize && (dwBase + dwNumEntries) > dwBase ;

    // In order to verify whether palette entries are updated, we'll need to
    // first initialize the palette with some values.
    for(index = 0; index < sizeof(pe); index++)
    {
        pData[index] = PRESET_PALETTE_ENTRY;
    }

    hr = pDDP->SetEntries(0, 0, nTableSize, pe);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawPalette,
            c_szSetEntries,
            hr,
            NULL,
            TEXT("during initialization"));
        return nITPR | ITPR_ABORT;
    }

    // Also determine which locations will be modified
    nPitch = 1;

    if(bValid)
    {
        // nFirstChange is the first location to be modified, nLastChange is
        // the first not to be
        nFirstChange = dwBase*nPitch;
        nLastChange = (dwBase + dwNumEntries)*nPitch;
        nBias = nFirstChange;
    }
    else
    {
        // Setting these variables out of range will work later because we'll
        // run only one of the three verification loops
        nFirstChange = nLastChange = sizeof(pe);
        nBias = 0;
    }

    // If we can't modify all 256 entries, then some things are different
    if(bDontAllow256)
    {
        nFirst = nPitch;
        nLast = (nTableSize - 1)*nPitch;
        nArraySize = sizeof(pe) - nPitch;

        if(nFirstChange == 0)
            nFirstChange = nPitch;
        else if(nFirstChange == nTableSize*nPitch)
            nFirstChange -= nPitch;

        if(nLastChange == 0)
            nLastChange = nPitch;
        else if(nLastChange == nTableSize*nPitch)
            nLastChange -= nPitch;
    }
    else
    {
        nFirst = 0;
        nLast = nTableSize*nPitch;
        nArraySize = sizeof(pe);
    }

    // Initialize our array to some known values
    for(index = 0; index < sizeof(pe); index++)
    {
        pData[index] = PRESET_LOCAL_VALUE;
    }

    // Call GetEntries
    hr = pDDP->GetEntries(
        0,
        dwBase,
        dwNumEntries,
        (PALETTEENTRY *)(pData + nBias));
    if((bValid && FAILED(hr)) ||
       (!bValid && hr != DDERR_INVALIDPARAMS))
    {
        Report(
            bValid ? RESULT_FAILURE : RESULT_UNEXPECTED,
            c_szIDirectDrawPalette,
            c_szGetEntries,
            hr,
            pszQualifier,
            pszAdditional,
            DDERR_INVALIDPARAMS);
        return nITPR | ITPR_FAIL;
    }

    // Verify that only the values that we requested were modified
    if(bValid)
    {
        // Area where nothing should change.
        for(index = nFirst; index < nFirstChange; index++)
        {
            if(pData[index] != PRESET_LOCAL_VALUE)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%s::%s%s%s%s modified our array at")
                    TEXT(" offset %d%s%s"),
                    c_szIDirectDrawPalette,
                    c_szGetEntries,
                    pszOpen,
                    pszQualifier,
                    pszClose,
                    index,
                    pszSpace,
                    pszAdditional);
                nITPR |= ITPR_FAIL;
                break;
            }
        }

        // Area where the palette should be modified
        for(index = nFirstChange; index < nLastChange; index++)
        {
            if(pData[index] != PRESET_PALETTE_ENTRY)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%s::%s%s%s%s did not set our array")
                    TEXT(" at offset %d%s%s"),
                    c_szIDirectDrawPalette,
                    c_szGetEntries,
                    pszOpen,
                    pszQualifier,
                    pszClose,
                    index,
                    pszSpace,
                    pszAdditional);
                nITPR |= ITPR_FAIL;
                break;
            }
        }

        // Second area where nothing should change
        for(index = nLastChange; index < nArraySize; index++)
        {
            if(pData[index] != PRESET_LOCAL_VALUE)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%s::%s%s%s%s modified our array at")
                    TEXT(" offset %d%s%s"),
                    c_szIDirectDrawPalette,
                    c_szGetEntries,
                    pszOpen,
                    pszQualifier,
                    pszClose,
                    index,
                    pszSpace,
                    pszAdditional);
                nITPR |= ITPR_FAIL;
                break;
            }
        }
    }
    else
    {
        // If the request wasn't valid, then nothing should have changed
        for(index = 0; index < sizeof(pe); index++)
        {
            if(pData[index] != PRESET_LOCAL_VALUE)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%s::%s%s%s%s modified the array we")
                    TEXT(" passed in at offset %d%s%s"),
                    c_szIDirectDrawPalette,
                    c_szGetEntries,
                    pszOpen,
                    pszQualifier,
                    pszClose,
                    index,
                    pszSpace,
                    pszAdditional);
                nITPR |= ITPR_FAIL;
                break;
            }
        }
    }

    // Reinitialize the array to call SetEntries
    for(index = 0; index < sizeof(pe); index++)
    {
        pData[index] = TEST_PALETTE_ENTRY;
    }

    // Call SetEntries
    hr = pDDP->SetEntries(
        0,
        dwBase,
        dwNumEntries,
        (PALETTEENTRY *)(pData + nBias));
    if((bValid && FAILED(hr)) ||
       (!bValid && hr != DDERR_INVALIDPARAMS))
    {
        Report(
            bValid ? RESULT_FAILURE : RESULT_UNEXPECTED,
            c_szIDirectDrawPalette,
            c_szSetEntries,
            hr,
            pszQualifier,
            pszAdditional,
            DDERR_INVALIDPARAMS);
        return nITPR | ITPR_FAIL;
    }

    // Verify that our array wasn't modified at all
    for(index = 0; index < sizeof(pe); index++)
    {
        if(pData[index] != TEST_PALETTE_ENTRY)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s%s%s%s modified the array we")
                TEXT(" passed in at offset %d%s%s"),
                c_szIDirectDrawPalette,
                c_szSetEntries,
                pszOpen,
                pszQualifier,
                pszClose,
                index,
                pszSpace,
                pszAdditional);
            nITPR |= ITPR_FAIL;
            break;
        }
    }

    // See what happened to the palette
    memset(pData, 0, sizeof(pe));
    hr = pDDP->GetEntries(0, 0, nTableSize, pe);
    if(FAILED(hr))
    {
        Report(
            RESULT_FAILURE,
            c_szIDirectDrawPalette,
            c_szGetEntries,
            hr,
            NULL,
            TEXT("during verification"));
        return nITPR | ITPR_FAIL;
    }

    // Now we should only check up to the size of the palette, since GetEntries
    // will (hopefully) not modify that area
    if(nFirstChange >= nTableSize*nPitch)
    {
        if(bDontAllow256)
        {
            nFirstChange = nLastChange = (nTableSize - 1)*nPitch;
        }
        else
        {
            nFirstChange = nLastChange = nTableSize*nPitch;
        }
    }

    // Area where nothing should change
    for(index = nFirst; index < nFirstChange; index++)
    {
        if(pData[index] != PRESET_PALETTE_ENTRY)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s%s%s%s appears to have modified")
                TEXT(" the palette at offset %d%s%s"),
                c_szIDirectDrawPalette,
                c_szSetEntries,
                pszOpen,
                pszQualifier,
                pszClose,
                index,
                pszSpace,
                pszAdditional);
            nITPR |= ITPR_FAIL;
            break;
        }
    }

    // Area where the palette should be modified
    for(index = nFirstChange; index < nLastChange; index++)
    {
        if(pData[index] != TEST_PALETTE_ENTRY)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s%s%s%s does not appear to have")
                TEXT(" modified the palette at offset %d%s%s"),
                c_szIDirectDrawPalette,
                c_szSetEntries,
                pszOpen,
                pszQualifier,
                pszClose,
                index,
                pszSpace,
                pszAdditional);
            nITPR |= ITPR_FAIL;
            break;
        }
    }

    // Second area where nothing should change
    for(index = nLastChange; index < nLast; index++)
    {
        if(pData[index] != PRESET_PALETTE_ENTRY)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s%s%s%s appears to have modified")
                TEXT(" the palette at offset %d%s%s"),
                c_szIDirectDrawPalette,
                c_szSetEntries,
                pszOpen,
                pszQualifier,
                pszClose,
                index,
                pszSpace,
                pszAdditional);
            nITPR |= ITPR_FAIL;
            break;
        }
    }

    // We don't need our macros anymore
#undef PRESET_LOCAL_VALUE
#undef PRESET_PALETTE_ENTRY
#undef TEST_PALETTE_ENTRY

    return nITPR;
}
#endif // TEST_IDDP

////////////////////////////////////////////////////////////////////////////////
