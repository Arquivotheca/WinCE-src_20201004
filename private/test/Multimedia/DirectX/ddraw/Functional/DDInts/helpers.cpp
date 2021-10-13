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
//  Module: helpers.cpp
//          General helper functions.
//
//  Revision History:
//      07/09/97    lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

////////////////////////////////////////////////////////////////////////////////
// Test_AddRefRelease
//  Tests AddRef and Release functionality.
//
// Parameters:
//  pObject         Pointer to the interface being tested.
//  pszName         Name of the interface.
//  pszAdditional   Additional text to log.
//
// Return value:
//  true if the test passes, false otherwise.

bool Test_AddRefRelease(
    IUnknown    *pObject,
    LPCTSTR     pszName,
    LPCTSTR     pszAdditional)
{
    ULONG   ulRef1 = 0,
            ulRef2 = 0,
            ulRef3,
            ulRef4,
            ulRef5,
            ulRef6;
    bool    bSuccess = true;
    LPCTSTR pszSpace;

    if(pszAdditional)
    {
        pszSpace = TEXT(" ");
    }
    else
    {
        pszAdditional = pszSpace = TEXT("");
    }

    // Test AddRef
    __try
    {
        ulRef1 = pObject->AddRef();
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        Report(RESULT_EXCEPTION, pszName, c_szAddRef, 0, NULL, pszAdditional);
        bSuccess = false;
    }

    // Test Release
    __try
    {
        ulRef2 = pObject->Release();
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        Report(RESULT_EXCEPTION, pszName, c_szRelease, 0, NULL, pszAdditional);
        bSuccess = false;
    }

    // If we crashed, there is no point in continuing
    if(!bSuccess)
        return false;

    // If we call AddRef again, we should get the same reference count as before
    PREFAST_ASSUME(pObject);
    ulRef3 = pObject->AddRef();

#if QAHOME_REFCOUNTING
    if(ulRef3 != ulRef1)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s::%s was expected to return %d%s%s")
            TEXT(" (returned %d)"),
            pszName,
            c_szAddRef,
            ulRef1,
            pszSpace,
            pszAdditional,
            ulRef3);
        bSuccess = false;
    }
#endif // QAHOME_REFCOUNTING

    // If we call AddRef another time, we should get a larger reference count
    ulRef5 = pObject->AddRef();
#if QAHOME_REFCOUNTING
    if(ulRef5 <= ulRef3)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s::%s was expected to increment the")
            TEXT(" reference count%s%s (was %d, returned %d)"),
            pszName,
            c_szAddRef,
            pszSpace,
            pszAdditional,
            ulRef3,
            ulRef5);
        bSuccess = false;
    }
#endif // QAHOME_REFCOUNTING

    // Release, too, should return something larger than before. Save this value
    // for later comparison.
    ulRef6 = pObject->Release();
    ulRef4 = pObject->Release();

#if QAHOME_REFCOUNTING
    if(ulRef4 != ulRef2)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s::%s was expected to return %d%s%s")
            TEXT(" (returned %d)"),
            pszName,
            c_szRelease,
            ulRef2,
            pszSpace,
            pszAdditional,
            ulRef4);
        bSuccess = false;
    }

    if(ulRef4 >= ulRef6)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s::%s was expected to decrement the")
            TEXT(" reference count%s%s (was %d, returned %d)"),
            pszName,
            c_szRelease,
            pszSpace,
            pszAdditional,
            ulRef6,
            ulRef4);
        bSuccess = false;
    }
#endif // QAHOME_REFCOUNTING

    return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////
// Test_QueryInterface
//  Tests QueryInterface functionality. Both valid and invalid IIDs are
//  requested.
//
// Parameters:
//  pObject         Pointer to the interface being tested.
//  pszName         Name of the interface.
//  pnValidIndex    Array containing the indices of the interfaces for which
//                  QueryInterface should succeed. IUnknown is always assumed
//                  to be valid, so it doesn't have to be specified here. All
//                  known interfaces not specified here are assumed to be
//                  invalid for this particular QueryInterface call.
//  nValidIndices   Number of entries in pnValidIndex.
//  nSpecial        Special handling code. If zero, no special handling is
//                  required (the default). Otherwise, this code says what has
//                  to be done differently to report a particular bug ID.
//
// Return value:
//  true if the test passes, false otherwise.

bool Test_QueryInterface(
    IUnknown    *pObject,
    LPCTSTR     pszName,
    INDEX_IID   *pnValidIndex,
    int         nValidIndices,
    SPECIAL     nSpecial)
{
    HRESULT     hr = S_OK;
    IUnknown    *pNewInterface = NULL,
                *pSecondInterface;
    int         index;
    bool        bSuccess = true,
                bContinue;
    INDEX_IID   idxCurrent;
    bool        bValid[INDEX_IID_COUNT],
                bFailed[INDEX_IID_COUNT];
    HRESULT     hrFailed[INDEX_IID_COUNT];
    ULONG       ulRef1,
                ulRef2;
    int         nFailed = 0;
    LPCTSTR     pszBugID;

    // Sanity check
    if(!pObject || !pszName || !pnValidIndex)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("Test_QueryInterface was passed NULL"));
        return false;
    }
    if(nValidIndices < 0 || nValidIndices > INDEX_IID_COUNT)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("Test_QueryInterface was passed a bogus")
            TEXT(" array size (%d)"),
            nValidIndices);
        return false;
    }

#if QAHOME_INVALIDPARAMS
    // Preliminary test: pass NULL as the second parameter
    __try
    {

#ifdef KATANA
#define BUGID   0
#else // KATANA
#define BUGID   1565
#endif // KATANA
        hr = pObject->QueryInterface(IID_IUnknown, NULL);
        if(hr != DDERR_INVALIDPARAMS)
        {

            Report(
                RESULT_UNEXPECTED,
                pszName,
                c_szQueryInterface,
                hr,
                TEXT("IID_IUnknown, NULL"),
                NULL,
                DDERR_INVALIDPARAMS,
                RAID_HPURAID,
                BUGID);
            bSuccess = false;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        Report(
            RESULT_EXCEPTION,
            pszName,
            c_szQueryInterface,
            0,
            TEXT("IID_IUnknown, NULL"),
            NULL,
            0,
            RAID_HPURAID,
            BUGID);
#undef BUGID
        bSuccess = false;
    }
#endif // QAHOME_INVALIDPARAMS

    // Determine which interfaces are valid and which are not
    memset(bValid, 0, sizeof(bValid));
    memset(bFailed, 0, sizeof(bFailed));

    // IUnknown is always valid
    bValid[INDEX_IID_IUnknown] = true;

    for(index = 0; index < nValidIndices; index++)
    {
        idxCurrent = pnValidIndex[index];
        if(idxCurrent < 0 || idxCurrent >= INDEX_IID_COUNT)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("Test_QueryInterface was passed an IID")
                TEXT(" index out of range (%d)"),
                idxCurrent);
            return false;
        }

        bValid[idxCurrent] = true;
    }

    // Try all interfaces
    for(index = 0; index < INDEX_IID_COUNT; index++)
    {
        bContinue = false;

        // Uncomment the following code if Bug Keep #1185 regresses
        // Bug Keep #1185 is preventing other tests from running, so skip
        // invalid IID tests until this bug is fixed.
        // if(!bValid[index])
        //     continue;

        __try
        {
            Debug(
                LOG_COMMENT,
                TEXT("Calling %s::%s(%s)..."),
                pszName,
                c_szQueryInterface,
                g_aKnownIIDName[index]);
            pNewInterface = (IUnknown *)PRESET;
            hr = pObject->QueryInterface(
                *g_aKnownIID[index],
                (void **)&pNewInterface);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            UINT nBugID = 0;

#ifdef KATANA
            if(nSpecial == SPECIAL_IDDS_QI)
            {
                nBugID = 3196;
            }
#endif // KATANA
            Report(
                RESULT_EXCEPTION,
                pszName,
                c_szQueryInterface,
                0,
                g_aKnownIIDName[index],
                NULL,
                S_OK,
                RAID_HPURAID,
                nBugID);
            bSuccess = false;
            bContinue = true;
        }

        if(bContinue)
            continue;

        // Did we get the expected error code?
        if((bValid[index] && FAILED(hr)) ||
           (!bValid[index] && hr != E_NOINTERFACE))
        {
            if(nSpecial == SPECIAL_IDDS_QI || nSpecial == SPECIAL_IDDP_QI)
            {
                bFailed[index] = true;
                hrFailed[index] = hr;
                nFailed++;
            }
            else
            {
#ifdef KATANA
#define BUGID   0
#else // KATANA
#define BUGID   4305
#endif // KATANA
                Report(
                    bValid[index] ? RESULT_FAILURE : RESULT_UNEXPECTED,
                    pszName,
                    c_szQueryInterface,
                    hr,
                    g_aKnownIIDName[index],
                    NULL,
                    E_NOINTERFACE,
                    RAID_HPURAID,
                    BUGID);
#undef BUGID
            }
            bSuccess = false;
        }

        // Does the returned pointer make sense?
        if(SUCCEEDED(hr))
        {
            // Better be a valid pointer
            if(!pNewInterface || pNewInterface == (IUnknown *)PRESET)
            {
                Report(
                    pNewInterface ? RESULT_INVALID : RESULT_NULL,
                    pszName,
                    c_szQueryInterface,
                    0,
                    g_aKnownIIDName[index]);
                bSuccess = false;
            }
        }
        else if(pNewInterface)
        {
#ifndef KATANA
            if(nSpecial == SPECIAL_IDD_QI)
            {
                pszBugID = TEXT("(Keep #10177) ");
            }
            else
            {
                pszBugID = TEXT("");
            }
#else // KATANA
            if((nSpecial == SPECIAL_IDDP_QI) && (pNewInterface == (IUnknown *)PRESET))
            {
                pszBugID = TEXT("(HPURAID #3193) ");
            }
            else
            {
                pszBugID = TEXT("");
            }
#endif // KATANA
            // QueryInterface returned failure and a non-NULL pointer.
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s%s::%s(%s) %s, even though it")
                TEXT(" failed (returned %s: %s)"),
                pszBugID,
                pszName,
                c_szQueryInterface,
                g_aKnownIIDName[index],
                (pNewInterface == (IUnknown *)PRESET) ?
                    TEXT("did not NULL out the returned pointer") :
                    TEXT("returned a non-NULL pointer"),
                GetErrorName(hr),
                GetErrorDescription(hr));
            bSuccess = false;
        }

        // Does the returned pointer appear to be valid?
        if(pNewInterface && pNewInterface != (IUnknown *)PRESET)
        {
            // If QueryInterface succeeded, see what happened to the reference
            // count
            if(SUCCEEDED(hr))
            {
                ulRef1 = pNewInterface->AddRef();
                pNewInterface->Release();
                hr = pObject->QueryInterface(
                    *g_aKnownIID[index],
                    (void **)&pSecondInterface);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        pszName,
                        c_szQueryInterface,
                        hr,
                        g_aKnownIIDName[index],
                        TEXT("the second time it was called"));
                    bSuccess = false;
                }
                else
                {
                    ulRef2 = pSecondInterface->AddRef();
                    pSecondInterface->Release();
                    pSecondInterface->Release();

#if QAHOME_REFCOUNTING
                    if(ulRef2 <= ulRef1)
                    {
#ifdef KATANA
#define pszError    TEXT("")
#else // KATANA
                        LPCTSTR pszError;
                        if(index == INDEX_IID_IUnknown)
                        {
                            pszError = TEXT("(HPURAID #2385) ");
                        }
                        else if(index == INDEX_IID_IDirectDrawSurface)
                        {
                            pszError = TEXT("(HPURAID #2386) ");
                        }
                        else 
                        {
                            // Unknown bug
                            pszError = TEXT("");
                        }
#endif // KATANA
                        Debug(
                            LOG_FAIL,
                            FAILURE_HEADER TEXT("%s%s::%s(%s) did not increment")
                            TEXT(" the reference count (before call: %d;")
                            TEXT(" after call: %d)"),
                            pszError,
                            pszName,
                            c_szQueryInterface,
                            g_aKnownIIDName[index],
                            ulRef1,
                            ulRef2);
                        bSuccess = false;
                    }
#ifdef KATANA
#undef pszError
#endif // KATANA
#endif // QAHOME_REFCOUNTING
                }
            }

            // Release the pointer
            __try
            {
                pNewInterface->Release();
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                TCHAR szMessage[128];

                _stprintf_s(
                    szMessage,
                    TEXT("on the pointer returned by %s::%s(%s)"),
                    pszName,
                    c_szQueryInterface,
                    g_aKnownIIDName[index]);
                Report(
                    RESULT_EXCEPTION,
                    c_szRelease,
                    NULL,
                    0,
                    NULL,
                    szMessage,
                    0);
                bSuccess = false;
            }
        }
    }

    if(nSpecial == SPECIAL_IDDS_QI)
    {
        ULONG nBugID;

        // In this case, we have bug Keep #1186 iff all interfaces that should
        // have returned E_NOINTERFACE in fact returned DD_OK. In either case,
        // we'll reproduce the error codes, but we'll output a bug ID only if
        // we recognize this bug correctly
        if(nFailed == INDEX_IID_COUNT - nValidIndices - 1)
        {
#ifdef KATANA
            nBugID = 1186;
#else // KATANA
            nBugID = 1056;
#endif // KATANA
        }
        else
        {
            nBugID = 0;
        }

        for(index = 0; index < INDEX_IID_COUNT; index++)
        {
            if(bFailed[index])
            {
#ifdef KATANA
#define DB      RAID_KEEP
#else // KATANA
#define DB      RAID_HPURAID
#endif // KATANA
                Report(
                    bValid[index] ? RESULT_FAILURE : RESULT_UNEXPECTED,
                    pszName,
                    c_szQueryInterface,
                    hrFailed[index],
                    g_aKnownIIDName[index],
                    NULL,
                    E_NOINTERFACE,
                    DB,
                    nBugID);
#undef DB
            }
        }
    }

    return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////
// Help_WaitForKeystroke
//  Stops execution until the user hits a key.
//
// Parameters:
//  None.
//
// Return value:
//  None.

void Help_WaitForKeystroke(void)
{
    MSG     msg;

    // Process any outstanding messages
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Clear the key hits flags
    g_bKeyHit = false;

    // Wait for a key to be hit
    while(GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if(g_bKeyHit)
        {
            break;
        }
    }

    // Process any remaining messages
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

HRESULT CALLBACK Help_GetBackBuffer_Callback(LPDIRECTDRAWSURFACE pDDS, LPDDSURFACEDESC pddsd, LPVOID pContext)
{
    // NOTE: Surfaces need to be released!
    if ((pddsd->dwFlags & DDSD_CAPS) && (pddsd->ddsCaps.dwCaps & DDSCAPS_BACKBUFFER))
    {
        *((LPDIRECTDRAWSURFACE *)pContext) = pDDS;
        return (HRESULT)DDENUMRET_CANCEL;
    }
    pDDS->Release();
    return (HRESULT)DDENUMRET_OK;
}

////////////////////////////////////////////////////////////////////////////////
