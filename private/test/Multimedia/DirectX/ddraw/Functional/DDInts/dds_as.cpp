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
//  Module: dds_as.cpp
//          BVTs for the IDirectDrawSurface interface attached surface-related
//          functions.
//
//  Revision History:
//      12/05/1997  lblanco     Split off from dds.cpp.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

#if TEST_IDDS

////////////////////////////////////////////////////////////////////////////////
// Local macros

#define NUM_ATTACHED_SURFACES       5

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static bool g_bEASCallbackExpected = false;
static int  g_nCallbackCalled;

////////////////////////////////////////////////////////////////////////////////
// Help_DDS_EnumAttachedSurfaces_Callback
//  Callback for the IDirectDrawSurface::EnumAttachedSurfaces method test.
//
// Parameters:
//  pDDS            Pointer to the surface being enumerated.
//  pddsd           Pointer to the description of the surface.
//  pContext        Context passed by the user.
//
// Return value:
//  DDENUMRET_OK to continue enumeration, DDENUMRET_CANCEL to stop.

HRESULT FAR PASCAL Help_DDS_EnumAttachedSurfaces_Callback(
    IDirectDrawSurface  *pDDS,
    DDSURFACEDESC       *pddsd,
    void                *pContext)
{
    // Were we expecting to be called back at this time?
    if(!g_bEASCallbackExpected)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("Unexpected %s::%s callback"),
            c_szIDirectDrawSurface,
            c_szEnumAttachedSurfaces);
        g_bTestPassed = false;
        return (HRESULT)DDENUMRET_CANCEL;
    }

    // Is the context value correct?
    if(pContext != g_pContext)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s::%s callback expected a")
            TEXT(" context of 0x%08x (got 0x%08x)"),
            c_szIDirectDrawSurface,
            c_szEnumAttachedSurfaces,
            g_pContext,
            pContext);
        g_bTestPassed = false;
    }

    // Do we have a valid surface pointer?
    if(!pDDS)
    {
        Report(
            RESULT_NULLNOTPOINTER,
            c_szIDirectDrawSurface,
            c_szEnumAttachedSurfaces,
            0,
            NULL,
            TEXT("surface pointer"));
        g_bTestPassed = false;
    }

    // Do we have a valid surface description pointer?
    if(!pddsd)
    {
        Report(
            RESULT_NULLNOTPOINTER,
            c_szIDirectDrawSurface,
            c_szEnumAttachedSurfaces,
            0,
            NULL,
            TEXT("surface description pointer"));
        g_bTestPassed = false;
    }

    g_nCallbackCalled++;

    if(pDDS)
        pDDS->Release();

    // Are we done with callbacks?
    if(!--g_nCallbackCount)
    {
        g_bEASCallbackExpected = false;
        Debug(
            LOG_COMMENT,
            TEXT("Help_DDS_EnumAttachedSurfaces_Callback requesting")
            TEXT(" enumeration to stop"));
        return (HRESULT)DDENUMRET_CANCEL;
    }

    Debug(
        LOG_COMMENT,
        TEXT("Help_DDS_EnumAttachedSurfaces_Callback requesting enumeration")
        TEXT(" to continue"));

    return (HRESULT)DDENUMRET_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_EnumAttachedSurfaces ***
//  Tests the IDirectDrawSurface::EnumAttachedSurfaces method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_EnumAttachedSurfaces(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    int     nBackBuffers = 5;
    ITPR    nITPR = ITPR_PASS;
    return TPRFromITPR(nITPR);

#if QAHOME_INVALIDPARAMS
    
#endif // QAHOME_INVALIDPARAMS
    
}

#endif // TEST_IDDS

////////////////////////////////////////////////////////////////////////////////
