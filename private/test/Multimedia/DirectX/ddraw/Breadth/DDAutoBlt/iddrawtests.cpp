#include "main.h"

#include <DDrawUty.h>
#include <QATestUty/WinApp.h>

#include "IDDrawTests.h"
#include "IUnknownTests.h"

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

namespace Test_IDirectDraw
{
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_AddRefRelease::Test
    //  Tests the EnumDisplayModes Method.
    //
    eTestResult CTest_AddRefRelease::TestIDirectDraw()
    {
        CComPointer<IUnknown> punk;
        eTestResult tr = trPass;
        HRESULT hr;

        // Get the IUnknown Interface
        hr = m_piDD->QueryInterface(IID_IUnknown, (void**)punk.AsTestedOutParam());
        CheckHRESULT(hr, "QI for IUnknown", trFail);
        CheckCondition(punk.InvalidOutParam(), "QI succeeded, but didn't fill out param", trFail);

        tr |= Test_IUnknown::TestAddRefRelease(punk.AsInParam());

        return tr;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_QueryInterface::Test
    //  Tests the EnumDisplayModes Method.
    //
    eTestResult CTest_QueryInterface::TestIDirectDraw()
    {
        std::vector<IID> vectValidIIDs,
                         vectInvalidIIDs;
        CComPointer<IUnknown> punk;
        eTestResult tr = trPass;
        HRESULT hr;

        // Thi IIDs that should work
        vectValidIIDs.push_back(IID_IDirectDraw);

        // The IIDs that shouldn't work
        vectInvalidIIDs.push_back(IID_IDirect3D);
        vectInvalidIIDs.push_back(IID_IDirect3D2);
        vectInvalidIIDs.push_back(IID_IDirect3D3);
        vectInvalidIIDs.push_back(IID_IDirect3DDevice);
        vectInvalidIIDs.push_back(IID_IDirect3DDevice2);
        vectInvalidIIDs.push_back(IID_IDirect3DDevice3);
        vectInvalidIIDs.push_back(IID_IDirect3DTexture);
        vectInvalidIIDs.push_back(IID_IDirect3DTexture2);
        vectInvalidIIDs.push_back(IID_IDirectDrawSurface);
        vectInvalidIIDs.push_back(IID_IDirectDrawClipper);
        vectInvalidIIDs.push_back(IID_IDirectDrawColorControl);
        vectInvalidIIDs.push_back(IID_IDirectDrawGammaControl);

        // Get the IUnknown Interface
        hr = m_piDD->QueryInterface(IID_IUnknown, (void**)punk.AsTestedOutParam());
        CheckHRESULT(hr, "QI for IUnknown", trFail);
        CheckCondition(punk.InvalidOutParam(), "QI succeeded, but didn't fill out param", trFail);

        tr |= Test_IUnknown::TestQueryInterface(punk.AsInParam(), vectValidIIDs, vectInvalidIIDs);

        return tr;
    }
    
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

        //      According to bug 72593, this doesn't need to be creatable.
//        // Make sure that we got a create-able Surface Description
//        CComPointer<IDirectDrawSurface> piDDS;
//        HRESULT hr;
//
//        hr = ptest->m_piDD->CreateSurface(lpDDSurfaceDesc, piDDS.AsTestedOutParam(), NULL);
//        QueryHRESULT(hr, "EnumDisplayModes Callback received non-creatable GUID", ptest->m_tr |= trFail)
//        else QueryCondition(piDDS.InvalidOutParam(), "EnumDisplayModes Callback received non-creatable GUID", ptest->m_tr |= trFail);

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
        hr = m_piDD->EnumDisplayModes(0, NULL, this, Callback);
        m_fCallbackExpected = false;
        CheckHRESULT(hr, "EnumDisplayModes Failed", trFail);

        // Output the number of callbacks we got
        dbgout(LOG_DETAIL) << "Enumerated " << m_nCallbackCount << " modes(s)" << endl;

        // Make sure the callback was called at least once
        QueryCondition(m_nCallbackCount < 1, "EnumDisplayModes failed to call callback", trFail);

        return m_tr;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_SetCooperativeLevel::Test
    //  Tests the DirectDraw::SetCooperativeLevel API.
    //
    eTestResult CTest_SetCooperativeLevel::TestIDirectDraw()
    {
        eTestResult tr = trPass;
        HRESULT hr;
        RECT rc;

        // Save the window's original position, since setting to
        // fullscreen seems to resize the window to be fullscreen
        GetWindowRect(g_DirectDraw.m_hWnd, &rc);

        // Loop a couple of times for fun
        for (int i = 0; i < 2; i++)
        {
            // Set the cooperative level to Normal with NULL HWND
            dbgout << "Testing SetCooperativeLevel(NULL, DDSCL_NORMAL)" << endl;
            hr = m_piDD->SetCooperativeLevel(NULL, DDSCL_NORMAL);
            CheckHRESULT(hr, "SetCooperativeLevel", trFail);

            // Set the cooperative level to Normal with valid HWND
            dbgout << "Testing SetCooperativeLevel(HWND, DDSCL_NORMAL)" << endl;
            hr = m_piDD->SetCooperativeLevel(g_DirectDraw.m_hWnd, DDSCL_NORMAL);
            CheckHRESULT(hr, "SetCooperativeLevel", trFail);

            // Set the cooperative level to Exclusive with valid HWND
            dbgout << "Testing SetCooperativeLevel(HWND, DDSCL_FULLSCREEN)" << endl;
            hr = m_piDD->SetCooperativeLevel(g_DirectDraw.m_hWnd, DDSCL_FULLSCREEN);
            CheckHRESULT(hr, "SetCooperativeLevel", trFail);

            // Restore the display mode
            dbgout << "Testing RestoreDisplayMode" << endl;
            hr = m_piDD->RestoreDisplayMode();
            CheckHRESULT(hr, "RestoreDisplayMode", trFail);
            
        }

        // Restore the window's original position
        MoveWindow(g_DirectDraw.m_hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, false);
        
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetCaps::Test
    //  Tests the DirectDraw::GetCaps API.
    //
    eTestResult CTest_GetCaps::TestIDirectDraw()
    {
        CDDCaps ddcapsHAL1, 
                ddcapsHAL2;
        eTestResult tr = trPass;
        HRESULT hr;
        
        // HAL Only
        dbgout << "Geting Caps for HAL only" << endl;
        hr = m_piDD->GetCaps(&ddcapsHAL1, NULL);
        CheckHRESULT(hr, "GetCaps HAL Only", trFail);
        
        // Both HEL and HAL
        dbgout << "Geting Caps for HAL again" << endl;
        hr = m_piDD->GetCaps(&ddcapsHAL2, NULL);
        CheckHRESULT(hr, "GetCaps HAL second time", trFail);

        // Compare the results from the two calls
        dbgout << "Comparing results from different calls" << endl;
        QueryCondition(memcmp(&ddcapsHAL1, &ddcapsHAL2, sizeof(ddcapsHAL1)), "HAL CAPs differed between calls", tr |= trFail);

        // Display the caps that we recieved
        dbgout(LOG_DETAIL)
            << indent
            << "HAL Caps:" << ddcapsHAL1 << endl
            << unindent;

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_CreateSurface::Test
    //  Tests the CreateSurface Method.
    //
    namespace privCTest_CreateSurface
    {
        eTestResult CreateVerifySurface(
                                IDirectDraw * piDD,
                                CComPointer<IDirectDrawSurface> &piDDS,
                                CDDSurfaceDesc &cddsd,
                                TCHAR *pszDesc)
        {
            CDDSurfaceDesc cddsdLock;
            HRESULT hr;
            
            // Output the current surface description
            dbgout << "Attempting to create surface : " << pszDesc << endl;
            
            // Attempt to create a surface from the current
            // surface description.
            hr = piDD->CreateSurface(&cddsd, piDDS.AsTestedOutParam(), NULL);

            // Try creating a smaller surface if a TOOBIG error was thrown by the driver.
            if (DDERR_TOOBIGSIZE == hr || DDERR_TOOBIGWIDTH == hr || DDERR_TOOBIGHEIGHT == hr)
            {
                dbgout << "A " << g_ErrorMap[hr] << " (" << HEX((DWORD)hr) << ") was returned by CreateSurface." 
                       << endl
                       << "Retrying with a smaller surface." 
                       << endl;

                // On Ragexl, Creating Overlay surfaces in video memory with width > 720 returns DDERR_TOOBIGSIZE
                // Retry by halving the width and height.
                while(((cddsd.dwWidth/2) > 0 && (cddsd.dwHeight/2) > 0) && 
                      (DDERR_TOOBIGSIZE == hr || DDERR_TOOBIGWIDTH == hr || DDERR_TOOBIGHEIGHT == hr))
                {
                    cddsd.dwWidth/=2;
                    cddsd.dwHeight/=2;

                    hr = piDD->CreateSurface(&cddsd, piDDS.AsTestedOutParam(), NULL);
                }
            }

            if (DDERR_OUTOFVIDEOMEMORY == hr || DDERR_OUTOFMEMORY == hr)
            {
                return OutOfMemory_Helpers::VerifyOOMCondition(hr, piDD, cddsd);
            }
            else if ((DDERR_NOOVERLAYHW == hr || DDERR_INVALIDCAPS == hr) &&
                     ((cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) ||
                      !(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_OVERLAY)))
            {
                // If we got a no overlay hardware error, the test still passes as
                // long as either the surface is in system memory or if the HAL
                // doesn't support overlays.
                return trPass;
            }
            else if((DDERR_NOFLIPHW == hr) && 
                        (cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP) &&
                        (!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) ||
                        !(cddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)))
            {
                // if we got a no flip hw error, and we're creating a flipping surface, and both the 
                // HEL and HAL do not support flipping, then it's not an error.
                return trPass;
            }
            else if ((DDERR_UNSUPPORTEDFORMAT == hr) &&
                    (cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC) &&
                    !(cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
            {
                return trPass;
            }
            else if (DDERR_INVALIDCAPS == hr && 
                     (cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) && 
                     (cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
            {
                return trPass;
            }

            CheckHRESULT(hr, "CreateSurface", trFail);
            CheckCondition(piDDS.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", trFail);

            // Verify that the pointer is a valid surface
            dbgout << "Verifying that surface pointer is valid" << endl;
            hr = piDDS->IsLost();
            CheckHRESULT(hr, "IsLost", trFail);
/*            hr = piDDS->Lock(NULL, &cddsdLock, DDLOCK_WAIT, NULL);
            CheckHRESULT(hr, "Lock", trFail);
            hr = piDDS->Unlock(cddsdLock.lpSurface);
            CheckHRESULT(hr, "Unlock", trFail);*/

            return trPass;
        }
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_CreateSurface::Test
    //  Tests the CreateSurface Method.
    //
    eTestResult CTest_CreateSurface::TestIDirectDraw()
    {
        using namespace privCTest_CreateSurface;
        
        CDirectDrawConfiguration::VectSurfaceType::iterator itrPrimary,
                                                            itrTemp;
        CDirectDrawConfiguration::VectPixelFormat::iterator itrPixel;
        CComPointer<IDirectDrawSurface> piDDSPrimary,
                                        piDDSTemp;
        eTestResult tr = trPass;
        CDDSurfaceDesc cddsd;
        TCHAR szDescSurf[256];
        HRESULT hr;

        // Release any existing primary (since we'll be
        // creating primary surfaces
        g_DDSPrimary.ReleaseObject();

        // Iterate all supported Primary surface types
        dbgout << "Iterating all supported Primary surface descriptions" << endl;
        for (itrPrimary  = g_DDConfig.m_vstSupportedPrimary.begin();
             itrPrimary != g_DDConfig.m_vstSupportedPrimary.end();
             itrPrimary++)
        {
            // Get the description of the current mode
            hr = g_DDConfig.GetSurfaceDescData(*itrPrimary, cddsd, szDescSurf, countof(szDescSurf));
            ASSERT(SUCCEEDED(hr));
            
            // Test that we can create a functional primary
            tr |= CreateVerifySurface(m_piDD.AsInParam(), piDDSPrimary, cddsd, szDescSurf);

            // Iterate all supported video memory surfaces
            dbgout << "Iterating all video memory surfaces" << indent;
            for (itrTemp  = g_DDConfig.m_vstSupportedVidMem.begin();
                 itrTemp != g_DDConfig.m_vstSupportedVidMem.end();
                 itrTemp++)
            {
                for (itrPixel  = g_DDConfig.m_vpfSupportedVidMem.begin();
                     itrPixel != g_DDConfig.m_vpfSupportedVidMem.end();
                     itrPixel++)
                {
//                    if (SUCCEEDED(g_DDConfig.GetSurfaceDesc(*itrTemp, *itrPixel, cddsd, szDescSurf, countof(szDescSurf))))
                    hr = g_DDConfig.GetSurfaceDesc(*itrTemp, *itrPixel, cddsd, szDescSurf, countof(szDescSurf));
                    if (S_OK == hr)
                    {
                        tr |= CreateVerifySurface(m_piDD.AsInParam(), piDDSTemp, cddsd, szDescSurf);
                    }
                }
            }
            dbgout << unindent << "Done video memory surfaces" << endl;

            // Iterate all supported system memory surfaces
            dbgout << "Iterating all system memory surfaces" << indent;
            for (itrTemp  = g_DDConfig.m_vstSupportedSysMem.begin();
                 itrTemp != g_DDConfig.m_vstSupportedSysMem.end();
                 itrTemp++)
            {
                for (itrPixel  = g_DDConfig.m_vpfSupportedSysMem.begin();
                     itrPixel != g_DDConfig.m_vpfSupportedSysMem.end();
                     itrPixel++)
                {
//                    if (SUCCEEDED(g_DDConfig.GetSurfaceDesc(*itrTemp, *itrPixel, cddsd, szDescSurf, countof(szDescSurf))))
                    hr = g_DDConfig.GetSurfaceDesc(*itrTemp, *itrPixel, cddsd, szDescSurf, countof(szDescSurf));
                    if (S_OK == hr)
                    {
                        tr |= CreateVerifySurface(m_piDD.AsInParam(), piDDSTemp, cddsd, szDescSurf);
                    }
                }
            }
            dbgout << unindent << "Done system memory surfaces" << endl;
            
            // Release the interface (not really necessary,
            // since the next assignment will do it)
            piDDSPrimary.ReleaseInterface();
        }

        return tr;
    }
    
#if CFG_TEST_IDirectDrawClipper
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_CreateClipper::Test
    //  Tests the CreateClipper API.
    //
    eTestResult CTest_CreateClipper::TestIDirectDraw()
    {
        CComPointer<IDirectDrawClipper> piDDC;
        eTestResult tr = trPass;
        HRESULT hr;
        BOOL fChanged = FALSE;

        // Create the DirectDraw Object
        dbgout << "Calling IDirectDraw::CreateClipper" << endl;
        hr = m_piDD->CreateClipper(0, piDDC.AsTestedOutParam(), NULL);
        CheckHRESULT(hr, "IDirectDraw::CreateClipper", trFail);
        CheckCondition(piDDC.InvalidOutParam(), "CreateClipper succeeded but failed to set the OUT parameter", trFail);

        // Verify that the pointer is valid
        dbgout << "Initializing the created DirectDrawClipper Object" << endl;
        hr = piDDC->IsClipListChanged(&fChanged);
        dbgout << "Rerurned hr=" << g_ErrorMap[hr] << "  (" << HEX((DWORD)hr) << ")" << endl;
        CheckHRESULT(hr, "IDirectDrawClipper::IsClipListChanged", trFail);

        return tr;
    }
#endif // CFG_TEST_IDirectDrawClipper

    ////////////////////////////////////////////////////////////////////////////////
    // privCTest_CreatePalette
    //  Helper namespace for the CTest_CreatePalette tests.
    //
    namespace privCTest_CreatePalette
    {
        struct PaletteDesc
        {
            int nBits;
            DWORD dwFlags;
            int nLowerBound;
            int nUpperBound;
        };

        static PaletteDesc rgpd[] =
        {
            { 8, 0, 0, 256 }
        };
    };

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_CreatePalette::Test
    //  Tests the CreatePalette Method.
    //
    eTestResult CTest_CreatePalette::TestIDirectDraw()
    {
        using namespace privCTest_CreatePalette;
        
        CComPointer<IDirectDrawPalette> piDDP;
        PALETTEENTRY rgpe[257] = { 0 };
        eTestResult tr = trPass;
        CDDCaps ddcapsHEL, 
                ddcapsHAL;
        HRESULT hr;

        // Get the caps for the DDraw object
        dbgout << "Geting Caps for HAL" << endl;
        hr = m_piDD->GetCaps(&ddcapsHAL, NULL);
        CheckHRESULT(hr, "GetCaps Failed", trAbort);

        // Create all of the palette descriptions
        for(int i = 0; i < countof(rgpd); i++)
        {
            if((ddcapsHAL.ddsCaps.dwCaps) & rgpd[i].dwFlags)
            {
                dbgout
                    << "Calling IDirectDraw::CreatePalette with Flags = "
                    << g_CapsPalCapsFlagMap[rgpd[i].dwFlags] << endl;
                hr = m_piDD->CreatePalette(rgpd[i].dwFlags, rgpe, piDDP.AsTestedOutParam(), NULL);
                QueryHRESULT(hr, "IDirectDraw::CreatePalette", tr |= trFail)
                else QueryCondition(piDDP.InvalidOutParam(), "CreatePalette succeeded but failed to set the OUT parameter", tr |= trFail)
                else
                {
                    // Verify that the pointer is valid
                    DWORD dwCaps;
                    dbgout << "Initializing the created DirectDrawPalette Object" << endl;
                    hr = piDDP->GetCaps(&dwCaps);
                    QueryHRESULT(hr, "IDirectDrawPalette::GetCaps", tr |= trFail);
                }
            }
        }
        
        return tr;
    }
};

