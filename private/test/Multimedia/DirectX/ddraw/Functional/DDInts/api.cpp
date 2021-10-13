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
//  Module: api.cpp
//          BVT for the DirectDraw API functions.
//
//  Revision History:
//      09/12/96    lblanco     Created for the TUX DLL Wizard.
//      09/20/96    lblanco     Added API BVTs.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

#if TEST_APIS

////////////////////////////////////////////////////////////////////////////////
// Local function prototypes

HRESULT CALLBACK Help_DirectDrawEnumerate_Callback(LPGUID, LPTSTR, LPTSTR, LPVOID, HMONITOR);

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static bool     g_bDDECallbackExpected = false;

////////////////////////////////////////////////////////////////////////////////
// Test_DirectDrawCreate
//  Tests the DirectDrawCreate API.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TRP_PASS if the test passes, TPR_FAIL if the tests fails, or possibly other
//  special conditions.

TESTPROCAPI Test_DirectDrawCreate(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT     hr = S_OK;
    IDirectDraw *pDD;
    ITPR        nITPR = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(DDERR_UNSUPPORTED == DirectDrawCreate(NULL, &pDD, NULL))
        return TPR_SKIP;
    if(pDD)
    {
        pDD->Release();
        pDD=NULL;
    }
#if QAHOME_INVALIDPARAMS
    Debug(LOG_DETAIL, TEXT("Running invalid param tests"));

    // Pass an invalid GUID pointer
    pDD = (IDirectDraw *)PRESET;
    INVALID_CALL(
        DirectDrawCreate((LPGUID)PRESET, &pDD, NULL),
        c_szDirectDrawCreate,
        NULL,
        TEXT("invalid GUID pointer"));

    // Pass a valid pointer to an invalid GUID
    GUID        guid;
    memset(&guid, 0, sizeof(guid));
    guid.Data1++;
    pDD = (IDirectDraw *)PRESET;
    INVALID_CALL_E(
        DirectDrawCreate(&guid, &pDD, NULL),
        c_szDirectDrawCreate,
        NULL,
        TEXT("pointer to invalid GUID"),
        DDERR_INVALIDPARAMS);

    // Pass NULL for the out parameter
    INVALID_CALL(
        DirectDrawCreate(NULL, NULL, NULL),
        c_szDirectDrawCreate,
        NULL,
        TEXT("NULL, NULL, NULL"));
#endif // QAHOME_INVALIDPARAMS

    // Create the default object
    Debug(
        LOG_COMMENT,
        TEXT("%s for default object"),
        lpFTE->lpDescription);
    pDD = (IDirectDraw *)PRESET;
    hr = DirectDrawCreate(NULL, &pDD, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
        pDD,
        c_szDirectDrawCreate,
        NULL,
        hr,
        TEXT("Default object")))
    {
        nITPR |= ITPR_FAIL;
    }
    else
    {
        DDCAPS ddcaps;
        // Verify that the pointer is really valid
        hr = pDD->GetCaps(&ddcaps, NULL);

        // We don't need this pointer anymore
        pDD->Release();

        // Did Initialize return what we expected?
        if(FAILED(hr))
        {
            // Oops... something went wrong.
            Report(
                RESULT_UNEXPECTED,
                c_szIDirectDraw,
                c_szInitialize,
                hr,
                NULL,
                TEXT("for successfully created object"),
                S_OK);
            nITPR |= ITPR_FAIL;
        }
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szDirectDrawCreate);

    return TPRFromITPR(nITPR);
}

#if TEST_IDDC
////////////////////////////////////////////////////////////////////////////////
// Test_DirectDrawCreateClipper
//  Tests the DirectDrawCreateClipper API.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TRP_PASS if the test passes, TPR_FAIL if the tests fails, or possibly other
//  special conditions.

TESTPROCAPI Test_DirectDrawCreateClipper(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDrawClipper  *pDDC;
    HRESULT             hr = S_OK;
    ITPR                nITPR = ITPR_PASS;


    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

#if QAHOME_INVALIDPARAMS
    INVALID_CALL(
        DirectDrawCreateClipper(0, NULL, NULL),
        c_szDirectDrawCreateClipper,
        NULL,
        TEXT("0, NULL, NULL"));

    INVALID_CALL_E_RAID(
        DirectDrawCreateClipper(0, &pDDC, (IUnknown *)PRESET),
        c_szDirectDrawCreate,
        NULL,
        TEXT("0, non-NULL, non-NULL"),
        CLASS_E_NOAGGREGATION,
        IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
        IFKATANAELSE(0, 1608));
#endif // QAHOME_INVALIDPARAMS

    pDDC = (IDirectDrawClipper *)PRESET;
    hr = DirectDrawCreateClipper(0, &pDDC, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASE | CRP_ALLOWNONULLOUT,
        pDDC,
        c_szDirectDrawCreateClipper,
        NULL,
        hr))
    {
        nITPR |= ITPR_FAIL;
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szDirectDrawCreateClipper);

    return TPRFromITPR(nITPR);
}
#endif // TEST_IDDC

////////////////////////////////////////////////////////////////////////////////
// Test_DirectDrawEnumerate
//  Tests the DirectDrawEnumerate API.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TRP_PASS if the test passes, TPR_FAIL if the tests fails, or possibly other
//  special conditions.

TESTPROCAPI Test_DirectDrawEnumerate(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT hr = S_OK;
    ITPR    nITPR = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

#if QAHOME_INVALIDPARAMS
    // Invalid parameters
    INVALID_CALL(
        DirectDrawEnumerateEx(NULL, NULL, 0),
        c_szDirectDrawEnumerate,
        NULL,
        TEXT("NULL, NULL"));
#endif // QAHOME_INVALIDPARAMS

    // Valid parameters
    g_bTestPassed = true;
    g_bDDECallbackExpected = true;
    g_pContext = (void *)PRESET;
    g_nCallbackCount = -1; // No effective limit

    hr = DirectDrawEnumerateEx((LPDDENUMCALLBACKEX)Help_DirectDrawEnumerate_Callback, g_pContext, 
        DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_DETACHEDSECONDARYDEVICES);
    g_bDDECallbackExpected = false;
    
    if(FAILED(hr))
    {
        Report(RESULT_FAILURE, c_szDirectDrawEnumerate, NULL, hr);
        nITPR |= ITPR_FAIL;
    }
    else if(!g_bTestPassed)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s returned success, but the call ")
            TEXT("behaved unexpectedly"),
            lpFTE->lpDescription);
        nITPR |= g_tprReturnVal;
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szDirectDrawEnumerate);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Help_DirectDrawEnumerate_Callback
//  Helper callback for the DirectDrawEnumerate test function.
//
// Parameters:
//  pGUID           GUID of a display driver.
//  pDescription    Driver description.
//  pName           Driver name.
//  pContext        Context (we always pass NULL).
//
// Return value:
//  DDENUMRET_OK to continue enumerating, DDENUMRET_CANCEL to stop.

HRESULT CALLBACK Help_DirectDrawEnumerate_Callback(
    LPGUID  pGUID,
    LPTSTR  pDescription,
    LPTSTR  pName,
    LPVOID  pContext,
    HMONITOR hm)
{
    IDirectDraw *pDD;
    HRESULT hr;
    TCHAR       szGUID[50];
    g_tprReturnVal=ITPR_FAIL;
    
    if(pGUID)
    {
        _stprintf_s(
            szGUID,
            TEXT("{ %08x-%08x-%08x-%02x%02x-%02x%02x%02x%02x%02x%02x }"),
            pGUID->Data1,
            pGUID->Data2,
            pGUID->Data3,
            pGUID->Data4[0],
            pGUID->Data4[1],
            pGUID->Data4[2],
            pGUID->Data4[3],
            pGUID->Data4[4],
            pGUID->Data4[5],
            pGUID->Data4[6],
            pGUID->Data4[7]);
    }
    else
        _tcscpy_s(szGUID, TEXT("NULL"));

    Debug(
        LOG_DETAIL,
        TEXT("DDObject: GUID = %s, Description = %s, Name = %s, ")
        TEXT("Context = 0x%08x, Monitor = 0x%08x"),
        szGUID,
        pDescription ? pDescription : TEXT("NULL"),
        pName ? pName : TEXT("NULL"),
        pContext, hm);

    // Were we expecting to be called back at this time?
    if(!g_bDDECallbackExpected)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("Unexpected DirectDrawEnumerate callback"));
        g_bTestPassed = false;
        return (HRESULT)DDENUMRET_CANCEL;
    }

    // Is the context value correct?
    if(pContext != g_pContext)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("DirectDrawEnumerate callback expected a")
            TEXT(" context of 0x%08x (got 0x%08x)"),
            g_pContext,
            pContext);
        g_bTestPassed = false;
    }

    // Is this GUID valid?
    pDD = (IDirectDraw *)PRESET;
    hr = DirectDrawCreate(pGUID, &pDD, NULL);

    if(DDERR_UNSUPPORTED == hr)
    {
        g_tprReturnVal = ITPR_SKIP;
        g_bTestPassed=false;
    }
    else if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
        pDD,
        c_szDirectDrawCreate,
        NULL,
        hr))
    {
        g_bTestPassed = false;
    }
    else
    {
        Debug(LOG_PASS, TEXT("DirectDrawEnumerate: GUID verified."));
        pDD->Release();
    }

    // Are we done with callbacks?
    if(!--g_nCallbackCount)
    {
        g_bDDECallbackExpected = false;
        return (HRESULT)DDENUMRET_CANCEL;
    }

    return (HRESULT)DDENUMRET_OK;
}
#endif // TEST_APIS

////////////////////////////////////////////////////////////////////////////////
