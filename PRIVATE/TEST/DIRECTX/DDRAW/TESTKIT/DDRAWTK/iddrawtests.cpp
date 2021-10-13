//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//

#include "main.h"

#include <DDrawUty.h>
#include <QATestUty/WinApp.h>

#include "IDDrawTests.h"

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

namespace Test_IDirectDraw
{

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_EnumDisplayModes::EnumModesCallback
    //  Callback function.
    //
    HRESULT WINAPI CTest_EnumDisplayModes::Callback(LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext)
    {
        CheckCondition(lpContext == NULL, "Callback Called with invalid Context", (HRESULT)DDENUMRET_CANCEL);

        // Get a pointer to the test object that we passed in
        CTest_EnumDisplayModes *ptest = static_cast<CTest_EnumDisplayModes*>(lpContext);

        // Do a number of sanity checks
        CheckCondition(!PresetUty::IsPreset(ptest->m_dwPreset), "DirectDrawEnumerate Callback called with invalid Context", (HRESULT)DDENUMRET_CANCEL);
        QueryCondition(!ptest->m_fCallbackExpected, "DirectDrawEnumerate Callback called when not expected", ptest->m_tr |= trFail);
        QueryCondition(lpDDSurfaceDesc == NULL, "DirectDrawEnumerate Callback received lpcstrDescription == NULL", ptest->m_tr |= trFail);

        // Log the callback information
        dbgout
            << "Surface Description #" << ptest->m_nCallbackCount
            << ":" << *lpDDSurfaceDesc << endl;
            
        // Increment our callback count
        ptest->m_nCallbackCount++;

        // Make sure that we got a create-able Surface Description
        CComPointer<IDirectDrawSurface> piDDS;
        HRESULT hr;

        hr = ptest->m_piDD->CreateSurface(lpDDSurfaceDesc, piDDS.AsTestedOutParam(), NULL);
        QueryCondition(DDERR_OUTOFMEMORY == hr, "Out of memory", ptest->m_tr |= trSkip)
        else QueryHRESULT(hr, "EnumDisplayModes Callback received non-creatable GUID", ptest->m_tr |= trFail)
        else QueryCondition(piDDS.InvalidOutParam(), "EnumDisplayModes Callback received non-creatable GUID", ptest->m_tr |= trFail);

        // Even if we've failed, keep iterating (since we won't
        // loose the failure on the next iteration).
        return (HRESULT)DDENUMRET_OK;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_EnumDisplayModes::Test
    //  Tests the EnumDisplayModes Method.
    //
    eTestResult CTest_EnumDisplayModes::TestIDirectDraw()
    {
        HRESULT hr;
        
        m_tr = trPass;
        m_nCallbackCount = 0;
        dbgout << "Calling EnumDisplayModes with DDEDM_REFRESHRATES and NULL SurfaceDesc" << endl;
        
        m_fCallbackExpected = true;
        hr = m_piDD->EnumDisplayModes(DDEDM_REFRESHRATES, NULL, this, Callback);
        m_fCallbackExpected = false;
        CheckHRESULT(hr, "EnumDisplayModes Failed", trFail);

        // Output the number of callbacks we got
        dbgout(LOG_DETAIL) << "Enumerated " << m_nCallbackCount << " modes(s)" << endl;

        // Make sure the callback was called at least once
        QueryCondition(m_nCallbackCount < 1, "EnumDisplayModes failed to call callback", trFail);

        return m_tr;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetCaps::Test
    //
    eTestResult CTest_GetCaps::TestIDirectDraw()
    {
        CDDCaps ddcapsHEL1, 
                ddcapsHAL1,
                ddcapsHEL2,
                ddcapsHAL2;
        eTestResult tr = trPass;
        HRESULT hr;
        
        // HAL Only
        dbgout << "Getting Caps for HAL only" << endl;
        hr = m_piDD->GetCaps(&ddcapsHAL1, NULL);
        CheckHRESULT(hr, "GetCaps HAL Only", trFail);

        // HEL Only
        dbgout << "Getting Caps for HEL only" << endl;
        hr = m_piDD->GetCaps(NULL, &ddcapsHEL1);
        CheckHRESULT(hr, "GetCaps HEL Only", trFail);

        // Both HEL and HAL
        dbgout << "Getting Caps for HAL and HEL together" << endl;
        hr = m_piDD->GetCaps(&ddcapsHAL2, &ddcapsHEL2);
        CheckHRESULT(hr, "GetCaps HAL and HEL together", trFail);

        // Compare the results from the two calls
        dbgout << "Comparing results from different calls" << endl;
        QueryCondition(memcmp(&ddcapsHAL1, &ddcapsHAL2, sizeof(ddcapsHAL1)), "HAL CAPs differed between calls", tr |= trFail);
        QueryCondition(memcmp(&ddcapsHEL1, &ddcapsHEL2, sizeof(ddcapsHEL1)), "HEL CAPs differed between calls", tr |= trFail);

        // Display the caps that we recieved
        dbgout(LOG_DETAIL)
            << indent
            << "HAL Caps:" << ddcapsHAL1 << endl
            << "HEL Caps:" << ddcapsHEL1 << endl
            << unindent;

        return tr;
    }

};

