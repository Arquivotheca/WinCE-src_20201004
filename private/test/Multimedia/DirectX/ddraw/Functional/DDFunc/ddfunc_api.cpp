#include "main.h"
#include "DDFunc_API.h"

#include <DDrawUty.h>

using namespace com_utilities;
using namespace DebugStreamUty;

namespace Test_DirectDrawAPI
{
    using namespace PresetUty;
   
    ////////////////////////////////////////////////////////////////////////////////
    // CreateDirectDrawGUID
    //  Creates and test a DirectDraw obeject with given GUID.
    //
    eTestResult CTest_DirectDrawCreate::CreateDirectDrawGUID(GUID *lpGUID)
    {
        CComPointer<IDirectDraw> piDD;
        eTestResult tr = trPass;
        HRESULT hr;

        // Used to test the returned IDirectDraw object
        DDCAPS ddcaps;

        HINSTANCE hInstDDraw;
        PFNDDRAWCREATE pfnDDCreate = NULL;
        
        if(NULL==(hInstDDraw = LoadLibrary(TEXT("ddraw.dll"))))
        {
            dbgout << "No direct draw support on system";
            return trSkip;
        }

#ifdef UNDER_CE
        pfnDDCreate = (PFNDDRAWCREATE)GetProcAddress(hInstDDraw, TEXT("DirectDrawCreate"));
#else
        pfnDDCreate = (PFNDDRAWCREATE)GetProcAddress(hInstDDraw, "DirectDrawCreate");
#endif
        QueryCondition(!pfnDDCreate, "Creating a pointer to DirectDrawCreate", tr|=trFail; goto Exit);
        // Create the DirectDraw Object
        hr = pfnDDCreate(NULL, piDD.AsTestedOutParam(), NULL);
        QueryCondition(DDERR_UNSUPPORTED==hr, "No direct draw support in driver", tr|=trSkip; goto Exit);
        QueryHRESULT(hr, "DirectDrawCreate Default", tr|=trFail; goto Exit);
        QueryCondition(piDD.InvalidOutParam(), "DirectDrawCreate succeeded but failed to set the OUT parameter", tr|=trFail; goto Exit);
        
        // Verify that the pointer is valid
        dbgout << "Initializing the created DDraw Object" << endl;
        hr = piDD->GetCaps(&ddcaps, NULL);
        QueryHRESULT(hr, "IDirectDraw::GetCaps", tr|=trFail);

Exit:
        piDD.ReleaseInterface();
        FreeLibrary(hInstDDraw);
        
        return tr;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_DirectDrawCreate::Test
    //  Tests the DirectDrawCreate API.
    //
    eTestResult CTest_DirectDrawCreate::Test()
    {
        eTestResult tr = trPass;

        // Check the default case
        dbgout << "Calling DirectDrawCreate on Default GUID" << endl;
        tr |= CreateDirectDrawGUID(NULL);
        
        return tr;
    }

#if CFG_TEST_IDirectDrawClipper    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_DirectDrawCreateClipper::Test
    //  Tests the DirectDrawCreateClipper API.
    //
    eTestResult CTest_DirectDrawCreateClipper::Test()
    {
        CComPointer<IDirectDrawClipper> piDDC;
        eTestResult tr = trPass;
        HRESULT hr;
        BOOL fChanged = FALSE;

        HINSTANCE hInstDDraw;
        PFNDDRAWCREATECLIPPER pfnDDCreateClipper = NULL;
        
        if(NULL==(hInstDDraw = LoadLibrary(TEXT("ddraw.dll"))))
        {
            dbgout << "No direct draw support on system";
            return trSkip;
        }

#ifdef UNDER_CE
        pfnDDCreateClipper = (PFNDDRAWCREATECLIPPER)GetProcAddress(hInstDDraw, TEXT("DirectDrawCreateClipper"));
#else
        pfnDDCreateClipper = (PFNDDRAWCREATECLIPPER)GetProcAddress(hInstDDraw, "DirectDrawCreateClipper");
#endif
        QueryCondition(!pfnDDCreateClipper, "Creating a pointer to DirectDrawCreate", tr|=trFail);
        if(tr!= trPass)
        {
            FreeLibrary(hInstDDraw);
            return tr;
        }
        // Create the DirectDraw Object
        dbgout << "Calling DirectDrawCreateClipper" << endl;
        hr = pfnDDCreateClipper(NULL, piDDC.AsTestedOutParam(), NULL);
        QueryHRESULT(hr, "DirectDrawCreateClipper", tr|=trFail; goto Exit);
        QueryCondition(piDDC.InvalidOutParam(), "DirectDrawCreate succeeded but failed to set the OUT parameter", tr|=trFail; goto Exit);

        // Verify that the pointer is valid
        dbgout << "Initializing the created DirectDrawClipper Object" << endl;
        hr = piDDC->IsClipListChanged(&fChanged);
        QueryHRESULT(hr, "IDirectDrawClipper::IsClipListChanged", tr|=trFail);

Exit:
        piDDC.ReleaseInterface();
        FreeLibrary(hInstDDraw);
        return tr;
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_DirectDrawEnumerate::Callback
    //  Callback function for the DirectDrawEnumerate API.
    //
    HRESULT CALLBACK CTest_DirectDrawEnumerate::Callback(LPGUID lpGUID, LPTSTR lpDescription, LPTSTR lpDriverName, LPVOID lpContext, HMONITOR hmonitor)
    {
        CheckCondition(lpContext == NULL, "Callback Called with invalid Context", (HRESULT)DDENUMRET_CANCEL);

        // Get a pointer to the test object that we passed in
        CTest_DirectDrawEnumerate *ptest = static_cast<CTest_DirectDrawEnumerate*>(lpContext);

        // Do a number of sanity checks
        CheckCondition(!IsPreset(ptest->m_dwPreset), "DirectDrawEnumerate Callback called with invalid Context", (HRESULT)DDENUMRET_CANCEL);
        QueryCondition(!ptest->m_fCallbackExpected, "DirectDrawEnumerate Callback called when not expected", ptest->m_tr |= trFail);
        QueryCondition(lpDescription == NULL, "DirectDrawEnumerate Callback received lpcstrDescription == NULL", ptest->m_tr |= trFail);
        QueryCondition(lpDriverName == NULL, "DirectDrawEnumerate Callback received lpContext == NULL", ptest->m_tr |= trFail);

        // Log the callback information
        dbgout(LOG_DETAIL)
            << "lpGUID = " << lpGUID << " (" << *lpGUID << ")" << endl
            << "lpDescription = " << lpDescription << endl
            << "lpDriverName = " << lpDriverName << endl;

        // Increment our callback count
        ptest->m_nCallbackCount++;

        // Make sure that we got a create-able GUID
        CComPointer<IDirectDraw> piDD;
        HRESULT hr;
        HINSTANCE hInstDDraw;
        PFNDDRAWCREATE pfnDDCreate = NULL;
        
        if(NULL==(hInstDDraw = LoadLibrary(TEXT("ddraw.dll"))))
        {
            dbgout << "No direct draw support on system";
            ptest->m_tr |=trSkip;
            return (HRESULT)DDENUMRET_OK;
        }

#ifdef UNDER_CE
        pfnDDCreate = (PFNDDRAWCREATE)GetProcAddress(hInstDDraw, TEXT("DirectDrawCreate"));
#else
        pfnDDCreate = (PFNDDRAWCREATE)GetProcAddress(hInstDDraw, "DirectDrawCreate");
#endif
        if(!pfnDDCreate)
        {
            dbgout << "Failure Creating a pointer to DirectDrawCreate" << endl;
            ptest->m_tr |= trSkip;
            goto Exit;
        }
        
        hr = pfnDDCreate(lpGUID, piDD.AsTestedOutParam(), NULL);
        if(DDERR_UNSUPPORTED==hr)
        {
            dbgout << "No direct draw support in driver";
            ptest->m_tr |=trSkip;
        }
        else QueryHRESULT(hr, "DirectDrawEnumerate Callback received non-creatable GUID", ptest->m_tr |= trFail)
        else QueryCondition(piDD.InvalidOutParam(), "DirectDrawEnumerate Callback received non-creatable GUID", ptest->m_tr |= trFail);

Exit:
        piDD.ReleaseInterface();
        FreeLibrary(hInstDDraw);

        // Even if we've failed, keep iterating (since we won't
        // loose the failure on the next iteration).
        return (HRESULT)DDENUMRET_OK;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_DirectDrawEnumerate::Test
    //  Tests the DirectDrawEnumerate API.
    //
    eTestResult CTest_DirectDrawEnumerate::Test()
    {
        HRESULT hr;

        HINSTANCE hInstDDraw;
        PFNDDEnum pfnDDEnum = NULL;
        
        if(NULL==(hInstDDraw = LoadLibrary(TEXT("ddraw.dll"))))
        {
            dbgout << "No direct draw support on system";
            return trSkip;
        }

#ifdef UNDER_CE
        pfnDDEnum = (PFNDDEnum)GetProcAddress(hInstDDraw, TEXT("DirectDrawEnumerateEx"));
#else
        pfnDDEnum = (PFNDDEnum)GetProcAddress(hInstDDraw, "DirectDrawEnumerateW");
#endif
        QueryCondition(!pfnDDEnum, "Creating a pointer to DirectDrawEnumerateEx", m_tr|=trFail; goto Exit);

        dbgout << "Calling DirectDrawEnumerateEx" << endl;
        
        m_fCallbackExpected = true;
        hr = pfnDDEnum((LPDDENUMCALLBACKEX)Callback, this, DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_DETACHEDSECONDARYDEVICES);
        m_fCallbackExpected = false;
        QueryHRESULT(hr, "DirectDrawEnumerate Failed", m_tr|=trFail; goto Exit);

        // Output the number of callbacks we got
        dbgout(LOG_DETAIL) << "Enumerated " << m_nCallbackCount << " device(s)" << endl;

        // Make sure the callback was called at least once
        QueryCondition(m_nCallbackCount < 1, "DirectDrawEnumerate failed to call callback", m_tr|=trFail);

Exit:
        FreeLibrary(hInstDDraw);
        return m_tr;
    }
}
