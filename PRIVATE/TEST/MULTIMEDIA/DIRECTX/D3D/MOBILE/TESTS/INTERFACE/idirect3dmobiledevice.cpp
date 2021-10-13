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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "IDirect3DMobileDevice.h"
#include "DebugOutput.h"
#include "FormatTools.h"
#include "SurfaceTools.h"
#include "BufferTools.h"
#include "TextureTools.h"
#include "ColorTools.h"
#include "PSLRandomizer.h"
#include "QAMath.h"
#include "KatoUtils.h"
#include "d3dmtypes.h"
#include "d3dmddk.h"
#include "TestCases.h"
#include "D3DMStrings.h"
#include <tux.h>
#include <tchar.h>
#include <stdio.h>
#include "DebugOutput.h"

// needed for OpenWindow
#include "IDirect3DMobileSwapChain.h"
// kfuncs needed for CeGetCallerTrust.
#include <kfuncs.h>


//
// GUID for invalid parameter tests
//
DEFINE_GUID(IID_D3DQAInvalidInterface, 
0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

#define D3DMPRESENT_BACK_BUFFERS_MAX 3

//
// "Default" texture sizes for various operations
//
#define D3DQA_DEFAULT_COLORFILL_SURF_WIDTH 16
#define D3DQA_DEFAULT_COLORFILL_SURF_HEIGHT 16
#define D3DQA_DEFAULT_COPYRECTS_SURF_WIDTH  100
#define D3DQA_DEFAULT_COPYRECTS_SURF_HEIGHT 100
#define D3DQA_DEFAULT_STRETCHRECT_SMALLSURF_WIDTH  8
#define D3DQA_DEFAULT_STRETCHRECT_SMALLSURF_HEIGHT  8
#define D3DQA_DEFAULT_STRETCHRECT_BIGSURF_WIDTH  16
#define D3DQA_DEFAULT_STRETCHRECT_BIGSURF_HEIGHT  16
#define D3DQA_DEFAULT_UPDATETEX_TEX_WIDTH  8
#define D3DQA_DEFAULT_UPDATETEX_TEX_HEIGHT 8

//
// Defaults for SetTexture test
//
#define D3DQA_DEFAULT_SETTEXTURE_TEXWIDTH 4
#define D3DQA_DEFAULT_SETTEXTURE_TEXHEIGHT 4

#define D3DQA_MAX_TESTED_BACKBUFS 5
#define D3DQA_NUM_MULTISAMPLE_ENUMS 15

//
// Defaults for DrawIndexedPrimitive test
//
#define D3DQA_DEFAULT_INDEXBUF_SIZE 4

//
// Limits useful for iterating through FVF types
//
#define D3DQA_NUM_VALID_FVF_POSTYPES 4
#define D3DQA_NUM_VALID_FVF_NORMOPTIONS 2
#define D3DQA_NUM_VALID_FVF_LIGHTCOMBOS 4
#define D3DQA_NUM_VALID_FVF_MAXTEXCOORDSETTYPES 5
#define D3DQA_NUM_VALID_FVF_TEXCOORDCOUNTTYPES 3


// For invalid-VB-size testing, an iterated test shall
// attempt all sizes from zero to this, and verify that
// only the valid sizes result in successful VB creation
//
#define D3DQA_MAX_TESTED_INVALID_VB_SIZE 500

//
// Number of palette entries per D3DM palette
//
#define D3DQA_NUM_PALETTE_ENTRIES 256

//
// D3DM Spec indicates that a maximum of 1024 RECTs can be passed to Clear
//
#define D3DQA_MAX_CLEAR_RECTS 1024

//
// Maximum power to be used during SetMaterial testing
//
#define D3DQA_MAX_MATERIALPOWER 20.0f

//
// Number of levels to test for auto-mip-map-extents test
//
#define D3DMQA_AUTOMIPMAPEXTENTS_NUM_TESTED_LEVELS 5

//
// CreateTexture test will defer failure until below threshhold is reached
//
#define D3DMQA_CREATETEXTURE_MAXFAILURE_THRESHHOLD 25

//
// Strings for manually retrieving DLL exports
//
#define D3DMDLL L"D3DM.DLL"
#define D3DMCREATE L"Direct3DMobileCreate"

INT SetResult(INT & Result, INT NewResult)
{
    if (Result < NewResult)
    {
        Result = NewResult;
    }
    return Result;
}

template <class _T>
int SafeRelease(_T & pInterface)
{
    int ref = 0;
    if (pInterface)
    {
        ref = pInterface->Release();
        pInterface = NULL;
    }
    return ref;
}

//
//  Flush
//
//    Forces a flush of the Direct3D Mobile command buffer
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    none
//
HRESULT IDirect3DMobileDeviceTest::Flush()
{
    //
    // Argument for ValidateDevice (unused by this function)
    //
    DWORD dwScratch;

    m_pd3dDevice->ValidateDevice(&dwScratch);

    return S_OK;
}

//
//  SucceededCall
//  
//    Verify the success of a function call.  Intended for nested use.  Intended
//    for use with APIs that cause a command-token insertion, but no flush.
//
//  Arguments:
//
//    HRESULT hr:  Return value of API 
//
//  Return Value:
//
//    BOOL:  TRUE if API succeeded; FALSE otherwise
//
BOOL IDirect3DMobileDeviceTest::SucceededCall(HRESULT hrReturnValue)
{

    if (FAILED(Flush()))
    {
        DebugOut(DO_ERROR, L"Flush failed.");
        return FALSE;
    }

    if (FAILED(hrReturnValue))
    {
        DebugOut(DO_ERROR, L"API failed. (hr = 0x%08x)", hrReturnValue);
        return FALSE;
    }

    return TRUE;
}

//
// FailedCall
//
//   Wrapper for SucceededCall
//
//  Return Value:
//
//    BOOL:  TRUE if API failed; FALSE otherwise
//
BOOL IDirect3DMobileDeviceTest::FailedCall(HRESULT hrReturnValue)
{
    return (!(SucceededCall(hrReturnValue)));
}

//
//  IDirect3DMobileDeviceTest
//
//    Constructor for IDirect3DMobileDeviceTest class; however, most initialization is deferred.
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    none
//
IDirect3DMobileDeviceTest::IDirect3DMobileDeviceTest()
{
    m_bInitSuccess = FALSE;
}


//
// IDirect3DMobileDeviceTest
//
//   Initialize IDirect3DMobileDeviceTest object
//
// Arguments:
//
//   UINT uiAdapterOrdinal: D3DM Adapter ordinal
//   WCHAR *pSoftwareDevice: Pointer to wide character string identifying a D3DM software device to instantiate
//   BOOL bDebug: Running over debug middleware?
//   UINT uiTestCase:  Test case ordinal (to be displayed in window)
//
// Return Value
//
//   HRESULT indicates success or failure
//
HRESULT IDirect3DMobileDeviceTest::Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, BOOL bDebug, UINT uiTestCase)
{
    HRESULT hr;
    if (FAILED(hr = D3DMInitializer::Init(D3DQA_PURPOSE_RAW_TEST,                      // D3DQA_PURPOSE Purpose
                                     pWindowArgs,                                 // LPWINDOW_ARGS pWindowArgs
                                     pTestCaseArgs->pwchSoftwareDeviceFilename,   // TCHAR *ptchDriver
                                     uiTestCase)))                                // UINT uiTestCase
    {
        DebugOut(DO_ERROR, L"Aborting initialization, due to prior initialization failure. (hr = 0x%08x) Failing.", hr);
        return E_FAIL;
    }

    m_bDebugMiddleware = bDebug;

    m_bInitSuccess = TRUE;
    return S_OK;
}



//
//  IsReady
//
//    Accessor method for "initialization state" of IDirect3DMobileDeviceTest object.  "Half
//    initialized" states should not occur.
//  
//  Return Value:
//  
//    BOOL:  True if the object is initialized; FALSE if it is not.
//
BOOL IDirect3DMobileDeviceTest::IsReady()
{
    if (FALSE == m_bInitSuccess)
    {
        DebugOut(DO_ERROR, L"IDirect3DMobileDeviceTest is not ready.");
        return FALSE;
    }

    DebugOut(L"IDirect3DMobileDeviceTest is ready.");
    return D3DMInitializer::IsReady();
}


// 
// ~IDirect3DMobileDeviceTest
//
//  Destructor for IDirect3DMobileDeviceTest.  Currently; there is nothing
//  to do here.
//
IDirect3DMobileDeviceTest::~IDirect3DMobileDeviceTest()
{
    return; // Success
}


// 
// ExecuteDirect3DMobileCreateTest
//
//   Calls Direct3DMobileCreate export
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteDirect3DMobileCreateTest()
{
    HRESULT hr;
    //
    // Function pointer to Direct3DMobileCreate
    //
    LPDIRECT3DMOBILE (WINAPI* pfnDirect3DMobileCreate)(UINT SDKVersion);

    //
    // Module handle
    //
    HMODULE hD3DMDLL;

    DebugOut(L"Beginning ExecuteDirect3DMobileCreateTest.");

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // D3DM.DLL Module Handle
    //
    hD3DMDLL = GetModuleHandle(D3DMDLL);

    //
    // The address of the DLL's exported function indicates success. 
    // NULL indicates failure. 
    //
    (FARPROC&)pfnDirect3DMobileCreate = GetProcAddress(hD3DMDLL,    // HMODULE hModule, 
                                                       D3DMCREATE); // LPCWSTR lpProcName
    if (NULL == pfnDirect3DMobileCreate)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        DebugOut(DO_ERROR, L"GetProcAddress failed for Direct3DMobileCreate. (hr = 0x%08x) Aborting.", hr);
        return TPR_ABORT;
    }

    //
    // Bad parameter test #1:  Bad SDK version
    //
    if (NULL != pfnDirect3DMobileCreate( 0xFFFFFFFF ))
    {
        DebugOut(DO_ERROR, L"Direct3DMobileCreate succeeded unexpectedly. Failing.");
        return TPR_FAIL;
    }

    return TPR_PASS;

}

// 
// ExecuteDevicePresentationIntervalTest
//
//   Verifies that presentation interval support, as reported by the driver in
//   D3DMCAPS.PresentationIntervals, work as expected.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteDevicePresentationIntervalTest()
{
    HRESULT hr;
    DebugOut(L"Beginning ExecuteDevicePresentationIntervalTest.");

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Presentation parameters for device resets
    //
    D3DMPRESENT_PARAMETERS PresentParms;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Clear out presentation parameters structure
    //
    ZeroMemory( &PresentParms, sizeof(PresentParms) );

    //
    // Set up the presentation parameters that are desired for this 
    // testing scenario.
    //
    PresentParms.Windowed   = FALSE;
    PresentParms.SwapEffect = D3DMSWAPEFFECT_DISCARD;
    PresentParms.BackBufferFormat = Mode.Format;
    PresentParms.BackBufferCount = 1;
    PresentParms.BackBufferWidth = Mode.Width;
    PresentParms.BackBufferHeight = Mode.Height;


    //
    // Test:  Presentation Interval 1
    //
    PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_ONE;

    if (FAILED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        //
        // Was the failure unexpected?
        //

        if (D3DMPRESENT_INTERVAL_ONE & Caps.PresentationIntervals)
        {
            //
            // The device should have supported this Reset successfully
            //
            DebugOut(DO_ERROR, L"Fatal error at Reset; function failed when it was expected to succeed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }
    }
    else
    {
        //
        // Was the success unexpected?
        //

        if (!(D3DMPRESENT_INTERVAL_ONE & Caps.PresentationIntervals))
        {
            //
            // The device should not have supported this Reset successfully
            //
            DebugOut(DO_ERROR, L"Fatal error at Reset; function succeeded when it was expected to fail. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }
    }

    //
    // Test:  Presentation Interval 2
    //
    PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_TWO;

    if (FAILED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        //
        // Was the failure unexpected?
        //

        if (D3DMPRESENT_INTERVAL_TWO & Caps.PresentationIntervals)
        {
            //
            // The device should have supported this Reset successfully
            //
            DebugOut(DO_ERROR, L"Fatal error at Reset; function failed when it was expected to succeed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }
    }
    else
    {
        //
        // Was the success unexpected?
        //

        if (!(D3DMPRESENT_INTERVAL_TWO & Caps.PresentationIntervals))
        {
            //
            // The device should not have supported this Reset successfully
            //
            DebugOut(DO_ERROR, L"Fatal error at Reset; function succeeded when it was expected to fail. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }

    }

    //
    // Test:  Presentation Interval 3
    //
    PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_THREE;

    if (FAILED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        //
        // Was the failure unexpected?
        //

        if (D3DMPRESENT_INTERVAL_THREE & Caps.PresentationIntervals)
        {
            //
            // The device should have supported this Reset successfully
            //
            DebugOut(DO_ERROR, L"Fatal error at Reset; function failed when it was expected to succeed. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }
    else
    {
        //
        // Was the success unexpected?
        //

        if (!(D3DMPRESENT_INTERVAL_THREE & Caps.PresentationIntervals))
        {
            //
            // The device should not have supported this Reset successfully
            //
            DebugOut(DO_ERROR, L"Fatal error at Reset; function succeeded when it was expected to fail. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

    }

    //
    // Test:  Presentation Interval 4
    //
    PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_FOUR;

    if (FAILED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        //
        // Was the failure unexpected?
        //

        if (D3DMPRESENT_INTERVAL_FOUR & Caps.PresentationIntervals)
        {
            //
            // The device should have supported this Reset successfully
            //
            DebugOut(DO_ERROR, L"Fatal error at Reset; function failed when it was expected to succeed. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }
    else
    {
        //
        // Was the success unexpected?
        //

        if (!(D3DMPRESENT_INTERVAL_FOUR & Caps.PresentationIntervals))
        {
            //
            // The device should not have supported this Reset successfully
            //
            DebugOut(DO_ERROR, L"Fatal error at Reset; function succeeded when it was expected to fail. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }

    }


    //
    // Test:  Presentation Interval Immediate
    //
    PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_IMMEDIATE;

    if (FAILED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        //
        // Was the failure unexpected?
        //
        if (D3DMPRESENT_INTERVAL_IMMEDIATE & Caps.PresentationIntervals)
        {
            //
            // The device should have supported this Reset successfully
            //
            DebugOut(DO_ERROR, L"Fatal error at Reset; function failed when it was expected to succeed. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }
    else
    {
        //
        // Was the success unexpected?
        //
        if (!(D3DMPRESENT_INTERVAL_IMMEDIATE & Caps.PresentationIntervals))
        {
            //
            // The device should not have supported this Reset successfully
            //
            DebugOut(DO_ERROR, L"Fatal error at Reset; function succeeded when it was expected to fail. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }


    //
    // Test:  Presentation Interval Default
    //
    PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_DEFAULT;

    if (FAILED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        //
        // The "default" interval should never be invalid.
        //

        //
        // The device should have supported this Reset successfully
        //
        DebugOut(DO_ERROR, L"Fatal error at Reset; function failed when it was expected to succeed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}



// 
// ExecuteQueryInterfaceTest
//
//   Verifies the QueryInterface call.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteQueryInterfaceTest()
{
    HRESULT hr;
    DebugOut(L"Beginning ExecuteQueryInterfaceTest.");

    //
    // Interface pointer to retrieve via QueryInterface
    //
    LPDIRECT3DMOBILEDEVICE pQIDevice;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }


    //     
    // Bad parameter tests
    //     

    //
    // Test case #1:  Invalid REFIID
    //
    if (SUCCEEDED(hr = m_pd3dDevice->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
                                                    (void**)(&pQIDevice))))    // void** ppvObj
    {
        DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        pQIDevice->Release();
        return TPR_FAIL;
    }

    //
    // Test case #2:  Valid REFIID, invalid PPVOID
    //
    if (SUCCEEDED(hr = m_pd3dDevice->QueryInterface(IID_IDirect3DMobileDevice, // REFIID riid
                                                    (void**)0)))               // void** ppvObj
    {
        DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Test case #3:  Invalid REFIID, invalid PPVOID
    //
    if (SUCCEEDED(hr = m_pd3dDevice->QueryInterface(IID_D3DQAInvalidInterface, // REFIID riid
                                                    (void**)0)))               // void** ppvObj
    {
        DebugOut(DO_ERROR, L"QueryInterface succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }


    //
    // Valid parameter tests
    //

    //     
    // Perform an query on the surface interface
    //     
    if (FAILED(hr = m_pd3dDevice->QueryInterface(IID_IDirect3DMobileDevice, // REFIID riid
                                                 (void**)(&pQIDevice))))    // void** ppvObj
    {
        DebugOut(DO_ERROR, L"QueryInterface failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Release the QI'd IDirect3DMobileDevice interface pointer; verify that the ref count is still non-zero
    //
    if (!(pQIDevice->Release()))
    {
        DebugOut(DO_ERROR, L"Reference count for IDirect3DMobileDevice is zero; unexpected. Failing");
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}

// 
// ExecuteDeviceTestCooperativeLevelTest
//
//   Verifies the TestCooperativeLevel call.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteDeviceTestCooperativeLevelTest()
{
    DebugOut(L"Beginning ExecuteDeviceTestCooperativeLevelTest.");

    //
    // Return val for certain function calls
    //
    HRESULT hr;
    
    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // This test is here primarily for future expansion.  Currently, TestCooperativeLevel is
    // simple an E_NOTIMPL function; thus, we need not do anything to verify functionality.
    //
    hr = m_pd3dDevice->TestCooperativeLevel();
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"TestCooperativeLevel failed when no Fullscreen apps open. (hr = 0x%08x) Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Further testing is done in the MultipleInstance tests
    //
    
    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}



// 
// ExecuteGetAvailablePoolMemTest
//
//   Verifies the GetAvailableTextureMem call.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetAvailablePoolMemTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetAvailablePoolMemTest.");

    //
    // Storage for GetAvailablePoolMem return val
    //
    UINT uiVideoMem, uiSystemMem, uiManagedMem;
    
    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Query device for available texture memory
    //
    uiVideoMem = m_pd3dDevice->GetAvailablePoolMem(D3DMPOOL_VIDEOMEM);
    uiSystemMem = m_pd3dDevice->GetAvailablePoolMem(D3DMPOOL_SYSTEMMEM);
    uiManagedMem = m_pd3dDevice->GetAvailablePoolMem(D3DMPOOL_MANAGED);

    // To add:  checks for resource creation pool caps, confirmation
    // that there is some memory available in each pool that the driver claims
    // to support

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}

// 
// ExecuteResourceManagerDiscardBytesTest
//
//   Verifies the ResourceManagerDiscardBytes call.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteResourceManagerDiscardBytesTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteResourceManagerDiscardBytesTest.");

    //
    // Amount of memory, in bytes, to attempt to discard
    //
    DWORD dwDiscard;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Verify that managed pool is supported.
    //
    if (!(D3DMSURFCAPS_MANAGEDPOOL & Caps.SurfaceCaps))
    {
        //
        // If D3DMSURFCAPS_MANAGEDPOOL, skip main test, but still call function to verify that
        // it executes to completion in the non-managed-pool case.
        //
        m_pd3dDevice->ResourceManagerDiscardBytes(0);

        DebugOut(DO_ERROR, L"Skipping test for ResourceManagerDiscardBytes; D3DMSURFCAPS_MANAGEDPOOL not supported. Skipping.");
        return TPR_SKIP;
    }

    //
    // Attempt to discard all bytes
    //
    if (FAILED(hr = m_pd3dDevice->ResourceManagerDiscardBytes(0)))
    {
        DebugOut(DO_ERROR, L"ResourceManagerDiscardBytes failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Step through a wide range of non-zero args; all powers of two
    // between (2^0) and (2^31)
    //
    for (dwDiscard = 0x00000001; dwDiscard != 0x80000000; dwDiscard <<= 1)
    {
        //
        // D3DM_OK will be returned from this method so long as the number of bytes requested to
        // be flushed are available for allocation by the system at the call's return. The actual
        // number of bytes that have been flushed from the pool and are an implementation detail
        // of the driver. An error will be returned if the requested number of bytes to be discarded
        // is not available for allocation after the call has returned.
        //
        m_pd3dDevice->ResourceManagerDiscardBytes(dwDiscard);
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}



// 
// ExecuteGetDirect3DTest
//
//   Verifies the GetDirect3D call.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetDirect3DTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetDirect3DTest.");

    //
    // For retrieving the remaining reference count of the Direct3D object,
    // at release time.
    //
    ULONG ulRefCount;

    //
    // D3D Interface Pointer, to be retrieved by GetDirect3D
    //
    IDirect3DMobile *pD3D;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Bad parameter test #1:  NULL arg
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetDirect3D(NULL)))
    {
        DebugOut(DO_ERROR, L"GetDirect3D succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    if (FAILED(hr = m_pd3dDevice->GetDirect3D(&pD3D)))
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDirect3D; failed to retrieve interface pointer. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }
    else
    {
        //
        // Succeeded in retrieving interface pointer, as expected.
        //

        //
        // Now that the interface pointer has been retrieved, D3DM should have increased the 
        // reference count by one.
        //
        // We will immediately release this interface; leaving the following "expected"
        // references on the interface:
        //
        //    * Reference due to CreateDevice
        //    * Reference due to one active device instance
        //

        ulRefCount = pD3D->Release();

        //
        // If the remaining reference count is anything other than the expected value,
        // return a failure
        // 
        if (2 != ulRefCount)
        {
            DebugOut(DO_ERROR, L"Fatal error at IDirect3DMobile::Release; incorrect remaining reference count. Failing.");
            return TPR_FAIL;
        }
    }


    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}

// 
// ExecuteGetDeviceCapsTest
//
//   Verifies the GetDeviceCaps call.  Compares the output of the following
//   two APIs to confirm consistency:
//
//     * IDirect3DMobileDevice::GetDeviceCaps
//     * IDirect3DMobile::GetDeviceCaps
//
//   Also performs NULL-parameter checking on these APIs
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetDeviceCapsTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetDeviceCapsTest.");

    //
    // Direct3D Mobile device capabilities
    //
    //   * One set will be retrieved via the device interface pointer,
    //     the other set will be retrieved via the D3D object interface
    //     pointer
    //
    D3DMCAPS Caps1, Caps2;

    //
    // Direct3D Mobile device creation parameters.  Use to ensure that the
    // CAPs retrieved via the D3D Mobile object are the same set to be
    // retrieved via the device
    //
    D3DMDEVICE_CREATION_PARAMETERS Parameters;

    //
    // D3D Interface Pointer, to be retrieved by GetDirect3D
    //
    IDirect3DMobile *pD3D;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps1)))
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps; unable to retrieve CAPs. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    if (!(FAILED(hr = m_pd3dDevice->GetDeviceCaps(NULL))))
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps; function returned success despite NULL pointer (failure expected). (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    if (FAILED(hr = m_pd3dDevice->GetDirect3D(&pD3D)))
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDirect3D; failed to retrieve interface pointer. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    if (FAILED(hr = m_pd3dDevice->GetCreationParameters(&Parameters))) // D3DMDEVICE_CREATION_PARAMETERS *pParameters
    {
        DebugOut(DO_ERROR, L"Fatal error at GetCreationParameters; failed to retrieve creation information. (hr = 0x%08x) Failing", hr);
        pD3D->Release();
        return TPR_FAIL;
    }

    if (FAILED(hr = pD3D->GetDeviceCaps(  Parameters.AdapterOrdinal,// UINT Adapter,
                                          Parameters.DeviceType,    // D3DMDEVTYPE DeviceType,
                                         &Caps2)))                  // D3DMCAPS* pCaps)
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps; unable to retrieve CAPs. (hr = 0x%08x) Failing", hr);
        pD3D->Release();
        return TPR_FAIL;
    }

    if (!(FAILED(hr = pD3D->GetDeviceCaps(  Parameters.AdapterOrdinal,// UINT Adapter,
                                            Parameters.DeviceType,    // D3DMDEVTYPE DeviceType,
                                            NULL))))                  // D3DMCAPS* pCaps)
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps; function returned success despite NULL pointer (failure expected). (hr = 0x%08x) Failing", hr);
        pD3D->Release();
        return TPR_FAIL;
    }

    //
    // Verify that adapter ordinal and device type, reported in D3DMCAPS, is consistent
    // with info from GetCreationParamaters
    //
    if ((Parameters.AdapterOrdinal != Caps1.AdapterOrdinal) ||
        (Parameters.DeviceType != Caps1.DeviceType))
    {
        DebugOut(DO_ERROR, L"Fatal error.  D3DMCAPS from IDirect3DMobileDevice::GetDeviceCaps is not consistent with info from GetCreationParameters. Failing.");
        pD3D->Release();
        return TPR_FAIL;
    }

    //
    // Verify that adapter ordinal and device type, reported in D3DMCAPS, is consistent
    // with info from GetCreationParamaters
    //
    if ((Parameters.AdapterOrdinal != Caps2.AdapterOrdinal) ||
        (Parameters.DeviceType != Caps2.DeviceType))
    {
        DebugOut(DO_ERROR, L"Fatal error.  D3DMCAPS from IDirect3DMobile::GetDeviceCaps is not consistent with info from GetCreationParameters. Failing.");
        pD3D->Release();
        return TPR_FAIL;
    }

    //
    // Retval of zero implies Caps1 is identical to Caps2
    //
    if (0 != memcmp(&Caps1, &Caps2, sizeof(D3DMCAPS)))
    {
        DebugOut(DO_ERROR, L"Fatal error.  D3DMCAPS not reported identically for IDirect3DMobileDevice::GetDeviceCaps and IDirect3DMobile::GetDeviceCaps. Failing.");
        pD3D->Release();
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    pD3D->Release();
    return TPR_PASS;
}


// 
// ExecuteGetDisplayModeTest
//
//   Verifies the GetDisplayMode call.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetDisplayModeTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetDisplayModeTest.");

    //
    // System Metrics, as reported by OS
    //
    int SysMetricsScreenX;
    int SysMetricsScreenY;

    //
    // The display mode's spatial resolution, color resolution,
    // and refresh frequency.
    //
    D3DMDISPLAYMODE Mode;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    SysMetricsScreenX = GetSystemMetrics(SM_CXSCREEN);
    SysMetricsScreenY = GetSystemMetrics(SM_CYSCREEN);

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode; function returned failure. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    if (Mode.Width != (UINT)SysMetricsScreenX)
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode; D3DMDISPLAYMODE.Width is not equal to SM_CXSCREEN. Failing.");
        return TPR_FAIL;
    }

    if (Mode.Height != (UINT)SysMetricsScreenY)
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode; D3DMDISPLAYMODE.Height is not equal to SM_CYSCREEN. Failing.");
        return TPR_FAIL;
    }

    //
    // Note:  This test currently does not do anything to verify RefreshRate or Format.
    //

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}




// 
// ExecuteGetCreationParametersTest
//
//   Verifies the GetCreationParameters call.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetCreationParametersTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetCreationParametersTest.");

    //
    // Direct3D Mobile device creation parameters.  
    //
    //  * UINT            AdapterOrdinal;
    //  * D3DMDEVTYPE     DeviceType;
    //  * HWND            hFocusWindow;
    //  * ULONG           BehaviorFlags;
    //
    D3DMDEVICE_CREATION_PARAMETERS Parameters;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Verify GetCreationParameters by comparing output to values used during
    // initial device creation via CreateDevice
    //

    if (FAILED(hr = m_pd3dDevice->GetCreationParameters(&Parameters))) // D3DMDEVICE_CREATION_PARAMETERS *pParameters
    {
        DebugOut(DO_ERROR, L"Fatal error at GetCreationParameters; failed to retrieve creation information. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    if (m_hWnd != Parameters.hFocusWindow)
    {
        DebugOut(DO_ERROR, L"Fatal error; GetCreationParameters returned a different HWND than was used at device creation time. Failing");
        return TPR_FAIL;
    }

    if (m_DevType != Parameters.DeviceType)
    {
        DebugOut(DO_ERROR, L"Fatal error; GetCreationParameters returned a different D3DMDEVTYPE than was used at device creation time. Failing.");
        return TPR_FAIL;
    }

    if (m_uiAdapterOrdinal != Parameters.AdapterOrdinal)
    {
        DebugOut(DO_ERROR, L"Fatal error; GetCreationParameters returned a different adapter ordinal than was used at device creation time. Failing.");
        return TPR_FAIL;
    }

    if (m_dwBehaviorFlags != Parameters.BehaviorFlags)
    {
        DebugOut(DO_ERROR, L"Fatal error; GetCreationParameters returned different behavior flags than were used at device creation time. Failing.");
        return TPR_FAIL;
    }

    //
    // Bad-parameter tests
    //
    // Test #1:  Null pointer
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetCreationParameters(NULL))) // D3DMDEVICE_CREATION_PARAMETERS *pParameters
    {
        DebugOut(DO_ERROR, L"GetCreationParameters succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}


INT IDirect3DMobileDeviceTest::HelperCreateAdditionalSwapChainTest(D3DMPRESENT_PARAMETERS * pParams, BOOL bFailureExpected)
{
    INT Result = TPR_PASS;

    HRESULT hr;

    LPDIRECT3DMOBILESWAPCHAIN pSwapChain = NULL;

    hr = m_pd3dDevice->CreateAdditionalSwapChain(
        pParams,
        &pSwapChain);
    if (FAILED(hr))
    {
        if (!bFailureExpected && D3DMERR_MEMORYPOOLEMPTY != hr && E_OUTOFMEMORY != hr)
        {
            DebugOut(DO_ERROR, L"CreateAdditionalSwapChain failed when success was expected. (hr = 0x%08x) Failing.", hr);
            SetResult(Result, TPR_FAIL);
        }
    }
    else
    {
        hr = pSwapChain->Present(NULL, NULL, NULL, NULL);
        if (FAILED(hr) && !bFailureExpected && D3DMERR_MEMORYPOOLEMPTY != hr && E_OUTOFMEMORY != hr)
        {
            DebugOut(DO_ERROR, L"Present call on created swap chain failed when swap chain should be supported. (hr = 0x%08x) Failing.", hr);
            SetResult(Result, TPR_FAIL);
        }
    }

    if (SUCCEEDED(hr) && bFailureExpected)
    {
        DebugOut(DO_ERROR, L"Create and Present succeeded when passed invalid PresentationParams");
        SetResult(Result, TPR_FAIL);
    }

    if (TPR_PASS != Result)
    {
        DebugOut(L"Presentation Parameters Leading to Failing Case:");
        DebugOut(L"                    BackBufferWidth: %d", pParams->BackBufferWidth);
        DebugOut(L"                   BackBufferHeight: %d", pParams->BackBufferHeight);
        DebugOut(L"                   BackBufferFormat: 0x%08x", pParams->BackBufferFormat);
        DebugOut(L"                    BackBufferCount: %d", pParams->BackBufferCount);
        DebugOut(L"                    MultiSampleType: 0x%08x", pParams->MultiSampleType);
        DebugOut(L"                         SwapEffect: 0x%08x", pParams->SwapEffect);
        DebugOut(L"                      hDeviceWindow: 0x%08x", pParams->hDeviceWindow);
        DebugOut(L"                           Windowed: %d", pParams->Windowed);
        DebugOut(L"             EnableAutoDepthStencil: %d", pParams->EnableAutoDepthStencil);
        DebugOut(L"             AutoDepthStencilFormat: 0x%08x", pParams->AutoDepthStencilFormat);
        DebugOut(L"                              Flags: 0x%08x", pParams->Flags);
        DebugOut(L"         FullScreen_RefreshRateInHz: %d", pParams->FullScreen_RefreshRateInHz);
        DebugOut(L"    FullScreen_PresentationInterval: 0x%08x", pParams->FullScreen_PresentationInterval);
    }

    SafeRelease(pSwapChain);
    
    return Result;
}

// 
// ExecuteCreateAdditionalSwapChainTest
//
//   Verifies the CreateAdditionalSwapChain call.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteCreateAdditionalSwapChainTest()
{
    HRESULT hr;

    INT Result = TPR_PASS;

    DebugOut(L"Beginning ExecuteCreateAdditionalSwapChainTest.");

    //
    // Presentation parameters; so that there is a valid piece of memory
    // to pass to CreateAdditionalSwapChain
    //
    D3DMPRESENT_PARAMETERS PresentParams;

    //
    // Swap chain interface pointer to pass to CreateAdditionalSwapChain
    //
    IDirect3DMobileSwapChain* pSwapChain;

    //
    // Possible valid sizes to use when creating an additional swap chain
    // 
    const UINT WINDOWSIZE = 64;
    const UINT RANDOMSIZE = 0xfffffffe;
    const UINT RANDOMITERS = 10;
    // minimum, maximum, and max area
    const UINT RANDOMBOUNDS[3] = {1, 1024, 307200};
    UINT rgSizes[][2] = {
        1, 1,
        WINDOWSIZE, WINDOWSIZE,
        1, WINDOWSIZE,
        WINDOWSIZE, 1,
        RANDOMSIZE, RANDOMSIZE,
        WINDOWSIZE, RANDOMSIZE,
        RANDOMSIZE, WINDOWSIZE,
    };

    UINT BackBufferCount;
    D3DMMULTISAMPLE_TYPE MultiSampleType;
    D3DMSWAPEFFECT SwapEffect;
    BOOL DepthEnabled;
    D3DMFORMAT DepthFormat;
    BOOL bFoundValidFormat = FALSE;
    BOOL LockableBackbuffer;
    BOOL Windowed;
    D3DMDISPLAYMODE Mode;
    D3DMCAPS Caps;
    BOOL bFailureExpected;
    RECT rcWindow = {0, 0, WINDOWSIZE, WINDOWSIZE};
    HWND Window;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    hr = m_pd3dDevice->GetDeviceCaps(&Caps);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
        return TPR_ABORT;
    }

    hr = m_pd3dDevice->GetDisplayMode(&Mode);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"GetDisplayMode failed. (hr = 0x%08x) Aborting.", hr);
        return TPR_ABORT;
    }

    hr = SwapChainTest::OpenWindow(&rcWindow, &Window);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"OpenWindow failed. (hr = 0x%08x) Aborting.", hr);
        return TPR_ABORT;
    }

    // We now have a window open, so we can't exit without closing it.
    

    ZeroMemory(&PresentParams, sizeof(D3DMPRESENT_PARAMETERS));

    //
    // Attempt to identify a valid depth/stencil format
    //
    for (DepthFormat = (D3DMFORMAT)0; DepthFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&DepthFormat))++)
    {
        
        switch(DepthFormat)
        {
        case D3DMFMT_D32:
        case D3DMFMT_D15S1:
        case D3DMFMT_D24S8:
        case D3DMFMT_D16:
        case D3DMFMT_D24X8:
        case D3DMFMT_D24X4S4:

            //
            // Continue iteration; this format is a D/S format
            //
            break;
        default:

            //
            // Skip iteration; this format is not a D/S format
            //
            continue;
        }

        if (SUCCEEDED(hr = m_pD3D->CheckDepthStencilMatch(m_uiAdapterOrdinal,     // UINT Adapter
                                                     m_DevType,              // D3DMDEVTYPE DeviceType
                                                     Mode.Format,            // D3DMFORMAT AdapterFormat
                                                     Mode.Format,            // D3DMFORMAT RenderTargetFormat
                                                     DepthFormat)))        // D3DMFORMAT DepthStencilFormat
        {
            bFoundValidFormat = TRUE;
            break;
        }
    }

    //
    // Iterate over all the presentation possibilities.
    //
    for (BackBufferCount = 0; BackBufferCount <= Caps.MaxBackBuffer; ++BackBufferCount)
    {
        bFailureExpected = BackBufferCount >= Caps.MaxBackBuffer;
        PresentParams.BackBufferCount = BackBufferCount;

        for (MultiSampleType = D3DMMULTISAMPLE_NONE; (int)MultiSampleType <= D3DMMULTISAMPLE_16_SAMPLES; (*((UINT*)&MultiSampleType))++)
        {
            hr = m_pD3D->CheckDeviceMultiSampleType(
                m_uiAdapterOrdinal,
                m_DevType,
                Mode.Format,
                TRUE, // Windowed
                MultiSampleType);
            bFailureExpected = bFailureExpected || FAILED(hr);

            for (SwapEffect = D3DMSWAPEFFECT_DISCARD; (int)SwapEffect <= D3DMSWAPEFFECT_COPY_VSYNC; (*((UINT*)&SwapEffect))++)
            {
                bFailureExpected = bFailureExpected || (MultiSampleType != D3DMMULTISAMPLE_NONE && SwapEffect != D3DMSWAPEFFECT_DISCARD);
                bFailureExpected = bFailureExpected ||
                    (BackBufferCount > 1 &&
                    SwapEffect != D3DMSWAPEFFECT_FLIP &&
                    SwapEffect != D3DMSWAPEFFECT_DISCARD);

                for (DepthEnabled = 0; DepthEnabled < 2; DepthEnabled++)
                {
                    bFailureExpected = bFailureExpected ||
                        (DepthEnabled && !bFoundValidFormat);

                    for (LockableBackbuffer = 0; LockableBackbuffer < 2; LockableBackbuffer++)
                    {
                        bFailureExpected = bFailureExpected ||
                            (LockableBackbuffer && MultiSampleType != D3DMMULTISAMPLE_NONE);

                        for (Windowed = 0; Windowed < 2; Windowed++)
                        {
                            bFailureExpected = bFailureExpected || !Windowed;
                            // Valid non-random sizes
                            for (int i = 0; i < _countof(rgSizes); ++i)
                            {
                                UINT Width, Height;
                                UINT SizeIters = 1;
                                Width = rgSizes[i][0];
                                Height = rgSizes[i][1];
                                PresentParams.BackBufferFormat = DepthFormat;
                                PresentParams.BackBufferCount = BackBufferCount;
                                PresentParams.MultiSampleType = MultiSampleType;
                                PresentParams.SwapEffect = SwapEffect;
                                PresentParams.hDeviceWindow = Window;
                                PresentParams.Windowed = Windowed;
                                PresentParams.EnableAutoDepthStencil = DepthEnabled;
                                PresentParams.Flags = LockableBackbuffer ? D3DMPRESENTFLAG_LOCKABLE_BACKBUFFER : 0;
                                PresentParams.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_DEFAULT;

                                if (Width == RANDOMSIZE || Height == RANDOMSIZE)
                                {
                                    SizeIters = RANDOMITERS;
                                }

                                for (int Size = 0; Size < SizeIters; ++Size)
                                {
                                    do
                                    {
                                        // Reset
                                        Width = rgSizes[i][0];
                                        Height = rgSizes[i][1];

                                        if (RANDOMSIZE == Width)
                                        {
                                            Width = rand() % (RANDOMBOUNDS[1] - RANDOMBOUNDS[0]) + RANDOMBOUNDS[0];
                                        }
                                        if (RANDOMSIZE == Height)
                                        {
                                            Height = rand() % (RANDOMBOUNDS[1] - RANDOMBOUNDS[0]) + RANDOMBOUNDS[0];
                                        }
                                    } while (Width * Height > RANDOMBOUNDS[2]);

                                    PresentParams.BackBufferWidth = Width;
                                    PresentParams.BackBufferHeight = Height;

                                    SetResult(Result, HelperCreateAdditionalSwapChainTest(&PresentParams, bFailureExpected));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    DestroyWindow(Window);
    return Result;
}

// 
// ExecuteResetTest
//
//   Verifies that IDirect3DMobileDevice::Reset works.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteResetTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteResetTest.");

    //
    // Presentation parameters for device resets
    //
    D3DMPRESENT_PARAMETERS PresentParms;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // D3DMFORMAT iterator; to cycle through formats until a valid depth/stencil
    // format is found
    //
    D3DMFORMAT CurrentFormat;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Indicates whether the search for a valid depth/stencil format has been
    // successful or not
    //
    BOOL bFoundValidFormat = FALSE;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Test:  Provide D3DMPRESENT_PARAMETERS that should result in a
    // successful reset.
    //
    memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );
    if (FAILED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        DebugOut(DO_ERROR, L"Fatal error; Reset returned a failure. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter test #1:  Invalid back buffer format
    //
    memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );
    PresentParms.BackBufferFormat = D3DMFMT_A8;
    if (SUCCEEDED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        DebugOut(DO_ERROR, L"Reset succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter test #2:  Invalid presentation interval
    //
    memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );
    PresentParms.FullScreen_PresentationInterval = 0xFFFFFFFF;
    if (SUCCEEDED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        DebugOut(DO_ERROR, L"Reset succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter test #3:  Invalid flags
    //
    memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );
    PresentParms.Flags = 0xFFFFFFFF;
    if (SUCCEEDED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        DebugOut(DO_ERROR, L"Reset succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter test #4:  Invalid swap effect
    //
    memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );
    PresentParms.SwapEffect = (D3DMSWAPEFFECT)0xFFFFFFFF;
    if (SUCCEEDED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        DebugOut(DO_ERROR, L"Reset succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter test #5:  Invalid back buffer count
    //
    if (0 != Caps.MaxBackBuffer)
    {
        memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );
        PresentParms.BackBufferCount = Caps.MaxBackBuffer + 1;
        if (SUCCEEDED(hr = m_pd3dDevice->Reset(&PresentParms)))
        {
            DebugOut(DO_ERROR, L"Reset succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }
    else
    {
        DebugOut(DO_ERROR, L"Caps.MaxBackBuffer indicates no theoretical maximum backbuffer count.  Thus, MaxBackBuffer+1 bad-parameter test is skipping.");
    }

    //
    // Bad parameter test #6:  Invalid multisample / swap effect combo
    //
    memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );
    PresentParms.SwapEffect = D3DMSWAPEFFECT_FLIP;
    PresentParms.MultiSampleType = D3DMMULTISAMPLE_2_SAMPLES;
    if (SUCCEEDED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        DebugOut(DO_ERROR, L"Reset succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter test #7:  Invalid dimensions for full-screen reset
    //
    memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );
    PresentParms.BackBufferWidth = 0;
    PresentParms.BackBufferHeight = 0;
    PresentParms.Windowed = FALSE;
    if (SUCCEEDED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        DebugOut(DO_ERROR, L"Reset succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }
        
    //
    // Test:  Provide D3DMPRESENT_PARAMETERS that should result in a
    // successful reset.  Windowed with zero width/height should
    // autodetect width/height
    //
    memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );
    PresentParms.BackBufferWidth = 0;
    PresentParms.BackBufferHeight = 0;
    PresentParms.Windowed = TRUE;
    if (FAILED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        DebugOut(DO_ERROR, L"Reset failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Attempt to identify a valid depth/stencil format
    //
    for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {
        
        switch(CurrentFormat)
        {
        case D3DMFMT_D32:
        case D3DMFMT_D15S1:
        case D3DMFMT_D24S8:
        case D3DMFMT_D16:
        case D3DMFMT_D24X8:
        case D3DMFMT_D24X4S4:

            //
            // Continue iteration; this format is a D/S format
            //
            break;
        default:

            //
            // Skip iteration; this format is not a D/S format
            //
            continue;
        }

        if (SUCCEEDED(hr = m_pD3D->CheckDepthStencilMatch(m_uiAdapterOrdinal,     // UINT Adapter
                                                     m_DevType,              // D3DMDEVTYPE DeviceType
                                                     Mode.Format,            // D3DMFORMAT AdapterFormat
                                                     Mode.Format,            // D3DMFORMAT RenderTargetFormat
                                                     CurrentFormat)))        // D3DMFORMAT DepthStencilFormat
        {
            bFoundValidFormat = TRUE;
            break;
        }
    }

    //
    // Test:  Provide D3DMPRESENT_PARAMETERS that should result in a
    // successful reset.  Test of EnableAutoDepthStencil.
    //
    if (TRUE == bFoundValidFormat)
    {
        memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );
        PresentParms.EnableAutoDepthStencil = TRUE;
        PresentParms.AutoDepthStencilFormat = CurrentFormat;
        if (FAILED(hr = m_pd3dDevice->Reset(&PresentParms)))
        {
            DebugOut(DO_ERROR, L"Reset failed. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }

    //
    // Reset with each possible presentation interval options
    //
    memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );

    PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_ONE;
    m_pd3dDevice->Reset(&PresentParms);

    PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_TWO;
    m_pd3dDevice->Reset(&PresentParms);

    PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_THREE;
    m_pd3dDevice->Reset(&PresentParms);

    PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_FOUR;
    m_pd3dDevice->Reset(&PresentParms);

    PresentParms.FullScreen_PresentationInterval = D3DMPRESENT_INTERVAL_IMMEDIATE;
    m_pd3dDevice->Reset(&PresentParms);

    //
    // Reset with original presentation parameters
    //
    memcpy( &PresentParms, &m_d3dpp, sizeof(PresentParms) );
    if (FAILED(hr = m_pd3dDevice->Reset(&PresentParms)))
    {
        DebugOut(DO_ERROR, L"Reset failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
     return TPR_PASS;
}

//
// ExecutePresentTest
//
//   Verify IDirect3DMobileDevice::Present.
//
//   Note:
//
//      * Direct3D Mobile does not support the 3rd and 4th arguments
//        for the Present API (hDestWindowOverride, pDirtyRegion).  There
//        is not any value in exercising the non-NULL use of these arguments,
//        because the D3DM middleware does nothing with them except for
//        a debug-only value check that comes with a DEBUGMSG and
//        D3DMERR_INVALIDCALL.
//
//      * Some paths in ::Present are branched on the condition of whether
//        the target is windowed or fullscreen.  Both paths are exercised
//        herein.
//
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecutePresentTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecutePresentTest.");

    //
    // Presentation parameters for device creation
    //
    D3DMPRESENT_PARAMETERS PresentParms;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Windowed mode or full-screen?
    //
    UINT uiWindowedSetting;

    //
    // Provide a valid HWND or an invalid HWND?
    //
    UINT uiWindowValidSetting;

    //
    // Window to use to obscure the Present window
    //
    HWND hwndObscure = NULL;
    
    //
    // Rect to use for present region
    //
    CONST RECT rSample = {0,0,1,1};

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Bad parameter test #2: Invalid pDirtyRegion
    //
    if (SUCCEEDED(hr = m_pd3dDevice->Present(NULL,                   // CONST RECT* pSourceRect
                                             NULL,                   // CONST RECT* pDestRect
                                             NULL,                   // HWND hDestWindowOverride
                                             (RGNDATA*)0xFFFFFFFF))) // CONST RGNDATA* pDirtyRegion
    {
        DebugOut(DO_ERROR, L"Present succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter tests #3,#4: Non-NULL rects with Present for D3DMSWAPEFFECT_FLIP
    //
    m_d3dpp.SwapEffect = D3DMSWAPEFFECT_FLIP;
    m_d3dpp.MultiSampleType = D3DMMULTISAMPLE_NONE;
    if (FAILED(hr = m_pd3dDevice->Reset(&m_d3dpp)))
    {
        DebugOut(DO_ERROR, L"Reset failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }
    if (SUCCEEDED(hr = m_pd3dDevice->Present(&rSample, // CONST RECT* pSourceRect
                                             NULL,     // CONST RECT* pDestRect
                                             NULL,     // HWND hDestWindowOverride
                                             NULL)))   // CONST RGNDATA* pDirtyRegion
    {
        DebugOut(DO_ERROR, L"Present succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }
    if (SUCCEEDED(hr = m_pd3dDevice->Present(NULL,     // CONST RECT* pSourceRect
                                             &rSample, // CONST RECT* pDestRect
                                             NULL,     // HWND hDestWindowOverride
                                             NULL)))   // CONST RGNDATA* pDirtyRegion
    {
        DebugOut(DO_ERROR, L"Present succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    if (FAILED(hr = m_pd3dDevice->Present(NULL,   // CONST RECT* pSourceRect
                                          NULL,   // CONST RECT* pDestRect
                                          NULL,   // HWND hDestWindowOverride
                                          NULL))) // CONST RGNDATA* pDirtyRegion
    {
        DebugOut(DO_ERROR, L"Fatal error; Present was expected to succeed, yet it failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    if (0 != m_pd3dDevice->Release())
    {
        DebugOut(DO_ERROR, L"Fatal error; attempting to release device to recreate in fullscreen mode; release failed. Failing");
        return TPR_FAIL;
    }


    //
    // Iterate through all possible flip-modes, in both full-screen and windowed,
    // valid HWND and invalid HWND
    //

    //
    // There are two valid "windowed settings" to iterate through (full-screen, windowed)
    //
    for(uiWindowedSetting = 0; uiWindowedSetting < 2; uiWindowedSetting++)
    {

        D3DMSWAPEFFECT SwapStyle;

        //
        // There are four valid swap effects currently:
        //
        //   * D3DMSWAPEFFECT_DISCARD           = 1,
        //   * D3DMSWAPEFFECT_FLIP              = 2,
        //   * D3DMSWAPEFFECT_COPY              = 3,
        //   * D3DMSWAPEFFECT_COPY_VSYNC        = 4,
        //
        for (SwapStyle = D3DMSWAPEFFECT_DISCARD; SwapStyle <= D3DMSWAPEFFECT_COPY_VSYNC; (*(ULONG*)(&SwapStyle))++)
        {

            //
            // The first iteration will cause a invalid HWND to be used,
            // the second iteration will cause an valid HWND to be used.
            //
            for(uiWindowValidSetting = 0; uiWindowValidSetting < 2; uiWindowValidSetting++)
            {
                DWORD dwObscureSetting;

                //
                // There are three Obscuring settings
                //    0: The Present window is unobscured
                //    1: The Present window is fully obscured
                //    2: The Present window is partially obscured
                //
                for (dwObscureSetting = 0; dwObscureSetting < 3; dwObscureSetting++)
                {
                    if (dwObscureSetting)
                    {
                        RECT PresentWindowRect;
                        GetWindowRect(m_hWnd, &PresentWindowRect);
                        if (NULL == hwndObscure)
                        {
                            TCHAR lpClassName[260] = {0};
                            GetClassName(m_hWnd, lpClassName, sizeof(lpClassName)/sizeof(*lpClassName));
                            hwndObscure = CreateWindow( lpClassName,
                                                        _T("Present Obscurer"),
                                                        0,                          // dwStyle
                                                        0,                          // x
                                                        0,                          // y
                                                        1,                          // nWidth
                                                        1,                          // nHeight
                                                        NULL,                       // hWndParent
                                                        NULL,                       // hMenu
                                                        GetModuleHandle(NULL),      // hInstance
                                                        NULL );                     // lpParam
                            if (NULL == hwndObscure)
                            {
                                DebugOut(DO_ERROR, L"Unable to create window for obscuring. (GLE = %d) Aborting.", GetLastError());
                                return TPR_ABORT;
                            }
                        }

                        SetWindowPos( hwndObscure, 
                                      HWND_TOPMOST, 
                                      PresentWindowRect.left,
                                      PresentWindowRect.top,
                                      PresentWindowRect.right - PresentWindowRect.left,
                                      (PresentWindowRect.bottom - PresentWindowRect.top) / dwObscureSetting,
                                      SWP_SHOWWINDOW );
                        UpdateWindow( hwndObscure );
                    }
                    else
                    {
                        if (NULL != hwndObscure)
                        {
                            ShowWindow( hwndObscure, SW_HIDE );
                        }
                    }

                    //
                    // Clear out presentation parameters structure
                    //
                    ZeroMemory( &PresentParms, sizeof(PresentParms) );

                    //
                    // Set up the presentation parameters that are desired for this 
                    // testing scenario.
                    //
                    PresentParms.Windowed   = uiWindowedSetting ? FALSE : TRUE;
                    PresentParms.SwapEffect = SwapStyle;
                    PresentParms.BackBufferFormat = Mode.Format;
                    PresentParms.BackBufferCount = 1;
                    PresentParms.BackBufferWidth = Mode.Width;
                    PresentParms.BackBufferHeight = Mode.Height;

                    //
                    // Create the D3DDevice
                    //
                    if( FAILED(hr =  m_pD3D->CreateDevice( m_uiAdapterOrdinal,                   // UINT Adapter,
                                                           m_DevType,                            // D3DMDEVTYPE DeviceType,
                                                           uiWindowValidSetting ? m_hWnd : NULL, // HWND hFocusWindow,
                                                           m_dwBehaviorFlags,                    // ULONG BehaviorFlags,
                                                          &PresentParms,                         // D3DMPRESENT_PARAMETERS* pPresentationParameters,
                                                          &m_pd3dDevice )))                      // IDirect3DMobileDevice** ppReturnedDeviceInterface
                    {
                        //
                        // CreateDevice failed.  Was this expected?
                        //

                        if (uiWindowValidSetting)
                        {
                            //
                            // No, a valid HWND was provided, thus the failure is unexpected.
                            //
                            DebugOut(DO_ERROR, L"Failed at CreateDevice; success expected. (hr = 0x%08x) Failing", hr);
                            m_pd3dDevice = NULL;
                            return TPR_FAIL;
                        }
                        else
                        {
                            //
                            // Yes, an invalid HWND was provided, thus the failure is expected.
                            //
                            // The remainder of this iteration shall be skipped, because it exercises
                            // the present method of the device, which won't be valid for this iteration
                            // due to failed CreateDevice.
                            //
                            continue;
                        }
                    }
                    else
                    {
                        //
                        // CreateDevice succeeded.  Was this expected?
                        //

                        if (uiWindowValidSetting)
                        {
                            //
                            // Yes, a valid HWND was provided, thus the success is expected.
                            //
                        }
                        else
                        {
                            //
                            // No, an invalid HWND was provided, thus the success is unexpected.
                            //
                            // Note:  device cleanup will be handled by the parent object.
                            //

                            DebugOut(DO_ERROR, L"Fatal error;  CreateDevice succeeded when it was expected to fail. (hr = 0x%08x) Failing", hr);
                            return TPR_FAIL;
                        }
                    }

                    if (FAILED(hr = m_pd3dDevice->Present(NULL,   // CONST RECT* pSourceRect
                                                          NULL,   // CONST RECT* pDestRect
                                                          NULL,   // HWND hDestWindowOverride
                                                          NULL))) // CONST RGNDATA* pDirtyRegion
                    {
                        DebugOut(DO_ERROR, L"Fatal error; Present returned failure, with full-screen window as target. (hr = 0x%08x) Failing", hr);
                        return TPR_FAIL;
                    }

                    if (0 != m_pd3dDevice->Release())
                    {
                        DebugOut(DO_ERROR, L"Fatal error; attempting to release device to recreate in fullscreen mode; release failed. Failing.");
                        return TPR_FAIL;
                    }
                    else
                    {
                        m_pd3dDevice = NULL;
                    }
                }
            }
        }
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetBackBufferTest
//
//   Verify IDirect3DMobileDevice::GetBackBuffer
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetBackBufferTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetBackBufferTest.");

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // Presentation parameters for device creation
    //
    D3DMPRESENT_PARAMETERS PresentParms;

    //
    // Number of backbuffers specified at device creation time
    // 
    UINT uiNumActualBackBuffers;

    //
    // Ordinal of back-buffer to use for GetBackBuffer test
    //
    UINT uiBackBufferOrdinalToLock;

    //
    // Number of back buffers to test
    //
    UINT uiBackBuffersToTest;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Surface pointer for back buffer
    //
    IDirect3DMobileSurface* pBackBuffer;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted.");
        return TPR_SKIP;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }


    //
    // Bad parameter test #1:  Invalid back-buffer ordinal
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetBackBuffer(0xFFFFFFFF,                // UINT BackBuffer
                                                   D3DMBACKBUFFER_TYPE_MONO,  // D3DMBACKBUFFER_TYPE Type
                                                  &pBackBuffer)))             // IDirect3DMobileSurface** ppBackBuffer
    {
        DebugOut(DO_ERROR, L"GetBackBuffer succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        pBackBuffer->Release();
        return TPR_FAIL;
    }

    //
    // Bad parameter test #2:  Invalid D3DMBACKBUFFER_TYPE
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetBackBuffer(0,                               // UINT BackBuffer
                                                   (D3DMBACKBUFFER_TYPE)0xFFFFFFFF, // D3DMBACKBUFFER_TYPE Type
                                                  &pBackBuffer)))                   // IDirect3DMobileSurface** ppBackBuffer
    {
        DebugOut(DO_ERROR, L"GetBackBuffer succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        pBackBuffer->Release();
        return TPR_FAIL;
    }

    //
    // Bad parameter test #3:  Invalid IDirect3DMobileSurface**
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetBackBuffer(0,                         // UINT BackBuffer
                                                   D3DMBACKBUFFER_TYPE_MONO,  // D3DMBACKBUFFER_TYPE Type
                                                   NULL)))                    // IDirect3DMobileSurface** ppBackBuffer
    {
        DebugOut(DO_ERROR, L"GetBackBuffer succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        pBackBuffer->Release();
        return TPR_FAIL;
    }

    //
    // Handle "special case" of caps bit
    //
    if (0 == Caps.MaxBackBuffer)
        uiBackBuffersToTest = D3DQA_MAX_TESTED_BACKBUFS;
    else
        uiBackBuffersToTest = Caps.MaxBackBuffer;

    if (0 == m_pd3dDevice->Release())
    {
        m_pd3dDevice = NULL;
    }
    else
    {
        DebugOut(DO_ERROR, L"Fatal error; attempting to release device to recreate in fullscreen mode; release failed. Failing.");
        return TPR_FAIL;
    }


    for(uiNumActualBackBuffers = 0; uiNumActualBackBuffers < uiBackBuffersToTest; uiNumActualBackBuffers++)
    {

        for(uiBackBufferOrdinalToLock = 0; uiBackBufferOrdinalToLock < uiBackBuffersToTest; uiBackBufferOrdinalToLock++)
        {
            //
            // Prepare for this iteration, or for exit.
            //
            if (NULL != m_pd3dDevice)
            {
                m_pd3dDevice->Release();
                m_pd3dDevice = NULL;
            }
            
            //
            // Is the number of back buffers valid (e.g., should CreateDevice succeed)?
            //
            BOOL bValid = TRUE;

            //
            // Given the number of expected backbuffers, should this particular ordinal be valid
            // (or, is it out-of-range)?
            //
            BOOL bValidLock = TRUE;

            //
            // Check for out-of-range lock ordinal.  Notes:
            //
            //   * Ordinal is zero-based
            //   * If zero back-buffers are specified, the middleware
            //     will override this request (becomes one back buffer)
            //   * If changing the below code, watch out for UINT rollover
            //     for any case where subtraction from a UINT occurs.
            //
            if (0 == uiNumActualBackBuffers)
            {
                //
                // Handle special-case, where middleware automatically converts
                // an input of zero backbuffers into one backbuffer
                //

                if (uiBackBufferOrdinalToLock != 0)
                    bValidLock = FALSE;
            }
            else
            {
                //
                // Handle "regular" case, where number of specified buffers is
                // actually what occurs in the implementation
                //

                if (uiBackBufferOrdinalToLock > (uiNumActualBackBuffers-1))
                    bValidLock = FALSE;
            }

            //
            // Check for invalid back-buffer settings; (note: middleware automatically changes zero to one)
            //
            if (uiNumActualBackBuffers > uiBackBuffersToTest)
                bValid = FALSE;

            //
            // Clear out presentation parameters structure
            //
            ZeroMemory( &PresentParms, sizeof(PresentParms) );

            //
            // Set up the presentation parameters that are desired for this 
            // testing scenario.
            //
            PresentParms.Windowed   = TRUE;
            PresentParms.SwapEffect = D3DMSWAPEFFECT_DISCARD;
            PresentParms.BackBufferFormat = Mode.Format;
            PresentParms.BackBufferCount = uiNumActualBackBuffers;
            PresentParms.BackBufferWidth = Mode.Width;
            PresentParms.BackBufferHeight = Mode.Height;

            //
            // Create the D3DDevice
            //
            if( FAILED(hr =  m_pD3D->CreateDevice( m_uiAdapterOrdinal, // UINT Adapter,
                                                   m_DevType,          // D3DMDEVTYPE DeviceType,
                                                   m_hWnd,             // HWND hFocusWindow,
                                                   m_dwBehaviorFlags,  // ULONG BehaviorFlags,
                                                  &PresentParms,       // D3DMPRESENT_PARAMETERS* pPresentationParameters,
                                                  &m_pd3dDevice )))    // IDirect3DMobileDevice** ppReturnedDeviceInterface
            {
                //
                // CreateDevice failed.  If this wasn't expected, cause an error.
                //
                if (TRUE == bValid)
                {
                    DebugOut(DO_ERROR, L"Failed at CreateDevice; success expected. (hr = 0x%08x) Failing", hr);
                    m_pd3dDevice = NULL;
                    return TPR_FAIL;
                }

                //
                // Because this is an intentional failure condition, further testing on this
                // device cannot continue (device creation failed).
                //
                continue;
            }
            else
            {
                //
                // CreateDevice succeeded.  If this wasn't expected, cause an error.
                //
                if (FALSE == bValid)
                {
                    DebugOut(DO_ERROR, L"Succeeded at CreateDevice; failure expected. (hr = 0x%08x) Failing", hr);
                    m_pd3dDevice->Release();
                    m_pd3dDevice = NULL;
                    return TPR_FAIL;
                }
            }


            if (FAILED(hr = m_pd3dDevice->GetBackBuffer(uiBackBufferOrdinalToLock, // UINT BackBuffer
                                                        D3DMBACKBUFFER_TYPE_MONO,  // D3DMBACKBUFFER_TYPE Type
                                                       &pBackBuffer)))             // IDirect3DMobileSurface** ppBackBuffer
            {
                //
                // GetBackBuffer failed.  If this is unexpected, cause an error.
                //
                if (TRUE == bValidLock)
                {
                    DebugOut(DO_ERROR, L"Fatal error; GetBackBuffer failed, success was expected. (hr = 0x%08x) Failing", hr);
                    m_pd3dDevice->Release();
                    m_pd3dDevice = NULL;
                    return TPR_FAIL;
                }
            }
            else
            {
                //
                // GetBackBuffer succeeded.  If this is unexpected, cause an error.
                //
                // Regardless of whether success was expected or not, release the buffer.
                //
                pBackBuffer->Release();

                if (FALSE == bValidLock)
                {
                    DebugOut(DO_ERROR, L"Fatal error; GetBackBuffer succeeded, failure was expected. (hr = 0x%08x) Failing", hr);
                    m_pd3dDevice->Release();
                    m_pd3dDevice = NULL;
                    return TPR_FAIL;
                }
            }

        }
    }

    //
    // Clean up the device that we created.
    //
    m_pd3dDevice->Release();
    m_pd3dDevice = NULL;

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteCreateTextureTest
//
//   Verifies IDirect3DMobileDevice::CreateTexture by calling it
//   with each possible permutation of the following variables:
//
//      * D3DMPOOL of either system or video
//      * Extents specified by each of the CREATETEXTUREDATA entries
//      * Every enumerator in D3DMFORMAT
//
//   The expectation is that, if any of the above settings should
//   cause a failure, CreateTexture should fail.  Otherwise, 
//   CreateTexture should succeed.
//   
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteCreateTextureTest()
{
    DebugOut(L"Beginning ExecuteCreateTextureTest.");

    //
    // Bitflags to indicate specific characteristics of 
    // sample surface extent data
    //
    CONST dwNoRestrictions = 0x00000000;
    CONST dwNoSquareOnly   = 0x00000001;
    CONST dwNoPow2Only     = 0x00000002;

    //
    // Structure for pairing extent data with characteristics
    // information
    //
    typedef struct _CREATETEXTUREDATA
    {
        UINT  uiWidth;           // Tex Width
        UINT  uiHeight;          // Tex Height
        DWORD dwAutoLevel;       // Number of levels expected to be generated, if setting levels==0
        DWORD dwCharacteristics; // Restrictions regarding driver capability constraints
    } CREATETEXTUREDATA;

    //
    // Number of extent data sets
    //
    CONST UINT uiNumExtents = 32;

    //
    // Sample extent data, with corresponding characteristics
    // data.  Two members of the RECT structure are unused:
    // left and top.
    //
    CREATETEXTUREDATA Extents[] = 
    {
    //  | WIDTH | HEIGHT | LEVELS |   RESTRICTIONS
    //  +-------+--------+--------+-------------------
        {     1 ,      1 ,      1 , dwNoRestrictions                   },
        {     2 ,      2 ,      2 , dwNoRestrictions                   },
        {     4 ,      4 ,      3 , dwNoRestrictions                   },
        {     8 ,      8 ,      4 , dwNoRestrictions                   },
        {    16 ,     16 ,      5 , dwNoRestrictions                   },
        {    32 ,     32 ,      6 , dwNoRestrictions                   },
        {    64 ,     64 ,      7 , dwNoRestrictions                   },
        {   128 ,    128 ,      8 , dwNoRestrictions                   },
        {     3 ,      3 ,      0 , dwNoPow2Only                       },
        {     5 ,      5 ,      0 , dwNoPow2Only                       },
        {     6 ,      6 ,      0 , dwNoPow2Only                       },
        {     7 ,      7 ,      0 , dwNoPow2Only                       },
        {     9 ,      9 ,      0 , dwNoPow2Only                       },
        {    10 ,     10 ,      0 , dwNoPow2Only                       },
        {    11 ,     11 ,      0 , dwNoPow2Only                       },
        {    12 ,     12 ,      0 , dwNoPow2Only                       },
        {     1 ,      2 ,      0 , dwNoSquareOnly                     },
        {     2 ,      4 ,      0 , dwNoSquareOnly                     },
        {     4 ,      8 ,      0 , dwNoSquareOnly                     },
        {     8 ,     16 ,      0 , dwNoSquareOnly                     },
        {    16 ,     32 ,      0 , dwNoSquareOnly                     },
        {    32 ,     64 ,      0 , dwNoSquareOnly                     },
        {    64 ,    128 ,      0 , dwNoSquareOnly                     },
        {   128 ,    256 ,      0 , dwNoSquareOnly                     },
        {     3 ,      6 ,      0 , dwNoPow2Only | dwNoSquareOnly      },
        {     6 ,      9 ,      0 , dwNoPow2Only | dwNoSquareOnly      },
        {     9 ,     12 ,      0 , dwNoPow2Only | dwNoSquareOnly      },
        {    12 ,     15 ,      0 , dwNoPow2Only | dwNoSquareOnly      },
        {    15 ,     18 ,      0 , dwNoPow2Only | dwNoSquareOnly      },
        {    18 ,     21 ,      0 , dwNoPow2Only | dwNoSquareOnly      },
        {    21 ,     24 ,      0 , dwNoPow2Only | dwNoSquareOnly      },
        {    24 ,     27 ,      0 , dwNoPow2Only | dwNoSquareOnly      }
    };

    //
    // Texture interface pointer
    //
    IDirect3DMobileTexture *pTexture = NULL;

    //
    // API Results
    //
    HRESULT hr;

    //
    // Iterator for walking through extent data sets
    //
    UINT uiExtent;

    //
    // Pool for texture creation, according to current iteration
    //
    D3DMPOOL TexturePool;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // D3DMFORMAT iterator; to cycle through all formats
    //
    D3DMFORMAT CurrentFormat;

    //
    // CreateTexture will be attempted, even with arguments that should
    // not be successful.  Both cases will be verified:  if it should
    // succeed, success will be expected; if it should fail, failure will
    // be expected.
    //
    BOOL bFmtFailureExpected;        // Failure expected due to format
    BOOL bPoolFailureExpected;       // Failure expected due to D3DMPOOL
    BOOL bSizeCapsFailureExpected;   // Failure expected due to D3DMCAPS constraints on width/height
    BOOL bOverallFailureExpected;    // Is a failure is expected due to any of the above factors?
    BOOL bDXTnExtentFailureExpected; // Is a failure expected due to non-4x4 extents being used with DXTn?
    BOOL bMismatchThisIteration;     // Per-iteration failure tracking

    //
    // Test Result
    //
    INT Result = TPR_PASS;

    //
    // Test failure will be deferred, unless a defined threshhold is reached
    //
    UINT uiFailureCount = 0;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Check for texturing support, which is mandatory for this test
    //
    if (!(D3DMDEVCAPS_TEXTURE & Caps.DevCaps))
    {
        DebugOut(DO_ERROR, L"Textures not supported; skipping CreateTexture test. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }
    
    //
    // Determine if this driver supports either of the pools that are covered in this test
    //
    if (!((D3DMSURFCAPS_SYSTEXTURE & Caps.SurfaceCaps) || (D3DMSURFCAPS_VIDTEXTURE & Caps.SurfaceCaps)))
    {
        DebugOut(DO_ERROR, L"Driver has neither D3DMSURFCAPS_SYSTEXTURE nor D3DMSURFCAPS_VIDTEXTURE.  Error. Failing.");
        Result = TPR_FAIL;
        goto cleanup;
    }
    
    //
    // Bad parameter test #1:  Invalid interface pointer
    //
       if (SUCCEEDED(hr = m_pd3dDevice->CreateTexture( Extents[0].uiWidth,  // UINT Width,
                                                    Extents[0].uiHeight, // UINT Height,
                                                    1,                   // UINT Levels,
                                                    0,                   // DWORD Usage,
                                                    (D3DMFORMAT)0,       // D3DFORMAT Format,
                                                    D3DMPOOL_SYSTEMMEM,  // D3DMPOOL Pool,
                                                    NULL)))              // IDirect3DMobileTexture** ppTexture
    {
        DebugOut(DO_ERROR, L"CreateTexture succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }


    //
    // Verify debug-only check
    //
    if (m_bDebugMiddleware)
    {
        CurrentFormat = (D3DMFORMAT)D3DMFMT_UNKNOWN;
    }
    else
    {
        CurrentFormat = (D3DMFORMAT)D3DMFMT_R8G8B8;
    }

    //
    // Iterate through all formats in the D3DMFORMAT enumeration, seeking formats that
    // are valid as surfaces.
    //
    for (; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {
        bFmtFailureExpected = FALSE;

        if (FAILED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
                                                  m_DevType,          // D3DMDEVTYPE DeviceType
                                                  Mode.Format,        // D3DMFORMAT AdapterFormat
                                                  0,                  // ULONG Usage
                                                  D3DMRTYPE_TEXTURE,  // D3DMRESOURCETYPE RType
                                                  CurrentFormat)))    // D3DMFORMAT CheckFormat
        {
            //
            // This is not a valid format for surfaces
            //
            bFmtFailureExpected = TRUE;
        }

        for (TexturePool = D3DMPOOL_VIDEOMEM; TexturePool <= D3DMPOOL_SYSTEMMEM; (*(ULONG*)(&TexturePool))++)
        {
            bPoolFailureExpected = FALSE;

            if ((D3DMPOOL_VIDEOMEM == TexturePool) &&
                (!(D3DMSURFCAPS_VIDTEXTURE & Caps.SurfaceCaps)))
            {
                //
                // This iteration attempts texture creation in the video memory pool, which
                // is not supported by this driver.
                //
                bPoolFailureExpected = TRUE;
            }

            if ((D3DMPOOL_SYSTEMMEM == TexturePool) &&
                (!(D3DMSURFCAPS_SYSTEXTURE & Caps.SurfaceCaps)))
            {
                //
                // This iteration attempts texture creation in the system memory pool, which
                // is not supported by this driver.
                //
                bPoolFailureExpected = TRUE;
            }

            for (uiExtent = 0; uiExtent < uiNumExtents; uiExtent++)
            {
                bDXTnExtentFailureExpected = FALSE;

                //
                // Enforce extent limitations of DXTn
                //
                switch(CurrentFormat)
                {
                case D3DMFMT_DXT1:
                case D3DMFMT_DXT2:
                case D3DMFMT_DXT3:
                case D3DMFMT_DXT4:
                case D3DMFMT_DXT5:

                    //
                    // Is width on 4 pixel boundary?
                    //
                    if (Extents[uiExtent].uiWidth & 0x3)
                    {
                        bDXTnExtentFailureExpected = TRUE;
                    }

                    //
                    // Is height on 4 pixel boundary?
                    //
                    if (Extents[uiExtent].uiHeight & 0x3)
                    {
                        bDXTnExtentFailureExpected = TRUE;
                    }
                    
                    break;

                default:
                    break;
                }

                bSizeCapsFailureExpected = FALSE;

                //
                // Expect failure if width/height exceed stated capabilities
                //
                if (Caps.MaxTextureWidth < Extents[uiExtent].uiWidth)
                {
                    bSizeCapsFailureExpected = TRUE;
                }
                if (Caps.MaxTextureHeight < Extents[uiExtent].uiHeight)
                {
                    bSizeCapsFailureExpected = TRUE;
                }

                //
                // Expect failure if texture aspect ratio exceeds stated capabilities
                //
                // A MaxTextureAspectRatio of zero implies no restriction.
                //
                if (Caps.MaxTextureAspectRatio)
                {
                    //
                    // This driver does have an aspect ratio limitation.  Determine
                    // whether or not this iteration will exceed the maximums
                    //

                    if (Extents[uiExtent].uiWidth > Extents[uiExtent].uiHeight)
                    {
                        //
                        // This is a wide texture
                        //
                        if (Caps.MaxTextureAspectRatio < ((float)Extents[uiExtent].uiWidth / (float)Extents[uiExtent].uiHeight))
                        {
                            bSizeCapsFailureExpected = TRUE;
                        }
                    }
                    else if (Extents[uiExtent].uiHeight > Extents[uiExtent].uiWidth)
                    {
                        //
                        // This is a tall texture
                        //
                        if (Caps.MaxTextureAspectRatio < ((float)Extents[uiExtent].uiHeight / (float)Extents[uiExtent].uiWidth))
                        {
                            bSizeCapsFailureExpected = TRUE;
                        }
                    }
                    else
                    {
                        //
                        // This must be a square texture.  No aspect ratio limit can restrict such a case.
                        //
                    }
            
                }

                //
                // If this is a texture that does not conform to power-of-two width/height constraints (if mandated
                // by the driver), expect failure
                //
                if ((D3DMPTEXTURECAPS_POW2 & Caps.TextureCaps) &&
                    (Extents[uiExtent].dwCharacteristics & dwNoPow2Only))
                {
                    bSizeCapsFailureExpected = TRUE;
                }

                //
                // If this is a texture that does not conform to square width/height constraints (if mandated
                // by the driver), expect failure
                //
                if ((D3DMPTEXTURECAPS_SQUAREONLY & Caps.TextureCaps) &&
                    (Extents[uiExtent].dwCharacteristics & dwNoSquareOnly))
                {
                    bSizeCapsFailureExpected = TRUE;
                }

     
                //
                // Should CreateTexture succeed or fail?
                //
                if (bFmtFailureExpected || bPoolFailureExpected || bSizeCapsFailureExpected || bDXTnExtentFailureExpected)
                {
                    //
                    // Due to some established reason, CreateTexture should fail
                    //
                    bOverallFailureExpected = TRUE;
                }
                else
                {
                    //
                    // There is no known reason for a CreateTexture failure.
                    //
                    bOverallFailureExpected = FALSE;
                }

                   hr = m_pd3dDevice->CreateTexture( Extents[uiExtent].uiWidth,  // UINT Width,
                                                  Extents[uiExtent].uiHeight, // UINT Height,
                                                  1,                          // UINT Levels,
                                                  0,                          // DWORD Usage,
                                                  CurrentFormat,              // D3DFORMAT Format,
                                                  TexturePool,                // D3DMPOOL Pool,
                                                 &pTexture);                  // IDirect3DMobileTexture** ppTexture

                bMismatchThisIteration = FALSE;
                if (FAILED(hr) && (FALSE == bOverallFailureExpected))
                {
                    // Expected: Success; Actual: Failure
                    DebugOut(DO_ERROR, L"CreateTexture failed unexpectedly. (hr = 0x%08x). Failing.", hr);
                    bMismatchThisIteration = TRUE;
                    Result = TPR_FAIL;
                }
                else if (SUCCEEDED(hr) && (TRUE == bOverallFailureExpected))
                {
                    // Expected: Failure; Actual: Success
                    DebugOut(DO_ERROR, L"CreateTexture succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
                    bMismatchThisIteration = TRUE;
                    Result = TPR_FAIL;
                }
                else if (SUCCEEDED(hr) && (FALSE == bOverallFailureExpected))
                {
                    // Expected: Success; Actual: Success

                    //
                    // Prepare for next iteration (interface pointer will be clobbered)
                    //
                    pTexture->Release();
                    pTexture = NULL;
                }
                else if (FAILED(hr) && (TRUE == bOverallFailureExpected))
                {
                    // Expected: Failure; Actual: Failure

                    //
                    // No need to release interface (it does not exist)
                    //
                }

                //
                // Deferred failure from above, for the purpose of consolidating diagnostic debug spew code
                //
                if (bMismatchThisIteration)
                {
                    uiFailureCount++;

                    DebugOut(L"Test Parameters:");
                    DebugOut(L"");
                    DebugOut(L"     * D3DMFORMAT: %s",   D3DQA_D3DMFMT_NAMES[CurrentFormat]);
                    DebugOut(L"     * D3DMPOOL: %s",     D3DQA_D3DMPOOL_NAMES[TexturePool]);
                    DebugOut(L"     * Extents: (%u,%u)", Extents[uiExtent].uiWidth, Extents[uiExtent].uiHeight);
                    DebugOut(L"");
                    DebugOut(L"Expected Behavior:");
                    DebugOut(L"");
                    DebugOut(L"     * Failure due to D3DMFORMAT: %s",               bFmtFailureExpected        ? L"Yes" : L"No");
                    DebugOut(L"     * Failure due to D3DMPOOL: %s",                 bPoolFailureExpected       ? L"Yes" : L"No");
                    DebugOut(L"     * Failure due to capability bits: %s",          bSizeCapsFailureExpected   ? L"Yes" : L"No");
                    DebugOut(L"     * Failure due to DXTn extent restrictions: %s", bDXTnExtentFailureExpected ? L"Yes" : L"No");
                    DebugOut(L"");
                    DebugOut(L"");
                    DebugOut(L"Actual Behavior:");
                    DebugOut(L"");
                    DebugOut(L"     * CreateTexture result: %s (hr = 0x%08x)", FAILED(hr) ? L"Failed" : L"Succeeded", hr);
                    DebugOut(L"");
                    DebugOut(L"Expected behavior mismatches actual.  Test will fail.");
                    DebugOut(L"");

                    if (uiFailureCount > D3DMQA_CREATETEXTURE_MAXFAILURE_THRESHHOLD)
                    {
                        DebugOut(DO_ERROR, L"Returning with TPR_FAIL immediately because of number of test failures. Failing.");
                        goto cleanup;
                    }
                }
            }
        }
    }

cleanup:

    //
    // If texture exists, clean it up
    //
    if (pTexture)
        pTexture->Release();

    return Result;

}

//
// ExecuteCreateVertexBufferTest
//
//   Verify IDirect3DMobileDevice::CreateVertexBuffer
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteCreateVertexBufferTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteCreateVertexBufferTest.");

    //
    // VB size to attempt to create
    //
    UINT uiSizeOneVert;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // Valid pool for vertex buffers
    //
    D3DMPOOL VertexBufPool;

    //
    // Vertex buffer interface pointer
    //
    IDirect3DMobileVertexBuffer* pVertexBuffer = NULL;

    //
    // Pointer for buffer locks
    //
    BYTE *pByte;

    //
    // FVF Bits for vertex buffer creation
    //
    DWORD dwFVFBits;

    //
    // Test Result
    //
    INT Result = TPR_PASS;

    //
    // Iterators for various FVF combos
    //
    UINT uiPositionIter, uiNormalIter, uiLightingIter, uiTexCoordSets, uiTexCoordCount;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_SYSTEMMEM;
    }
    else if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_VIDEOMEM;
    }
    else
    {
        DebugOut(DO_ERROR, L"Fatal error; no pool supported for vertex buffers in D3DMCAPS.SurfaceCaps. Failing.");
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // There are currently four valid position types:
    //
    //  * D3DMFVF_XYZ_FLOAT   
    //  * D3DMFVF_XYZ_FIXED   
    //  * D3DMFVF_XYZRHW_FLOAT
    //  * D3DMFVF_XYZRHW_FIXED
    //
    for (uiPositionIter = 0; uiPositionIter < D3DQA_NUM_VALID_FVF_POSTYPES; uiPositionIter++)
    {
        //
        // There are currently five valid "normal" types:
        //
        //  * D3DMFVF_NORMAL_MASK      
        //  * D3DMFVF_NORMAL_NONE      
        //  * D3DMFVF_NORMAL_FLOAT     
        //  * D3DMFVF_NORMAL_16BITFIXED
        //  * D3DMFVF_NORMAL_8BITFIXED 
        //
        for (uiNormalIter = 0; uiNormalIter < D3DQA_NUM_VALID_FVF_NORMOPTIONS; uiNormalIter++)
        {
            //
            // Lighting can be any combination of the two types (D3DMFVF_DIFFUSE, D3DMFVF_SPECULAR),
            // including both, neither, or only one of them
            //
            for (uiLightingIter = 0; uiLightingIter < D3DQA_NUM_VALID_FVF_LIGHTCOMBOS; uiLightingIter++)
            {
                //
                // The number of texture sets can be any of the following:
                //
                //   * D3DMFVF_TEX0
                //   * D3DMFVF_TEX1
                //   * D3DMFVF_TEX2
                //   * D3DMFVF_TEX3
                //   * D3DMFVF_TEX4
                //
                for (uiTexCoordSets = 0; uiTexCoordSets < D3DQA_NUM_VALID_FVF_MAXTEXCOORDSETTYPES; uiTexCoordSets++)
                {

                    //
                    // The number of texture coordinates per set can be any of the following:
                    //
                    //   * D3DMFVF_TEXCOORDCOUNT1
                    //   * D3DMFVF_TEXCOORDCOUNT2
                    //   * D3DMFVF_TEXCOORDCOUNT3
                    //
                    for (uiTexCoordCount = 0; uiTexCoordCount < D3DQA_NUM_VALID_FVF_TEXCOORDCOUNTTYPES; uiTexCoordCount++)
                    {
                        dwFVFBits = 0;

                        //
                        // Format of position data
                        //
                        switch(uiPositionIter)
                        {
                        case 0:
                            dwFVFBits |= D3DMFVF_XYZ_FLOAT;
                            break;
                        case 1:
                            dwFVFBits |= D3DMFVF_XYZ_FIXED;
                            break;
                        case 2:
                            dwFVFBits |= D3DMFVF_XYZRHW_FIXED;
                            break;
                        case 3:
                            dwFVFBits |= D3DMFVF_XYZRHW_FLOAT;
                            break;
                        }

                        //
                        // Format of normal (or no normal at all)
                        //
                        // Note that D3DMFVF_NORMAL is only valid in D3DMFVF_XYZ_* vertex buffers, and is
                        // never valid in D3DMFVF_XYZRHW_* vertex buffers, so we only apply a normal if
                        // the outer loop has selected a D3DMFVF_XYZ_* vertex buffer.
                        // 
                        // Also, note that currently D3DMFVF_NORMAL_NONE is 0x00000000.
                        //
                        switch(uiNormalIter)
                        {
                        case 0:
                            dwFVFBits |= D3DMFVF_NORMAL_NONE;
                            break;
                        case 1:

                            //
                            // Beware, don't do bitwise AND with D3DMFVF_XYZ_FLOAT, it is zero-valued.
                            //
                            switch (D3DMFVF_POSITION_MASK & dwFVFBits)
                            {
                            case D3DMFVF_XYZ_FIXED:
                                dwFVFBits |= D3DMFVF_NORMAL_FIXED;
                                break;
                            case D3DMFVF_XYZ_FLOAT:
                                dwFVFBits |= D3DMFVF_NORMAL_FLOAT;
                                break;
                            case D3DMFVF_XYZRHW_FLOAT:
                            case D3DMFVF_XYZRHW_FIXED:
                                DebugOut(DO_ERROR, L"Omitting D3DMFVF_NORMAL_* setting because vertex is of type D3DMFVF_XYZRHW_*");
                                break;
                            }
                            break;
                        }

                        //
                        // Lighting type
                        //
                        switch(uiLightingIter)
                        {
                        case 0:
                            dwFVFBits |= D3DMFVF_DIFFUSE;
                            dwFVFBits |= D3DMFVF_SPECULAR;
                            break;
                        case 1:
                            dwFVFBits |= D3DMFVF_DIFFUSE;
                            break;
                        case 2:
                            dwFVFBits |= D3DMFVF_SPECULAR;
                            break;
                        case 3:
                            dwFVFBits |= 0;
                            break;
                        }

                        //
                        // Number of sets (of texture coordinates)
                        //
                        switch(uiTexCoordSets)
                        {
                        case 0:
                            dwFVFBits |= D3DMFVF_TEX0;
                            break;
                        case 1:
                            dwFVFBits |= D3DMFVF_TEX1;
                            break;
                        case 2:
                            dwFVFBits |= D3DMFVF_TEX2;
                            break;
                        case 3:
                            dwFVFBits |= D3DMFVF_TEX3;
                            break;
                        case 4:
                            dwFVFBits |= D3DMFVF_TEX4;
                            break;
                        }

                        //
                        // Dimension of tex coord set
                        //
                        switch(uiTexCoordCount)
                        {
                        case 0:
                            if (uiTexCoordSets >= 1)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE1(0);
                            if (uiTexCoordSets >= 2)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE1(1);
                            if (uiTexCoordSets >= 3)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE1(2);
                            if (uiTexCoordSets >= 4)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE1(3);
                            break;
                        case 1:
                            if (uiTexCoordSets >= 1)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE2(0);
                            if (uiTexCoordSets >= 2)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE2(1);
                            if (uiTexCoordSets >= 3)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE2(2);
                            if (uiTexCoordSets >= 4)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE2(3);
                            break;
                        case 2:
                            if (uiTexCoordSets >= 1)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE3(0);
                            if (uiTexCoordSets >= 2)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE3(1);
                            if (uiTexCoordSets >= 3)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE3(2);
                            if (uiTexCoordSets >= 4)
                                dwFVFBits |= D3DMFVF_TEXCOORDSIZE3(3);
                            break;
                        }

                        //
                        // Set the texture coordinate format to match the position's format.  Note that
                        // D3DMFVF_TEXCOORDFORMAT_FLOAT is zero (default unless specifically setting
                        // texture coordinates with D3DMFVF_TEXCOORDFIXED).
                        //
                        if (((D3DMFVF_POSITION_MASK & dwFVFBits) == D3DMFVF_XYZ_FIXED)     ||
                            ((D3DMFVF_POSITION_MASK & dwFVFBits) == D3DMFVF_XYZRHW_FIXED))
                        {
                            if (uiTexCoordSets >= 1)
                                dwFVFBits |= D3DMFVF_TEXCOORDFIXED(0);
                            if (uiTexCoordSets >= 2)
                                dwFVFBits |= D3DMFVF_TEXCOORDFIXED(1);
                            if (uiTexCoordSets >= 3)
                                dwFVFBits |= D3DMFVF_TEXCOORDFIXED(2);
                            if (uiTexCoordSets >= 4)
                                dwFVFBits |= D3DMFVF_TEXCOORDFIXED(3);
                        }

                        //
                        // Determine size of a one-vert buffer, for this FVF
                        //
                        uiSizeOneVert = BytesPerVertex(dwFVFBits);

                        //
                        // Create the vertex buffer
                        //
                        if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(uiSizeOneVert,    // UINT Length
                                                                         0,                // DWORD Usage
                                                                         dwFVFBits,        // DWORD FVF
                                                                         VertexBufPool,    // D3DMPOOL Pool
                                                                         &pVertexBuffer)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
                        {
                            DebugOut(DO_ERROR, L"FVF: 0x%08lx; CreateVertexBuffer failed, success expected. (hr = 0x%08x). Failing.", dwFVFBits, hr);
                            Result = TPR_FAIL;
                            continue;

                            //
                            // Don't go to cleanup; allow remainder of test to execute, to gather more debugging info
                            //
                        }

                        //
                        // Vertex buffer created successfully; attempt a lock.
                        //
                        if (FAILED(hr = pVertexBuffer->Lock(0,              // OffsetToLock
                                                            uiSizeOneVert,  // UINT SizeToLock
                                                            (VOID**)&pByte, // BYTE** ppbData
                                                            0)))            // DWORD Flags
                        {
                            DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Lock failed, success expected. (hr = 0x%08x) Failing", hr);
                            Result = TPR_FAIL;
                            goto cleanup;
                        }

                        //
                        // Vertex buffer locked successfully; attempt a unlock.
                        //
                        if (FAILED(hr = pVertexBuffer->Unlock()))
                        {
                            DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Unlock failed, success expected. (hr = 0x%08x) Failing", hr);
                            Result = TPR_FAIL;
                            goto cleanup;
                        }

                        //
                        // Nothing more to do with this vertex buffer
                        //
                        pVertexBuffer->Release();
                        pVertexBuffer=NULL;

                    }
                }
            }
        }
    }

    //
    // Bad parameter tests for CreateVertexBuffer
    //
    //
    // Test #1:  Zero-length buffer request
    //
    if (SUCCEEDED(hr = m_pd3dDevice->CreateVertexBuffer(0,                // UINT Length
                                                        0,                // DWORD Usage
                                                        D3DMFVF_XYZ_FLOAT,// DWORD FVF
                                                        VertexBufPool,    // D3DMPOOL Pool
                                                        &pVertexBuffer))) // IDirect3DMobileVertexBuffer** ppVertexBuffer
    {
        DebugOut(L"Fatal error; CreateVertexBuffer succeeded; failure expected: (hr = 0x%08x) Failing", hr);
        DebugOut(DO_ERROR, L"    0 length, 0 usage, FVF = D3DMFVF_XYZ_FLOAT.");
        Result = TPR_FAIL;
    }

    //
    // Bad parameter tests for CreateVertexBuffer
    //
    //
    // Test #2:  NULL Vertex buffer arg
    //
    if (SUCCEEDED(hr = m_pd3dDevice->CreateVertexBuffer(1,                // UINT Length
                                                        0,                // DWORD Usage
                                                        D3DMFVF_XYZ_FLOAT,// DWORD FVF
                                                        VertexBufPool,    // D3DMPOOL Pool
                                                        NULL)))           // IDirect3DMobileVertexBuffer** ppVertexBuffer
    {
        DebugOut(L"Fatal error; CreateVertexBuffer succeeded; failure expected: (hr = 0x%08x) Failing", hr);
        DebugOut(DO_ERROR, L"    1 length, 0 usage, FVF = D3DMFVF_XYZ_FLOAT, NULL vertex buffer pointer.");
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Test #3:  Create in invalid pool
    //
    if (!(D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps))
    {
        if (SUCCEEDED(hr = m_pd3dDevice->CreateVertexBuffer(sizeof(float)*3,  // UINT Length
                                                            0,                // DWORD Usage
                                                            D3DMFVF_XYZ_FLOAT,// DWORD FVF
                                                            D3DMPOOL_VIDEOMEM,// D3DMPOOL Pool
                                                            &pVertexBuffer))) // IDirect3DMobileVertexBuffer** ppVertexBuffer
        {
            DebugOut(L"Fatal error; CreateVertexBuffer succeeded; failure expected: (hr = 0x%08x) Failing", hr);
            DebugOut(DO_ERROR, L"    sizeof(float)*3 length, 0 usage, D3DMFVF_XYZ_FLOAT, unsupported pool (VIDEOMEM).");
            Result = TPR_FAIL;
            goto cleanup;
        }
    }


    //
    // Test #4:  Create in invalid pool
    //
    if (!(D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps))
    {
        if (SUCCEEDED(hr = m_pd3dDevice->CreateVertexBuffer(sizeof(float)*3,   // UINT Length
                                                            0,                 // DWORD Usage
                                                            D3DMFVF_XYZ_FLOAT, // DWORD FVF
                                                            D3DMPOOL_SYSTEMMEM,// D3DMPOOL Pool
                                                            &pVertexBuffer)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
        {
            DebugOut(L"Fatal error; CreateVertexBuffer succeeded; failure expected: (hr = 0x%08x) Failing", hr);
            DebugOut(DO_ERROR, L"    sizeof(float)*3 length, 0 usage, D3DMFVF_XYZ_FLOAT, unsupported pool (SYSTEMMEM).");
            Result = TPR_FAIL;
            goto cleanup;
        }
    }


    //
    // Test #5:  Invalid usage bits
    //
    if (SUCCEEDED(hr = m_pd3dDevice->CreateVertexBuffer( sizeof(float)*3,  // UINT Length
                                                         0xFFFFFFFF,       // DWORD Usage
                                                         D3DMFVF_XYZ_FLOAT,// DWORD FVF
                                                         VertexBufPool,    // D3DMPOOL Pool
                                                        &pVertexBuffer)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
    {
        DebugOut(L"Fatal error; CreateVertexBuffer succeeded; failure expected: (hr = 0x%08x) Failing", hr);
        DebugOut(DO_ERROR, L"    sizeof(float)*3 length, invalid usage (0xffffffff), D3DMFVF_XYZ_FLOAT.");
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Test #6:  Invalid FVF
    //
    if (SUCCEEDED(hr = m_pd3dDevice->CreateVertexBuffer( sizeof(float)*3,  // UINT Length
                                                         0,                // DWORD Usage
                                                         0xFFFFFFFF,       // DWORD FVF
                                                         VertexBufPool,    // D3DMPOOL Pool
                                                        &pVertexBuffer)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
    {
        DebugOut(L"Fatal error; CreateVertexBuffer succeeded; failure expected: (hr = 0x%08x) Failing", hr);
        DebugOut(DO_ERROR, L"    sizeof(float)*3 length, 0 usage, invalid FVF (0xffffffff).");
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Test #7:  Buffer size not a multiple of vertex size
    //
    if (SUCCEEDED(hr = m_pd3dDevice->CreateVertexBuffer( sizeof(float)*4,  // UINT Length
                                                         0,                // DWORD Usage
                                                         D3DMFVF_XYZ_FLOAT,// DWORD FVF
                                                         VertexBufPool,    // D3DMPOOL Pool
                                                        &pVertexBuffer)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
    {
        DebugOut(L"Fatal error; CreateVertexBuffer succeeded; failure expected: (hr = 0x%08x) Failing", hr);
        DebugOut(DO_ERROR, L"    invalid size (sizeof(float)*4), 0 usage, D3DMFVF_XYZ_FLOAT.");
        Result = TPR_FAIL;
        goto cleanup;
    }

cleanup:

    if (pVertexBuffer)
        pVertexBuffer->Unlock();

    if (pVertexBuffer)
        pVertexBuffer->Release();

    //
    // All failure conditions have already returned.
    //
    return Result;
}


//
// ExecuteCreateIndexBufferTest
//
//   Verify IDirect3DMobileDevice::CreateIndexBuffer
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteCreateIndexBufferTest()
{
    DebugOut(L"Beginning ExecuteCreateIndexBufferTest.");

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // D3DMFORMAT iterator; to cycle through all formats
    //
    D3DMFORMAT CurrentFormat;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Pool for index buffer creation, according to current iteration
    //
    D3DMPOOL IndexBufPool;

    //
    // Number of bytes to attempt, for index buffer creation
    //
    UINT uiNumBytes;

    //
    // Create index buffers from zero bytes to this many bytes in length
    //
    CONST UINT uiMaxBytesAttempted = 100;

    //
    // Interface pointer for index buffer
    //
    IDirect3DMobileIndexBuffer* pIndexBuffer;

    //
    // API results
    //
    HRESULT hr;

    //
    // CreateIndexBuffer will be attempted, even with arguments that should
    // not be successful.  Both cases will be verified:  if it should
    // succeed, success will be expected; if it should fail, failure will
    // be expected.
    //
    BOOL bPoolFailureExpected;     // Failure expected due to D3DMPOOL
    BOOL bLengthFailureExpected;   // Failure expected due proposed index buffer length
    BOOL bOverallFailureExpected;  // Is a failure is expected due to any of the above factors?

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Iterate through all the index buffer formats in the D3DMFORMAT enumeration
    //
    for (CurrentFormat = D3DMFMT_INDEX16; CurrentFormat <= D3DMFMT_INDEX32; (*(ULONG*)(&CurrentFormat))++)
    {
        for (IndexBufPool = D3DMPOOL_VIDEOMEM; IndexBufPool <= D3DMPOOL_SYSTEMMEM; (*(ULONG*)(&IndexBufPool))++)
        {
            bPoolFailureExpected = FALSE;

            if ((D3DMPOOL_VIDEOMEM == IndexBufPool) &&
                (!(D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)))
            {
                //
                // This iteration attempts texture creation in the video memory pool, which
                // is not supported by this driver.
                //
                bPoolFailureExpected = TRUE;
            }

            if ((D3DMPOOL_SYSTEMMEM == IndexBufPool) &&
                (!(D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)))
            {
                //
                // This iteration attempts texture creation in the system memory pool, which
                // is not supported by this driver.
                //
                bPoolFailureExpected = TRUE;
            }

            for(uiNumBytes = 0; uiNumBytes < uiMaxBytesAttempted; uiNumBytes++)
            {
                bLengthFailureExpected = FALSE;

                //
                // The length of the buffer must be divisable by the index size
                //
                if (D3DMFMT_INDEX16 == CurrentFormat)
                {
                    if (uiNumBytes % 2)
                    {
                        bLengthFailureExpected = TRUE;
                    }
                }
                else if (D3DMFMT_INDEX32 == CurrentFormat)
                {
                    if (uiNumBytes % 4)
                    {
                        bLengthFailureExpected = TRUE;
                    }
                }

                //
                // Zero-length buffers should not be allowed
                //
                if (0 == uiNumBytes)
                {
                    bLengthFailureExpected = TRUE;
                }

                //
                // Should CreateIndexBuffer succeed or fail?
                //
                if (bPoolFailureExpected || bLengthFailureExpected)
                {
                    //
                    // Due to some established reason, CreateIndexBuffer should fail
                    //
                    bOverallFailureExpected = TRUE;
                }
                else
                {
                    //
                    // There is no known reason for a CreateIndexBuffer failure.
                    //
                    bOverallFailureExpected = FALSE;
                }

                hr = m_pd3dDevice->CreateIndexBuffer(uiNumBytes,     // UINT Length
                                                     0,              // DWORD Usage
                                                     CurrentFormat,  // D3DMFORMAT Format
                                                     IndexBufPool,   // D3DMPOOL Pool
                                                    &pIndexBuffer);  // IDirect3DMobileIndexBuffer** ppIndexBuffer

                                                    
                if (FAILED(hr) && (FALSE == bOverallFailureExpected))
                {
                    // Expected: Success; Actual: Failure
                    DebugOut(DO_ERROR, L"Fatal error; CreateIndexBuffer failed unexpectedly. (hr = 0x%08x) Failing", hr);
                    return TPR_FAIL;
                }
                else if (SUCCEEDED(hr) && (TRUE == bOverallFailureExpected))
                {
                    // Expected: Failure; Actual: Success
                    DebugOut(DO_ERROR, L"Fatal error; CreateIndexBuffer succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
                    pIndexBuffer->Release();
                    return TPR_FAIL;
                }
                else if (SUCCEEDED(hr) && (FALSE == bOverallFailureExpected))
                {
                    // Expected: Success; Actual: Success

                    //
                    // Prepare for next iteration (interface pointer will be clobbered)
                    //
                    if (0 != pIndexBuffer->Release())
                    {
                        DebugOut(DO_ERROR, L"Fatal error; at Release time, the IDirect3DMobileIndexBuffer was expected to be at a refcount of zero. Failing.");
                        return TPR_FAIL;
                    }

                }
                else if (FAILED(hr) && (TRUE == bOverallFailureExpected))
                {
                    // Expected: Failure; Actual: Failure

                    //
                    // No need to release interface (it does not exist)
                    //
                }
            }
        }
    }


    //
    // Bad parameter test:  invalid format
    //
    if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
    {
        if (SUCCEEDED(hr = m_pd3dDevice->CreateIndexBuffer(4,                 // UINT Length
                                                           0,                 // DWORD Usage
                                                           D3DMFMT_UNKNOWN,   // D3DMFORMAT Format
                                                           D3DMPOOL_VIDEOMEM, // D3DMPOOL Pool
                                                          &pIndexBuffer)))    // IDirect3DMobileIndexBuffer** ppIndexBuffer
        {
            pIndexBuffer->Release();
            DebugOut(DO_ERROR, L"CreateIndexBuffer succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }
    else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
    {
        if (SUCCEEDED(hr = m_pd3dDevice->CreateIndexBuffer(4,                 // UINT Length
                                                           0,                 // DWORD Usage
                                                           D3DMFMT_UNKNOWN,   // D3DMFORMAT Format
                                                           D3DMPOOL_SYSTEMMEM,// D3DMPOOL Pool
                                                          &pIndexBuffer)))    // IDirect3DMobileIndexBuffer** ppIndexBuffer
        {
            pIndexBuffer->Release();
            DebugOut(DO_ERROR, L"CreateIndexBuffer succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }
    else
    {
        DebugOut(DO_ERROR, L"Device supports neither D3DMSURFCAPS_SYSINDEXBUFFER nor D3DMSURFCAPS_VIDINDEXBUFFER. Failing.");
        return TPR_FAIL;
    }


    //
    // Bad parameter test:  invalid usage bits
    //
    if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
    {
        if (SUCCEEDED(hr = m_pd3dDevice->CreateIndexBuffer(4,                 // UINT Length
                                                           0xFFFFFFFF,        // DWORD Usage
                                                           D3DMFMT_INDEX16,   // D3DMFORMAT Format
                                                           D3DMPOOL_VIDEOMEM, // D3DMPOOL Pool
                                                          &pIndexBuffer)))    // IDirect3DMobileIndexBuffer** ppIndexBuffer
        {
            pIndexBuffer->Release();
            DebugOut(DO_ERROR, L"CreateIndexBuffer succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }
    else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
    {
        if (SUCCEEDED(hr = m_pd3dDevice->CreateIndexBuffer(4,                 // UINT Length
                                                           0xFFFFFFFF,        // DWORD Usage
                                                           D3DMFMT_INDEX16,   // D3DMFORMAT Format
                                                           D3DMPOOL_SYSTEMMEM,// D3DMPOOL Pool
                                                          &pIndexBuffer)))    // IDirect3DMobileIndexBuffer** ppIndexBuffer
        {
            pIndexBuffer->Release();
            DebugOut(DO_ERROR, L"CreateIndexBuffer succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }
    else
    {
        DebugOut(DO_ERROR, L"Device supports neither D3DMSURFCAPS_SYSINDEXBUFFER nor D3DMSURFCAPS_VIDINDEXBUFFER. Failing.");
        return TPR_FAIL;
    }

    //
    // Bad parameter test:  Invalid IDirect3DMobileIndexBuffer*
    //
    if (SUCCEEDED(hr = m_pd3dDevice->CreateIndexBuffer(4,                 // UINT Length
                                                       0,                 // DWORD Usage
                                                       D3DMFMT_INDEX16,   // D3DMFORMAT Format
                                                       D3DMPOOL_SYSTEMMEM,// D3DMPOOL Pool
                                                       NULL)))            // IDirect3DMobileIndexBuffer** ppIndexBuffer
    {
        DebugOut(DO_ERROR, L"CreateIndexBuffer succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteCreateRenderTargetTest
//
//   Verify IDirect3DMobileDevice::CreateRenderTarget, by attempting it with all
//   permutations of the following variables:
//
//      * Lockable/Not Lockable
//      * All D3DMFORMATs
//
//   If CheckDeviceFormat indicates that the surface should be allowed, success
//   is expected at CreateRenderTarget time; otherwise, failure is expected.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteCreateRenderTargetTest()
{
    DebugOut(L"Beginning ExecuteCreateRenderTargetTest.");

    //
    // Surface interface pointer for render targets
    //
    IDirect3DMobileSurface* pRenderTargetSurface = NULL;

    //
    // D3DMFORMAT iterator; to cycle through all formats
    //
    D3DMFORMAT CurrentFormat;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Iterator for "lockability" states
    //
    UINT uiLockable;

    //
    // Return val for certain function calls
    //
    HRESULT hr;

    //
    // Does driver support lockable render targets?
    //
    BOOL bSupportsLockableRenderTarget;

    //
    // Test Result
    //
    INT Result = TPR_PASS;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // Storage for tracking expectation for CreateRenderTarget, and
    // actual result of CreateRenderTarget
    //
    BOOL bCheckDeviceFormatSuccess, bCapsSuccess, bExpectSuccess, bSuccess;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Does driver support lockable render targets?
    //
    bSupportsLockableRenderTarget = (D3DMSURFCAPS_LOCKRENDERTARGET & Caps.SurfaceCaps) ? TRUE : FALSE;

    //
    // Iterate through all formats in the D3DMFORMAT enumeration, seeking formats that
    // are valid as render targets.
    //
    for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {

        //
        // There are only two states for the lockable flag:  lock, don't lock
        //
        for (uiLockable = 0; uiLockable < 2; uiLockable++)
        {
            //
            // Indicators of current lock state to attempt
            //
            BOOL bLock;

            if (0 == uiLockable)
            {
                bLock = FALSE;
            }
            else if (1 == uiLockable)
            {
                bLock = TRUE;
            }
            else
            {
                DebugOut(DO_ERROR, L"Unexpected value for 'lockable' iterator.");
                continue;
            }

            //
            // Determine whether driver has reported that it will be able to create
            // this type of surface or not
            //
            if (SUCCEEDED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal,     // UINT Adapter
                                                         m_DevType,              // D3DMDEVTYPE DeviceType
                                                         Mode.Format,            // D3DMFORMAT AdapterFormat
                                                         D3DMUSAGE_RENDERTARGET, // ULONG Usage
                                                         D3DMRTYPE_SURFACE,      // D3DMRESOURCETYPE RType
                                                         CurrentFormat)))        // D3DMFORMAT CheckFormat
            {
                bCheckDeviceFormatSuccess = TRUE;
            }
            else
            {
                bCheckDeviceFormatSuccess = FALSE;
            }

            //
            // If expecting to create a lockable render target, without requisite capability bits,
            // failure is expected.  All three of the other permutations are OK:
            //
            //    * Creating non-lockable while running on driver with lockable support
            //    * Creating non-lockable while running on driver with no lockable support
            //    * Creating lockable while running on driver with lockable support
            //
            if (bLock && (!bSupportsLockableRenderTarget))
            {
                bCapsSuccess = FALSE;
            }
            else
            {
                bCapsSuccess = TRUE;
            }

            //
            // Only expect success if both prerequisites are satisfied
            //
            bExpectSuccess = FALSE;
            if (bCapsSuccess && bCheckDeviceFormatSuccess) 
                bExpectSuccess = TRUE;

            DebugOut(L"CreateRenderTarget Parameters:");
            DebugOut(L"");
            DebugOut(L"     * Extents: (%u,%u)", Mode.Width, Mode.Height);
            DebugOut(L"     * D3DMFORMAT: %s",   D3DQA_D3DMFMT_NAMES[CurrentFormat]);
            DebugOut(L"     * D3DMMULTISAMPLETYPE:  D3DMMULTISAMPLE_NONE");
            DebugOut(L"     * Lockable?: %s",    bLock ? L"TRUE" : L"FALSE");
            DebugOut(L"");
            DebugOut(L"Test Queries:");
            DebugOut(L"     * Driver supports D3DMSURFCAPS_LOCKRENDERTARGET?: %s",    bSupportsLockableRenderTarget ? L"TRUE" : L"FALSE");
            DebugOut(L"     * CheckDeviceFormat succeeded?: %s",    bCheckDeviceFormatSuccess ? L"TRUE" : L"FALSE");
            DebugOut(L"");
            DebugOut(L"Expected Behavior: %s",   bExpectSuccess ? L"SUCCESS" : L"FAILURE");
            DebugOut(L"");

            //
            // Attempt the surface creation
            //
            hr = m_pd3dDevice->CreateRenderTarget(Mode.Width,           // UINT Width
                                                  Mode.Height,          // UINT Height
                                                  CurrentFormat,        // D3DMFORMAT Format
                                                  D3DMMULTISAMPLE_NONE, // D3DMMULTISAMPLE_TYPE MultiSample
                                                  bLock,                // BOOL Lockable
                                                 &pRenderTargetSurface);// IDirect3DMobileSurface** ppSurface

            bSuccess = SUCCEEDED(hr);

            DebugOut(L"Actual Behavior: %s (hr = 0x%08x)",   bSuccess ? L"SUCCESS" : L"FAILURE", hr);
            DebugOut(L"");

            //
            // Release the surface, if it needs to be released
            //
            if (pRenderTargetSurface)
            {
                pRenderTargetSurface->Release();
                pRenderTargetSurface = NULL;
            }

            //
            // Was the CreateRenderTarget result as expected?
            //
            if ((!bExpectSuccess && bSuccess) || (bExpectSuccess && !bSuccess))
            {
                DebugOut(DO_ERROR, L"Unexpected CreateRenderTarget result. Failing.");
                Result = TPR_FAIL;
                goto cleanup;
            }
        }
    }

    //
    // Bad parameter test #1:  Invalid IDirect3DMobileSurface*
    //
    if (SUCCEEDED(hr = m_pd3dDevice->CreateRenderTarget(Mode.Width,           // UINT Width
                                                        Mode.Height,          // UINT Height
                                                        D3DMFMT_R8G8B8,       // D3DMFORMAT Format
                                                        D3DMMULTISAMPLE_NONE, // D3DMMULTISAMPLE_TYPE MultiSample
                                                        FALSE,                // BOOL Lockable
                                                        NULL)))               // IDirect3DMobileSurface** ppSurface
    {
        DebugOut(DO_ERROR, L"CreateRenderTarget succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

cleanup:

    if (pRenderTargetSurface)
        pRenderTargetSurface->Release();

    return Result;

}


//
// ExecuteCreateDepthStencilSurfaceTest
//
//   Verify IDirect3DMobileDevice::CreateDepthStencilSurface, by attempting it 
//   with all D3DMFORMATS.
//
//   If CheckDepthStencilMatch indicates that the surface should be allowed, success
//   is expected at CheckDepthStencilMatch time; otherwise, failure is expected.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteCreateDepthStencilSurfaceTest()
{
    DebugOut(L"Beginning ExecuteCreateDepthStencilSurfaceTest.");

    //
    // Surface interface pointer for depth/stencils
    //
    IDirect3DMobileSurface* pSurface;

    //
    // D3DMFORMAT iterator; to cycle through all formats
    //
    D3DMFORMAT CurrentFormat;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Return val for certain function calls
    //
    HRESULT hr;

    //
    // Storage for tracking expectation for CreateDepthStencilSurface, and
    // actual result of CreateDepthStencilSurface
    //
    BOOL bExpectSuccess, bSuccess;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Iterate through all formats in the D3DMFORMAT enumeration
    //
    for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {

        //
        // Determine whether driver has reported that it will be able to create
        // this type of surface or not
        //
        if (SUCCEEDED(hr = m_pD3D->CheckDepthStencilMatch(m_uiAdapterOrdinal, // UINT Adapter
                                                          m_DevType,          // D3DMDEVTYPE DeviceType
                                                          Mode.Format,        // D3DMFORMAT AdapterFormat
                                                          Mode.Format,        // D3DMFORMAT RenderTargetFormat
                                                          CurrentFormat)))    // D3DMFORMAT DepthStencilFormat
        {
            bExpectSuccess = TRUE;
        }
        else
        {
            bExpectSuccess = FALSE;
        }

        //
        // Attempt the surface creation
        //
        hr = m_pd3dDevice->CreateDepthStencilSurface(Mode.Width,           // UINT Width
                                                     Mode.Height,          // UINT Height
                                                     CurrentFormat,        // D3DMFORMAT Format
                                                     D3DMMULTISAMPLE_NONE, // D3DMMULTISAMPLE_TYPE MultiSample
                                                    &pSurface);            // IDirect3DMobileSurface** ppSurface

        bSuccess = SUCCEEDED(hr);


        //
        // Was the CreateDepthStencilSurface result as expected?
        //
        if (bExpectSuccess && bSuccess)
        {
            DebugOut(L"CreateDepthStencilSurface complete.  Expected: success; Actual: success.");
            pSurface->Release();
        }
        else if (!bExpectSuccess && !bSuccess)
        {
            DebugOut(L"CreateDepthStencilSurface complete.  Expected: failure; Actual: failure.");
        }
        else if (!bExpectSuccess && bSuccess)
        {
            DebugOut(DO_ERROR, L"CreateDepthStencilSurface complete.  Expected: failure; Actual: success. (hr = 0x%08x) Failing", hr);
            pSurface->Release();
            return TPR_FAIL;
        }
        else // only one remaining condition: (bExpectSuccess && !bSuccess)
        {
            DebugOut(DO_ERROR, L"CreateDepthStencilSurface complete.  Expected: success; Actual: failure. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteCreateImageSurfaceTest
//
//   Verify IDirect3DMobileDevice::CreateImageSurface
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteCreateImageSurfaceTest()
{
    DebugOut(L"Beginning ExecuteCreateImageSurfaceTest.");

    //
    // Surface interface pointer for image surfaces
    //
    IDirect3DMobileSurface* pSurface;

    //
    // D3DMFORMAT iterator; to cycle through all formats
    //
    D3DMFORMAT CurrentFormat;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Return val for certain function calls
    //
    HRESULT hr;

    //
    // Storage for tracking expectation for CreateImageSurface, and
    // actual result of CreateImageSurface
    //
    BOOL bExpectSuccess, bSuccess;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Iterate through all formats in the D3DMFORMAT enumeration
    //
    for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {

        //
        // Determine whether driver has reported that it will be able to create
        // this type of surface or not
        //
        if (SUCCEEDED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
                                                     m_DevType,          // D3DMDEVTYPE DeviceType
                                                     Mode.Format,        // D3DMFORMAT AdapterFormat
                                                     0,                  // ULONG Usage
                                                     D3DMRTYPE_SURFACE,  // D3DMRESOURCETYPE RType
                                                     CurrentFormat)))    // D3DMFORMAT CheckFormat
        {
            bExpectSuccess = TRUE;
        }
        else
        {
            bExpectSuccess = FALSE;
        }

        //
        // Attempt the surface creation
        //
        hr = m_pd3dDevice->CreateImageSurface(Mode.Width,           // UINT Width
                                              Mode.Height,          // UINT Height
                                              CurrentFormat,        // D3DMFORMAT Format
                                             &pSurface);            // IDirect3DMobileSurface** ppSurface
                                            

        bSuccess = SUCCEEDED(hr);


        //
        // Was the CreateImageSurface result as expected?
        //
        if (bExpectSuccess && bSuccess)
        {
            DebugOut(L"CreateImageSurface complete.  Expected: success; Actual: success.");
            pSurface->Release();
        }
        else if (!bExpectSuccess && !bSuccess)
        {
            DebugOut(L"CreateImageSurface complete.  Expected: failure; Actual: failure.");
        }
        else if (!bExpectSuccess && bSuccess)
        {
            DebugOut(DO_ERROR, L"CreateImageSurface complete.  Expected: failure; Actual: success. (hr = 0x%08x) Failing", hr);
            pSurface->Release();
            return TPR_FAIL;
        }
        else // only one remaining condition: (bExpectSuccess && !bSuccess)
        {
            DebugOut(DO_ERROR, L"CreateImageSurface complete.  Expected: success; Actual: failure. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }

    }

    //
    // Bad-parameter test #1:  Invalid format
    //
    if (SUCCEEDED(hr = m_pd3dDevice->CreateImageSurface(Mode.Width,           // UINT Width
                                                        Mode.Height,          // UINT Height
                                                        D3DMFMT_INDEX16,      // D3DMFORMAT Format
                                                       &pSurface)))           // IDirect3DMobileSurface** ppSurface
    {
        DebugOut(DO_ERROR, L"CreateImageSurface succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        pSurface->Release();
        return TPR_FAIL;
    }

    //
    // Bad-parameter test #2:  Invalid IDirect3DMobileSurface*
    //
    if (SUCCEEDED(hr = m_pd3dDevice->CreateImageSurface(Mode.Width,           // UINT Width
                                                        Mode.Height,          // UINT Height
                                                        D3DMFMT_R8G8B8,       // D3DMFORMAT Format
                                                        NULL)))               // IDirect3DMobileSurface** ppSurface
    {
        DebugOut(DO_ERROR, L"CreateImageSurface succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        pSurface->Release();
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteCopyRectsTest
//
//   Verify IDirect3DMobileDevice::CopyRects.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteCopyRectsTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteCopyRectsTest.");

    //
    // Test result
    //
    INT iResult = TPR_PASS;

    //
    // D3DMFORMAT iterator; to cycle through all formats
    //
    D3DMFORMAT CurrentFormat;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Surface interface pointers for image surfaces
    //
    IDirect3DMobileSurface *pSurface1 = NULL, *pSurface2 = NULL;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Iterate through all pixel formats that include RGB, in the D3DMFORMAT enumeration.
    // Drivers are required to support at least one RGB, ARGB, or XRGB format for image surfaces.
    //
    for (CurrentFormat = D3DMFMT_R8G8B8; CurrentFormat <= D3DMFMT_X4R4G4B4; (*(ULONG*)(&CurrentFormat))++)
    {

        //
        // Determine whether driver has reported that it will be able to create
        // this type of surface or not
        //
        if (SUCCEEDED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
                                                     m_DevType,          // D3DMDEVTYPE DeviceType
                                                     Mode.Format,        // D3DMFORMAT AdapterFormat
                                                     0,                  // ULONG Usage
                                                     D3DMRTYPE_SURFACE,  // D3DMRESOURCETYPE RType
                                                     CurrentFormat)))    // D3DMFORMAT CheckFormat
        {
            //
            // Rectangle extents that indicate a region that is "one pixel" smaller
            // on the right and bottom, than the whole surface.
            //
            RECT rOneSmaller = {0, 0, D3DQA_DEFAULT_COPYRECTS_SURF_WIDTH-1, D3DQA_DEFAULT_COPYRECTS_SURF_HEIGHT - 1};

            //
            // Target position
            //
            POINT DestPoint = {0,0};

            //
            // Bytes per pixel, for this format type
            //
            UINT uiBPP = D3DQA_BYTES_PER_PIXEL(CurrentFormat);

            //
            // Attempt the surface creation, for two surfaces to perform copy testing
            //
            if (FAILED(hr = m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_COPYRECTS_SURF_WIDTH, // UINT Width
                                                             D3DQA_DEFAULT_COPYRECTS_SURF_HEIGHT,// UINT Height
                                                             CurrentFormat,                      // D3DMFORMAT Format
                                                            &pSurface1)))                        // IDirect3DMobileSurface** ppSurface
            {
                DebugOut(DO_ERROR, L"Fatal error at CreateImageSurface. (hr = 0x%08x) Failing", hr);
                iResult = TPR_FAIL;
                goto cleanup;
            }
            if (FAILED(hr = m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_COPYRECTS_SURF_WIDTH, // UINT Width
                                                             D3DQA_DEFAULT_COPYRECTS_SURF_HEIGHT,// UINT Height
                                                             CurrentFormat,                      // D3DMFORMAT Format
                                                            &pSurface2)))                        // IDirect3DMobileSurface** ppSurface
            {
                DebugOut(DO_ERROR, L"Fatal error at CreateImageSurface. (hr = 0x%08x) Failing", hr);
                iResult = TPR_FAIL;
                goto cleanup;
            }

            //
            // Copy between surfaces (interface test; no particular data is set on these surfaces)
            //
            if (FailedCall(hr = m_pd3dDevice->CopyRects(pSurface1,   // IDirect3DMobileSurface* pSourceSurface
                                                       &rOneSmaller, // CONST RECT* pSourceRectsArray
                                                        1,           // UINT cRects
                                                        pSurface2,   // IDirect3DMobileSurface* pDestinationSurface
                                                       &DestPoint))) // CONST POINT* pDestPointsArray
            {
                DebugOut(DO_ERROR, L"CopyRects failed. (hr = 0x%08x) Failing", hr);
                iResult = TPR_FAIL;
                goto cleanup;
            }

            //
            // Verify that CopyRects fails when passed some valid and some
            // invalid RECTs
            //
            CONST UINT uiCopyOps = 4;
            CONST RECT rInvalid[uiCopyOps] = {
            // | Left | Right | Top | Bottom |
            // +------+-------+-----+--------+
               {  0 ,     0   ,  0  ,    0   },  // Invalid
               {  0 ,     0   ,  0  ,    0   },  // Invalid
               {  0 ,     1   ,  0  ,    1   },  // Valid
               {  0 ,     1   ,  0  ,    1   }   // Valid
            };

            CONST POINT pntValid[uiCopyOps] = {
            // |   X   |   Y   |
            // +-------+-------+
               {  16   ,  16   },  // Valid
               {  17   ,  17   },  // Valid
               {  18   ,  18   },  // Valid
               {  19   ,  19   }   // Valid
            };

            if (SucceededCall(hr = m_pd3dDevice->CopyRects(pSurface1,    // IDirect3DMobileSurface* pSourceSurface
                                                           rInvalid,     // CONST RECT* pSourceRectsArray
                                                           uiCopyOps,    // UINT cRects
                                                           pSurface2,    // IDirect3DMobileSurface* pDestinationSurface
                                                           pntValid)))   // CONST POINT* pDestPointsArray
            {
                DebugOut(DO_ERROR, L"CopyRects succeeded; failure expected. (hr = 0x%08x) Failing", hr);
                iResult = TPR_FAIL;
                goto cleanup;
            }



            //
            // Verify that CopyRects fails when passed some valid and some
            // invalid POINTs
            //
            CONST RECT rValid[uiCopyOps] = {
            // | Left | Right | Top | Bottom |
            // +------+-------+-----+--------+
               {  0 ,     0   ,  3  ,    3   },  // Valid
               {  4 ,     4   ,  7  ,    7   },  // Valid
               {  8 ,     8   , 11  ,   11   },  // Valid
               { 12 ,    12   , 15  ,   15   }   // Valid
            };

            CONST POINT pntInvalid[uiCopyOps] = {
            // |   X   |   Y   |
            // +-------+-------+
               {   4   ,   4   }, // Valid
               {   0   ,  -1   }, // Invalid
               {  -1   ,  -1   }, // Invalid
               {  -1   ,   0   }  // Invalid
            };

            if (SucceededCall(hr = m_pd3dDevice->CopyRects(pSurface1,    // IDirect3DMobileSurface* pSourceSurface
                                                           rValid,       // CONST RECT* pSourceRectsArray
                                                           uiCopyOps,    // UINT cRects
                                                           pSurface2,    // IDirect3DMobileSurface* pDestinationSurface
                                                           pntInvalid))) // CONST POINT* pDestPointsArray
            {
                DebugOut(DO_ERROR, L"CopyRects succeeded; failure expected. (hr = 0x%08x) Failing", hr);
                iResult = TPR_FAIL;
                goto cleanup;
            }


            if (FailedCall(hr = m_pd3dDevice->CopyRects(pSurface1,    // IDirect3DMobileSurface* pSourceSurface
                                                        rValid,       // CONST RECT* pSourceRectsArray
                                                        uiCopyOps,    // UINT cRects
                                                        pSurface2,    // IDirect3DMobileSurface* pDestinationSurface
                                                        pntValid)))   // CONST POINT* pDestPointsArray
            {
                DebugOut(DO_ERROR, L"CopyRects failed; success expected. (hr = 0x%08x) Failing", hr);
                iResult = TPR_FAIL;
                goto cleanup;
            }


            //
            // Prepare for next iteration (surface pointers will be clobbored)
            //
            pSurface1->Release();
            pSurface2->Release();
            pSurface1 = NULL;
            pSurface2 = NULL;

            
        }
        else
        {
            DebugOut(DO_ERROR, L"Skipping CopyRects test for format type 0x%08x, because CheckDeviceFormat indicates no support.", CurrentFormat);
        }
    }


cleanup:


    if (pSurface1)
        pSurface1->Release();

    if (pSurface2)
        pSurface2->Release();
    
    //
    // Flush
    //
    if (FAILED(hr = Flush()))
    {
        DebugOut(DO_ERROR, L"Flush failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    return iResult;

}


//
// ExecuteUpdateTextureTest
//
//   Verify IDirect3DMobileDevice::UpdateTexture
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteUpdateTextureTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteUpdateTextureTest.");

    //
    // Texture interface pointers
    //
    IDirect3DMobileTexture *pTextureSrc, *pTextureDst;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // D3DMFORMAT iterator; to cycle through all formats
    //
    D3DMFORMAT CurrentFormat;

    //
    // Iterator for choosing test scenario to exercise with respect to locking
    //
    UINT uiLockSurface;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Information about surface rect region to be locked
    //
    D3DMLOCKED_RECT LockedRect;


    //
    // Some iterations call for only a partial surface lock.  These iterations use
    // the following extents at LockRect-time.
    //
    RECT rHalf = {0,                                      // LONG left; 
                  0,                                      // LONG top; 
                  D3DQA_DEFAULT_UPDATETEX_TEX_WIDTH / 2,  // LONG right; 
                  D3DQA_DEFAULT_UPDATETEX_TEX_HEIGHT / 2};// LONG bottom; 


    //
    // Bad parameter test #1:  NULL args
    //
    if (SUCCEEDED(hr = m_pd3dDevice->UpdateTexture(NULL,   // IDirect3DMobileBaseTexture* pSourceTexture
                                                   NULL))) // IDirect3DMobileBaseTexture* pDestinationTexture
    {
        DebugOut(DO_ERROR, L"UpdateTexture succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Check for texturing support, which is mandatory for this test
    //
    if (!(D3DMDEVCAPS_TEXTURE & Caps.DevCaps))
    {
        DebugOut(DO_ERROR, L"Textures not supported; skipping CreateTexture test. Skipping.");
        return TPR_SKIP;
    }

    //
    // Bad parameter test #2:  Attempt with two SYSTEMMEM textures
    //
    if (D3DMSURFCAPS_SYSTEXTURE & Caps.SurfaceCaps)
    {

        //
        // Iterate through all formats in the D3DMFORMAT enumeration, seeking formats that
        // are valid as textures.
        //
        for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
        {

            //
            // Skip iterations that are for a format that is unsupported by the driver.
            //
            if (FAILED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
                                                      m_DevType,          // D3DMDEVTYPE DeviceType
                                                      Mode.Format,        // D3DMFORMAT AdapterFormat
                                                      0,                  // ULONG Usage
                                                      D3DMRTYPE_TEXTURE,  // D3DMRESOURCETYPE RType
                                                      CurrentFormat)))    // D3DMFORMAT CheckFormat
            {
                //
                // This is not a valid format for textures
                //
                continue;
            }

            //
            // Create the "source" texture
            //
               if (FAILED(hr = m_pd3dDevice->CreateTexture(D3DQA_DEFAULT_UPDATETEX_TEX_WIDTH,  // UINT Width,
                                                        D3DQA_DEFAULT_UPDATETEX_TEX_HEIGHT, // UINT Height,
                                                        1,                   // UINT Levels,
                                                        0,                   // DWORD Usage,
                                                        CurrentFormat,       // D3DFORMAT Format,
                                                        D3DMPOOL_SYSTEMMEM,  // D3DMPOOL Pool,
                                                       &pTextureSrc)))       // IDirect3DMobileTexture** ppTexture
            {
                DebugOut(DO_ERROR, L"CreateTexture failed. (hr = 0x%08x) Failing", hr);
                return TPR_FAIL;
            }

            //
            // Create the "destination" texture
            //
               if (FAILED(hr = m_pd3dDevice->CreateTexture(D3DQA_DEFAULT_UPDATETEX_TEX_WIDTH,  // UINT Width,
                                                        D3DQA_DEFAULT_UPDATETEX_TEX_HEIGHT, // UINT Height,
                                                        1,                   // UINT Levels,
                                                        0,                   // DWORD Usage,
                                                        CurrentFormat,       // D3DFORMAT Format,
                                                        D3DMPOOL_SYSTEMMEM,  // D3DMPOOL Pool,
                                                       &pTextureDst)))       // IDirect3DMobileTexture** ppTexture
            {
                DebugOut(DO_ERROR, L"CreateTexture failed. (hr = 0x%08x) Failing", hr);
                pTextureSrc->Release();
                return TPR_FAIL;
            }

            //
            // Update from among set of two system memory textures is invalid
            //
            if (SUCCEEDED(hr = m_pd3dDevice->UpdateTexture(pTextureSrc,   // IDirect3DMobileBaseTexture* pSourceTexture
                                                           pTextureDst))) // IDirect3DMobileBaseTexture* pDestinationTexture
            {
                DebugOut(DO_ERROR, L"UpdateTexture succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
                pTextureDst->Release();
                pTextureSrc->Release();
                return TPR_FAIL;
            }

            pTextureDst->Release();
            pTextureSrc->Release();
        }

    }

    //
    // Bad parameter test #3:  Attempt with two VIDEOMEM textures
    //
    if (D3DMSURFCAPS_VIDTEXTURE & Caps.SurfaceCaps)
    {

        //
        // Iterate through all formats in the D3DMFORMAT enumeration, seeking formats that
        // are valid as textures.
        //
        for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
        {

            //
            // Skip iterations that are for a format that is unsupported by the driver.
            //
            if (FAILED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
                                                      m_DevType,          // D3DMDEVTYPE DeviceType
                                                      Mode.Format,        // D3DMFORMAT AdapterFormat
                                                      0,                  // ULONG Usage
                                                      D3DMRTYPE_TEXTURE,  // D3DMRESOURCETYPE RType
                                                      CurrentFormat)))    // D3DMFORMAT CheckFormat
            {
                //
                // This is not a valid format for textures
                //
                continue;
            }

            //
            // Create the "source" texture
            //
               if (FAILED(hr = m_pd3dDevice->CreateTexture(D3DQA_DEFAULT_UPDATETEX_TEX_WIDTH,  // UINT Width,
                                                        D3DQA_DEFAULT_UPDATETEX_TEX_HEIGHT, // UINT Height,
                                                        1,                   // UINT Levels,
                                                        0,                   // DWORD Usage,
                                                        CurrentFormat,       // D3DFORMAT Format,
                                                        D3DMPOOL_VIDEOMEM,   // D3DMPOOL Pool,
                                                       &pTextureSrc)))       // IDirect3DMobileTexture** ppTexture
            {
                DebugOut(DO_ERROR, L"CreateTexture failed. (hr = 0x%08x) Failing", hr);
                return TPR_FAIL;
            }

            //
            // Create the "destination" texture
            //
               if (FAILED(hr = m_pd3dDevice->CreateTexture(D3DQA_DEFAULT_UPDATETEX_TEX_WIDTH,  // UINT Width,
                                                        D3DQA_DEFAULT_UPDATETEX_TEX_HEIGHT, // UINT Height,
                                                        1,                   // UINT Levels,
                                                        0,                   // DWORD Usage,
                                                        CurrentFormat,       // D3DFORMAT Format,
                                                        D3DMPOOL_VIDEOMEM,   // D3DMPOOL Pool,
                                                       &pTextureDst)))       // IDirect3DMobileTexture** ppTexture
            {
                DebugOut(DO_ERROR, L"CreateTexture failed. (hr = 0x%08x) Failing", hr);
                pTextureSrc->Release();
                return TPR_FAIL;
            }

            //
            // Update from among set of two video memory textures is invalid
            //
            if (SUCCEEDED(hr = m_pd3dDevice->UpdateTexture(pTextureSrc,   // IDirect3DMobileBaseTexture* pSourceTexture
                                                           pTextureDst))) // IDirect3DMobileBaseTexture* pDestinationTexture
            {
                DebugOut(DO_ERROR, L"UpdateTexture succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
                pTextureDst->Release();
                pTextureSrc->Release();
                return TPR_FAIL;
            }

            pTextureDst->Release();
            pTextureSrc->Release();
        }

    }

    //
    // If the driver does not support both video and system memory textures, UpdateTexture
    // is not useful.  Thus, in this case, the test will skip.
    //
    if (!((D3DMSURFCAPS_SYSTEXTURE & Caps.SurfaceCaps) && (D3DMSURFCAPS_VIDTEXTURE & Caps.SurfaceCaps)))
    {
        DebugOut(DO_ERROR, L"Driver must support both D3DMSURFCAPS_SYSTEXTURE and D3DMSURFCAPS_VIDTEXTURE, for UpdateTexture test.  Skipping.");
        return TPR_SKIP;
    }

    //
    // This test requires lockable textures
    //
    if (!(D3DMSURFCAPS_LOCKTEXTURE & Caps.SurfaceCaps))
    {
        DebugOut(DO_ERROR, L"Driver must support D3DMSURFCAPS_LOCKTEXTURE to execute the UpdateTexture test.  Skipping.");
        return TPR_SKIP;
    }

    //
    // Iterate through all formats in the D3DMFORMAT enumeration, seeking formats that
    // are valid as textures.
    //
    for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {

        //
        // Skip iterations that are for a format that is unsupported by the driver.
        //
        if (FAILED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
                                                  m_DevType,          // D3DMDEVTYPE DeviceType
                                                  Mode.Format,        // D3DMFORMAT AdapterFormat
                                                  0,                  // ULONG Usage
                                                  D3DMRTYPE_TEXTURE,  // D3DMRESOURCETYPE RType
                                                  CurrentFormat)))    // D3DMFORMAT CheckFormat
        {
            //
            // This is not a valid format for textures
            //
            continue;
        }

        //
        // Iterate through three possibilities (to ensure coverage of distinct UpdateTexture codepaths):
        //
        //   (1) Call UpdateTexture in the case where it has "nothing to do" (e.g., no texture lock
        //       has occurred on the source texture).
        //
        //   (2) Call UpdateTexture in the case where it needs to copy an entire texture (whole source
        //       texture _has_ been locked previously).
        //
        //   (3) Call UpdateTexture in the case where it only needs to copy _part_ of the source texture
        //       (LockRect called on just part of source texture previously)
        //
        for (uiLockSurface = 0; uiLockSurface <=2; uiLockSurface++)
        {


            //
            // Create the "source" texture
            //
               if (FAILED(hr = m_pd3dDevice->CreateTexture(D3DQA_DEFAULT_UPDATETEX_TEX_WIDTH,  // UINT Width,
                                                        D3DQA_DEFAULT_UPDATETEX_TEX_HEIGHT, // UINT Height,
                                                        1,                   // UINT Levels,
                                                        D3DMUSAGE_LOCKABLE,  // DWORD Usage,
                                                        CurrentFormat,       // D3DFORMAT Format,
                                                        D3DMPOOL_SYSTEMMEM,  // D3DMPOOL Pool,
                                                       &pTextureSrc)))       // IDirect3DMobileTexture** ppTexture
            {
                DebugOut(DO_ERROR, L"CreateTexture failed. (hr = 0x%08x) Failing", hr);
                return TPR_FAIL;
            }

            //
            // Create the "destination" texture
            //
               if (FAILED(hr = m_pd3dDevice->CreateTexture(D3DQA_DEFAULT_UPDATETEX_TEX_WIDTH,  // UINT Width,
                                                        D3DQA_DEFAULT_UPDATETEX_TEX_HEIGHT, // UINT Height,
                                                        1,                   // UINT Levels,
                                                        D3DMUSAGE_LOCKABLE,  // DWORD Usage,
                                                        CurrentFormat,       // D3DFORMAT Format,
                                                        D3DMPOOL_VIDEOMEM,   // D3DMPOOL Pool,
                                                       &pTextureDst)))       // IDirect3DMobileTexture** ppTexture
            {
                DebugOut(DO_ERROR, L"CreateTexture failed. (hr = 0x%08x) Failing", hr);
                pTextureSrc->Release();
                return TPR_FAIL;
            }


            //
            // Select a locking scenario to exercise, to ensure that all UpdateTexture code paths are exercised.
            //
            switch(uiLockSurface)
            {
            case 0:
                //
                // Don't lock
                //
                break;

            case 1:
                //
                // Partial surface lock
                //

                //
                // Lock 1/4th of the texture's region.
                //
                if (FAILED(hr = pTextureSrc->LockRect( 0,          // UINT Level
                                                      &LockedRect, // D3DMLOCKED_RECT* pLockedRect
                                                      &rHalf,      // CONST RECT* pRect
                                                       0)))        // DWORD Flags
                {
                    DebugOut(DO_ERROR, L"LockRect failed. (hr = 0x%08x) Failing", hr);
                    pTextureDst->Release();
                    pTextureSrc->Release();
                    return TPR_FAIL;
                }

                //
                // Unlock; to prepare for UpdateTexture call
                //
                if (FAILED(hr = pTextureSrc->UnlockRect(0)))
                {
                    DebugOut(DO_ERROR, L"UnlockRect failed. (hr = 0x%08x) Failing", hr);
                    pTextureDst->Release();
                    pTextureSrc->Release();
                    return TPR_FAIL;
                }
                break;

            case 2:
                //
                // Full surface lock
                //
                if (FAILED(hr = pTextureSrc->LockRect( 0,          // UINT Level
                                                      &LockedRect, // D3DMLOCKED_RECT* pLockedRect
                                                       NULL,       // CONST RECT* pRect
                                                       0)))        // DWORD Flags
                {
                    DebugOut(DO_ERROR, L"LockRect failed. (hr = 0x%08x) Failing", hr);
                    pTextureDst->Release();
                    pTextureSrc->Release();
                    return TPR_FAIL;
                }

                //
                // Unlock; to prepare for UpdateTexture call
                //
                if (FAILED(hr = pTextureSrc->UnlockRect(0)))
                {
                    DebugOut(DO_ERROR, L"UnlockRect failed. (hr = 0x%08x) Failing", hr);
                    pTextureDst->Release();
                    pTextureSrc->Release();
                    return TPR_FAIL;
                }
                break;
            }

            //
            // Call UpdateTexture to refresh video memory texture.
            //
            if (FAILED(hr = m_pd3dDevice->UpdateTexture(pTextureSrc,   // IDirect3DMobileBaseTexture* pSourceTexture
                                                        pTextureDst))) // IDirect3DMobileBaseTexture* pDestinationTexture
            {
                DebugOut(DO_ERROR, L"UpdateTexture failed. (hr = 0x%08x) Failing", hr);
                pTextureDst->Release();
                pTextureSrc->Release();
                return TPR_FAIL;
            }

            pTextureDst->Release();
            pTextureSrc->Release();
        }
    }

    //
    // Flush
    //
    if (FAILED(hr = Flush()))
    {
        DebugOut(DO_ERROR, L"Flush failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter tests
    //

    //
    // Test #1:  NULL arguments
    //
    if (SUCCEEDED(hr = m_pd3dDevice->UpdateTexture(NULL,   // IDirect3DMobileBaseTexture* pSourceTexture
                                                   NULL))) // IDirect3DMobileBaseTexture* pDestinationTexture
    {
        DebugOut(DO_ERROR, L"UpdateTexture succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetFrontBufferTest
//
//   Verify IDirect3DMobileDevice::GetFrontBuffer
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetFrontBufferTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetFrontBufferTest.");

    //
    // Interface pointer for accessing front buffer
    //
    IDirect3DMobileSurface* pFrontBuffer = NULL;

    //
    // Function result
    //
    INT Result = TPR_PASS;

    //
    // Locked region descriptor
    //
    D3DMLOCKED_RECT LockedRect;

    //
    // Adapter mode
    //
    D3DMDISPLAYMODE Mode;

    //
    // Invalid Rect Scaling
    //
    float ScalingValues[][2] = {
        1.0f, 1.1f,
        1.1f, 1.0f,
        1.1f, 1.1f,
        0.9f, 0.9f,
        1.1f, 0.9f,
        0.9f, 1.1f,
        0.5f, 0.5f,
        1.5f, 1.5f,
        2.0f, 2.0f
    };

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // The current display mode is needed, to determine desired format
    //
    if( FAILED(hr =  m_pd3dDevice->GetDisplayMode( &Mode ) ) )
    {
        DebugOut(DO_ERROR, L"GetDisplayMode failed. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }
    
    //
    // Create an image surface, of the same size to receive the front buffer contents
    //
    if( FAILED(hr =  m_pd3dDevice->CreateImageSurface(Mode.Width,     // UINT Width,
                                                    Mode.Height,    // INT Height,
                                                    Mode.Format,    // D3DMFORMAT Format,
                                                    &pFrontBuffer)))// IDirect3DMobileSurface** ppSurface
    {
        DebugOut(DO_ERROR, L"CreateImageSurface failed. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Retrieve front buffer copy 
    //
    if( FAILED(hr =  m_pd3dDevice->GetFrontBuffer(pFrontBuffer))) // IDirect3DMobileSurface* pFrontBuffer
    {
        DebugOut(DO_ERROR, L"GetFrontBuffer failed. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Force processing of any deferred tokens
    //
    if (FAILED(hr = pFrontBuffer->LockRect(&LockedRect, // D3DMLOCKED_RECT* pLockedRect,
                                      NULL,        // CONST RECT* pRect
                                      0)))         // DWORD Flags
    {
        DebugOut(DO_ERROR, L"LockRect failed. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }


    if (FAILED(hr = pFrontBuffer->UnlockRect()))
    {
        DebugOut(DO_ERROR, L"UnlockRect failed. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    if (pFrontBuffer)
    {
        pFrontBuffer->Release();
        pFrontBuffer = NULL;
    }

    //
    // Bad parameter test #1:  Invalid arg
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetFrontBuffer(NULL))) // IDirect3DMobileSurface** pFrontBuffer
    {
        DebugOut(DO_ERROR, L"GetFrontBuffer succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
    }    

    //
    // Bad parameter test #2: Invalid rects
    //
    for (int i = 0; i < sizeof(ScalingValues)/sizeof(ScalingValues[0]); ++i)
    {
        UINT uiInvalidWidth = (UINT)(Mode.Width * ScalingValues[i][0]);
        UINT uiInvalidHeight = (UINT)(Mode.Height * ScalingValues[i][1]);
        //
        // Create an image surface, of the same size to receive the front buffer contents
        //
        hr =  m_pd3dDevice->CreateImageSurface(
            uiInvalidWidth,     // UINT Width,
            uiInvalidHeight,    // INT Height,
            Mode.Format,    // D3DMFORMAT Format,
            &pFrontBuffer);  // IDirect3DMobileSurface** ppSurface
        if( FAILED(hr))
        {
            DebugOut(DO_ERROR, 
                L"CreateImageSurface failed creating surface of size %dx%d. (hr = 0x%08x) Aborting", 
                uiInvalidWidth,
                uiInvalidHeight,
                hr);
            Result = TPR_ABORT;
            continue;
        }
        
        //
        // Retrieve front buffer copy 
        //
        if( SUCCEEDED(hr =  m_pd3dDevice->GetFrontBuffer(pFrontBuffer))) // IDirect3DMobileSurface* pFrontBuffer
        {
            DebugOut(DO_ERROR, 
                L"GetFrontBuffer succeeded with invalid surface size (%dx%d). (hr = 0x%08x) Failing", 
                uiInvalidWidth,
                uiInvalidHeight,
                hr);
            Result = TPR_FAIL;
        }

        if (pFrontBuffer)
        {
            pFrontBuffer->Release();
            pFrontBuffer = NULL;
        }
    }

    //
    // Bad parameter test #3: Invalid Formats
    //
    for (D3DMFORMAT InvalidFormat = D3DMFMT_R8G8B8; InvalidFormat < D3DMFMT_D24X4S4; ++(*((UINT*)(&InvalidFormat))))
    {
        if (InvalidFormat == Mode.Format)
        {
            continue;
        }
        //
        // Create an image surface, of the same size to receive the front buffer contents
        //
        hr =  m_pd3dDevice->CreateImageSurface(
            Mode.Width,     // UINT Width,
            Mode.Height,    // INT Height,
            InvalidFormat,  // D3DMFORMAT Format,
            &pFrontBuffer);  // IDirect3DMobileSurface** ppSurface
        if( FAILED(hr))
        {
            continue;
        }
        
        //
        // Retrieve front buffer copy 
        //
        if( SUCCEEDED(hr =  m_pd3dDevice->GetFrontBuffer(pFrontBuffer))) // IDirect3DMobileSurface* pFrontBuffer
        {
            DebugOut(DO_ERROR, 
                L"GetFrontBuffer succeeded with invalid format (0x%08x). (hr = 0x%08x) Failing", 
                InvalidFormat,
                hr);
            Result = TPR_FAIL;
        }

        if (pFrontBuffer)
        {
            pFrontBuffer->Release();
            pFrontBuffer = NULL;
        }
    }

cleanup:

    if (pFrontBuffer)
        pFrontBuffer->Release();

    //
    // All failure conditions have already returned.
    //
    return Result;

}


//
// ExecuteSetRenderTargetTest
//
//   Verify IDirect3DMobileDevice::SetRenderTarget
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetRenderTargetTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetRenderTargetTest.");

    //
    // Does driver support lockable render targets?
    //
    BOOL bSupportsLockableRenderTarget;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // D3DMFORMAT iterator; to cycle through formats
    //
    D3DMFORMAT CurrentFormat;

    //
    // Indicates lock state to attempt
    //
    DWORD dwLockFlags;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // Test result
    //
    INT Result = TPR_PASS;

    //
    // Surface interface pointer for render targets
    //
    IDirect3DMobileSurface* pSurface = NULL;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // If driver claims support for lockable backbuffers, attempt to create such a backbuffer
    //
    if (Caps.SurfaceCaps & D3DMSURFCAPS_LOCKRENDERTARGET)
    {
        dwLockFlags = D3DMUSAGE_LOCKABLE;
    }
    else
    {
        dwLockFlags = 0x00000000;
    }

    //
    // Does driver support lockable render targets?
    //
    bSupportsLockableRenderTarget = (D3DMSURFCAPS_LOCKRENDERTARGET & Caps.SurfaceCaps) ? TRUE : FALSE;

    //
    // Iterate through all formats in the D3DMFORMAT enumeration
    //
    for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {

        //
        // Determine whether driver has reported that it will be able to create
        // this type of surface or not
        //
        if (SUCCEEDED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal,                    // UINT Adapter
                                                m_DevType,                             // D3DMDEVTYPE DeviceType
                                                Mode.Format,                           // D3DMFORMAT AdapterFormat
                                                D3DMUSAGE_RENDERTARGET,                // ULONG Usage
                                                D3DMRTYPE_SURFACE,                     // D3DMRESOURCETYPE RType
                                                CurrentFormat)))                       // D3DMFORMAT CheckFormat
        {


            //
            // Attempt the surface creation
            //
            if (FAILED(hr = m_pd3dDevice->CreateRenderTarget(Mode.Width,                    // UINT Width
                                                        Mode.Height,                   // UINT Height
                                                        CurrentFormat,                 // D3DMFORMAT Format
                                                        D3DMMULTISAMPLE_NONE,          // D3DMMULTISAMPLE_TYPE MultiSample
                                                        bSupportsLockableRenderTarget, // BOOL Lockable
                                                       &pSurface)))                    // IDirect3DMobileSurface** ppSurface
            {
                DebugOut(DO_ERROR, L"Fatal error at CreateRenderTarget. (hr = 0x%08x) Failing", hr);
                Result = TPR_FAIL;
                goto cleanup;
            }

            //
            // A render target has successfully been created; attempt to set it.
            //
            if (FAILED(hr = m_pd3dDevice->SetRenderTarget(pSurface,    // IDirect3DMobileSurface* pRenderTarget
                                                     NULL     ))) // IDirect3DMobileSurface* pNewZStencil
            {
                DebugOut(DO_ERROR, L"SetRenderTarget failed. (hr = 0x%08x) Failing", hr);
                Result = TPR_FAIL;
                goto cleanup;
            }

            //
            // Verify that SetRenderTarget fails during a scene
            //
            if (FAILED(hr = m_pd3dDevice->BeginScene()))
            {
                DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x) Failing", hr);
                Result = TPR_FAIL;
                goto cleanup;
            }

            if (SUCCEEDED(hr = m_pd3dDevice->SetRenderTarget(pSurface,    // IDirect3DMobileSurface* pRenderTarget
                                                        NULL     ))) // IDirect3DMobileSurface* pNewZStencil
            {
                DebugOut(DO_ERROR, L"SetRenderTarget succeeded; failure expected. (hr = 0x%08x) Failing", hr);
                Result = TPR_FAIL;
                goto cleanup;
            }

            //
            // Leave the scene
            //
            if (FAILED(hr = m_pd3dDevice->EndScene()))
            {
                DebugOut(DO_ERROR, L"EndScene failed. (hr = 0x%08x) Failing", hr);
                Result = TPR_FAIL;
                goto cleanup;
            }

            //
            // The middleware should have added a ref-count internally; our refcount should be
            // released to avoid a memory leak at cleanup time
            //
            pSurface->Release();
            pSurface = NULL;
        }
    }

cleanup:

    //
    // Cleanup anything that is not cleaned up already
    //
    if (pSurface)
        pSurface->Release();

    return Result;

}


//
// ExecuteGetRenderTargetTest
//
//   Verify IDirect3DMobileDevice::GetRenderTarget
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetRenderTargetTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetRenderTargetTest.");

    //
    // Surface interface pointer for render target
    //
    IDirect3DMobileSurface* pRenderTarget;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    if (FAILED(hr = m_pd3dDevice->GetRenderTarget(&pRenderTarget))) // IDirect3DMobileSurface** ppRenderTarget
    {
        DebugOut(DO_ERROR, L"GetRenderTarget failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    pRenderTarget->Release();

    //
    // Bad parameter test #1:  Invalid arg
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetRenderTarget(NULL))) // IDirect3DMobileSurface** ppRenderTarget
    {
        DebugOut(DO_ERROR, L"GetRenderTarget succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetDepthStencilSurfaceTest
//
//   Verify IDirect3DMobileDevice::GetDepthStencilSurface
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetDepthStencilSurfaceTest()
{
    HRESULT hr;
    INT Result = TPR_PASS;

    DebugOut(L"Beginning ExecuteGetDepthStencilSurfaceTest.");

    IDirect3DMobileSurface* pZStencilSurface;

    D3DMFORMAT Format;

    D3DMDISPLAYMODE Mode;
    
    D3DMSURFACE_DESC SurfaceDesc;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    if (FAILED(hr = m_pd3dDevice->GetDepthStencilSurface(&pZStencilSurface))) // IDirect3DMobileSurface** ppZStencilSurface
    {
        DebugOut(DO_ERROR, L"GetDepthStencilSurface failed. (hr = 0x%08x) Failing", hr);
        SetResult(Result, TPR_FAIL);
    }

    //
    // The interface pointer may be NULL
    //
    SafeRelease(pZStencilSurface);
    
    //
    // Bad parameter test #1: Invalid arg
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetDepthStencilSurface(NULL))) // IDirect3DMobileSurface** ppZStencilSurface
    {
        DebugOut(DO_ERROR, L"GetDepthStencilSurface succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        SetResult(Result, TPR_FAIL);
    }

    hr = m_pd3dDevice->GetDisplayMode(&Mode);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not retrieve display mode for device. (hr = 0x%08x) Aborting.", hr);
        return TPR_ABORT;
    }
    
    //
    // Go through the possible Depth/Stencil Buffer formats, and reset the device to use each supported format.
    // Then get the depth/stencil buffer and verify proper format and behavior.
    //
    for (Format = D3DMFMT_D32; Format <= (int)D3DMFMT_D24X4S4 + 1; ++(*(int*)&Format))
    {
        bool SurfaceExpected;
        hr = m_pD3D->CheckDeviceFormat(
            m_uiAdapterOrdinal,
            D3DMDEVTYPE_DEFAULT,
            Mode.Format,
            0,
            D3DMRTYPE_SURFACE,
            Format);
        if (FAILED(hr))
        {
            continue;
        }

        if (Format > D3DMFMT_D24X4S4)
        {
            m_d3dpp.EnableAutoDepthStencil = FALSE;
            m_d3dpp.AutoDepthStencilFormat = D3DMFMT_UNKNOWN;
            SurfaceExpected = false;
        }
        else
        {
            m_d3dpp.EnableAutoDepthStencil = TRUE;
            m_d3dpp.AutoDepthStencilFormat = Format;
            SurfaceExpected = true;
        }

        hr = m_pd3dDevice->Reset(&m_d3dpp);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, 
                L"Failed to Reset the device with DepthStencil %s and Format 0x%08x. (hr = 0x%08x) Aborting",
                m_d3dpp.EnableAutoDepthStencil ? L"Enabled" : L"Disabled",
                m_d3dpp.AutoDepthStencilFormat,
                hr);
            SetResult(Result, TPR_ABORT);
            continue;
        }

        hr = m_pd3dDevice->GetDepthStencilSurface(
            &pZStencilSurface);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR,
                L"GetDepthStencilSurface failed with DepthStencil %s and Format 0x%08x "
                L"(if no surface, should return NULL for pointer and succeed). (hr = 0x%08x) Failing",
                m_d3dpp.EnableAutoDepthStencil ? L"Enabled" : L"Disabled",
                m_d3dpp.AutoDepthStencilFormat,
                hr);
            SetResult(Result, TPR_FAIL);
        }
        if (NULL == pZStencilSurface)
        {
            if (SurfaceExpected)
            {
                DebugOut(DO_ERROR,
                    L"GetDepthStencilSurface returned a NULL pointer when surface creation was expected. Format 0x%08x. Failing",
                    Format);
                SetResult(Result, TPR_FAIL);
            }
            continue;
        }

        hr = pZStencilSurface->GetDesc(&SurfaceDesc);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR,
                L"GetDesc failed on retrieved DepthStencilSurface. Is this a valid surface? (hr = 0x%08x) Failing",
                hr);
            SetResult(Result, TPR_FAIL);
            SafeRelease(pZStencilSurface);
            continue;
        }

        if (SurfaceDesc.Format != Format)
        {
            DebugOut(DO_ERROR,
                L"DepthStencil surface is incorrect format. Device reset with format 0x%08x, retrieved surface with format 0x%08x. Failing",
                Format,
                SurfaceDesc.Format);
            SetResult(Result, TPR_FAIL);
        }

        SafeRelease(pZStencilSurface);
    }

    return Result;
}


//
// ExecuteBeginSceneTest
//
//   Verify IDirect3DMobileDevice::BeginScene
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteBeginSceneTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteBeginSceneTest.");


    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    if (FailedCall(hr = m_pd3dDevice->BeginScene()))
    {
        DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    if (SucceededCall(hr = m_pd3dDevice->BeginScene()))
    {
        DebugOut(DO_ERROR, L"BeginScene succeeded two times in succession, without an EndScene in between. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Leave the scene
    //
    if (FailedCall(hr = m_pd3dDevice->EndScene()))
    {
        DebugOut(DO_ERROR, L"EndScene failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}

//
// ExecuteEndSceneTest
//
//   Verify IDirect3DMobileDevice::EndScene
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteEndSceneTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteEndSceneTest.");

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted.");
        return TPR_SKIP;
    }

    //
    // EndScene & force a flush; the EndScene token should cause a failure
    //
    if (SucceededCall(hr = m_pd3dDevice->EndScene()))
    {
        DebugOut(DO_ERROR, L"Failure:  EndScene succeeded, without a prior BeginScene call. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // BeginScene & force a flush; the BeginScene token should not cause a failure
    //
    if (FailedCall(hr = m_pd3dDevice->BeginScene()))
    {
        DebugOut(DO_ERROR, L"BeginScene failed unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // EndScene & force a flush; the EndScene token should not cause a failure
    //
    if (FailedCall(hr = m_pd3dDevice->EndScene()))
    {
        DebugOut(DO_ERROR, L"EndScene failed unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}

D3DMFORMAT IDirect3DMobileDeviceTest::FindZStencilFormat(
    LPDIRECT3DMOBILEDEVICE pDevice,
    BOOL Depth,
    BOOL Stencil)
{
    LPDIRECT3DMOBILE pD3D =NULL;
    HRESULT hr;
    D3DMCAPS Caps;
    D3DMDISPLAYMODE Mode;
    D3DMFORMAT Format = D3DMFMT_UNKNOWN;
    D3DMFORMAT FormatDepthOnly[] = {
        D3DMFMT_D32,
        D3DMFMT_D16,
        D3DMFMT_D24X8,
    };
    D3DMFORMAT FormatBoth[] = {
        D3DMFMT_D15S1,
        D3DMFMT_D24S8,
        D3DMFMT_D24X4S4,
    };
    D3DMFORMAT * pWorkingFormats = NULL;
    UINT cFormats = 0;

    if ((Stencil && !Depth) || NULL == pDevice)
    {
        goto cleanup;
    }

    hr = pDevice->GetDirect3D(&pD3D);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"GetDirect3D failed (hr = 0x%08x)", hr);
        goto cleanup;
    }

    hr = pDevice->GetDeviceCaps(&Caps);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"GetDeviceCaps failed (hr = 0x%08x)", hr);
        goto cleanup;
    }
    hr = pDevice->GetDisplayMode(&Mode);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"GetDisplayMode failed (hr = 0x%08x)", hr);
        goto cleanup;
    }

    if (Depth && !Stencil)
    {
        pWorkingFormats = FormatDepthOnly;
        cFormats = _countof(FormatDepthOnly);
    }
    else
    {
        pWorkingFormats = FormatBoth;
        cFormats = _countof(FormatBoth);
    }

    for (int i = 0; i < cFormats; ++i)
    {
        hr = pD3D->CheckDeviceFormat(
            Caps.AdapterOrdinal,
            Caps.DeviceType,
            Mode.Format,
            0, // Usage
            D3DMRTYPE_SURFACE,
            pWorkingFormats[i]);
        if (SUCCEEDED(hr))
        {
            Format = pWorkingFormats[i];
            break;
        }
    }
cleanup:
    SafeRelease(pD3D);
    return Format;
}

//
// ExecuteClearTest
//
//   Verify IDirect3DMobileDevice::Clear
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteClearTest()
{
    DebugOut(L"Beginning ExecuteClearTest.");

    //
    // Region data to pass to Clear
    //
    RECT *pRects;

    //
    // Surface interface pointer for render target
    //
    IDirect3DMobileSurface* pRenderTarget;

    //
    // Descriptor info for the render target
    //
    D3DMSURFACE_DESC RenderTargDesc;

    //
    // API Result
    //
    HRESULT hr;

    //
    // Test Result
    //
    INT Result = TPR_PASS;

    //
    // The clear operation will be attempted with various RECT arrays
    // that cover the entire surface.  A variety of these RECT arrays
    // will be used, with various numbers of RECTs in each.
    //
    CONST UINT uiNumClearRectSets = 13;
    CONST NumClearRects[] = {
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        8,
        9,
        10,
        100,
        1000,
        4000,
    };
    struct FlagSet {
        DWORD Flag;
        TCHAR * FlagName;
    };
    #define FLAGENTRY(x) {x, _T(#x)}
    CONST FlagSet ClearFlags[] = {
        FLAGENTRY(D3DMCLEAR_TARGET),
        FLAGENTRY(D3DMCLEAR_ZBUFFER),
        FLAGENTRY(D3DMCLEAR_STENCIL),
        FLAGENTRY(D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER),
        FLAGENTRY(D3DMCLEAR_ZBUFFER | D3DMCLEAR_STENCIL),
        FLAGENTRY(D3DMCLEAR_TARGET | D3DMCLEAR_STENCIL),
        FLAGENTRY(D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER | D3DMCLEAR_STENCIL),
    };
    #undef FLAGENTRY

    //
    // Iterator for set of pre-defined rect options
    //
    UINT uiRectIter;
    //
    // Iterator for set of pre-defined flag options
    //
    UINT uiFlagIter;
    //
    // Iterator for set of pre-defined flag options
    //
    UINT uiZStencilIter;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve the render target interface pointer, so that the render target dimensions can be retrieved
    //
    if (FAILED(hr = m_pd3dDevice->GetRenderTarget(&pRenderTarget))) // IDirect3DMobileSurface** ppRenderTarget
    {
        DebugOut(DO_ERROR, L"GetRenderTarget failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve render target description, which contains width/height info that is needed for this test.
    //
    if (FAILED(hr = pRenderTarget->GetDesc(&RenderTargDesc))) // D3DMSURFACE_DESC *pDesc
    {
        DebugOut(DO_ERROR, L"Unable to retrieve render target description. (hr = 0x%08x) Failing", hr);
        pRenderTarget->Release();
        return TPR_FAIL;
    }

    //
    // The only reason that the render target interface pointer was needed, was for getting the 
    // surface descriptor.  Now that this has been completed successfully, release the interface.
    //
    pRenderTarget->Release();


    __try
    {
        for (uiFlagIter=0; uiFlagIter < _countof(ClearFlags); ++uiFlagIter)
        {
            DWORD CurrentFlags = ClearFlags[uiFlagIter].Flag;
            DebugOut(L"Testing Clear with flags: %s", ClearFlags[uiFlagIter].FlagName);
            for (uiZStencilIter=0; uiZStencilIter < 3; ++uiZStencilIter)
            {
                D3DMFORMAT ZStencilFormat = D3DMFMT_UNKNOWN;
                switch(uiZStencilIter)
                {
                case 0:
                    // No depth or stencil target
                    DebugOut(L"Clear with no ZStencil surface");
                    break;
                case 1:
                    // Depth target only
                    DebugOut(L"Clear with only depth surface");
                    ZStencilFormat = FindZStencilFormat(m_pd3dDevice, TRUE, FALSE);
                    break;
                case 2:
                    // Combined target
                    DebugOut(L"Clear with depth/stencil surface");
                    ZStencilFormat = FindZStencilFormat(m_pd3dDevice, TRUE, TRUE);
                    break;
                }

                if (D3DMFMT_UNKNOWN != ZStencilFormat)
                {
                    DebugOut(L"Using ZStencil of format 0x%08x", ZStencilFormat);
                    LPDIRECT3DMOBILESURFACE pZStencilSurface = NULL;
                    hr = m_pd3dDevice->CreateDepthStencilSurface(
                        RenderTargDesc.Width,
                        RenderTargDesc.Height,
                        ZStencilFormat,
                        D3DMMULTISAMPLE_NONE,
                        &pZStencilSurface);
                    
                    if (FAILED(hr))
                    {
                        DebugOut(DO_ERROR, L"Could not create DepthStencil surface (Fmt = 0x%08x) (hr = 0x%08x). Skipping this iteration.", ZStencilFormat, hr);
                        continue;
                    }
     
                    hr = m_pd3dDevice->SetRenderTarget(NULL, pZStencilSurface);
                    if (FAILED(hr))
                    {
                        DebugOut(DO_ERROR, L"SetRenderTarget(NULL, Surface) failed. (hr = 0x%08x). Aborting.", hr);
                        SafeRelease(pZStencilSurface);
                        SetResult(Result, TPR_ABORT);
                        continue;
                    }
                    SafeRelease(pZStencilSurface);
                }
                else
                {
                    DebugOut(L"Disabling ZStencil surface");
                    hr = m_pd3dDevice->SetRenderTarget(NULL, NULL);
                    if (FAILED(hr))
                    {
                        DebugOut(DO_ERROR, L"SetRenderTarget(NULL, NULL) failed. (hr = 0x%08x). Aborting.", hr);
                        SetResult(Result, TPR_ABORT);
                    }
                    
                }
                
                for (uiRectIter=0; uiRectIter < uiNumClearRectSets; uiRectIter++)
                {
                    HRESULT hrClear;
                    //
                    // If we allocated rects previously, free them here (not later, when a continue would skip the free)
                    //
                    if (pRects)
                    {
                        free(pRects);
                        pRects = NULL;
                    }

                    if (FAILED(hr = FullCoverageRECTs(&pRects,                     // RECT **ppRects
                                                  NumClearRects[uiRectIter],  // UINT uiNumRects
                                                  RenderTargDesc.Width,       // UINT uiWidth
                                                  RenderTargDesc.Height)))    // UINT uiHeight
                    {
                        DebugOut(DO_ERROR, L"Unable to generate RECT set for testing. (hr = 0x%08x) Aborting.", hr);
                        SetResult(Result, TPR_ABORT); 
                        continue;
                    }

                    hrClear = m_pd3dDevice->Clear(NumClearRects[uiRectIter],// DWORD Count
                                             pRects,                   // CONST RECT* pRects
                                             CurrentFlags,         // DWORD Flags
                                             0xFFFFFFFF,               // D3DMCOLOR Color
                                             0.0f,                     // float Z
                                             0);                       // DWORD Stencil

                    //
                    // Flush the clear
                    //
                    hr = m_pd3dDevice->Present(NULL, NULL, NULL, NULL);
                    if (FAILED(hr))
                    {
                        DebugOut(DO_ERROR, L"Present failed after clearing rects (hr = 0x%08x). Failing", hr);
                        SetResult(Result, TPR_FAIL);
                        continue;
                    }

                    //
                    // Bad-parameter tests
                    //
                    // Test #1:  Non-zero rect count; NULL rect pointer
                    //
                    if (SUCCEEDED(hr = m_pd3dDevice->Clear(NumClearRects[uiRectIter], // DWORD Count
                                                      NULL,                      // CONST RECT* pRects
                                                      CurrentFlags,          // DWORD Flags
                                                      0xFFFFFFFF,                // D3DMCOLOR Color
                                                      0.0f,                      // float Z
                                                      0)))                       // DWORD Stencil
                    {
                        DebugOut(DO_ERROR, L"Clear succeeded; failure expected. (hr = 0x%08x) Failing", hr);
                        free(pRects);
                        SetResult(Result, TPR_FAIL); 
                        continue;
                    }

                    //
                    // Test #2:  Invalid flags
                    //
                    if (SUCCEEDED(hr = m_pd3dDevice->Clear(NumClearRects[uiRectIter], // DWORD Count
                                                      pRects,                    // CONST RECT* pRects
                                                      0xFFFFFFFF,                // DWORD Flags
                                                      0xFFFFFFFF,                // D3DMCOLOR Color
                                                      0.0f,                      // float Z
                                                      0)))                       // DWORD Stencil
                    {
                        DebugOut(DO_ERROR, L"Clear succeeded; failure expected. (hr = 0x%08x) Failing", hr);
                        free(pRects);
                        SetResult(Result, TPR_FAIL); 
                        continue;
                    }

                    //
                    // Test #3:  Invalid Z
                    //
                    if (SUCCEEDED(hr = m_pd3dDevice->Clear(NumClearRects[uiRectIter], // DWORD Count
                                                      pRects,                    // CONST RECT* pRects
                                                      CurrentFlags,          // DWORD Flags
                                                      0xFFFFFFFF,                // D3DMCOLOR Color
                                                      10.0f,                     // float Z
                                                      0)))                       // DWORD Stencil
                    {
                        DebugOut(DO_ERROR, L"Clear succeeded; failure expected. (hr = 0x%08x) Failing", hr);
                        free(pRects);
                        SetResult(Result, TPR_FAIL); 
                        continue;
                    }

                    if (FAILED(hrClear) && (NumClearRects[uiRectIter] > D3DQA_MAX_CLEAR_RECTS))
                    {
                        DebugOut(L"Clear failed; but the number of passed RECTs was > 1024, so the failure is acceptable.");
                    }
                    else if (FAILED(hrClear))
                    {
                        
                        DebugOut(DO_ERROR, L"Unexpected failure for IDirect3DMobileDevice::Clear. (hr = 0x%08x) Failing", hrClear);
                        SetResult(Result, TPR_FAIL); 
                        continue;
                    }
                    
                    //
                    // Flush the clear
                    //
                    hr = m_pd3dDevice->Present(NULL, NULL, NULL, NULL);
                    if (FAILED(hr))
                    {
                        DebugOut(DO_ERROR, L"Present failed after bad parameter checks! (hr = 0x%08x). Failing", hr);
                        SetResult(Result, TPR_FAIL);
                        continue;
                    }
                }

                //
                // Clear with no RECTs
                //
                if (FAILED(hr = m_pd3dDevice->Clear(0,                // DWORD Count
                                               NULL,             // CONST RECT* pRects
                                               CurrentFlags, // DWORD Flags
                                               0xFFFFFFFF,       // D3DMCOLOR Color
                                               1.0f,             // float Z
                                               0)))              // DWORD Stencil
                {
                    DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x) Failing", hr);
                    SetResult(Result, TPR_FAIL); 
                    continue;
                }

                //
                // Flush the clear
                //
                hr = m_pd3dDevice->Present(NULL, NULL, NULL, NULL);
                if (FAILED(hr))
                {
                    DebugOut(DO_ERROR, L"Present failed after clearing with no rects! (hr = 0x%08x). Failing", hr);
                    SetResult(Result, TPR_FAIL);
                    continue;
                }

                hr = m_pd3dDevice->SetRenderTarget(NULL, NULL);
                if (FAILED(hr))
                {
                    DebugOut(DO_ERROR, L"SetRenderTarget failed after all clears. (hr = 0x%08x). Failing", hr);
                    SetResult(Result, TPR_FAIL);
                    continue;
                }
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        DebugOut(DO_ERROR, L"Exception occurred during test! Failing.");
        SetResult(Result, TPR_FAIL);
    }

    if (pRects)
    {
        free(pRects);
        pRects = NULL;
    }

    return Result;
}


//
// ExecuteSetTransformTest
//
//   Verify IDirect3DMobileDevice::SetTransform
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetTransformTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetTransformTest.");

    //
    // Iterator for attempting various transform types
    //
    D3DMTRANSFORMSTATETYPE TransformIter;

    //
    // For calling a function that generates data that is exactly
    // representible in both single-precision floating point and
    // fixed point (s15.16).
    //
    float *pfValues;
    UINT uiNumValues;

    //
    // Iterator for indexing automatically-generated data
    //
    DWORD dwIndex;

    //
    // Matrix storage for sets/gets to/from matrix transform types:
    //
    //     * D3DMTS_WORLD     
    //     * D3DMTS_VIEW      
    //     * D3DMTS_PROJECTION
    //     * D3DMTS_TEXTURE0  
    //     * D3DMTS_TEXTURE1  
    //     * D3DMTS_TEXTURE2  
    //     * D3DMTS_TEXTURE3  
    //
    D3DMMATRIX SetMatrix;
    D3DMMATRIX GetMatrix;

    //
    // Iterator for input format of matrix
    //
    D3DMFORMAT InputTypeIter;

    //
    // Output format reported by middleware at GetTransform-time
    //
    D3DMFORMAT OutputFormat;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    if (FAILED(hr = GetValuesCommonToFixedAndFloat(&pfValues, &uiNumValues)))
    {
        DebugOut(DO_ERROR, L"Unable to generate data for transform matrix testing. (hr = 0x%08x) Aborting.", hr);
        return TPR_ABORT;
    }


    //
    // Iterate through each matrix type
    //
    for(TransformIter=(D3DMTRANSFORMSTATETYPE)0; TransformIter < D3DMTS_NUMTRANSFORM; (*(ULONG*)(&TransformIter))++)
    {
        //
        // Iterate through all of the generated data.  16 entries fit in a single matrix (sizeof(D3DMMATRIX)/sizeof(D3DMVALUE))
        //
        for (dwIndex=0; dwIndex < uiNumValues; dwIndex+=sizeof(D3DMMATRIX)/sizeof(D3DMVALUE))
        {

            //
            // Attempt Matrix input as either float or fixed.
            //
            for(InputTypeIter=D3DMFMT_D3DMVALUE_FLOAT; InputTypeIter <= D3DMFMT_D3DMVALUE_FIXED; (*(ULONG*)(&InputTypeIter))++)
            {        
                memcpy((void*)&SetMatrix, &pfValues[dwIndex], sizeof(D3DMMATRIX));

                if (D3DMFMT_D3DMVALUE_FIXED == InputTypeIter)
                {
                    // Convert floats to fixed.
                    FloatToFixedArray((DWORD*)&SetMatrix, sizeof(D3DMMATRIX)/sizeof(D3DMVALUE));
                }
                else
                {
                    // No action needed.  Values are already in float.
                }

                //
                // Set matrix values for this transform type
                // 
                if (FAILED(hr = m_pd3dDevice->SetTransform(TransformIter,    // D3DMTRANSFORMSTATETYPE State
                                                      &SetMatrix,       // CONST D3DMMATRIX* pMatrix
                                                      InputTypeIter)))  // D3DMFORMAT Format
                {
                    DebugOut(DO_ERROR, L"SetTransform failed. (hr = 0x%08x) Failing", hr);
                    free(pfValues);
                    return TPR_FAIL;
                }

                //
                // Verify matrix values for this transform type
                // 
                if (FAILED(hr = m_pd3dDevice->GetTransform(TransformIter,    // D3DMTRANSFORMSTATETYPE State
                                                      &GetMatrix,       // D3DMMATRIX* pMatrix
                                                      &OutputFormat)))  // D3DMFORMAT* pFormat
                {
                    DebugOut(DO_ERROR, L"GetTransform failed. (hr = 0x%08x) Failing", hr);
                    free(pfValues);
                    return TPR_FAIL;
                }

                //
                // Compare the input matrix with the output matrix,
                // verify that they are identical
                //
                if (0 != memcmp(&SetMatrix, &GetMatrix, sizeof(D3DMMATRIX)))
                {
                    DebugOut(DO_ERROR, L"Input matrix does not match output matrix. Failing.");
                    free(pfValues);
                    return TPR_FAIL;
                }

                if (OutputFormat != InputTypeIter)
                {
                    DebugOut(DO_ERROR, L"Input format does not match output format. Failing.");
                    free(pfValues);
                    return TPR_FAIL;
                }
            }
        }
    }

    free(pfValues);

    //
    // Bad parameter test #1:  Invalid pointer to matrix
    // 
    if (SUCCEEDED(hr = m_pd3dDevice->SetTransform(D3DMTS_WORLD,              // D3DMTRANSFORMSTATETYPE State
                                             NULL,                      // CONST D3DMMATRIX* pMatrix
                                             D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
    {
        DebugOut(DO_ERROR, L"SetTransform succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter test #1:  Invalid D3DMTRANSFORMSTATETYPE
    // 
    if (SUCCEEDED(hr = m_pd3dDevice->SetTransform(D3DMTS_NUMTRANSFORM,       // D3DMTRANSFORMSTATETYPE State
                                             &SetMatrix,                // CONST D3DMMATRIX* pMatrix
                                             D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
    {
        DebugOut(DO_ERROR, L"SetTransform succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}


//
// ExecuteGetTransformTest
//
//   Verify IDirect3DMobileDevice::GetTransform
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetTransformTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetTransformTest.");

    //
    // Data for use as args in GetTransform call.
    //
    D3DMMATRIX Matrix;
    D3DMFORMAT Format;

    //
    // Iterator for attempting various transform types
    //
    D3DMTRANSFORMSTATETYPE TransformIter;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }


    //
    // Iterate through each matrix type, performing bad-parameter tests
    //
    for(TransformIter=(D3DMTRANSFORMSTATETYPE)0; TransformIter < D3DMTS_NUMTRANSFORM; (*(ULONG*)(&TransformIter))++)
    {

        //
        // Bad-parameter test #1:  Pass a NULL matrix pointer.
        //
        if (SUCCEEDED(hr = m_pd3dDevice->GetTransform(TransformIter, // D3DMTRANSFORMSTATETYPE State
                                                 NULL,          // D3DMMATRIX* pMatrix
                                                &Format)))      // D3DMFORMAT* pFormat
        {
            DebugOut(DO_ERROR, L"GetTransform succeeded; failure expected. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
        //
        // Bad-parameter test #2:  Pass a NULL format pointer.
        //
        if (SUCCEEDED(hr = m_pd3dDevice->GetTransform(TransformIter, // D3DMTRANSFORMSTATETYPE State
                                                &Matrix,        // D3DMMATRIX* pMatrix
                                                 NULL)))        // D3DMFORMAT* pFormat
        {
            DebugOut(DO_ERROR, L"GetTransform succeeded; failure expected. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }

        //
        // Bad-parameter test #3:  Pass a NULL matrix pointer, pass a NULL format pointer.
        //
        if (SUCCEEDED(hr = m_pd3dDevice->GetTransform(TransformIter, // D3DMTRANSFORMSTATETYPE State
                                                 NULL,          // D3DMMATRIX* pMatrix
                                                 NULL)))        // D3DMFORMAT* pFormat
        {
            DebugOut(DO_ERROR, L"GetTransform succeeded; failure expected. (hr = 0x%08x) Failing", hr);
            return TPR_FAIL;
        }
    }

    //
    // Bad-parameter test #4:  Invalid D3DMTRANSFORMSTATETYPE
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetTransform((D3DMTRANSFORMSTATETYPE)0xFFFFFFFF, // D3DMTRANSFORMSTATETYPE State
                                             &Matrix,                            // D3DMMATRIX* pMatrix
                                             &Format)))                          // D3DMFORMAT* pFormat
    {
        DebugOut(DO_ERROR, L"GetTransform succeeded unexpectedly. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }


    //
    // Call the test code for SetTransform, which provides valid-case coverage for GetTransform
    //
    return ExecuteSetTransformTest();
}


//
// ExecuteSetViewportTest
//
//   Verify IDirect3DMobileDevice::SetViewport
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetViewportTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetViewportTest.");

    //
    // Data structure for testing SetViewport
    //
    D3DMVIEWPORT Viewport;

    //
    // Surface interface pointer for render target
    //
    IDirect3DMobileSurface* pRenderTarget;

    //
    // Descriptor info for the render target
    //
    D3DMSURFACE_DESC RenderTargDesc;

    //
    // X,Y coordinates to attempt to set, for the viewport
    //
    UINT uiAttemptX, uiAttemptY;

    //
    // Iterator for selecting a ZMin/ZMax setting
    //
    UINT uiZPair;

    //
    // ZMin/ZMax pairs to attempt to set via SetViewport
    //
    CONST UINT uiNumZPairs = 13;
    CONST float fZPairs[uiNumZPairs][2] =
    {// |  Min  |  Max  |
     // +-------+-------+
        { 0.0f  ,  1.0f },  // Typical user scenario
        { 0.0f  ,  0.0f },  // Force rendering to foreground
        { 1.0f  ,  1.0f },  // Force rendering to background

        { 0.0f  ,  0.1f },  // * 
        { 0.1f  ,  0.2f },  // **
        { 0.2f  ,  0.3f },  // ***
        { 0.3f  ,  0.4f },  // **** 
        { 0.4f  ,  0.5f },  // *****  Attempt various valid
        { 0.5f  ,  0.6f },  // *****  ranges from 0.0f to 1.0f
        { 0.6f  ,  0.7f },  // **** 
        { 0.7f  ,  0.8f },  // *** 
        { 0.8f  ,  0.9f },  // ** 
        { 0.9f  ,  1.0f }   // * 
    };

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // The render target interface pointer is needed, to retrieve the specifications thereof
    //
    if (FAILED(hr = m_pd3dDevice->GetRenderTarget(&pRenderTarget))) // IDirect3DMobileSurface** ppRenderTarget
    {
        DebugOut(DO_ERROR, L"GetRenderTarget failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve render target description, which contains:
    //
    //      * D3DMFORMAT           Format;
    //      * D3DMRESOURCETYPE     Type;
    //      * ULONG                Usage;
    //      * D3DMPOOL             Pool;
    //      * UINT                 Size;
    //      * D3DMMULTISAMPLE_TYPE MultiSampleType;
    //      * UINT                 Width;
    //      * UINT                 Height;
    //
    if (FAILED(hr = pRenderTarget->GetDesc(&RenderTargDesc))) // D3DMSURFACE_DESC *pDesc
    {
        DebugOut(DO_ERROR, L"Unable to retrieve render target description. (hr = 0x%08x) Failing", hr);
        pRenderTarget->Release();
        return TPR_FAIL;
    }

    //
    // Verify that SetViewport fails when proposed viewport width exceeds render target width
    //
    Viewport.X = Viewport.Y = 0;
    Viewport.Width  = RenderTargDesc.Width + 1;
    Viewport.Height = RenderTargDesc.Height;
    Viewport.MinZ = 0.0f;
    Viewport.MaxZ = 0.0f;
    if (SUCCEEDED(hr = m_pd3dDevice->SetViewport(&Viewport))) // CONST D3DMVIEWPORT* pViewport
    {
        DebugOut(DO_ERROR, L"SetViewport succeeded despite unsupportable width. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Verify that SetViewport fails when proposed viewport height exceeds render target height
    //
    Viewport.X = Viewport.Y = 0;
    Viewport.Width  = RenderTargDesc.Width;
    Viewport.Height = RenderTargDesc.Height+1;
    Viewport.MinZ = 0.0f;
    Viewport.MaxZ = 0.0f;
    if (SUCCEEDED(hr = m_pd3dDevice->SetViewport(&Viewport))) // CONST D3DMVIEWPORT* pViewport
    {
        DebugOut(DO_ERROR, L"SetViewport succeeded despite unsupportable height. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }


    //
    // The render target specs have been retrieved, this test no longer requires the interface pointer.
    //
    pRenderTarget->Release();

    //
    // Step through a variety of valid X coordinates
    //
    for(uiAttemptX = 0; uiAttemptX < (RenderTargDesc.Width-1); uiAttemptX+=(RenderTargDesc.Width/5))
    {
        //
        // Step through a variety of valid Y coordinates
        //
        for(uiAttemptY = 0; uiAttemptY < (RenderTargDesc.Height-1); uiAttemptY+=(RenderTargDesc.Height/5))
        {
            for (uiZPair = 0; uiZPair < uiNumZPairs; uiZPair++)
            {
                Viewport.X = uiAttemptX;                 // ULONG
                Viewport.Y = uiAttemptY;                 // ULONG
                Viewport.Width = RenderTargDesc.Width - uiAttemptX;   // ULONG
                Viewport.Height = RenderTargDesc.Height - uiAttemptY; // ULONG
                Viewport.MinZ = fZPairs[uiZPair][0];     // float
                Viewport.MaxZ = fZPairs[uiZPair][1];     // float

                if (FailedCall(hr = m_pd3dDevice->SetViewport(&Viewport))) // CONST D3DMVIEWPORT* pViewport
                {
                    DebugOut(DO_ERROR, L"SetViewport failed. (hr = 0x%08x) Failing", hr);
                    return TPR_FAIL;
                }
            }
        }
    }

    //
    // Bad-parameter tests
    //
    // Test #1:  NULL argument
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetViewport(NULL))) // CONST D3DMVIEWPORT* pViewport
    {
        DebugOut(DO_ERROR, L"SetViewport succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}


//
// ExecuteGetViewportTest
//
//   Verify IDirect3DMobileDevice::GetViewport.  This test case exercises a similar
//   case to the testing for SetViewport, except that after every SetViewport call,
//   there is a GetViewport call to verify that the settings were stored.
//   
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetViewportTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetViewportTest.");

    //
    // Data structure for testing SetViewport
    //
    D3DMVIEWPORT Viewport;

    //
    // Surface interface pointer for render target
    //
    IDirect3DMobileSurface* pRenderTarget;

    //
    // Descriptor info for the render target
    //
    D3DMSURFACE_DESC RenderTargDesc;

    //
    // X,Y coordinates to attempt to set, for the viewport
    //
    UINT uiAttemptX, uiAttemptY;

    //
    // Iterator for selecting a ZMin/ZMax setting
    //
    UINT uiZPair;

    //
    // ZMin/ZMax pairs to attempt to set via SetViewport
    //
    CONST UINT uiNumZPairs = 13;
    CONST float fZPairs[uiNumZPairs][2] =
    {// |  Min  |  Max  |
     // +-------+-------+
        { 0.0f  ,  1.0f },  // Typical user scenario
        { 0.0f  ,  0.0f },  // Force rendering to foreground
        { 1.0f  ,  1.0f },  // Force rendering to background

        { 0.0f  ,  0.1f },  // * 
        { 0.1f  ,  0.2f },  // **
        { 0.2f  ,  0.3f },  // ***
        { 0.3f  ,  0.4f },  // **** 
        { 0.4f  ,  0.5f },  // *****  Attempt various valid
        { 0.5f  ,  0.6f },  // *****  ranges from 0.0f to 1.0f
        { 0.6f  ,  0.7f },  // **** 
        { 0.7f  ,  0.8f },  // *** 
        { 0.8f  ,  0.9f },  // ** 
        { 0.9f  ,  1.0f }   // * 
    };

    //
    // Viewport structure resulting from GetViewport call
    //
    D3DMVIEWPORT ResultViewport;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // The render target interface pointer is needed, to retrieve the specifications thereof
    //
    if (FAILED(hr = m_pd3dDevice->GetRenderTarget(&pRenderTarget))) // IDirect3DMobileSurface** ppRenderTarget
    {
        DebugOut(DO_ERROR, L"GetRenderTarget failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve render target description, which contains:
    //
    //      * D3DMFORMAT           Format;
    //      * D3DMRESOURCETYPE     Type;
    //      * ULONG                Usage;
    //      * D3DMPOOL             Pool;
    //      * UINT                 Size;
    //      * D3DMMULTISAMPLE_TYPE MultiSampleType;
    //      * UINT                 Width;
    //      * UINT                 Height;
    //
    if (FAILED(hr = pRenderTarget->GetDesc(&RenderTargDesc))) // D3DMSURFACE_DESC *pDesc
    {
        DebugOut(DO_ERROR, L"Unable to retrieve render target description. (hr = 0x%08x) Failing", hr);
        pRenderTarget->Release();
        return TPR_FAIL;
    }

    //
    // The render target specs have been retrieved, this test no longer requires the interface pointer.
    //
    pRenderTarget->Release();

    //
    // Get viewport
    //
    if (FAILED(hr = m_pd3dDevice->GetViewport(&ResultViewport))) // D3DMVIEWPORT* pViewport
    {
        DebugOut(DO_ERROR, L"GetViewport failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Do viewport extents match render target extents?
    //
    if (RenderTargDesc.Width != ResultViewport.Width)
    {
        DebugOut(L"D3DMSURFACE_DESC for render target does not match D3DMVIEWPORT.Width. Failing.");
        DebugOut(DO_ERROR, L"Desc width: %d; Viewport width: %d", RenderTargDesc.Width, ResultViewport.Width);
        return TPR_FAIL;
    }
    if (RenderTargDesc.Height != ResultViewport.Height)
    {
        DebugOut(L"D3DMSURFACE_DESC for render target does not match D3DMVIEWPORT.Height. Failing.");
        DebugOut(DO_ERROR, L"Desc height: %d; Viewport height: %d", RenderTargDesc.Height, ResultViewport.Height);
        return TPR_FAIL;
    }

    //
    // Step through a variety of valid X coordinates
    //
    for(uiAttemptX = 0; uiAttemptX < (RenderTargDesc.Width-1); uiAttemptX+=10)
    {
        //
        // Step through a variety of valid Y coordinates
        //
        for(uiAttemptY = 0; uiAttemptY < (RenderTargDesc.Height-1); uiAttemptY+=10)
        {
            for (uiZPair = 0; uiZPair < uiNumZPairs; uiZPair++)
            {
                Viewport.X = uiAttemptX;                              // ULONG
                Viewport.Y = uiAttemptY;                              // ULONG
                Viewport.Width = RenderTargDesc.Width - uiAttemptX;   // ULONG
                Viewport.Height = RenderTargDesc.Height - uiAttemptY; // ULONG
                Viewport.MinZ = fZPairs[uiZPair][0];                  // float
                Viewport.MaxZ = fZPairs[uiZPair][1];                  // float

                if (FailedCall(hr = m_pd3dDevice->SetViewport(&Viewport))) // CONST D3DMVIEWPORT* pViewport
                {
                    DebugOut(DO_ERROR, L"SetViewport failed. (hr = 0x%08x) Failing", hr);
                    return TPR_FAIL;
                }

                if (FailedCall(hr = m_pd3dDevice->GetViewport(&ResultViewport))) // D3DMVIEWPORT* pViewport
                {
                    DebugOut(DO_ERROR, L"GetViewport failed. (hr = 0x%08x) Failing", hr);
                    return TPR_FAIL;
                }

                if (0 != memcmp((void*)&ResultViewport, (void*)&Viewport, sizeof(D3DMVIEWPORT)))
                {
                    DebugOut(DO_ERROR, L"Viewport retrieved by GetViewport does not match viewport set in SetViewport. Failing.");
                    return TPR_FAIL;
                }
            }
        }
    }

    //
    // Bad-parameter tests
    //
    // Test #1:  NULL argument
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetViewport(NULL))) // D3DMVIEWPORT* pViewport
    {
        DebugOut(DO_ERROR, L"GetViewport succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteSetMaterialTest
//
//   Verify IDirect3DMobileDevice::SetMaterial
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetMaterialTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetMaterialTest.");

    //
    // Material structures for use with SetMaterial/GetMaterial
    //
    D3DMMATERIAL Material, ResultMaterial;

    //
    //  Formats for use with SetMaterial/GetMaterial
    //
    D3DMFORMAT Format, ResultFormat;

    //
    // Iterators for various material components
    //
    UINT AmbientColor, SpecularColor, DiffuseColor;
    float Power;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Initialize D3DMMATERIAL memory
    //
    ZeroMemory(&Material, sizeof(D3DMMATERIAL));

    //
    // Attempt a wide variety of parameters, for SetMaterial/GetMaterial
    //
    // Reasonable time constraints are the limiting factor in the scope of this testing (and
    // this is the reason for skipping some of the "sample color table" entries)
    //
    for (Format = D3DMFMT_D3DMVALUE_FLOAT; Format <= D3DMFMT_D3DMVALUE_FIXED; (*(ULONG*)(&Format))++)
    {
        for (AmbientColor = 0; AmbientColor < uiNumSampleSingleColorValues; AmbientColor+=8)
        {
            for (SpecularColor = 0; SpecularColor < uiNumSampleSingleColorValues; SpecularColor+=8)
            {
                for (DiffuseColor = 0; DiffuseColor < uiNumSampleSingleColorValues; DiffuseColor+=8)
                {
                    for (Power = 0.0f; Power < D3DQA_MAX_MATERIALPOWER; Power+=5.0f)
                    {

                        Material.Diffuse  = SampleColorValues[DiffuseColor];  // D3DMCOLORVALUE: Diffuse color RGBA
                        Material.Ambient  = SampleColorValues[AmbientColor];  // D3DMCOLORVALUE: Ambient color RGB
                        Material.Specular = SampleColorValues[SpecularColor]; // D3DMCOLORVALUE: Specular 'shininess'
                        Material.Power    = Power;                            // D3DMVALUE: Sharpness if specular

                        //
                        // The material is current in float format; convert if necessary
                        //
                        if (D3DMFMT_D3DMVALUE_FIXED == Format)
                        {
                            // Convert floats to fixed.
                            FloatToFixedArray((DWORD*)&Material, sizeof(D3DMMATERIAL)/sizeof(D3DMVALUE));
                        }

                        // 
                        // Pass the material to the middleware
                        // 
                        if (FAILED(hr = m_pd3dDevice->SetMaterial(&Material,  // CONST D3DMMATERIAL* pMaterial
                                                             Format)))   // D3DMFORMAT Format
                        {
                            DebugOut(DO_ERROR, L"SetMaterial failed. (hr = 0x%08x) Failing", hr);
                            return TPR_FAIL;
                        }

                        // 
                        // Retrieve the material
                        // 
                        if (FAILED(hr = m_pd3dDevice->GetMaterial(&ResultMaterial, // D3DMMATERIAL* pMaterial
                                                             &ResultFormat))) // D3DMFORMAT* pFormat
                        {
                            DebugOut(DO_ERROR, L"GetMaterial failed. (hr = 0x%08x) Failing", hr);
                            return TPR_FAIL;
                        }

                        //
                        // Compare the input material with the output material,
                        // verify that they are identical
                        //
                        if (0 != memcmp(&Material, &ResultMaterial, sizeof(D3DMMATERIAL)))
                        {
                            DebugOut(DO_ERROR, L"Input material does not match output material. Failing.");
                            return TPR_FAIL;
                        }

                        //
                        // Compare the input format with the output format (ensure that they are identical)
                        //
                        if (Format != ResultFormat)
                        {
                            DebugOut(DO_ERROR, L"Input format does not match output format. Failing.");
                            return TPR_FAIL;
                        }
                    }
                }
            }
        }
    }

    //
    //
    // Bad parameter tests...
    //
    //

    //
    // Test #1:  NULL material with format of float.
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetMaterial(NULL,                      // CONST D3DMMATERIAL* pMaterial
                                            D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
    {
        DebugOut(DO_ERROR, L"SetMaterial succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Test #2:  NULL material with format of fixed.
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetMaterial(NULL,                      // CONST D3DMMATERIAL* pMaterial
                                            D3DMFMT_D3DMVALUE_FIXED))) // D3DMFORMAT Format
    {
        DebugOut(DO_ERROR, L"SetMaterial succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetMaterialTest
//
//   Verify IDirect3DMobileDevice::GetMaterial
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetMaterialTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetMaterialTest.");

    //
    // Arguments for GetMaterial
    //
    D3DMMATERIAL Material;
    D3DMFORMAT Format;

    CONST D3DMMATERIAL ExpectedDefaultMaterial = {
        {0x00010000, 0x00010000, 0x00010000, 0x00000000}, // D3DMCOLORVALUE  Diffuse; 
        {0x00000000, 0x00000000, 0x00000000, 0x00000000}, // D3DMCOLORVALUE  Ambient; 
        {0x00000000, 0x00000000, 0x00000000, 0x00000000}, // D3DMCOLORVALUE  Specular;
         0.0f};                                           // float           Power;   

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }


    //
    // Confirm default value expected to be set up by middleware at device initialization time
    //
    if (FAILED(hr = m_pd3dDevice->GetMaterial(&Material, // D3DMMATERIAL* pMaterial
                                         &Format))) // D3DMFORMAT* pFormat
    {
        DebugOut(DO_ERROR, L"GetMaterial failed. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Default format is expected to by of type D3DMFMT_D3DMVALUE_FIXED
    //
    if (D3DMFMT_D3DMVALUE_FIXED != Format)
    {
        DebugOut(DO_ERROR, L"GetMaterial default material is not D3DMFMT_D3DMVALUE_FIXED. Failing.");
        return TPR_FAIL;
    }

    //
    // Is default material contents identical to expectations
    //
    if (0 != memcmp(&ExpectedDefaultMaterial, &Material, sizeof(D3DMMATERIAL)))
    {
        DebugOut(DO_ERROR, L"Unexpected GetMaterial default material contents. Failing.");
        return TPR_FAIL;
    }


    //
    // Bad parameter tests:
    //

    //
    // Test #1:  NULL Material
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetMaterial(NULL,      // D3DMMATERIAL* pMaterial
                                            &Format))) // D3DMFORMAT* pFormat
    {
        DebugOut(DO_ERROR, L"GetMaterial succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Test #2:  NULL Format
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetMaterial(&Material, // D3DMMATERIAL* pMaterial
                                            NULL)))    // D3DMFORMAT* pFormat
    {
        DebugOut(DO_ERROR, L"GetMaterial succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // Test #3:  NULL Material, NULL Format
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetMaterial(NULL,   // D3DMMATERIAL* pMaterial
                                            NULL))) // D3DMFORMAT* pFormat
    {
        DebugOut(DO_ERROR, L"GetMaterial succeeded; failure expected. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // More thorough testing occurs in a closely-related test:
    //
    return ExecuteSetMaterialTest();

}


//
// ExecuteSetLightTest
//
//   Verify IDirect3DMobileDevice::SetLight
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetLightTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetLightTest.");

    //
    // Maximum light index to attempt:
    //
    UINT uiMaxLight;

    //
    // Iterator for light indices
    //
    UINT uiCurLightIndex;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // If Caps.MaxActiveLights is more than the number below,
    // limit the number of light indices (to be tested) to
    // this hard-coded "reasonable" number:
    //
    CONST UINT uiMaxLightIndices = 100;


    //
    // Input light for SetLight; output light for GetLight
    //
    D3DMLIGHT Light, ResultLight;

    //
    // Input format for SetLight; output format for GetLight
    //
    D3DMFORMAT Format, ResultFormat;


    //
    // For calling a function that generates data that is exactly
    // representible in both single-precision floating point and
    // fixed point (s15.16).
    //
    float *pFloatValues;
    DWORD *pFixedValues;
    UINT uiNumValues;
    UINT uiFixedIndex = 0;
    UINT uiFloatIndex = 0;

    //
    // Light to be used with SetLight operation; iterator for light types
    //
    D3DMLIGHTTYPE Type;
    UINT uiType;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Failing", hr);
        return TPR_FAIL;
    }

    //
    // See if lighting is supported at all (directional and positional lights
    // are the only ones that are used with SetLight; ambient lighting is
    // set by render state).
    //
    if (!((Caps.VertexProcessingCaps & D3DMVTXPCAPS_DIRECTIONALLIGHTS) ||
          (Caps.VertexProcessingCaps & D3DMVTXPCAPS_POSITIONALLIGHTS)))
    {
        DebugOut(DO_ERROR, L"Skipping SetLight test, because driver supports neither D3DMVTXPCAPS_DIRECTIONALLIGHTS nor D3DMVTXPCAPS_POSITIONALLIGHTS. Skipping.");
        return TPR_SKIP;
    }

    //
    // It has been verified that the driver supports either directional or positional lights
    // (or both); if it has zero set for MaxActiveLights, this is illogical, and is a driver
    // bug worthy of failing this test.
    //
    if (0 == Caps.MaxActiveLights)
    {
        DebugOut(DO_ERROR, L"D3DMCAPS reports support for D3DMVTXPCAPS_DIRECTIONALLIGHTS or D3DMVTXPCAPS_POSITIONALLIGHTS (or both), but zero for MaxActiveLights.  The D3DMCAPS settings are conflicting. Failing.");
        return TPR_FAIL;
    }

    //
    // Get some "sample data" for use with lights. (FLOAT)
    //    
    if (FAILED(hr = GetValuesCommonToFixedAndFloat(&pFloatValues, &uiNumValues)))
    {
        DebugOut(DO_ERROR, L"Unable to generate data for transform matrix testing. (hr = 0x%08x) Aborting.", hr);
        return TPR_ABORT;
    }

    //
    // Get some "sample data" for use with lights. (FIXED)
    //    
    if (FAILED(hr = GetValuesCommonToFixedAndFloat((float**)(&pFixedValues), &uiNumValues)))
    {
        DebugOut(DO_ERROR, L"Unable to generate data for transform matrix testing. (hr = 0x%08x) Aborting.", hr);
        return TPR_ABORT;
    }
    else
    {
        FloatToFixedArray(pFixedValues, uiNumValues);
    }

    //
    // This test will specifically verify that SetLight will allow more than
    // MaxActiveLights to be specified.  MaxActiveLights applies to LightEnable,
    // not SetLight.
    //
    // If the MaxActiveLights value is more than can be reasonably tested, 
    // set some other feasible maximum
    //
    if (Caps.MaxActiveLights > uiMaxLightIndices)
        uiMaxLight = uiMaxLightIndices * 2;
    else
        uiMaxLight = Caps.MaxActiveLights * 2;

    //
    // Iterate through various permutations of SetLight callers
    //
    for (Format = D3DMFMT_D3DMVALUE_FLOAT; Format <= D3DMFMT_D3DMVALUE_FIXED; (*(ULONG*)(&Format))++)
    {
        for(uiCurLightIndex = 0; uiCurLightIndex < uiMaxLight; uiCurLightIndex++)
        {

            //
            // Note:  enumeration values for D3DMLIGHT_POINT and D3DMLIGHT_DIRECTIONAL
            // are not contiguous
            //
            for(uiType = 0; uiType <= 2; uiType++)
            {  
                switch(uiType)
                {
                case 0:
                    Type = D3DMLIGHT_POINT;
                    break;
                case 1:
                    Type = D3DMLIGHT_DIRECTIONAL;
                    break;
                }


                const DWORD *pdwData;

                // 
                // Type of light source
                // 
                Light.Type = Type;          

                // 
                // Protect against the case where the test "runs out" of sample data
                // 
                if (uiFloatIndex + (sizeof(D3DMLIGHT)-sizeof(D3DMLIGHTTYPE)) / sizeof(DWORD) >= uiNumValues)
                    uiFloatIndex = 0;

                if (uiFixedIndex + (sizeof(D3DMLIGHT)-sizeof(D3DMLIGHTTYPE)) / sizeof(DWORD) >= uiNumValues)
                    uiFixedIndex = 0;

                //
                // Fill remaining fields, all D3DMVALUEs, with predefined data.  Fields:
                //
                //      * D3DMCOLORVALUE   Diffuse;   
                //      * D3DMCOLORVALUE   Specular;  
                //      * D3DMCOLORVALUE   Ambient;   
                //      * D3DMVECTOR       Position;  
                //      * D3DMVECTOR       Direction; 
                //      * D3DMVALUE        Range;     
                //
                switch(Format)
                {
                case D3DMFMT_D3DMVALUE_FLOAT:
                    pdwData = (DWORD*)(&pFloatValues[uiFloatIndex]);
                    memcpy(&(Light.Diffuse), pdwData, sizeof(D3DMLIGHT)-sizeof(D3DMLIGHTTYPE));
                    uiFloatIndex+=(sizeof(D3DMLIGHT)-sizeof(D3DMLIGHTTYPE)) / sizeof(DWORD);
                    break;
                case D3DMFMT_D3DMVALUE_FIXED:
                    pdwData = (DWORD*)(&pFixedValues[uiFixedIndex]);
                    memcpy(&(Light.Diffuse), pdwData, sizeof(D3DMLIGHT)-sizeof(D3DMLIGHTTYPE));
                    uiFixedIndex+=(sizeof(D3DMLIGHT)-sizeof(D3DMLIGHTTYPE)) / sizeof(DWORD);
                    break;
                };

                if (FailedCall(hr = m_pd3dDevice->SetLight(uiCurLightIndex, // DWORD Index
                                                      &Light,          // CONST D3DMLIGHT* pLight
                                                      Format)))        // D3DMFORMAT Format
                {
                    DebugOut(DO_ERROR, L"SetLight failed. (hr = 0x%08x). Failing.", hr);
                    free(pFloatValues);
                    free(pFixedValues);
                    return TPR_FAIL;
                }

                // 
                // Retrieve the light
                // 
                if (FAILED(hr = m_pd3dDevice->GetLight( uiCurLightIndex, // DWORD Index
                                                  &ResultLight,     // D3DMLIGHT* pLight
                                                  &ResultFormat)))  // D3DMFORMAT* pFormat
                {
                    DebugOut(DO_ERROR, L"GetLight failed. (hr = 0x%08x). Failing.", hr);
                    free(pFloatValues);
                    free(pFixedValues);
                    return TPR_FAIL;
                }

                //
                // Compare the input light with the output light,
                // verify that they are identical
                //
                if (0 != memcmp(&Light, &ResultLight, sizeof(D3DMLIGHT)))
                {
                    DebugOut(DO_ERROR, L"Input light does not match output light. Failing.");
                    free(pFloatValues);
                    free(pFixedValues);
                    return TPR_FAIL;
                }

                //
                // Compare the input format with the output format (ensure that they are identical)
                //
                if (Format != ResultFormat)
                {
                    DebugOut(DO_ERROR, L"Input format does not match output format. Failing.");
                    free(pFloatValues);
                    free(pFixedValues);
                    return TPR_FAIL;
                }
            }
        }
    }

    free(pFloatValues);
    free(pFixedValues);

    //
    // Bad parameter testing:
    //

    //
    // Test #1:  NULL D3DMLIGHT pointer w/ D3DMFMT_D3DMVALUE_FLOAT
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetLight(0,                         // DWORD Index
                                         NULL,                      // CONST D3DMLIGHT* pLight
                                         D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
    {
        DebugOut(DO_ERROR, L"SetLight succeeded; failure expected due to NULL D3DMLIGHT. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Test #2:  NULL D3DMLIGHT pointer w/ D3DMFMT_D3DMVALUE_FIXED
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetLight(0,                         // DWORD Index
                                         NULL,                      // CONST D3DMLIGHT* pLight
                                         D3DMFMT_D3DMVALUE_FIXED))) // D3DMFORMAT Format
    {
        DebugOut(DO_ERROR, L"SetLight succeeded; failure expected due to NULL D3DMLIGHT. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetLightTest
//
//   Verify IDirect3DMobileDevice::GetLight
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetLightTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetLightTest.");

    //
    // Args for GetLight
    //
    D3DMLIGHT ResultLight;
    D3DMFORMAT ResultFormat;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Bad parameter tests:
    //

    //
    // Test #1:  NULL D3DMLIGHT pointer
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetLight( 0,             // DWORD Index
                                          NULL,          // D3DMLIGHT* pLight
                                         &ResultFormat)))// D3DMFORMAT* pFormat
    {
        DebugOut(DO_ERROR, L"GetLight succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Test #2:  NULL D3DMFORMAT pointer
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetLight( 0,             // DWORD Index
                                         &ResultLight,   // D3DMLIGHT* pLight
                                          NULL)))        // D3DMFORMAT* pFormat
    {
        DebugOut(DO_ERROR, L"GetLight succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Test #3:  NULL D3DMLIGHT pointer, NULL D3DMFORMAT pointer
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetLight(0,     // DWORD Index
                                         NULL,  // D3DMLIGHT* pLight
                                         NULL)))// D3DMFORMAT* pFormat
    {
        DebugOut(DO_ERROR, L"GetLight succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Test #4:  Invalid light index
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetLight(0,               // DWORD Index
                                         &ResultLight,    // D3DMLIGHT* pLight
                                         &ResultFormat))) // D3DMFORMAT* pFormat
    {
        DebugOut(DO_ERROR, L"GetLight succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // More thorough testing occurs in a closely-related test:
    //
    return ExecuteSetLightTest();
}


//
// ExecuteLightEnableTest
//
//   Verify IDirect3DMobileDevice::LightEnable
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteLightEnableTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteLightEnableTest.");

    //
    // Iterator for light indices
    //
    UINT uiCurLightIndex;

    //
    // Input format for SetLight/LightEnable
    //
    D3DMFORMAT Format;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // If Caps.MaxActiveLights is more than the number below,
    // limit the number of light indices (to be tested) to
    // this hard-coded "reasonable" number:
    //
    CONST UINT uiMaxLightIndices = 100;

    //
    // Number of light indices to attempt to enable simultaneously
    //
    UINT uiNumLightsToTest;

    //
    // Light structure input for SetLight
    //
    D3DMLIGHT Light;

    //
    // Iterates through a pair of possibilities:  (1) pass a valid light to SetLight, or
    // (2) expect LightEnable to automatically generate a light at the specified index.
    //
    UINT uiUseDefault;

    //
    // When exercising LightEnable, enable this many lights beyond the stated maximum,
    // and verify that a failure occurs.
    //
    CONST UINT uiExtraLights = 5;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // See if lighting is supported at all (directional and positional lights
    // are the only ones that are used with SetLight; ambient lighting is
    // set by render state).
    //
    if (!((Caps.VertexProcessingCaps & D3DMVTXPCAPS_DIRECTIONALLIGHTS) ||
          (Caps.VertexProcessingCaps & D3DMVTXPCAPS_POSITIONALLIGHTS)))
    {
        DebugOut(DO_ERROR, L"Skipping SetLight test, because driver supports neither D3DMVTXPCAPS_DIRECTIONALLIGHTS nor D3DMVTXPCAPS_POSITIONALLIGHTS. Skipping.");
        return TPR_SKIP;
    }

    //
    // It has been verified that the driver supports either directional or positional lights
    // (or both); if it has zero set for MaxActiveLights, this is illogical, and is a driver
    // bug worthy of failing this test.
    //
    if (0 == Caps.MaxActiveLights)
    {
        DebugOut(DO_ERROR, L"D3DMCAPS reports support for D3DMVTXPCAPS_DIRECTIONALLIGHTS or D3DMVTXPCAPS_POSITIONALLIGHTS (or both), but zero for MaxActiveLights.  The D3DMCAPS settings are conflicting. Failing.");
        return TPR_FAIL;
    }

    //
    // Either enable as many lights as the driver allows, or some
    // more reasonable number of lights if the driver has a large
    // MaxActiveLights value.
    //
    if (Caps.MaxActiveLights > uiMaxLightIndices)
        uiNumLightsToTest = uiMaxLightIndices;
    else
        uiNumLightsToTest = Caps.MaxActiveLights;

    //
    // Currently, the LightEnable test uses a light that is simply zero'd out; except for the
    // absolute minimum.
    //
    ZeroMemory(&Light, sizeof(D3DMLIGHT));
    Light.Type = D3DMLIGHT_POINT;

    //
    // Iterate through both valid number formats, for LightEnable testing
    //
    for (Format = D3DMFMT_D3DMVALUE_FLOAT; Format <= D3DMFMT_D3DMVALUE_FIXED; (*(ULONG*)(&Format))++)
    {
        //
        // Enable exactly as many lights as are allowed; or, if this number is prohibitively large,
        // cap the iterations at some pre-defined value
        //
        for (uiCurLightIndex = 0; uiCurLightIndex < uiNumLightsToTest; uiCurLightIndex++)
        {
            for (uiUseDefault = 0; uiUseDefault < 2; uiUseDefault++)
            {
                switch (uiUseDefault)
                {
                case 0:
                    //
                    // Expect the Direct3D Mobile middleware to generate a default light
                    // at LightEnable-time.  Or, if this index has been used previously,
                    // the prior settings will be used.
                    //
                    break;
                case 1:
                    //
                    // Specifically create the light for this index
                    //
                    if (FAILED(hr = m_pd3dDevice->SetLight( uiCurLightIndex, // DWORD Index
                                                      &Light,           // CONST D3DMLIGHT* pLight
                                                       Format)))        // D3DMFORMAT Format
                    {
                        DebugOut(DO_ERROR, L"SetLight failed. (hr = 0x%08x). Failing.", hr);
                        return TPR_FAIL;
                    }
                    break;
                }

                //
                // Enable the light at the desired index.
                //
                if (FailedCall(hr = m_pd3dDevice->LightEnable(uiCurLightIndex, // DWORD Index
                                                         TRUE)))          // BOOL Enable
                {
                    DebugOut(DO_ERROR, L"LightEnable failed. (hr = 0x%08x). Failing.", hr);
                    return TPR_FAIL;
                }
            }
        }

        //
        // Disable all lights so that the next iteration has a "clean slate" to begin with
        //
        for (uiCurLightIndex = 0; uiCurLightIndex < uiNumLightsToTest; uiCurLightIndex++)
        {
            //
            // Disable the light at the desired index.
            //
            if (FailedCall(hr = m_pd3dDevice->LightEnable(uiCurLightIndex, // DWORD Index
                                                     FALSE)))         // BOOL Enable
            {
                DebugOut(DO_ERROR, L"LightEnable failed. (hr = 0x%08x). Failing.", hr);
                return TPR_FAIL;
            }
        }
    }

    //
    // If feasable, verify that LightEnable fails, if too many simultaneous lights are attempted.
    //
    if (Caps.MaxActiveLights <= uiMaxLightIndices)
    {
        //
        // Enable exactly as many lights as are allowed
        //
        for (uiCurLightIndex = 0; uiCurLightIndex < Caps.MaxActiveLights; uiCurLightIndex++)
        {
            //
            // Enable the light at the desired index.
            //
            if (FailedCall(hr = m_pd3dDevice->LightEnable(uiCurLightIndex, // DWORD Index
                                                     TRUE)))          // BOOL Enable
            {
                DebugOut(DO_ERROR, L"LightEnable failed. (hr = 0x%08x). Failing.", hr);
                return TPR_FAIL;
            }
        }

        //
        // Verify debug-only check
        //
        if (m_bDebugMiddleware)
        {
            //
            // Enable a few more, and verify that this fails.
            //
            for (uiCurLightIndex = Caps.MaxActiveLights; uiCurLightIndex < (Caps.MaxActiveLights+uiExtraLights); uiCurLightIndex++)
            {

                //
                // Enable the light at the desired index.
                //
                if (SucceededCall(hr = m_pd3dDevice->LightEnable(uiCurLightIndex, // DWORD Index
                                                            TRUE)))          // BOOL Enable
                {
                    DebugOut(DO_ERROR, L"LightEnable succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
                    return TPR_FAIL;
                }

            }
        }
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetLightEnableTest
//
//   Verify IDirect3DMobileDevice::GetLightEnable.
//
//       * Typical case (valid params)
//       * NULL BOOL*
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetLightEnableTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetLightEnableTest.");

    //
    // Iterator for light indices
    //
    UINT uiCurLightIndex;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // If Caps.MaxActiveLights is more than the number below,
    // limit the number of light indices (to be tested) to
    // this hard-coded "reasonable" number:
    //
    CONST UINT uiMaxLightIndices = 100;
    UINT uiMaxIter;

    //
    // Enabled/disabled info from GetLightEnable
    //
    BOOL bEnableResult;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }


    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // See if lighting is supported at all (directional and positional lights
    // are the only ones that are used with SetLight; ambient lighting is
    // set by render state).
    //
    if (!((Caps.VertexProcessingCaps & D3DMVTXPCAPS_DIRECTIONALLIGHTS) ||
          (Caps.VertexProcessingCaps & D3DMVTXPCAPS_POSITIONALLIGHTS)))
    {
        DebugOut(DO_ERROR, L"Skipping SetLight test, because driver supports neither D3DMVTXPCAPS_DIRECTIONALLIGHTS nor D3DMVTXPCAPS_POSITIONALLIGHTS. Skipping.");
        return TPR_SKIP;
    }

    //
    // It has been verified that the driver supports either directional or positional lights
    // (or both); if it has zero set for MaxActiveLights, this is illogical, and is a driver
    // bug worthy of failing this test.
    //
    if (0 == Caps.MaxActiveLights)
    {
        DebugOut(DO_ERROR, L"D3DMCAPS reports support for D3DMVTXPCAPS_DIRECTIONALLIGHTS or D3DMVTXPCAPS_POSITIONALLIGHTS (or both), but zero for MaxActiveLights.  The D3DMCAPS settings are conflicting. Failing.");
        return TPR_FAIL;
    }

    //
    // If the limit on active lights is large; limit test iterations to a smaller value,
    // to ease time constraints for test execution
    //
    uiMaxIter = Caps.MaxActiveLights;
    if (Caps.MaxActiveLights > uiMaxLightIndices)
        uiMaxIter = uiMaxLightIndices;

    //
    // Iterate within the constraint of MaxActiveLights
    //
    for (uiCurLightIndex = 0; uiCurLightIndex < uiMaxIter; uiCurLightIndex++)
    {
        //
        // Enable the light at the desired index.
        //
        if (FAILED(hr = m_pd3dDevice->LightEnable(uiCurLightIndex, // DWORD Index
                                             TRUE)))          // BOOL Enable
        {
            DebugOut(DO_ERROR, L"LightEnable failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        //
        // Retrieve enabled/disabled information for this light index
        //
        if (FAILED(hr = m_pd3dDevice->GetLightEnable( uiCurLightIndex, // DWORD Index
                                                &bEnableResult))) // BOOL* pEnable
        {
            DebugOut(DO_ERROR, L"GetLightEnable failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        //
        // Verify that retrieved value corresponds to set value
        //
        if (TRUE != bEnableResult)
        {
            DebugOut(DO_ERROR, L"GetLightEnable: unexpected result. Failing.");
            return TPR_FAIL;
        }

        //
        // Disable the light at the desired index.
        //
        if (FAILED(hr = m_pd3dDevice->LightEnable(uiCurLightIndex, // DWORD Index
                                             FALSE)))         // BOOL Enable
        {
            DebugOut(DO_ERROR, L"LightEnable failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        //
        // Retrieve enabled/disabled information for this light index
        //
        if (FAILED(hr = m_pd3dDevice->GetLightEnable( uiCurLightIndex, // DWORD Index
                                                &bEnableResult))) // BOOL* pEnable
        {
            DebugOut(DO_ERROR, L"GetLightEnable failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        //
        // Verify that retrieved value corresponds to set value
        //
        if (FALSE != bEnableResult)
        {
            DebugOut(DO_ERROR, L"GetLightEnable: unexpected result. Failing.");
            return TPR_FAIL;
        }
    }

    //
    // Bad parameter testing
    //

    //
    // Test #1:  Pass NULL as BOOL pointer
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetLightEnable(0,      // DWORD Index
                                               NULL))) // BOOL* pEnable
    {
        DebugOut(DO_ERROR, L"GetLightEnable succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Test #2:  Pass non-existent light index as DWORD parameter
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetLightEnable( 0xFFFFFFFF,      // DWORD Index
                                               &bEnableResult))) // BOOL* pEnable
    {
        DebugOut(DO_ERROR, L"GetLightEnable succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteSetRenderStateTest
//
//   Verify IDirect3DMobileDevice::SetRenderState.  Currently, this test is
//   only intended for "stability" testing; e.g., will the driver accept a
//   wide range of values for each render state setting, without a fatal
//   error.
//
//   Testing of SetRenderState is difficult because the results of SetRenderState,
//   as an API, are undefined.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetRenderStateTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetRenderStateTest.");

    //
    // Iterator for render states
    //
    UINT uiRSIter;

    //
    // Value to pass as arg for render state
    //
    DWORD dwValue;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    for(uiRSIter = 0; uiRSIter < D3DM_MAXRENDERSTATES; uiRSIter++)
    {
        //
        // Inclusive range of values to pass to SetRenderState
        //
        DWORD dwBeginRange, dwEndRange;

        switch(uiRSIter)
        {
        case D3DMRS_FILLMODE:
            // D3DMFILLMODE
            dwBeginRange = D3DMFILL_POINT;
            dwEndRange = D3DMFILL_SOLID;
            break;             
            
        case D3DMRS_SHADEMODE:
            // D3DMSHADEMODE
            dwBeginRange = D3DMSHADE_FLAT;
            dwEndRange = D3DMSHADE_GOURAUD;
            break;             
            
        case D3DMRS_ZWRITEENABLE:
            // TRUE/FALSE
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;            
            
        case D3DMRS_ALPHATESTENABLE:
            // TRUE/FALSE
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;            
            
        case D3DMRS_LASTPIXEL:
            // TRUE/FALSE            
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;             
            
        case D3DMRS_SRCBLEND:
            // D3DMBLEND             
            dwBeginRange = D3DMBLEND_ZERO;
            dwEndRange = D3DMBLEND_SRCALPHASAT;
            break;             
            
        case D3DMRS_DESTBLEND:
            // D3DMBLEND            
            dwBeginRange = D3DMBLEND_ZERO;
            dwEndRange = D3DMBLEND_SRCALPHASAT;
            break;               

        case D3DMRS_CULLMODE:
            // D3DMCULL            
            dwBeginRange = D3DMCULL_NONE;
            dwEndRange = D3DMCULL_CCW;
            break;               

        case D3DMRS_ZFUNC:
            // D3DMCMPFUNC            
            dwBeginRange = D3DMCMP_NEVER;
            dwEndRange = D3DMCMP_ALWAYS;
            break;               

        case D3DMRS_ALPHAREF:
            // ULONG, but only the low 8 bits is used
            dwBeginRange = 0;
            dwEndRange = 255;
            break;              
            
        case D3DMRS_ALPHAFUNC:
            // D3DMCMPFUNC          
            dwBeginRange = D3DMCMP_NEVER;
            dwEndRange = D3DMCMP_ALWAYS;
            break;               

        case D3DMRS_DITHERENABLE:
            // TRUE/FALSE         
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;              
            
        case D3DMRS_ALPHABLENDENABLE:
            // TRUE/FALSE    
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;             
            
        case D3DMRS_FOGENABLE:
            // TRUE/FALSE           
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;              
            
        case D3DMRS_SPECULARENABLE:
            // TRUE/FALSE      
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;               

        case D3DMRS_FOGCOLOR:
            // D3DMCOLOR       
            dwBeginRange = 0x80808080;      // Test some mid-range colors (RGBA).  Currently, 
            dwEndRange = 0x80808100;        // this test is not very thorough with colors.
            break;               

        case D3DMRS_FOGTABLEMODE:
            // D3DMFOGMODE       
            dwBeginRange = D3DMFOG_NONE;
            dwEndRange = D3DMFOG_LINEAR;
            break;              
            
        case D3DMRS_FOGSTART:
            // D3DMVALUE         
            dwBeginRange = 0;               // Currently, this test is not very useful
            dwEndRange = 255;               // for float value testing w/ Render States
            break;             
            
        case D3DMRS_FOGEND:
            // D3DMVALUE         
            dwBeginRange = 0;               // Currently, this test is not very useful
            dwEndRange = 255;               // for float value testing w/ Render States
            break;               

        case D3DMRS_FOGDENSITY:
            // D3DMVALUE        
            dwBeginRange = 0;               // Currently, this test is not very useful
            dwEndRange = 255;               // for float value testing w/ Render States
            break;               

        case D3DMRS_RANGEFOGENABLE:
            // TRUE/FALSE       
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;               

        case D3DMRS_STENCILENABLE:
            // TRUE/FALSE      
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;               

        case D3DMRS_STENCILFAIL:
            // D3DMSTENCILOP      
            dwBeginRange = D3DMSTENCILOP_KEEP;
            dwEndRange = D3DMSTENCILOP_DECR;
            break;               

        case D3DMRS_STENCILZFAIL:
            // D3DMSTENCILOP      
            dwBeginRange = D3DMSTENCILOP_KEEP;
            dwEndRange = D3DMSTENCILOP_DECR;
            break;               

        case D3DMRS_STENCILPASS:
            // D3DMSTENCILOP      
            dwBeginRange = D3DMSTENCILOP_KEEP;
            dwEndRange = D3DMSTENCILOP_DECR;
            break;               

        case D3DMRS_STENCILFUNC:
            // D3DMCMPFUNC       
            dwBeginRange = D3DMCMP_NEVER;
            dwEndRange = D3DMCMP_ALWAYS;
            break;               

        case D3DMRS_STENCILREF:
            // ULONG       
            dwBeginRange = 0;               // Integer reference value for the stencil test. 
            dwEndRange = 255;
            break;               

        case D3DMRS_STENCILMASK:
            // ULONG       
            dwBeginRange = 0xFFFFFF00;      // Mask value for stencil test
            dwEndRange = 0xFFFFFFFF;
            break;               

        case D3DMRS_STENCILWRITEMASK:
            // ULONG      
            dwBeginRange = 0xFFFFFF00;      // Mask value for stencil buffer
            dwEndRange = 0xFFFFFFFF;
            break;            
            
        case D3DMRS_TEXTUREFACTOR:
            // D3DMCOLOR      
            dwBeginRange = 0x80808080;      // Test some mid-range colors (RGBA).  Currently, 
            dwEndRange = 0x80808100;        // this test is not very thorough with colors.
            break;           
            
        case D3DMRS_TEXTUREPERSPECTIVE:
            // TRUE/FALSE     
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;           
            
        case D3DMRS_WRAP0:
            // D3DMWRAPCOORD_*    
            dwBeginRange = D3DMWRAPCOORD_0;
            dwEndRange = D3DMWRAPCOORD_1;
            break;            
            
        case D3DMRS_WRAP1:
            // D3DMWRAPCOORD_*     
            dwBeginRange = D3DMWRAPCOORD_0;
            dwEndRange = D3DMWRAPCOORD_1;
            break;            
            
        case D3DMRS_WRAP2:
            // D3DMWRAPCOORD_*      
            dwBeginRange = D3DMWRAPCOORD_0;
            dwEndRange = D3DMWRAPCOORD_1;
            break;           
            
        case D3DMRS_WRAP3:
            // D3DMWRAPCOORD_*     
            dwBeginRange = D3DMWRAPCOORD_0;
            dwEndRange = D3DMWRAPCOORD_1;
            break;          
            
        case D3DMRS_CLIPPING:
            // TRUE/FALSE     
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;          
            
        case D3DMRS_LIGHTING:
            // TRUE/FALSE     
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;         
            
        case D3DMRS_AMBIENT:
            // D3DMCOLOR     
            dwBeginRange = 0x80808080;      // Test some mid-range colors (RGBA).  Currently, 
            dwEndRange = 0x80808100;        // this test is not very thorough with colors.
            break;               

        case D3DMRS_FOGVERTEXMODE:
            // D3DMFOGMODE     
            dwBeginRange = D3DMFOG_NONE;
            dwEndRange = D3DMFOG_LINEAR;
            break;               

        case D3DMRS_COLORVERTEX:
            // TRUE/FALSE     
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;               

        case D3DMRS_LOCALVIEWER:
            // TRUE/FALSE     
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;               

        case D3DMRS_NORMALIZENORMALS:
            // TRUE/FALSE   
            dwBeginRange = FALSE;
            dwEndRange = TRUE;
            break;               

        case D3DMRS_DIFFUSEMATERIALSOURCE:
            // D3DMMATERIALCOLORSOURCE
            dwBeginRange = D3DMMCS_MATERIAL;
            dwEndRange = D3DMMCS_COLOR2;
            break;               

        case D3DMRS_SPECULARMATERIALSOURCE:
            // D3DMMATERIALCOLORSOURCE
            dwBeginRange = D3DMMCS_MATERIAL;
            dwEndRange = D3DMMCS_COLOR2;
            break;               

        case D3DMRS_AMBIENTMATERIALSOURCE:
            // D3DMMATERIALCOLORSOURCE
            dwBeginRange = D3DMMCS_MATERIAL;
            dwEndRange = D3DMMCS_COLOR2;
            break;               

        case D3DMRS_COLORWRITEENABLE:
            // D3DMCOLORWRITEENABLE_*
            dwBeginRange = D3DMCOLORWRITEENABLE_RED;
            dwEndRange = D3DMCOLORWRITEENABLE_ALL;
            break;               

        case D3DMRS_BLENDOP:
            // D3DMBLENDOP 
            dwBeginRange = D3DMBLENDOP_ADD;
            dwEndRange = D3DMBLENDOP_MAX;
            break;               
        }

        //
        // Perform an extra check to prevent the above code from being modified in
        // a bad way (infinite loop).
        //
        if (0xFFFFFFFF == dwEndRange) dwEndRange--;


        for (dwValue = dwBeginRange; dwValue <= dwEndRange; dwValue++)
        {

            //
            // Return values for SetRenderState are undefined.
            //
            m_pd3dDevice->SetRenderState((D3DMRENDERSTATETYPE)uiRSIter, // D3DMRENDERSTATETYPE State
                                         dwValue);                      // DWORD Value
        }
    }


    //
    // Bad parameter test #1:  Invalid D3DMRENDERSTATETYPE
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetRenderState(D3DM_MAXRENDERSTATES, // D3DMRENDERSTATETYPE State
                                               dwValue)))            // DWORD Value
    {
        DebugOut(DO_ERROR, L"SetRenderState succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Force the middleware to flush the buffer, if it has not yet done so.
    //
    if (FAILED(hr = Flush()))
    {
        DebugOut(DO_ERROR, L"Flush failed. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}


//
// ExecuteGetRenderStateTest
//
// Verify IDirect3DMobileDevice::GetRenderState
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetRenderStateTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetRenderStateTest.");

    //
    // Iterator for render states
    //
    UINT uiRSIter;

    //
    // Result of render state query
    //
    DWORD dwResult;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Query every defined render state
    //
    for(uiRSIter = 0; uiRSIter < D3DM_MAXRENDERSTATES; uiRSIter++)
    {
        if (FAILED(hr = m_pd3dDevice->GetRenderState( (D3DMRENDERSTATETYPE)uiRSIter, // D3DMRENDERSTATETYPE State
                                                &dwResult)))                    // DWORD* pValue
        {
            DebugOut(DO_ERROR, L"GetRenderState failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        //
        // Bad parameter testing
        //
        if (SUCCEEDED(hr = m_pd3dDevice->GetRenderState((D3DMRENDERSTATETYPE)uiRSIter, // D3DMRENDERSTATETYPE State
                                                   NULL)))                        // DWORD* pValue
        {
            DebugOut(DO_ERROR, L"GetRenderState succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }
    }

    //
    // Bad parameter test #1:  Invalid render state
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetRenderState(D3DM_MAXRENDERSTATES, // D3DMRENDERSTATETYPE State
                                               &dwResult)))          // DWORD* pValue
    {
        DebugOut(DO_ERROR, L"GetRenderState succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }


    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}


//
// ExecuteSetClipStatusTest
//
//   Verify IDirect3DMobileDevice::SetClipStatus
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetClipStatusTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetClipStatusTest.");

    //
    // Arguments for SetClipStatus, GetClipStatus
    //
    D3DMCLIPSTATUS ClipStatus, ResultClipStatus;

    //
    // Iterators for attempting various clip status data
    //
    ULONG ulClipUnion;
    ULONG ulClipIntersection;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Verify prerequisite of test
    //
    if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_CLIPSTATUS))

    {
        DebugOut(DO_ERROR, L"Skipping SetClipStatus test, because driver does not expose D3DMVTXPCAPS_CLIPSTATUS. Skipping.");
        return TPR_SKIP;
    }

    //
    // Attempt a variety of clip status data
    //
    for (ulClipUnion = 0x00000001; ulClipUnion != 0x80000000; ulClipUnion <<= 1)
    {
        for (ulClipIntersection = 0x00000001; ulClipIntersection != 0x80000000; ulClipIntersection <<= 1)
        {
            ClipStatus.ClipUnion = ulClipUnion;
            ClipStatus.ClipIntersection = ulClipIntersection;

            if (FAILED(hr = m_pd3dDevice->SetClipStatus(&ClipStatus))) // CONST D3DMCLIPSTATUS* pClipStatus
            {
                DebugOut(DO_ERROR, L"SetClipStatus failed. (hr = 0x%08x). Failing.", hr);
                return TPR_FAIL;
            }

            if (FAILED(hr = m_pd3dDevice->GetClipStatus(&ResultClipStatus))) // D3DMCLIPSTATUS* pClipStatus
            {
                DebugOut(DO_ERROR, L"SetClipStatus failed. (hr = 0x%08x). Failing.", hr);
                return TPR_FAIL;
            }

            if (ClipStatus.ClipUnion != ResultClipStatus.ClipUnion)
            {
                DebugOut(DO_ERROR, L"Failure.  Unexpected ClipUnion mismatch. Failing.");
                return TPR_FAIL;
            }

            if (ClipStatus.ClipIntersection != ResultClipStatus.ClipIntersection)
            {
                DebugOut(DO_ERROR, L"Failure.  Unexpected ClipIntersection mismatch. Failing.");
                return TPR_FAIL;
            }
        }
    }

    //
    // Bad parameter testing
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetClipStatus(NULL))) // CONST D3DMCLIPSTATUS* pClipStatus
    {
        DebugOut(DO_ERROR, L"SetClipStatus succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetClipStatusTest
//
//   Verify IDirect3DMobileDevice::GetClipStatus.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetClipStatusTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetClipStatusTest.");

    //
    // Device's clip status
    //
    D3DMCLIPSTATUS ClipStatus;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Verify prerequisite of test
    //
    if (!(Caps.VertexProcessingCaps & D3DMVTXPCAPS_CLIPSTATUS))
    {
        DebugOut(DO_ERROR, L"Skipping SetClipStatus test, because driver does not expose D3DMVTXPCAPS_CLIPSTATUS. Skipping.");
        return TPR_SKIP;
    }

    //
    // Ensure that command-buffer is flushed
    //
    (void)Flush();

    //
    // Retrieve clip status
    //
    if (FAILED(hr = m_pd3dDevice->GetClipStatus(&ClipStatus))) // D3DMCLIPSTATUS* pClipStatus
    {
        DebugOut(DO_ERROR, L"GetClipStatus failed. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter testing
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetClipStatus(NULL))) // D3DMCLIPSTATUS* pClipStatus
    {
        DebugOut(DO_ERROR, L"GetClipStatus succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // More thorough testing occurs in a closely-related test:
    //
    return ExecuteSetClipStatusTest();

}


//
// ExecuteGetTextureTest
//
//   Verify IDirect3DMobileDevice::GetTexture
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetTextureTest()
{
    HRESULT hr;
    INT Result = TPR_PASS;

    DebugOut(L"Beginning ExecuteGetTextureTest.");

    //
    // Texture interface for GetTexture arg
    //
    LPDIRECT3DMOBILEBASETEXTURE pTexture;

    //
    // Caps for determining stage support
    //
    D3DMCAPS Caps;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }
    
    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Aborting.", hr);
        return TPR_ABORT;
    }

    //
    // Bad Parameter Test #1
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetTexture(0,      // DWORD Stage
                                           NULL))) // IDirect3DMobileBaseTexture** ppTexture
    {
        DebugOut(DO_ERROR, L"GetTexture succeeded:  failure expected. (hr = 0x%08x). Failing.", hr);
        SetResult(Result, TPR_FAIL);
    }

    //
    // Bad Parameter Test #2:  Invalid stage
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetTexture(D3DM_TEXSTAGECOUNT, // DWORD Stage
                                           &pTexture)))        // IDirect3DMobileBaseTexture** ppTexture
    {
        DebugOut(DO_ERROR, L"GetTexture succeeded:  failure expected. (hr = 0x%08x). Failing.", hr);
        SetResult(Result, TPR_FAIL);
        SafeRelease(pTexture);
    }

    //
    // If no texture is selected, GetTexture should still succeed.
    //
    for (int i = 0; i < D3DM_TEXSTAGECOUNT; ++i)
    {
        bool SuccessExpected = i < Caps.MaxTextureBlendStages;
        hr = m_pd3dDevice->GetTexture(i, &pTexture);
        if (SuccessExpected && FAILED(hr))
        {
            DebugOut(DO_ERROR, 
                L"GetTexture failed for valid state at stage %d, should return success along with a NULL texture. (hr = 0x%08x). Failing.", 
                i, 
                hr);
            SetResult(Result, TPR_FAIL);
        }
        if (NULL != pTexture)
        {
            DebugOut(DO_ERROR, L"GetTexture returned a non-NULL pointer (%p) for an unset texture at stage %d", pTexture, i);
            SetResult(Result, TPR_FAIL);
        }
    }

    //
    // More thorough testing occurs in a closely-related test:
    //
    return SetResult(Result, ExecuteSetTextureTest());
}


//
// ExecuteSetTextureTest
//
//   Verify IDirect3DMobileDevice::SetTexture
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetTextureTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetTextureTest.");

    //
    // Iterator for texture stages
    //
    UINT uiStage;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // D3DMFORMAT iterator; to cycle through formats until a valid format
    // is found,
    //
    D3DMFORMAT CurrentFormat;

    //
    // Valid pool for textures
    //
    D3DMPOOL TexturePool;

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Texture interface pointers
    //
    IDirect3DMobileTexture* pTexture, *pTexResult;

    //
    // Toggled to TRUE, if a valid texture format is found.
    //
    BOOL bFoundValidFormat = FALSE;


    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode; function returned failure. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Find a valid texture pool
    //
    if (D3DMSURFCAPS_SYSTEXTURE & Caps.SurfaceCaps)
    {
        TexturePool = D3DMPOOL_SYSTEMMEM;
    }
    else if (D3DMSURFCAPS_VIDTEXTURE & Caps.SurfaceCaps)
    {
        TexturePool = D3DMPOOL_VIDEOMEM;
    }
    else
    {
        DebugOut(DO_ERROR, L"D3DMCAPS.SurfaceCaps:  No valid texture pool. Skipping.");
        return TPR_SKIP;
    }

    //
    // Find a valid format for textures
    //
    for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {
        if (!(FAILED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
                                               m_DevType,          // D3DMDEVTYPE DeviceType
                                               Mode.Format,        // D3DMFORMAT AdapterFormat
                                               0,                  // ULONG Usage
                                               D3DMRTYPE_TEXTURE,  // D3DMRESOURCETYPE RType
                                               CurrentFormat))))   // D3DMFORMAT CheckFormat
        {
            bFoundValidFormat = TRUE;
            break;
        }
    }

    if (FALSE == bFoundValidFormat)
    {
        DebugOut(DO_ERROR, L"CheckDeviceFormat indicates zero valid texture formats; although SurfaceCaps indicate texture support. Failing.");
        return TPR_FAIL;
    }

    //
    // Iterate through stages, verifying SetTexture 
    //
    for (uiStage = 0; uiStage < Caps.MaxTextureBlendStages; uiStage++)
    {

        //
        // Create a texture of a valid format, in a valid pool
        //
           if (FAILED(hr = m_pd3dDevice->CreateTexture( D3DQA_DEFAULT_SETTEXTURE_TEXWIDTH,  // UINT Width,
                                                D3DQA_DEFAULT_SETTEXTURE_TEXHEIGHT, // UINT Height,
                                                1,                                  // UINT Levels,
                                                0,                                  // DWORD Usage,
                                                CurrentFormat,                      // D3DFORMAT Format,
                                                TexturePool,                        // D3DMPOOL Pool,
                                               &pTexture)))                         // IDirect3DMobileTexture** ppTexture
        {
            DebugOut(DO_ERROR, L"CreateTexture failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }
        
        //
        // Attach a texture to this stage
        //
        if (FAILED(hr = m_pd3dDevice->SetTexture(uiStage,    // DWORD Stage
                                            pTexture))) // IDirect3DMobileBaseTexture* pTexture
        {
            DebugOut(DO_ERROR, L"SetTexture failed. (hr = 0x%08x). Failing.", hr);
            pTexture->Release();
            return TPR_FAIL;
        }
        
        //
        // Re-attach texture to this stage
        //
        if (FAILED(hr = m_pd3dDevice->SetTexture(uiStage,    // DWORD Stage
                                            pTexture))) // IDirect3DMobileBaseTexture* pTexture
        {
            DebugOut(DO_ERROR, L"SetTexture failed. (hr = 0x%08x). Failing.", hr);
            pTexture->Release();
            return TPR_FAIL;
        }

        //
        // Retrieve an interface pointer to the texture attached to this stage
        //
        if (FAILED(hr = m_pd3dDevice->GetTexture(uiStage,                                     // DWORD Stage
                                            (IDirect3DMobileBaseTexture**)&pTexResult))) // IDirect3DMobileBaseTexture** ppTexture
        {
            DebugOut(DO_ERROR, L"GetTexture failed. (hr = 0x%08x). Failing.", hr);
            pTexture->Release();
            return TPR_FAIL;
        }

        //
        // Verify that GetTexture's texture is the same as the arg for SetTexture;
        //
        if (pTexResult != pTexture)
        {
            DebugOut(DO_ERROR, L"GetTexture: unexpected result. Failing.");
            pTexture->Release();
            pTexResult->Release();
            return TPR_FAIL;
        }

        //
        // Clear texture from stage (reverse effect of previous SetTexture)
        //
        if (FAILED(hr = m_pd3dDevice->SetTexture(uiStage,  // DWORD Stage
                                            NULL)))   // IDirect3DMobileBaseTexture* pTexture
        {
            DebugOut(DO_ERROR, L"SetTexture:  unable to clear texture from stage. (hr = 0x%08x) Failing.", hr);
            pTexture->Release();
            pTexResult->Release();
            return TPR_FAIL;
        }

        //
        // Reverse effect of GetTexture call
        //
        pTexResult->Release();

        //
        // Verify that all reference are gone, after Releasing to reverse effect
        // of CreateTexture
        //
        if (0 != pTexture->Release())
        {
            DebugOut(DO_ERROR, L"IDirect3DMobileTexture:  unexpected dangling reference. Failing.");
            return TPR_FAIL;
        }
    }


    //
    // Bad parameter test #1:  Invalid stage
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetTexture(D3DM_TEXSTAGECOUNT, // DWORD Stage
                                           NULL)))             // IDirect3DMobileBaseTexture* pTexture
    {
        DebugOut(DO_ERROR, L"SetTexture succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetTextureStageStateTest
//
//   Verify IDirect3DMobileDevice::GetTextureStageState
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetTextureStageStateTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetTextureStageStateTest.");

    //
    // Argument for GetTextureStageState
    //
    DWORD dwValue;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Bad parameter tests
    //

    //
    // Test #1: NULL DWORD pointer
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetTextureStageState(0,               // DWORD Stage
                                                     D3DMTSS_COLOROP, // D3DMTEXTURESTAGESTATETYPE Type
                                                     NULL)))          // DWORD* pValue
    {
        DebugOut(DO_ERROR, L"GetTextureStageState succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Test #2: Invalid index
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetTextureStageState(D3DMTSS_MAXTEXTURESTATES, // DWORD Stage
                                                     D3DMTSS_COLOROP,          // D3DMTEXTURESTAGESTATETYPE Type
                                                     &dwValue)))               // DWORD* pValue
    {
        DebugOut(DO_ERROR, L"GetTextureStageState succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }



    //
    // More thorough testing occurs in a closely-related test:
    //
    return ExecuteSetTextureStageStateTest();

}


//
// ExecuteSetTextureStageStateTest
//
//   Verify IDirect3DMobileDevice::SetTextureStageState.  Currently, this
//   code is only a stability test.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetTextureStageStateTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetTextureStageStateTest.");

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // Iterator for texture stage state index
    //
    UINT uiTexStage;

    //
    // Iterator for lookup table of state types
    //
    UINT uiStateType;

    //
    // Iterator for lookup table of values
    //
    UINT uiArgSet;

    //
    // Result of GetTextureStage
    //
    DWORD dwTexStageResult;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Verify that the related D3DMCAPS setting is valid
    //
    if (Caps.MaxTextureBlendStages > D3DM_TEXSTAGECOUNT)
    {
        DebugOut(DO_ERROR, L"Invalid setting for D3DMCAPS.MaxTextureBlendStages ( > D3DMTSS_MAXTEXTURESTATES). Failing.");
        return TPR_FAIL;
    }


    //
    // Iterate beyond the valid range; for bad parameter testing
    //
    for(uiTexStage = 0; uiTexStage < (Caps.MaxTextureBlendStages*2); uiTexStage++)
    {   
        //
        // Iterate through all valid texture stage state types (pre-defined argument sets are indexed herein)
        //
        for(uiStateType = 0; uiStateType < D3DQA_NUM_SETTEXTURESTAGESTATEARGS; uiStateType++)
        {
            //
            // For this particular texture stage state and state type, attempt various arguments
            //
            for(uiArgSet = 0; uiArgSet < TexStageStateArgs[uiStateType].ulNumEntries; uiArgSet++)
            {
                m_pd3dDevice->SetTextureStageState(uiTexStage,                                           // DWORD Stage
                                                   TexStageStateArgs[uiStateType].StateType,             // D3DMTEXTURESTAGESTATETYPE Type
                                                   TexStageStateArgs[uiStateType].pdwValues[uiArgSet]);  // DWORD Value

                m_pd3dDevice->GetTextureStageState(uiTexStage,                              // DWORD Stage
                                                   TexStageStateArgs[uiStateType].StateType,// D3DMTEXTURESTAGESTATETYPE Type
                                                   &dwTexStageResult);                      // DWORD* pValue

                //
                // Ensure that tokens are flushed to driver
                //
                Flush();
            }
        }
    }

    //
    // Bad parameter test #1:  Invalid D3DMTEXTURESTAGESTATETYPE
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetTextureStageState(D3DM_TEXSTAGECOUNT,       // DWORD Stage
                                                     D3DMTSS_COLORARG1,        // D3DMTEXTURESTAGESTATETYPE Type
                                                     0)))                      // DWORD Value
    {
        DebugOut(DO_ERROR, L"SetTextureStageState succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter test #2:  Invalid stage state
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetTextureStageState(0,                       // DWORD Stage
                                                     D3DMTSS_MAXTEXTURESTATES,// D3DMTEXTURESTAGESTATETYPE Type
                                                     0)))                     // DWORD Value
    {
        DebugOut(DO_ERROR, L"SetTextureStageState succeeded unexpectedly. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteValidateDeviceTest
//
//   Verify IDirect3DMobileDevice::ValidateDevice.  Currently, this test
//   is very simple.  It causes a flush, via ValidateDevice, then calls
//   ValidateDevice a second time, expecting a successful return value.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteValidateDeviceTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteValidateDeviceTest.");

    //
    // Argument for ValidateDevice (unused by this function)
    //
    DWORD dwScratch;

    //
    // Test Result
    //
    INT Result = TPR_PASS;

    //
    // Vertex buffer
    //
    LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;


    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Cause a flush
    //
    Flush();

    //
    // Confirm that ValidateDevice fails when no VB stream source is set
    //
    if (SUCCEEDED(hr = m_pd3dDevice->ValidateDevice(&dwScratch)))
    {
        DebugOut(DO_ERROR, L"ValidateDevice succeeded despite no valid vertex input stream setting. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    pVB = CreateActiveBuffer( m_pd3dDevice,     // LPDIRECT3DMOBILEDEVICE pd3dDevice,
                              1,                // UINT uiNumVerts,
                              D3DMFVF_XYZ_FLOAT,// DWORD dwFVF,
                              sizeof(float)*3,  // DWORD dwFVFSize,
                              0);               // DWORD dwUsage
    if (NULL == pVB)
    {
        DebugOut(DO_ERROR, L"ValidateDevice test needs to be able to create vertex buffer to continue; failing. Aborting.");
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Confirm that ValidateDevice succeeds when a VB stream source is set
    //
    if (FAILED(hr = m_pd3dDevice->ValidateDevice(&dwScratch)))
    {
        DebugOut(DO_ERROR, L"ValidateDevice failed despite a valid vertex input stream setting. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Bad-parameter tests
    //
    // Test #1:  Invalid pointer argument
    //
    if (SUCCEEDED(hr = m_pd3dDevice->ValidateDevice(NULL)))
    {
        DebugOut(DO_ERROR, L"ValidateDevice succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }


cleanup:

    if (m_pd3dDevice)
        m_pd3dDevice->SetStreamSource(0,             // UINT StreamNumber
                                      NULL,          // IDirect3DMobileVertexBuffer* pStreamData
                                      0);            // UINT Stride

    if (pVB)
        pVB->Release();

    return Result;

}


//
// ExecuteGetInfoTest
//
//   Verify IDirect3DMobileDevice::GetInfo
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetInfoTest()
{
    DebugOut(L"Beginning ExecuteGetInfoTest.");

    //
    // Device Info ID
    //
    DWORD dwDevInfoID;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }


    //
    // Attempt a wide range of DevInfoIDs (all powers-of-two within the
    // range of a DWORD).  The particular values selected here are arbitrary;
    // the intent is simply to exercise a wide selection of values.
    //
    for(dwDevInfoID = 0x00000001; dwDevInfoID != 0x80000000; dwDevInfoID <<= 1)
    {
        //
        // Provide a NULL struct pointer, and a zero-length indicator.  In most
        // cases, this should cause the driver to return a failure.
        //
        // However, there are some possible situations where a driver could accept
        // this (e.g., if it is using the dev id to toggle something, and needs
        // no output storage).
        //
        // Thus, the API will simply be called, with no logic on the return value.
        //
        m_pd3dDevice->GetInfo(dwDevInfoID, // DWORD DevInfoID
                              NULL,        // void* pDevInfoStruct
                              0);          // DWORD DevInfoStructSize
    }

    //
    // Bad-parameter tests
    //
    // Test #1:  NULL pointer, greater-than-zero struct size
    //
    m_pd3dDevice->GetInfo(0xFFFFFFFF,  // DWORD DevInfoID
                          NULL,        // void* pDevInfoStruct
                          1);          // DWORD DevInfoStructSize

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteSetPaletteEntriesTest
//
//   Verify IDirect3DMobileDevice::SetPaletteEntries
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetPaletteEntriesTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetPaletteEntriesTest.");

    //
    // The palette entries to set
    // 
    PALETTEENTRY Colors[D3DQA_NUM_PALETTE_ENTRIES];

    //
    // Storage for retrieved palette (GetPaletteEntries)
    //
    PALETTEENTRY ColorsRetrieved[D3DQA_NUM_PALETTE_ENTRIES];

    //
    // Used to generate contrived data for palette entries
    //
    UINT uiIter;
    BYTE bCurrent;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Generate some contrived data for a palette
    //
    for(bCurrent = 0, uiIter = 0; uiIter < D3DQA_NUM_PALETTE_ENTRIES; uiIter++)
    {
        Colors[uiIter].peRed = bCurrent++;
        Colors[uiIter].peGreen = bCurrent++;
        Colors[uiIter].peBlue = bCurrent++;
        Colors[uiIter].peFlags = bCurrent++;
    }

    //
    // Set the palette in a particular entry
    //
    if (FailedCall(hr = m_pd3dDevice->SetPaletteEntries(0,         // UINT PaletteNumber
                                                   Colors)))  // CONST PALETTEENTRY* pEntries
    {
        DebugOut(DO_ERROR, L"Fatal error; SetPaletteEntries failed, success expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve the palette (should have been previously set)
    //
    if (FailedCall(hr = m_pd3dDevice->GetPaletteEntries(0,                  // UINT PaletteNumber
                                                   ColorsRetrieved)))  // PALETTEENTRY* pEntries
    {
        DebugOut(DO_ERROR, L"Fatal error; GetPaletteEntries failed, success expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Verify palette match.  Return value of 0 indicates buf1 identical to buf2.
    //
    if (0 != memcmp(ColorsRetrieved, Colors, sizeof(PALETTEENTRY)*D3DQA_NUM_PALETTE_ENTRIES))
    {
        DebugOut(DO_ERROR, L"Fatal error; Palette entry mismatch. Failing.");
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetPaletteEntriesTest
//
//   Verify IDirect3DMobileDevice::GetPaletteEntries
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetPaletteEntriesTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetPaletteEntriesTest.");

    //
    // Iterator for selecting a palette index to manipulate
    //
    UINT uiPaletteIterator;

    //
    // Storage for retrieved palette (GetPaletteEntries)
    //
    PALETTEENTRY Colors[D3DQA_NUM_PALETTE_ENTRIES];

    //
    // Clear palette entries (for the purposes of this interface test,
    // the specific palette data does not matter)
    //
    ZeroMemory(Colors, sizeof(PALETTEENTRY)*D3DQA_NUM_PALETTE_ENTRIES);

    //
    // Top of range, for palette indices, to be used by this test.
    // The choice is somewhat arbitrary, as long as it allows
    // a wide range of palette indices to be exercised, and does not
    // cause a prohibitively long test execution time.
    //
    CONST UINT uiMaxPaletteIndex = 100;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    for (uiPaletteIterator = 0; uiPaletteIterator < uiMaxPaletteIndex; uiPaletteIterator++)
    {
        //
        // Set the palette to NULL, for a particular index
        //
        if (FAILED(hr = m_pd3dDevice->SetPaletteEntries(uiPaletteIterator, // UINT PaletteNumber
                                                   Colors)))          // CONST PALETTEENTRY* pEntries
        {
            DebugOut(DO_ERROR, L"SetPaletteEntries failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        //
        // Attempt to retrieve the palette (previously NULL'd out by this test)
        //
        if (FAILED(hr = m_pd3dDevice->GetPaletteEntries(uiPaletteIterator, // UINT PaletteNumber
                                                   Colors)))          // PALETTEENTRY* pEntries
        {
            DebugOut(DO_ERROR, L"GetPaletteEntries failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }
    }

    //
    // Bad parameter test #1:  NULL PALETTEENTRY*
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetPaletteEntries(0,       // UINT PaletteNumber
                                                  NULL)))  // PALETTEENTRY* pEntries
    {
        DebugOut(DO_ERROR, L"GetPaletteEntries succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Bad parameter test #2:  Invalid palette index
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetPaletteEntries(0xFFFFFFFF, // UINT PaletteNumber
                                                  Colors)))   // PALETTEENTRY* pEntries
    {
        DebugOut(DO_ERROR, L"GetPaletteEntries succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteSetCurrentTexturePaletteTest
//
//   Verify IDirect3DMobileDevice::SetCurrentTexturePalette
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetCurrentTexturePaletteTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetCurrentTexturePaletteTest.");

    //
    // The palette entries to set
    // 
    PALETTEENTRY Colors[D3DQA_NUM_PALETTE_ENTRIES];

    //
    // Arbitrary selections for palette indices to manipulate in this test.
    //
    CONST UINT uiPaletteEntryToUseAsValid = 0;
    CONST UINT uiPaletteEntryToUseAsInvalid = (UINT)(-1); // maximum value representable in UINT data type

    //
    // Used to generate contrived data for palette entries
    //
    UINT uiIter;
    BYTE bCurrent;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Generate some contrived data for a palette
    //
    for(bCurrent = 0, uiIter = 0; uiIter < D3DQA_NUM_PALETTE_ENTRIES; uiIter++)
    {
        Colors[uiIter].peRed = bCurrent++;
        Colors[uiIter].peGreen = bCurrent++;
        Colors[uiIter].peBlue = bCurrent++;
        Colors[uiIter].peFlags = bCurrent++;
    }

    //
    // Set the palette in a particular entry
    //
    if (FailedCall(hr = m_pd3dDevice->SetPaletteEntries(uiPaletteEntryToUseAsValid, // UINT PaletteNumber
                                                   Colors)))                   // CONST PALETTEENTRY* pEntries
    {
        DebugOut(DO_ERROR, L"Fatal error; SetPaletteEntries failed, success expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Verify that the texture palette can be set to a valid palette entry
    //
    if (FailedCall(hr = m_pd3dDevice->SetCurrentTexturePalette(uiPaletteEntryToUseAsValid))) // UINT PaletteNumber
    {
        DebugOut(DO_ERROR, L"Fatal error; SetCurrentTexturePalette failed, success expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // When setting the texture palette to an invalid index, a failure is expected.
    //
    if (SucceededCall(hr = m_pd3dDevice->SetCurrentTexturePalette(uiPaletteEntryToUseAsInvalid))) // UINT PaletteNumber
    {
        DebugOut(DO_ERROR, L"SetCurrentTexturePalette succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetCurrentTexturePaletteTest
//
//   Verify IDirect3DMobileDevice::GetCurrentTexturePalette
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetCurrentTexturePaletteTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetCurrentTexturePaletteTest.");

    //
    // The palette entries to set
    // 
    PALETTEENTRY Colors[D3DQA_NUM_PALETTE_ENTRIES];

    //
    // Palette index to target; palette index reported
    //
    UINT uiCurrentTexPalette;
    UINT uiReportedTexPalette;

    //
    // Used to generate contrived data for palette entries
    //
    UINT uiIter;
    BYTE bCurrent;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Verify that GetCurrentTexturePalette gracefully handles the
    // NULL pointer case
    //
    if (!(FAILED(hr = m_pd3dDevice->GetCurrentTexturePalette(NULL)))) // UINT *PaletteNumber
    {
        DebugOut(DO_ERROR, L"Fatal error; GetCurrentTexturePalette succeeded with NULL pointer, failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Check texture palette default
    //
    if (FAILED(hr = m_pd3dDevice->GetCurrentTexturePalette(&uiReportedTexPalette))) // UINT *PaletteNumber
    {
        DebugOut(DO_ERROR, L"GetCurrentTexturePalette failed. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }
    if (uiReportedTexPalette != 0)
    {
        DebugOut(DO_ERROR, L"GetCurrentTexturePalette resulted in unexpected default palette . Failing.");
        return TPR_FAIL;
    }
    if (FAILED(hr = m_pd3dDevice->GetPaletteEntries(uiReportedTexPalette,     // UINT PaletteNumber
                                               (PALETTEENTRY*)Colors))) // PALETTEENTRY* pEntries
    {
        DebugOut(DO_ERROR, L"GetPaletteEntries failed. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }
    for(bCurrent = 0, uiIter = 0; uiIter < D3DQA_NUM_PALETTE_ENTRIES; uiIter++)
    {
        if ((0xFF != Colors[uiIter].peRed)   || 
            (0xFF != Colors[uiIter].peGreen) || 
            (0xFF != Colors[uiIter].peBlue ) || 
            (0xFF != Colors[uiIter].peFlags))
        {
            DebugOut(DO_ERROR, L"Default PALETTEENTRY mismatches 0xFFFFFFFF. Failing.");
            return TPR_FAIL;
        }
    }

    //
    // Generate some contrived data for a palette
    //
    for(bCurrent = 0, uiIter = 0; uiIter < D3DQA_NUM_PALETTE_ENTRIES; uiIter++)
    {
        Colors[uiIter].peRed = bCurrent++;
        Colors[uiIter].peGreen = bCurrent++;
        Colors[uiIter].peBlue = bCurrent++;
        Colors[uiIter].peFlags = bCurrent++;
    }

    //
    // Attempt a variety of palette ordinals; upper limit is arbitrary but reasonable.
    // A factor is a reference driver implementation detail that implies a lot of memory
    // usage for high texture palette indices
    //
    for (uiCurrentTexPalette = 1; uiCurrentTexPalette < 16; uiCurrentTexPalette++ )
    {
        //
        // Set the palette in a particular entry
        //
        if (FailedCall(hr = m_pd3dDevice->SetPaletteEntries(uiCurrentTexPalette, // UINT PaletteNumber
                                                       Colors)))            // CONST PALETTEENTRY* pEntries
        {
            DebugOut(DO_ERROR, L"Fatal error; SetPaletteEntries failed, success expected. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        //
        // Verify that the texture palette can be set to a valid palette entry
        //
        if (FailedCall(hr = m_pd3dDevice->SetCurrentTexturePalette(uiCurrentTexPalette))) // UINT PaletteNumber
        {
            DebugOut(DO_ERROR, L"Fatal error; SetCurrentTexturePalette failed, success expected. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        if (FailedCall(hr = m_pd3dDevice->GetCurrentTexturePalette(&uiReportedTexPalette))) // UINT *PaletteNumber
        {
            DebugOut(DO_ERROR, L"Fatal error; GetCurrentTexturePalette failed, success expected. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        if (uiReportedTexPalette != uiCurrentTexPalette)
        {
            DebugOut(DO_ERROR, L"Fatal error; GetCurrentTexturePalette resulted in an unexpected palette index. Failing.");
            return TPR_FAIL;
        }

    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteDrawPrimitiveTest
//
//   Verify IDirect3DMobileDevice::DrawPrimitive
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteDrawPrimitiveTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteDrawPrimitiveTest.");

    //
    // Args for componentized test case library
    //
    TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,NULL,0,&m_d3dpp,0};

    //
    // Iterator to exercise each primitive type
    //
    D3DMPRIMITIVETYPE PrimitiveType;

    //
    // Test result
    //
    INT Result = TPR_PASS;

    //
    // Vertex buffer interface pointer
    //
    IDirect3DMobileVertexBuffer* pVertexBuffer = NULL;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // Valid pool for vertex buffers
    //
    D3DMPOOL VertexBufPool;

    //
    // Iterator for testing sets of bad vertices
    //
    UINT uiBadVertIter;

    //
    // Vertex format
    //
    #define D3DQA_DPTEST_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE
    struct D3DQA_DPTEST
    {
        FLOAT x, y, z, rhw;
        DWORD Diffuse;
    };

    //
    // Maximum vertex offset to test with DrawPrimitive; iterator
    //
    CONST UINT uiMaxStartVertex = 5;
    UINT uiStartVertex;

    //
    // Beyond last "start vertex", must have enough verts to finish a triangle
    //
    CONST UINT uiNumVerts = uiMaxStartVertex + 2; 

    //
    // Overall VB size
    //
    UINT uiVBSize = uiNumVerts*BytesPerVertex(D3DQA_DPTEST_FVF);

    //
    // VB-related variables
    //
    D3DQA_DPTEST *pVertexBufferData;
    D3DQA_DPTEST VertexTemplate = {1.0f,1.0f,1.0f,1.0f,D3DMCOLOR_XRGB(0xFF,0x00,0x00)};

    //
    // Iterator for filling VB
    //
    UINT uiIter;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_SYSTEMMEM;
    }
    else if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_VIDEOMEM;
    }
    else
    {
        DebugOut(DO_ERROR, L"Fatal error; no pool supported for vertex buffers in D3DMCAPS.SurfaceCaps. Failing.");
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Create the vertex buffer
    //
    if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(uiVBSize,         // UINT Length
                                                0,                // DWORD Usage
                                                D3DQA_DPTEST_FVF, // DWORD FVF
                                                VertexBufPool,    // D3DMPOOL Pool
                                               &pVertexBuffer)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
    {
        DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Vertex buffer created successfully; attempt a lock.
    //
    if (FAILED(hr = pVertexBuffer->Lock(    0,                           // OffsetToLock
                                    uiVBSize,                    // UINT SizeToLock
                                    (VOID **)&pVertexBufferData, // BYTE** ppbData
                                    0)))                         // DWORD Flags
    {
        DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Lock failed, success expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Fill vertex buffer with valid data (albeit data that isn't very useful for drawing real-world primitives)
    //
    for(uiIter = 0; uiIter < uiNumVerts; uiIter++)
        memcpy(&pVertexBufferData[uiIter], &VertexTemplate, sizeof(D3DQA_DPTEST));

    //
    // Vertex buffer locked successfully; attempt a unlock.
    //
    if (FAILED(hr = pVertexBuffer->Unlock()))
    {
        DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Unlock failed, success expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Indicate intent to use vertex buffer
    //
    if (FAILED(hr = m_pd3dDevice->SetStreamSource(0,             // UINT StreamNumber
                                             pVertexBuffer, // IDirect3DMobileVertexBuffer* pStreamData
                                             0)))           // UINT Stride
    {
        DebugOut(DO_ERROR, L"SetStreamSource failed. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Begin the scene
    //
    if (FAILED(hr = m_pd3dDevice->BeginScene()))
    {
        DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Call DrawPrimitive with various arguments
    //
    for (PrimitiveType = D3DMPT_POINTLIST; PrimitiveType <= D3DMPT_TRIANGLEFAN; (*(ULONG*)(&PrimitiveType))++)
    {
        for (uiStartVertex = 0; uiStartVertex < uiMaxStartVertex; uiStartVertex++)
        {

            if (FAILED(hr = m_pd3dDevice->DrawPrimitive( PrimitiveType, // D3DMPRIMITIVETYPE PrimitiveType
                                                    uiStartVertex, // UINT StartVertex
                                                    1)))           // UINT PrimitiveCount
            {
                DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x). Failing.", hr);
                Result = TPR_FAIL;
                goto cleanup;
            }
        }
    }

    //
    // Leave the scene
    //
    if (FAILED(hr = m_pd3dDevice->EndScene()))
    {
        DebugOut(DO_ERROR, L"EndScene failed. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Present the scene
    //
    if (FAILED(hr = m_pd3dDevice->Present(NULL,NULL,NULL,NULL)))
    {
        DebugOut(DO_ERROR, L"Present failed. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Bad-parameter tests
    //
    // Test #1:  Invalid primitive type
    //

    if (SUCCEEDED(hr = m_pd3dDevice->DrawPrimitive( (D3DMPRIMITIVETYPE)0xFFFFFFFF, // D3DMPRIMITIVETYPE PrimitiveType
                                               0,                             // UINT StartVertex
                                               1)))                           // UINT PrimitiveCount
    {
        DebugOut(DO_ERROR, L"DrawPrimitive succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Test #2:  Invalid primitive count
    //
    if (SUCCEEDED(hr = m_pd3dDevice->DrawPrimitive( D3DMPT_POINTLIST, // D3DMPRIMITIVETYPE PrimitiveType
                                               0,                // UINT StartVertex
                                               0)))              // UINT PrimitiveCount
    {
        DebugOut(DO_ERROR, L"DrawPrimitive succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Test #3:  Valid arguments; but not calling from within a scene
    //
    if (SUCCEEDED(hr = m_pd3dDevice->DrawPrimitive( D3DMPT_POINTLIST, // D3DMPRIMITIVETYPE PrimitiveType
                                               0,                // UINT StartVertex
                                               1)))              // UINT PrimitiveCount
    {
        DebugOut(DO_ERROR, L"DrawPrimitive succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Test #4:  Render primitives that are beyond viewport extents.
    //           Driver requirement:  don't crash.
    //
    for (uiBadVertIter = 0; uiBadVertIter < D3DQAID_BADVERTTEST_COUNT; uiBadVertIter++)
    {
        TestCaseArgs.dwTestIndex = uiBadVertIter;
        (VOID)BadVertTest(&TestCaseArgs);
    }

    //
    // Test #5:  DrawPrimitive with no active vertex stream
    //

    //
    // Clear active vertex stream
    //
    if (FAILED(hr = m_pd3dDevice->SetStreamSource(0,             // UINT StreamNumber
                                             NULL,          // IDirect3DMobileVertexBuffer* pStreamData
                                             0)))           // UINT Stride
    {
        DebugOut(DO_ERROR, L"SetStreamSource failed. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Begin the scene
    //
    if (FAILED(hr = m_pd3dDevice->BeginScene()))
    {
        DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    if (SUCCEEDED(hr = m_pd3dDevice->DrawPrimitive( D3DMPT_POINTLIST, // D3DMPRIMITIVETYPE PrimitiveType
                                               0,                // UINT StartVertex
                                               1)))              // UINT PrimitiveCount
    {
        DebugOut(DO_ERROR, L"DrawPrimitive succeeded with no active vertex stream. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Leave the scene
    //
    if (FAILED(hr = m_pd3dDevice->EndScene()))
    {
        DebugOut(DO_ERROR, L"EndScene failed. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }



cleanup:
    if (m_pd3dDevice)
        m_pd3dDevice->SetStreamSource(0,             // UINT StreamNumber
                                      NULL,          // IDirect3DMobileVertexBuffer* pStreamData
                                      0);            // UINT Stride

    if (pVertexBuffer)
        pVertexBuffer->Release();

    return Result;

}


//
// ExecuteDrawIndexedPrimitiveTest
//
//   Verify IDirect3DMobileDevice::DrawIndexedPrimitive
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteDrawIndexedPrimitiveTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteDrawIndexedPrimitiveTest.");

    //
    // Test result
    //
    INT Result = TPR_PASS;

    //
    // Pool for index buffer creation
    //
    D3DMPOOL IndexBufPool;

    //
    // Index buffer for testing purposes
    //
    LPDIRECT3DMOBILEINDEXBUFFER pIndexBuf = NULL;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Pick the pool that the driver supports, as reported in D3DMCAPS
    //
    if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
    {
        IndexBufPool = D3DMPOOL_VIDEOMEM;
    }
    else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
    {
        IndexBufPool = D3DMPOOL_SYSTEMMEM;
    }
    else
    {
        DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER. Aborting.");
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Create a new index buffer
    //
    if (FAILED(hr = m_pd3dDevice->CreateIndexBuffer(D3DQA_DEFAULT_INDEXBUF_SIZE, // UINT Length
                                               0,                           // DWORD Usage
                                               D3DMFMT_INDEX16,             // D3DMFORMAT Format
                                               IndexBufPool,                // D3DMPOOL Pool
                                              &pIndexBuf)))                 // IDirect3DMobileIndexBuffer** ppIndexBuffer
    {
        DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Bad parameter test #1:  DrawIndexPrimitive with no prior call to SetIndices
    //
    if (SUCCEEDED(hr = m_pd3dDevice->DrawIndexedPrimitive(D3DMPT_POINTLIST, // D3DMPRIMITIVETYPE
                                                     0,                // INT BaseVertexIndex
                                                     0,                // UINT minIndex
                                                     1,                // UINT NumVertices
                                                     0,                // UINT startIndex
                                                     1)))              // UINT primCount
    {
        DebugOut(DO_ERROR, L"DrawIndexedPrimitive succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Set as active index buffer
    //
    if (FAILED(hr = m_pd3dDevice->SetIndices(pIndexBuf))) // IDirect3DMobileIndexBuffer* pIndexData
    {
        DebugOut(DO_ERROR, L"SetIndices failed. (hr = 0x%08x). Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Bad parameter test #2:  Invalid primitive type
    //
    if (SUCCEEDED(hr = m_pd3dDevice->DrawIndexedPrimitive((D3DMPRIMITIVETYPE)0xFFFFFFFF, // D3DMPRIMITIVETYPE
                                                     0,                             // INT BaseVertexIndex
                                                     0,                             // UINT minIndex
                                                     1,                             // UINT NumVertices
                                                     0,                             // UINT startIndex
                                                     1)))                           // UINT primCount
    {
        DebugOut(DO_ERROR, L"DrawIndexedPrimitive succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }


    //
    // Bad parameter test #3:  Invalid number of vertices
    //
    if (SUCCEEDED(hr = m_pd3dDevice->DrawIndexedPrimitive(D3DMPT_POINTLIST, // D3DMPRIMITIVETYPE
                                                     0,                // INT BaseVertexIndex
                                                     0,                // UINT minIndex
                                                     0,                // UINT NumVertices
                                                     0,                // UINT startIndex
                                                     1)))              // UINT primCount
    {
        DebugOut(DO_ERROR, L"DrawIndexedPrimitive succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Bad parameter test #4:  Not in a scene
    //
    if (SUCCEEDED(hr = m_pd3dDevice->DrawIndexedPrimitive(D3DMPT_POINTLIST, // D3DMPRIMITIVETYPE
                                                     0,                // INT BaseVertexIndex
                                                     0,                // UINT minIndex
                                                     1,                // UINT NumVertices
                                                     0,                // UINT startIndex
                                                     1)))              // UINT primCount
    {
        DebugOut(DO_ERROR, L"DrawIndexedPrimitive succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

cleanup:

    if (pIndexBuf)
        pIndexBuf->Release();

    m_pd3dDevice->SetIndices(NULL);
    
    return Result;

}


//
// ExecuteProcessVerticesTest
//
//   Verify IDirect3DMobileDevice::ProcessVertices
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteProcessVerticesTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteProcessVerticesTest.");

    //
    // All failure conditions set this value appropriately
    //
    INT Result = TPR_PASS;

    //
    // Vertex buffer interface pointers
    //
    IDirect3DMobileVertexBuffer* pVertexBufferSrc = NULL;
    IDirect3DMobileVertexBuffer* pVertexBufferDst = NULL;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // Valid pool for vertex buffers
    //
    D3DMPOOL VertexBufPool;

    //
    // source/dest vertex format
    //
    #define D3DQA_PVTEST_SRCFVF D3DMFVF_XYZ_FLOAT
    struct D3DQA_PVTEST_SRC
    {
        FLOAT x, y, z;
    };
    #define D3DQA_PVTEST_DSTFVF D3DMFVF_XYZRHW_FLOAT
    struct D3DQA_PVTEST_DST
    {
        FLOAT x, y, z, rhw;
    };

    DWORD rgVBFlags[] = {
        0,
        D3DMUSAGE_DONOTCLIP,
        D3DMUSAGE_DYNAMIC,
        D3DMUSAGE_WRITEONLY,
        D3DMUSAGE_DONOTCLIP | D3DMUSAGE_DYNAMIC,
        D3DMUSAGE_DONOTCLIP | D3DMUSAGE_WRITEONLY,
        D3DMUSAGE_DYNAMIC | D3DMUSAGE_WRITEONLY,
        D3DMUSAGE_DONOTCLIP | D3DMUSAGE_DYNAMIC | D3DMUSAGE_WRITEONLY,
    };

    //
    // Maximum vertex offset to test with ProcessVertices; iterator
    //
    CONST UINT uiNumVerts = 5;
    UINT uiStartVertex;

    //
    // Overall VB size
    //
    UINT uiVBSizeSrc = uiNumVerts*BytesPerVertex(D3DQA_PVTEST_SRCFVF);
    UINT uiVBSizeDst = uiNumVerts*BytesPerVertex(D3DQA_PVTEST_DSTFVF);

    //
    // VB-related variables
    //
    D3DQA_PVTEST_SRC *pVertexBufferSrcData = NULL;
    D3DQA_PVTEST_SRC *pVertexBufferDstData = NULL;
    D3DQA_PVTEST_SRC VertexTemplateSrc = {1.0f,1.0f,1.0f};

    //
    // Iterator for filling VB
    //
    UINT uiIter;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_SYSTEMMEM;
    }
    else if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_VIDEOMEM;
    }
    else
    {
        DebugOut(DO_ERROR, L"Fatal error; no pool supported for vertex buffers in D3DMCAPS.SurfaceCaps. Failing.");
        Result = TPR_FAIL;
        goto cleanup;
    }

    if (FAILED(hr = m_pd3dDevice->SetRenderState(D3DMRS_LIGHTING, FALSE)))
    {
        DebugOut(DO_ERROR, L"Fatal error setting renderstate LIGHTING to false. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }
    if (FAILED(hr = m_pd3dDevice->SetRenderState(D3DMRS_CLIPPING, FALSE)))
    {
        DebugOut(DO_ERROR, L"Fatal error setting renderstate LIGHTING to false. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    for (int i = 0; i < sizeof(rgVBFlags) / sizeof(*rgVBFlags); i++)
    {
        DebugOut(L"Testing ProcessVertices with usage flags: 0x%08x", rgVBFlags[i]);
        //
        // Make sure the buffers we're using are ready
        //
        SafeRelease(pVertexBufferSrc);
        SafeRelease(pVertexBufferDst);

        //
        // Create the vertex buffers
        //
        // Source buffer:
        //
        if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(uiVBSizeSrc,         // UINT Length
                                                    rgVBFlags[i],                   // DWORD Usage
                                                    D3DQA_PVTEST_SRCFVF, // DWORD FVF
                                                    VertexBufPool,       // D3DMPOOL Pool
                                                   &pVertexBufferSrc)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
        {
            DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }

        //
        // Destination buffer:
        //
        if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(uiVBSizeDst,         // UINT Length
                                                    rgVBFlags[i],                   // DWORD Usage
                                                    D3DQA_PVTEST_DSTFVF, // DWORD FVF
                                                    VertexBufPool,       // D3DMPOOL Pool
                                                   &pVertexBufferDst)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
        {
            DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }

        //
        // Vertex buffer created successfully; attempt a lock.
        //
        if (FAILED(hr = pVertexBufferSrc->Lock(0,                              // OffsetToLock
                                          uiVBSizeSrc,                    // UINT SizeToLock
                                          (VOID **)&pVertexBufferSrcData, // BYTE** ppbData
                                          0)))                            // DWORD Flags
        {
            DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Lock failed, success expected. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }

        //
        // Fill vertex buffer with valid data (albeit data that isn't very useful for drawing real-world primitives)
        //
        for(uiIter = 0; uiIter < uiNumVerts; uiIter++)
            memcpy(&pVertexBufferSrcData[uiIter], &VertexTemplateSrc, sizeof(D3DQA_PVTEST_SRC));

        //
        // Vertex buffer locked successfully; attempt a unlock.
        //
        if (FAILED(hr = pVertexBufferSrc->Unlock()))
        {
            DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Unlock failed, success expected. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }

        //
        // Indicate intent to use vertex buffer
        //
        if (FAILED(hr = m_pd3dDevice->SetStreamSource(0,                // UINT StreamNumber
                                                 pVertexBufferSrc, // IDirect3DMobileVertexBuffer* pStreamData
                                                 0)))              // UINT Stride
        {
            DebugOut(DO_ERROR, L"SetStreamSource failed. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }

        //
        // Call PV with various arguments
        //
        for (uiStartVertex = 0; uiStartVertex < uiNumVerts; uiStartVertex++)
        {
            if (FAILED(hr = m_pd3dDevice->ProcessVertices(uiStartVertex,    // UINT SrcStartIndex
                                                     uiStartVertex,    // UINT DestIndex
                                                     1,                // UINT VertexCount
                                                     pVertexBufferDst, // IDirect3DMobileVertexBuffer* pDestBuffer
                                                     0)))              // DWORD Flags
            {
                DebugOut(DO_ERROR, L"ProcessVertices failed. (hr = 0x%08x). Failing.", hr);
                SetResult(Result, TPR_FAIL);
                continue;
            }
        }

        //
        // Attempt to lock the source (to flush the commands). This should succeed.
        //
        if (FAILED(hr = pVertexBufferDst->Lock(0,                              // OffsetToLock
                                          uiVBSizeDst,                    // UINT SizeToLock
                                          (VOID **)&pVertexBufferDstData, // BYTE** ppbData
                                          0)))                            // DWORD Flags
        {
            DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Lock failed, success expected. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }
        //
        // Vertex buffer locked successfully; attempt a unlock.
        //
        if (FAILED(hr = pVertexBufferDst->Unlock()))
        {
            DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Unlock failed, success expected. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }

        //
        // Bad-parameter tests
        //

        //
        // Bad-parameter test #1:  Invalid dest buffer
        //
        if (SUCCEEDED(hr = m_pd3dDevice->ProcessVertices(0,        // UINT SrcStartIndex
                                                    0,        // UINT DestIndex
                                                    1,        // UINT VertexCount
                                                    NULL,     // IDirect3DMobileVertexBuffer* pDestBuffer
                                                    0)))      // DWORD Flags
        {
            DebugOut(DO_ERROR, L"ProcessVertices succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }

        //
        // Bad-parameter test #2:  Invalid vertex count
        //
        if (SUCCEEDED(hr = m_pd3dDevice->ProcessVertices(0,                // UINT SrcStartIndex
                                                    0,                // UINT DestIndex
                                                    0,                // UINT VertexCount
                                                    pVertexBufferDst, // IDirect3DMobileVertexBuffer* pDestBuffer
                                                    0)))              // DWORD Flags
        {
            DebugOut(DO_ERROR, L"ProcessVertices succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }


        //
        // Bad-parameter test #3:  Invalid vertex range, due to source buffer
        //
        if (SUCCEEDED(hr = m_pd3dDevice->ProcessVertices(uiNumVerts-1,     // UINT SrcStartIndex
                                                    0,                // UINT DestIndex
                                                    2,                // UINT VertexCount
                                                    pVertexBufferDst, // IDirect3DMobileVertexBuffer* pDestBuffer
                                                    0)))              // DWORD Flags
        {
            DebugOut(DO_ERROR, L"ProcessVertices succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }


        //
        // Bad-parameter test #4:  Invalid vertex range, due to dest buffer
        //
        if (SUCCEEDED(hr = m_pd3dDevice->ProcessVertices(0,                // UINT SrcStartIndex
                                                    uiNumVerts-1,     // UINT DestIndex
                                                    2,                // UINT VertexCount
                                                    pVertexBufferDst, // IDirect3DMobileVertexBuffer* pDestBuffer
                                                    0)))              // DWORD Flags
        {
            DebugOut(DO_ERROR, L"ProcessVertices succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }


        //
        // Bad-parameter test #5:  Invalid FVF for dest buffer
        //
        if (SUCCEEDED(hr = m_pd3dDevice->ProcessVertices(0,                // UINT SrcStartIndex
                                                    0,                // UINT DestIndex
                                                    1,                // UINT VertexCount
                                                    pVertexBufferSrc, // IDirect3DMobileVertexBuffer* pDestBuffer
                                                    0)))              // DWORD Flags
        {
            DebugOut(DO_ERROR, L"ProcessVertices succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }

        //
        // Setup for next bad param test
        //
        if (FAILED(hr = m_pd3dDevice->SetStreamSource(0,                 // UINT StreamNumber
                                                 pVertexBufferDst,  // IDirect3DMobileVertexBuffer* pStreamData
                                                 0)))               // UINT Stride
        {
            DebugOut(DO_ERROR, L"SetStreamSource failed. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }


        //
        // Bad-parameter test #6:  Invalid FVF for src buffer
        //
        if (SUCCEEDED(hr = m_pd3dDevice->ProcessVertices(0,                // UINT SrcStartIndex
                                                    0,                // UINT DestIndex
                                                    1,                // UINT VertexCount
                                                    pVertexBufferDst, // IDirect3DMobileVertexBuffer* pDestBuffer
                                                    0)))              // DWORD Flags
        {
            DebugOut(DO_ERROR, L"ProcessVertices succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }


        //
        // Clear vertex buffer from stream source
        //
        if (FAILED(hr = m_pd3dDevice->SetStreamSource(0,     // UINT StreamNumber
                                                 NULL,  // IDirect3DMobileVertexBuffer* pStreamData
                                                 0)))   // UINT Stride
        {
            DebugOut(DO_ERROR, L"SetStreamSource failed. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }

        //
        // Bad-parameter test #7:  Invalid Stream Source
        //
        if (SUCCEEDED(hr = m_pd3dDevice->ProcessVertices(0,                // UINT SrcStartIndex
                                                    0,                // UINT DestIndex
                                                    1,                // UINT VertexCount
                                                    pVertexBufferDst, // IDirect3DMobileVertexBuffer* pDestBuffer
                                                    0)))              // DWORD Flags
        {
            DebugOut(DO_ERROR, L"ProcessVertices succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            SetResult(Result, TPR_FAIL);
            continue;
        }
        
        //
        // Nothing more to do with these vertex buffers
        //
        SafeRelease(pVertexBufferSrc);
        SafeRelease(pVertexBufferDst);
    }

cleanup:

    //
    // Nothing more to do with these vertex buffers
    //
    SafeRelease(pVertexBufferSrc);
    SafeRelease(pVertexBufferDst);

    //
    // Remove ref count due to active stream source
    //
    m_pd3dDevice->SetStreamSource(0, NULL, 0);

    return Result;
}


//
// ExecuteSetStreamSourceTest
//
//   Verify IDirect3DMobileDevice::SetStreamSource
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetStreamSourceTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetStreamSourceTest.");

    //
    // Iterator to exercise each primitive type
    //
    D3DMPRIMITIVETYPE PrimitiveType;

    //
    // Vertex buffer interface pointer
    //
    IDirect3DMobileVertexBuffer* pVertexBuffer;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // Valid pool for vertex buffers
    //
    D3DMPOOL VertexBufPool;

    //
    // Vertex format
    //
    #define D3DQA_SSTEST_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE
    struct D3DQA_SSTEST
    {
        FLOAT x, y, z, rhw;
        DWORD Diffuse;
    };

    //
    // Just enough for the maximum required by any primitive type
    //
    CONST UINT uiNumVerts = 3; 

    //
    // Overall VB size
    //
    UINT uiVBSize = uiNumVerts*BytesPerVertex(D3DQA_SSTEST_FVF);

    //
    // VB-related variables
    //
    D3DQA_SSTEST *pVertexBufferData;
    D3DQA_SSTEST VertexTemplate = {1.0f,1.0f,1.0f,1.0f,D3DMCOLOR_XRGB(0xFF,0x00,0x00)};

    //
    // Iterator for filling VB
    //
    UINT uiIter;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_SYSTEMMEM;
    }
    else if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_VIDEOMEM;
    }
    else
    {
        DebugOut(DO_ERROR, L"Fatal error; no pool supported for vertex buffers in D3DMCAPS.SurfaceCaps. Failing.");
        return TPR_FAIL;
    }

    //
    // Begin the scene
    //
    if (FAILED(hr = m_pd3dDevice->BeginScene()))
    {
        DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x) Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Call DrawPrimitive with various arguments
    //
    for (PrimitiveType = D3DMPT_POINTLIST; PrimitiveType <= D3DMPT_TRIANGLEFAN; (*(ULONG*)(&PrimitiveType))++)
    {
        //
        // Create the vertex buffer
        //
        if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(uiVBSize,         // UINT Length
                                                    0,                // DWORD Usage
                                                    D3DQA_SSTEST_FVF, // DWORD FVF
                                                    VertexBufPool,    // D3DMPOOL Pool
                                                   &pVertexBuffer)))  // IDirect3DMobileVertexBuffer** ppVertexBuffer
        {
            DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        //
        // Vertex buffer created successfully; attempt a lock.
        //
        if (FAILED(hr = pVertexBuffer->Lock(    0,                           // OffsetToLock
                                        uiVBSize,                    // UINT SizeToLock
                                        (VOID **)&pVertexBufferData, // BYTE** ppbData
                                        0)))                         // DWORD Flags
        {
            DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Lock failed, success expected. (hr = 0x%08x). Failing.", hr);
            pVertexBuffer->Release();
            return TPR_FAIL;
        }

        //
        // Fill vertex buffer with valid data (albeit data that isn't very useful for drawing real-world primitives)
        //
        for(uiIter = 0; uiIter < uiNumVerts; uiIter++)
            memcpy(&pVertexBufferData[uiIter], &VertexTemplate, sizeof(D3DQA_SSTEST));

        //
        // Vertex buffer locked successfully; attempt a unlock.
        //
        if (FAILED(hr = pVertexBuffer->Unlock()))
        {
            DebugOut(DO_ERROR, L"Fatal error; IDirect3DMobileVertexBuffer::Unlock failed, success expected. (hr = 0x%08x). Failing.", hr);
            pVertexBuffer->Release();
            return TPR_FAIL;
        }

        //
        // Indicate intent to use vertex buffer
        //
        if (FAILED(hr = m_pd3dDevice->SetStreamSource(0,             // UINT StreamNumber
                                                 pVertexBuffer, // IDirect3DMobileVertexBuffer* pStreamData
                                                 0)))           // UINT Stride
        {
            DebugOut(DO_ERROR, L"SetStreamSource failed. (hr = 0x%08x). Failing.", hr);
            pVertexBuffer->Release();
            return TPR_FAIL;
        }

        if (FAILED(hr = m_pd3dDevice->DrawPrimitive( PrimitiveType, // D3DMPRIMITIVETYPE PrimitiveType
                                                0,             // UINT StartVertex
                                                1)))           // UINT PrimitiveCount
        {
            DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x). Failing.", hr);
            pVertexBuffer->Release();
            return TPR_FAIL;
        }

        //
        // Nothing more to do with this vertex buffer
        //
        pVertexBuffer->Release();

    }

    //
    // Leave the scene
    //
    if (FAILED(hr = m_pd3dDevice->EndScene()))
    {
        DebugOut(DO_ERROR, L"EndScene failed. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Present the scene
    //
    if (FAILED(hr = m_pd3dDevice->Present(NULL,NULL,NULL,NULL)))
    {
        DebugOut(DO_ERROR, L"Present failed. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }


    //
    // Bad-parameter tests
    //
    // Test #1:  Invalid stream number
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetStreamSource(1,    // UINT StreamNumber
                                                NULL, // IDirect3DMobileVertexBuffer* pStreamData
                                                0)))  // UINT Stride
    {
        DebugOut(DO_ERROR, L"SetStreamSource succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Test #2:  Valid stream number; invalid stride
    //
    if (SUCCEEDED(hr = m_pd3dDevice->SetStreamSource(0,    // UINT StreamNumber
                                                NULL, // IDirect3DMobileVertexBuffer* pStreamData
                                                1)))  // UINT Stride
    {
        DebugOut(DO_ERROR, L"SetStreamSource succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetStreamSourceTest
//
//   Verify IDirect3DMobileDevice::GetStreamSource
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetStreamSourceTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetStreamSourceTest.");

    //
    // Vertex buffer interface pointer
    //
    IDirect3DMobileVertexBuffer* pVertexBuffer;

    //
    // Interface and stride reported by GetStreamSource
    //
    IDirect3DMobileVertexBuffer* pStreamData;
    UINT uiStride;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // Valid pool for vertex buffers
    //
    D3DMPOOL VertexBufPool;

    //
    // Vertex format
    //
    #define D3DQA_GSTEST_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE
    struct D3DQA_GSTEST
    {
        FLOAT x, y, z, rhw;
        DWORD Diffuse;
    };

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Bad-Parameter Test #1
    //
    // (no previous SetStreamSource)
    //
    if (FAILED(hr = m_pd3dDevice->GetStreamSource( 0,           // UINT StreamNumber
                                             &pStreamData, // IDirect3DMobileVertexBuffer** ppStreamData
                                             &uiStride)))  // UINT* pStride
    {
        DebugOut(DO_ERROR, L"GetStreamSource failed. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }
    
    //
    // Verify expected results
    // 
    if ((0 != uiStride) ||
        (NULL != pStreamData))
    {
        DebugOut(DO_ERROR, L"GetStreamSource:  unexpected results. Failing.");
        return TPR_FAIL;
    }

    //
    // Bad-Parameter Test #2
    //
    // (NULL IDirect3DMobileVertexBuffer**)
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetStreamSource( 0,           // UINT StreamNumber
                                                 NULL,        // IDirect3DMobileVertexBuffer** ppStreamData
                                                &uiStride)))  // UINT* pStride
    {
        DebugOut(DO_ERROR, L"GetStreamSource succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }
    
    //
    // Bad-Parameter Test #3
    //
    // (NULL UINT*)
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetStreamSource( 0,           // UINT StreamNumber
                                                &pStreamData, // IDirect3DMobileVertexBuffer** ppStreamData
                                                NULL)))       // UINT* pStride
    {
        DebugOut(DO_ERROR, L"GetStreamSource succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }
    
    //
    // Bad-Parameter Test #4
    //
    // (NULL UINT*, NULL IDirect3DMobileVertexBuffer**)
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetStreamSource( 0,       // UINT StreamNumber
                                                 NULL,    // IDirect3DMobileVertexBuffer** ppStreamData
                                                 NULL)))  // UINT* pStride
    {
        DebugOut(DO_ERROR, L"GetStreamSource succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    
    //
    // Bad-Parameter Test #5
    //
    // Invalid stream number
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetStreamSource( 1,       // UINT StreamNumber
                                                 NULL,    // IDirect3DMobileVertexBuffer** ppStreamData
                                                 NULL)))  // UINT* pStride
    {
        DebugOut(DO_ERROR, L"GetStreamSource succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_SYSTEMMEM;
    }
    else if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
    {
        VertexBufPool = D3DMPOOL_VIDEOMEM;
    }
    else
    {
        DebugOut(DO_ERROR, L"Fatal error; no pool supported for vertex buffers in D3DMCAPS.SurfaceCaps. Failing.");
        return TPR_FAIL;
    }

    //
    // Create the vertex buffer
    //
    if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(sizeof(D3DQA_GSTEST), // UINT Length
                                                0,                    // DWORD Usage
                                                D3DQA_GSTEST_FVF,     // DWORD FVF
                                                VertexBufPool,        // D3DMPOOL Pool
                                               &pVertexBuffer)))      // IDirect3DMobileVertexBuffer** ppVertexBuffer
    {
        DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }


    //
    // Indicate intent to use vertex buffer
    //
    if (FAILED(hr = m_pd3dDevice->SetStreamSource(0,             // UINT StreamNumber
                                             pVertexBuffer, // IDirect3DMobileVertexBuffer* pStreamData
                                             0)))           // UINT Stride
    {
        DebugOut(DO_ERROR, L"SetStreamSource failed. (hr = 0x%08x). Failing.", hr);
        pVertexBuffer->Release();
        return TPR_FAIL;
    }

    //
    // Retrieve stream info
    //
    if (FAILED(hr = m_pd3dDevice->GetStreamSource( 0,           // UINT StreamNumber
                                             &pStreamData, // IDirect3DMobileVertexBuffer** ppStreamData
                                             &uiStride)))  // UINT* pStride
    {
        DebugOut(DO_ERROR, L"GetStreamSource failed. (hr = 0x%08x). Failing.", hr);
        pVertexBuffer->Release();
        return TPR_FAIL;
    }

    //
    // Verify that stream source is the VB that was indicated to SetStreamSource
    //
    if (pStreamData != pVertexBuffer)
    {
        DebugOut(DO_ERROR, L"GetStreamSource:  unexpected result. Failing.");
        pVertexBuffer->Release();
        return TPR_FAIL;
    }
    
    //
    // Release "pair" for GetStreamSource
    //
    pStreamData->Release();

    //
    // Release ref-count caused by SetStreamSource
    //
    if (FAILED(hr = m_pd3dDevice->SetStreamSource(0,    // UINT StreamNumber
                                             NULL, // IDirect3DMobileVertexBuffer* pStreamData
                                             0)))  // UINT Stride
    {
        DebugOut(DO_ERROR, L"SetStreamSource failed. (hr = 0x%08x). Failing.", hr);
        pVertexBuffer->Release();
        return TPR_FAIL;
    }

    //
    // Nothing more to do with this vertex buffer
    //
    if (0 != pVertexBuffer->Release())
    {
        DebugOut(DO_ERROR, L"IDirect3DMobileVertexBuffer::Release indicates dangling reference to VB. Failing.");
        return TPR_FAIL;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;
}


//
// ExecuteSetIndicesTest
//
//   Verify IDirect3DMobileDevice::SetIndices
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSetIndicesTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteSetIndicesTest.");

    //
    // Pool for index buffer creation
    //
    D3DMPOOL IndexBufPool;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // "Sample" date for testing index buffers
    //
    typedef struct _INDEXBUFSAMPLES 
    {
        UINT uiSize;
        D3DMFORMAT Format;
        DWORD Usage;
    } INDEXBUFSAMPLES;

    CONST UINT uiNumIndexTests = 20;

    INDEXBUFSAMPLES IndexTests[] =
    {
    // | Size |      Format     |    Usage    |
    // +------+-----------------+-------------+
        {   2,  D3DMFMT_INDEX16,  0x00000000  },
        {   4,  D3DMFMT_INDEX16,  0x00000000  },
        {   6,  D3DMFMT_INDEX16,  0x00000000  },
        {   8,  D3DMFMT_INDEX16,  0x00000000  },
        {  10,  D3DMFMT_INDEX16,  0x00000000  },
        { 100,  D3DMFMT_INDEX16,  0x00000000  },
        { 200,  D3DMFMT_INDEX16,  0x00000000  },
        { 300,  D3DMFMT_INDEX16,  0x00000000  },
        { 400,  D3DMFMT_INDEX16,  0x00000000  },
        { 500,  D3DMFMT_INDEX16,  0x00000000  },
        {   4,  D3DMFMT_INDEX32,  0x00000000  },
        {   8,  D3DMFMT_INDEX32,  0x00000000  },
        {  12,  D3DMFMT_INDEX32,  0x00000000  },
        {  16,  D3DMFMT_INDEX32,  0x00000000  },
        {  20,  D3DMFMT_INDEX32,  0x00000000  },
        { 100,  D3DMFMT_INDEX32,  0x00000000  },
        { 200,  D3DMFMT_INDEX32,  0x00000000  },
        { 300,  D3DMFMT_INDEX32,  0x00000000  },
        { 400,  D3DMFMT_INDEX32,  0x00000000  },
        { 500,  D3DMFMT_INDEX32,  0x00000000  }
    };

    //
    // Iterator for stepping through individual test cases
    //
    UINT uiIter;

    //
    // Interface pointer for index buffer; description structs
    //
    IDirect3DMobileIndexBuffer *pIndexBuffer1, *pIndexBuffer2;
    D3DMINDEXBUFFER_DESC IndexBufferDesc1, IndexBufferDesc2;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Pick the pool that the driver supports, as reported in D3DMCAPS
    //
    if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
    {
        IndexBufPool = D3DMPOOL_VIDEOMEM;
    }
    else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
    {
        IndexBufPool = D3DMPOOL_SYSTEMMEM;
    }
    else
    {
        DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER. Aborting.");
        return TPR_ABORT;
    }


    for(uiIter = 0; uiIter < uiNumIndexTests; uiIter++)
    {

        //
        // Create an index buffer according to predefined data
        //
        if (FailedCall(hr = m_pd3dDevice->CreateIndexBuffer(IndexTests[uiIter].uiSize, // UINT Length
                                                       IndexTests[uiIter].Usage,  // DWORD Usage
                                                       IndexTests[uiIter].Format, // D3DMFORMAT Format
                                                       IndexBufPool,              // D3DMPOOL Pool
                                                      &pIndexBuffer1)))           // IDirect3DMobileIndexBuffer** ppIndexBuffer
        {
            DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        //
        // Set the index buffer for active use
        //
        if (FailedCall(hr = m_pd3dDevice->SetIndices(pIndexBuffer1)))// IDirect3DMobileIndexBuffer* pIndexData
        {
            DebugOut(DO_ERROR, L"SetIndices failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        if (FailedCall(hr = m_pd3dDevice->GetIndices(&pIndexBuffer2))) // IDirect3DMobileIndexBuffer** ppIndexData
        {
            DebugOut(DO_ERROR, L"GetIndices failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        if (FAILED(hr = pIndexBuffer1->GetDesc(&IndexBufferDesc1))) // D3DMINDEXBUFFER_DESC *pDesc
        {
            DebugOut(DO_ERROR, L"IDirect3DMobileIndexBuffer::GetDesc failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        if (FAILED(hr = pIndexBuffer2->GetDesc(&IndexBufferDesc2))) // D3DMINDEXBUFFER_DESC *pDesc
        {
            DebugOut(DO_ERROR, L"IDirect3DMobileIndexBuffer::GetDesc failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        if ((IndexBufferDesc1.Format != IndexBufferDesc2.Format) ||
            (IndexBufferDesc1.Format != IndexTests[uiIter].Format))
        {
            DebugOut(DO_ERROR, L"IDirect3DMobileIndexBuffer::GetDesc format mismatch. Failing.");
            return TPR_FAIL;
        }

        if ((IndexBufferDesc1.Type != IndexBufferDesc2.Type) ||
            (IndexBufferDesc1.Type != D3DMRTYPE_INDEXBUFFER))
        {
            DebugOut(DO_ERROR, L"IDirect3DMobileIndexBuffer::GetDesc resource type mismatch. Failing.");
            return TPR_FAIL;
        }

        if ((IndexBufferDesc1.Size != IndexBufferDesc2.Size) ||
            (IndexBufferDesc1.Size != IndexTests[uiIter].uiSize))
        {
            DebugOut(DO_ERROR, L"IDirect3DMobileIndexBuffer::GetDesc size mismatch. Failing.");
            return TPR_FAIL;
        }

        pIndexBuffer1->Release();
        pIndexBuffer2->Release();

        //
        // Clear active index buffer
        //
        if (FailedCall(hr = m_pd3dDevice->SetIndices(NULL))) // IDirect3DMobileIndexBuffer* pIndexData
        {
            DebugOut(DO_ERROR, L"SetIndices failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}


//
// ExecuteGetIndicesTest
//
//   Verify IDirect3DMobileDevice::GetIndices
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteGetIndicesTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteGetIndicesTest.");

    //
    // Interface for test case; initialized to non-null to help
    // with first test call
    //
    LPDIRECT3DMOBILEINDEXBUFFER pIndexBuffer = (LPDIRECT3DMOBILEINDEXBUFFER)0xFFFFFFFF;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // GetIndices with no prior call to SetIndices
    //
    if (FAILED(hr = m_pd3dDevice->GetIndices(&pIndexBuffer)))   // IDirect3DMobileIndexBuffer** ppIndexData
    {
        DebugOut(DO_ERROR, L"GetIndices failed. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    if (NULL != pIndexBuffer)
    {
        DebugOut(DO_ERROR, L"GetIndices returned unexpected interface. Failing.");
        return TPR_FAIL;
    }
    
    //
    // Bad parameter testing
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetIndices(NULL)))   // IDirect3DMobileIndexBuffer** ppIndexData
    {
        DebugOut(DO_ERROR, L"GetIndices succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // More thorough testing occurs in a closely-related test:
    //
    return ExecuteSetIndicesTest();

}


//
// ExecuteStretchRectTest
//
//   Verify IDirect3DMobileDevice::StretchRect.  Because source and dest surfaces are same format in these
//   test cases; IDirect3DMobile::CheckDeviceFormatConversion is not a prerequisite to the StretchRect calls.
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteStretchRectTest()
{
    DebugOut(L"Beginning ExecuteStretchRectTest.");

    //
    // There must be at least one valid format, for creating surfaces.
    // The first time a valid format is found, this BOOL is toggled.
    //
    // If the BOOL is never toggled, the test will fail after attempting
    // all possible formats.
    //
    BOOL bFoundValidFormat = FALSE;

    //
    // Device Capabilities
    //
    D3DMCAPS Caps;

    //
    // D3DMFORMAT iterator; to cycle through formats until a valid format is found.
    //
    D3DMFORMAT CurrentFormat;

    //
    // The display mode's spatial resolution, color resolution,
    // and refresh frequency.
    //
    D3DMDISPLAYMODE Mode;

    //
    // Interfaces for test operation
    //
    LPDIRECT3DMOBILESURFACE pSurfaceSmall = NULL, pSurfaceBig = NULL;

    //
    // API Result code
    //
    HRESULT hr;

    //
    // Test Result
    //
    INT Result = TPR_PASS;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Retrieve driver capabilities
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDeviceCaps. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode; function returned failure. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {
        if (!(FAILED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
                                               m_DevType,          // D3DMDEVTYPE DeviceType
                                               Mode.Format,        // D3DMFORMAT AdapterFormat
                                               0,                  // ULONG Usage
                                               D3DMRTYPE_SURFACE,  // D3DMRESOURCETYPE RType
                                               CurrentFormat))))   // D3DMFORMAT CheckFormat
        {
            bFoundValidFormat = TRUE;
            break;
        }
    }

    if (FALSE == bFoundValidFormat)
    {
        DebugOut(DO_ERROR, L"No valid surface formats found. Failing.");
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Create a surface
    //
    if (FAILED(hr = m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_STRETCHRECT_SMALLSURF_WIDTH, // UINT Width
                                                D3DQA_DEFAULT_STRETCHRECT_SMALLSURF_HEIGHT,// UINT Height
                                                CurrentFormat,                             // D3DMFORMAT Format
                                               &pSurfaceSmall)))                           // IDirect3DMobileSurface** ppSurface
    {
        DebugOut(DO_ERROR, L"Fatal error; CreateImageSurface failed on a surface format that was approved by CheckDeviceFormat. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Create another surface
    //
    if (FAILED(hr = m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_STRETCHRECT_BIGSURF_WIDTH, // UINT Width
                                                D3DQA_DEFAULT_STRETCHRECT_BIGSURF_HEIGHT,// UINT Height
                                                CurrentFormat,                           // D3DMFORMAT Format
                                               &pSurfaceBig)))                           // IDirect3DMobileSurface** ppSurface
    {
        DebugOut(DO_ERROR, L"Fatal error; CreateImageSurface failed on a surface format that was approved by CheckDeviceFormat. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Bad-parameter tests
    //
    // Test #1:  Bad filter type
    //
    if (SUCCEEDED(hr = m_pd3dDevice->StretchRect(NULL,            // IDirect3DMobileSurface* pSourceSurface
                                            NULL,            // CONST RECT* pSourceRect
                                            NULL,            // IDirect3DMobileSurface* pDestSurface
                                            NULL,            // CONST RECT* pDestRect
                                            D3DMTEXF_NONE))) // D3DMTEXTUREFILTERTYPE Filter
    {
        DebugOut(DO_ERROR, L"StretchRect succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Test #2:  Source interface pointer same as dest interface pointer
    //
    if (SUCCEEDED(hr = m_pd3dDevice->StretchRect(pSurfaceSmall,   // IDirect3DMobileSurface* pSourceSurface
                                            NULL,            // CONST RECT* pSourceRect
                                            pSurfaceSmall,   // IDirect3DMobileSurface* pDestSurface
                                            NULL,            // CONST RECT* pDestRect
                                            D3DMTEXF_POINT)))// D3DMTEXTUREFILTERTYPE Filter
    {
        DebugOut(DO_ERROR, L"StretchRect succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Test #3:  NULL Source interface pointer
    //
    if (SUCCEEDED(hr = m_pd3dDevice->StretchRect(NULL,            // IDirect3DMobileSurface* pSourceSurface
                                            NULL,            // CONST RECT* pSourceRect
                                            pSurfaceBig,     // IDirect3DMobileSurface* pDestSurface
                                            NULL,            // CONST RECT* pDestRect
                                            D3DMTEXF_POINT)))// D3DMTEXTUREFILTERTYPE Filter
    {
        DebugOut(DO_ERROR, L"StretchRect succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Test #4:  NULL Dest interface pointer
    //
    if (SUCCEEDED(hr = m_pd3dDevice->StretchRect(pSurfaceSmall,   // IDirect3DMobileSurface* pSourceSurface
                                            NULL,            // CONST RECT* pSourceRect
                                            NULL,            // IDirect3DMobileSurface* pDestSurface
                                            NULL,            // CONST RECT* pDestRect
                                            D3DMTEXF_POINT)))// D3DMTEXTUREFILTERTYPE Filter
    {
        DebugOut(DO_ERROR, L"StretchRect succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }


    //
    // Verify debug-only checks
    //
    if (m_bDebugMiddleware)
    {
        //
        // Minification tests
        //

        //
        // Point minification
        //
        hr = m_pd3dDevice->StretchRect(pSurfaceBig,     // IDirect3DMobileSurface* pSourceSurface
                                       NULL,            // CONST RECT* pSourceRect
                                       pSurfaceSmall,   // IDirect3DMobileSurface* pDestSurface
                                       NULL,            // CONST RECT* pDestRect
                                       D3DMTEXF_POINT); // D3DMTEXTUREFILTERTYPE Filter

        if (SUCCEEDED(hr) && (!(Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MINFPOINT)))
        {
            DebugOut(DO_ERROR, L"StretchRect succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            Result = TPR_FAIL;
            goto cleanup;
        }
        else if (FAILED(hr) && (Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MINFPOINT))
        {
            DebugOut(DO_ERROR, L"StretchRect failed; success expected. (hr = 0x%08x). Failing.", hr);
            Result = TPR_FAIL;
            goto cleanup;
        }

        
        //
        // Linear minification
        //
        hr = m_pd3dDevice->StretchRect(pSurfaceBig,     // IDirect3DMobileSurface* pSourceSurface
                                       NULL,            // CONST RECT* pSourceRect
                                       pSurfaceSmall,   // IDirect3DMobileSurface* pDestSurface
                                       NULL,            // CONST RECT* pDestRect
                                       D3DMTEXF_LINEAR);// D3DMTEXTUREFILTERTYPE Filter

        if (SUCCEEDED(hr) && (!(Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MINFLINEAR)))
        {
            DebugOut(DO_ERROR, L"StretchRect succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            Result = TPR_FAIL;
            goto cleanup;
        }
        else if (FAILED(hr) && (Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MINFLINEAR))
        {
            DebugOut(DO_ERROR, L"StretchRect failed; success expected. (hr = 0x%08x). Failing.", hr);
            Result = TPR_FAIL;
            goto cleanup;
        }


        //
        // Anisotropic minification
        //
        hr = m_pd3dDevice->StretchRect(pSurfaceBig,           // IDirect3DMobileSurface* pSourceSurface
                                       NULL,                  // CONST RECT* pSourceRect
                                       pSurfaceSmall,         // IDirect3DMobileSurface* pDestSurface
                                       NULL,                  // CONST RECT* pDestRect
                                       D3DMTEXF_ANISOTROPIC); // D3DMTEXTUREFILTERTYPE Filter

        if (SUCCEEDED(hr))
        {
            DebugOut(DO_ERROR, L"StretchRect succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            Result = TPR_FAIL;
            goto cleanup;
        }

        //
        // Magnification tests
        //

        //
        // Point magnification
        //
        hr = m_pd3dDevice->StretchRect(pSurfaceSmall,   // IDirect3DMobileSurface* pSourceSurface
                                       NULL,            // CONST RECT* pSourceRect
                                       pSurfaceBig,     // IDirect3DMobileSurface* pDestSurface
                                       NULL,            // CONST RECT* pDestRect
                                       D3DMTEXF_POINT); // D3DMTEXTUREFILTERTYPE Filter

        if (SUCCEEDED(hr) && (!(Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MAGFPOINT)))
        {
            DebugOut(DO_ERROR, L"StretchRect succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            Result = TPR_FAIL;
            goto cleanup;
        }
        else if (FAILED(hr) && (Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MAGFPOINT))
        {
            DebugOut(DO_ERROR, L"StretchRect failed; success expected. (hr = 0x%08x). Failing.", hr);
            Result = TPR_FAIL;
            goto cleanup;
        }

        //
        // Linear magnification
        //
        hr = m_pd3dDevice->StretchRect(pSurfaceSmall,   // IDirect3DMobileSurface* pSourceSurface
                                       NULL,            // CONST RECT* pSourceRect
                                       pSurfaceBig,     // IDirect3DMobileSurface* pDestSurface
                                       NULL,            // CONST RECT* pDestRect
                                       D3DMTEXF_LINEAR);// D3DMTEXTUREFILTERTYPE Filter
        if (SUCCEEDED(hr) && (!(Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MAGFLINEAR)))
        {
            DebugOut(DO_ERROR, L"StretchRect succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            Result = TPR_FAIL;
            goto cleanup;
        }
        else if (FAILED(hr) && (Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MAGFLINEAR))
        {
            DebugOut(DO_ERROR, L"StretchRect failed; success expected. (hr = 0x%08x). Failing.", hr);
            Result = TPR_FAIL;
            goto cleanup;
        }

        //
        // Anisotropic magnification
        //
        hr = m_pd3dDevice->StretchRect(pSurfaceSmall,         // IDirect3DMobileSurface* pSourceSurface
                                       NULL,                  // CONST RECT* pSourceRect
                                       pSurfaceBig,           // IDirect3DMobileSurface* pDestSurface
                                       NULL,                  // CONST RECT* pDestRect
                                       D3DMTEXF_ANISOTROPIC); // D3DMTEXTUREFILTERTYPE Filter

        if (SUCCEEDED(hr))
        {
            DebugOut(DO_ERROR, L"StretchRect succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
            Result = TPR_FAIL;
            goto cleanup;
        }
    }

    //
    // Push command buffer to driver
    //
    if (FAILED(hr = Flush()))
    {
        DebugOut(DO_ERROR, L"Flush failed. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

cleanup:

    if (pSurfaceBig)
        pSurfaceBig->Release();

    if (pSurfaceSmall)
        pSurfaceSmall->Release();

    return Result;
}


//
// ExecuteColorFillTest
//
//   Verify IDirect3DMobileDevice::ColorFill
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteColorFillTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteColorFillTest.");

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Surface format to consider for use (via CheckDeviceFormat)
    //
    D3DMFORMAT CurrentFormat;

    //
    // Color value to use for ColorFill.  Currently, the specific
    // color chosen for use in this test is arbitrary and not
    // particularly significant to the scenarios that this test
    // exercises.
    //
    CONST D3DMCOLOR ArbitraryColor = 0x7F7F7F7F;

    //
    // Interface pointer, to be used with ColorFill
    //
    IDirect3DMobileSurface* pSurface;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Iterate through all formats in the D3DMFORMAT enumeration, seeking formats that
    // are valid as surfaces.  Any such format is used as an argument to CreateImageSurface,
    // and the resultant surface is then used with ColorFill.
    //
    for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {
        if (FAILED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal,     // UINT Adapter
                                             m_DevType,              // D3DMDEVTYPE DeviceType
                                             Mode.Format,            // D3DMFORMAT AdapterFormat
                                             D3DMUSAGE_RENDERTARGET, // ULONG Usage
                                             D3DMRTYPE_SURFACE,      // D3DMRESOURCETYPE RType
                                             CurrentFormat)))        // D3DMFORMAT CheckFormat
        {
            //
            // This is not a valid format for surfaces
            //
            continue;
        }

        if (FAILED(hr = m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_COLORFILL_SURF_WIDTH, // UINT Width
                                                    D3DQA_DEFAULT_COLORFILL_SURF_HEIGHT,// UINT Height
                                                    CurrentFormat,                      // D3DMFORMAT Format
                                                   &pSurface)))                         // IDirect3DMobileSurface** ppSurface
        {
            DebugOut(DO_ERROR, L"Fatal error; CreateImageSurface failed, success expected. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        //
        // Attempt a ColorFill operation on the image surface
        //
        if (FailedCall(hr = m_pd3dDevice->ColorFill(pSurface,         // IDirect3DMobileSurface* pSurface
                                               NULL,             // CONST RECT* pRect
                                               ArbitraryColor))) // D3DMCOLOR Color
        {
            DebugOut(DO_ERROR, L"Fatal error; ColorFill failed, success expected. (hr = 0x%08x). Failing.", hr);
            pSurface->Release();
            return TPR_FAIL;
        }

        //
        // Prepare for next iteration (interface pointer will be clobbered)
        //
        pSurface->Release();
    }

    //
    // Bad-parameter tests
    //

    //
    // Test #1:  NULL surface interface pointer
    //
    if (SUCCEEDED(hr = m_pd3dDevice->ColorFill(NULL,             // IDirect3DMobileSurface* pSurface
                                          NULL,             // CONST RECT* pRect
                                          ArbitraryColor))) // D3DMCOLOR Color
    {
        DebugOut(DO_ERROR, L"ColorFill succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }


    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}

//
// IsPowTwo
//
//   Inspects a value to determine if it is a power of two
//
// Arguments:
//
//   UINT uiValue:  value to inspect
//
// Return Value:
//
//   BOOL:  FALSE if not a power of two, TRUE if a power of two
//   
//   
BOOL IDirect3DMobileDeviceTest::IsPowTwo(UINT uiValue)
{
    return (!(uiValue == 0 || (uiValue & (uiValue - 1)) != 0));
}

// 
// ExecuteVerifyAutoMipLevelExtentsTest
// 
//   Verifies that CreateTexture automatically generates the correct number
//   of mip-map levels; and that those automatically-generated mip-map levels
//   are of the correct dimensions.
// 
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteVerifyAutoMipLevelExtentsTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteVerifyAutoMipLevelExtentsTest.");

    //
    // Iterators for attempting various widths/heights
    //
    UINT *puiLargeExtent, *puiSmallExtent;

    //
    // Storage for width/height extents to be passed to CreateTexture
    //
    UINT uiWidth, uiHeight;

    //
    // Indicates the number of automatically-generated levels that are anticipated, for
    // a particular call to CreateTexture
    //
    UINT uiLevelsExpected;

    //
    // Indicates the expected extents of a automatically-generated level, for a particular
    // call to CreateTexture
    //
    UINT uiExpectedLevelWidth, uiExpectedLevelHeight;

    //
    // Toggle for selecting a tall texture or a wide texture
    //
    UINT uiPickExtentStyle;

    //
    // Reasonable maximum number of level tests to iterate through; limiting number of
    // iterations to keep run-time feasible
    // 
    CONST UINT uiMaxLevel = D3DMQA_AUTOMIPMAPEXTENTS_NUM_TESTED_LEVELS;

    //
    // Iterator for inspecting texture levels
    //
    UINT uiLevelDescIter;

    //
    // Surface description for an individual texture level
    //
    D3DMSURFACE_DESC Desc;

    //
    // All failure conditions specifically set this
    //
    INT Result = TPR_PASS;

    //
    // Interface pointer for use with CreateTexture
    //
    LPDIRECT3DMOBILETEXTURE pTexture = NULL;

    //
    // D3DMFORMAT iterator; to cycle through formats until a valid format
    // is found.
    //
    D3DMFORMAT FormatIter;

    //
    // The display mode's spatial resolution, color resolution,
    // and refresh frequency.
    //
    D3DMDISPLAYMODE Mode;

    //
    // Number of levels generated automatically
    //
    DWORD dwLevelCount;

    //
    // There must be at least one valid format.
    // The first time a valid format is found, this BOOL is toggled.
    //
    // If the BOOL is never toggled, the test will fail after attempting
    // all possible formats.
    //
    BOOL bFoundValidFormat = FALSE;

    //
    // Device capabilities; for inspecting mip-mapping support
    //
    D3DMCAPS Caps;

    //
    // Pool for texture creation
    //
    D3DMPOOL TexturePool;

    //
    // Potential limitations on texture extents
    //
    BOOL bSquareOnly = FALSE;
    BOOL bPowTwoOnly = FALSE;
    
    
    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Get device capabilities to check for mip-mapping support
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
    {
        DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x). Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    if (!(Caps.TextureCaps & D3DMPTEXTURECAPS_MIPMAP))
    {
        DebugOut(DO_ERROR, L"No mip-mapping support. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }
    
    //
    // Does device require POW2 textures?
    //
    if (Caps.TextureCaps & D3DMPTEXTURECAPS_POW2)
    {
        bPowTwoOnly = TRUE;
    }
    
    //
    // Does device require square textures?
    //
    if (Caps.TextureCaps & D3DMPTEXTURECAPS_SQUAREONLY)
    {
        bSquareOnly = TRUE;
    }

    //
    // Determine valid pool for textures
    //
    if (D3DMSURFCAPS_SYSTEXTURE & Caps.SurfaceCaps)
    {
        TexturePool = D3DMPOOL_SYSTEMMEM;
    }
    else if (D3DMSURFCAPS_VIDTEXTURE & Caps.SurfaceCaps)
    {
        TexturePool = D3DMPOOL_VIDEOMEM;
    }
    else
    {
        DebugOut(DO_ERROR, L"Fatal error; no pool supported for textures in D3DMCAPS.SurfaceCaps. Failing.");
        return TPR_FAIL;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"GetDisplayMode failed. (hr = 0x%08x). Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Find a format supported for textures
    //
    for (FormatIter = (D3DMFORMAT)0; FormatIter < D3DMFMT_NUMFORMAT; (*(ULONG*)(&FormatIter))++)
    {
        if (!(FAILED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
                                               m_DevType,          // D3DMDEVTYPE DeviceType
                                               Mode.Format,        // D3DMFORMAT AdapterFormat
                                               0,                  // ULONG Usage
                                               D3DMRTYPE_TEXTURE,  // D3DMRESOURCETYPE RType
                                               FormatIter))))      // D3DMFORMAT CheckFormat
        {
            bFoundValidFormat = TRUE;
            break;
        }
    }

    if (FALSE == bFoundValidFormat)
    {
        DebugOut(DO_ERROR, L"No valid texture format. Aborting.");
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Iterator picks between tall textures and wide textures
    //
    for(uiPickExtentStyle = 0; uiPickExtentStyle <= 1; uiPickExtentStyle++)
    {
        switch(uiPickExtentStyle)
        {
        case 0:
            puiLargeExtent = &uiWidth;
            puiSmallExtent = &uiHeight;
            break;
        case 1:
            puiLargeExtent = &uiHeight;
            puiSmallExtent = &uiWidth;
            break;
        }

        //
        // Iterator picks the scenario to test, with respect to number of anticipated generated levels
        //
        for(uiLevelsExpected = 1; uiLevelsExpected < uiMaxLevel; uiLevelsExpected++)
        {
            //
            // Iterate through all of the potential extents that would generate a mip-map of the anticipated number of levels
            //
            for(*puiLargeExtent = (0x00000001 << (uiLevelsExpected - 1)); *puiLargeExtent < (UINT)(0x00000001 << (uiLevelsExpected)); (*puiLargeExtent)++)
            {
                for(*puiSmallExtent = 1; *puiSmallExtent <= *puiLargeExtent; (*puiSmallExtent)++)
                {

                    //
                    // Skip iterations that violate the driver's limitations
                    //
                    //
                    // The driver may mandate that texture extents are only valid if square
                    //
                    if (bSquareOnly && (uiWidth != uiHeight))
                    {
                        continue;
                    }

                    //
                    // The driver may mandate that texture extents are only valid if POW2
                    //
                    if (bPowTwoOnly)
                    {
                        if (!IsPowTwo(uiWidth))
                            continue;

                        if (!IsPowTwo(uiHeight))
                            continue;
                    }
                    

                    //
                    // Create a mip-map with automatically-generated levels
                    //
                    if (FAILED(hr = m_pd3dDevice->CreateTexture(uiWidth,         // UINT Width,
                                                           uiHeight,        // UINT Height,
                                                           0,               // UINT Levels,
                                                           0,               // DWORD Usage,
                                                           FormatIter,      // D3DMFORMAT Format,
                                                           TexturePool,     // D3DMPOOL Pool,
                                                           &pTexture)))     // LPDIRECT3DMOBILETEXTURE** ppTexture,
                    {
                        if (CapsAllowCreateTexture(m_pd3dDevice, // LPDIRECT3DMOBILEDEVICE pDevice,
                                                   uiWidth,      // UINT uiWidth,
                                                   uiHeight))    // UINT uiHeight
                        {
                            //
                            // Texture creation should have succeeded
                            //
                            DebugOut(DO_ERROR, L"CreateTexture failed. (hr = 0x%08x). Failing.", hr);
                            Result = TPR_FAIL;
                            goto cleanup;
                        }
                        else
                        {
                            //
                            // Texture creation failed for a valid reason (CAPs constraint)
                            //
                            continue;
                        }
                    }

                    //
                    // Did CreateTexture generate the correct number of levels?
                    //
                    dwLevelCount = pTexture->GetLevelCount();
                    if (uiLevelsExpected != dwLevelCount)
                    {
                        DebugOut(DO_ERROR, L"Unexpected level count. Failing.");
                        Result = TPR_FAIL;
                        goto cleanup;
                    }

                    //
                    // Initialize level width/height expectations
                    //
                    uiExpectedLevelWidth = uiWidth;
                    uiExpectedLevelHeight = uiHeight;

                    //
                    // Pick a level to inspect
                    //
                    for (uiLevelDescIter = 0; uiLevelDescIter < uiLevelsExpected; uiLevelDescIter++)
                    {
                        //
                        // Based on the level description; are the extents correct?
                        //
                        if (FAILED(hr = pTexture->GetLevelDesc(uiLevelDescIter,// UINT Level,
                                                          &Desc)))        // D3DSURFACE_DESC *pDesc
                        {
                            DebugOut(DO_ERROR, L"GetLevelDesc failed. (hr = 0x%08x). Failing.", hr);
                            Result = TPR_FAIL;
                            goto cleanup;
                        }

                        if (uiExpectedLevelWidth != Desc.Width)
                        {
                            DebugOut(DO_ERROR, L"Level width mismatch. Failing.");
                            Result = TPR_FAIL;
                            goto cleanup;
                        }

                        if (uiExpectedLevelHeight != Desc.Height)
                        {
                            DebugOut(DO_ERROR, L"Level height mismatch. Failing.");
                            Result = TPR_FAIL;
                            goto cleanup;
                        }

                        uiExpectedLevelWidth /= 2;
                        uiExpectedLevelHeight /= 2;

                        //
                        // Clamp to zero
                        //
                        if (0 == uiExpectedLevelWidth)
                            uiExpectedLevelWidth = 1;
                        if (0 == uiExpectedLevelHeight)
                            uiExpectedLevelHeight = 1;
                    }

                    pTexture->Release();
                    pTexture = NULL;
                }
            }
        }
    }
cleanup:

    if (pTexture)
        pTexture->Release();
    
    return Result;
}



//
// ExecuteAttemptPresentFlagMultiSampleMismatchTest
//
//   Attempt to use multisampling when disallowed by present flag
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteAttemptPresentFlagMultiSampleMismatchTest()
{
    HRESULT hr;

    DebugOut(L"Beginning ExecuteAttemptPresentFlagMultiSampleMismatchTest.");

    //
    // Variables for finding a valid multisample type, if one exists for this device
    //
    D3DMMULTISAMPLE_TYPE MultiSampleType;
    BOOL bValidMultiSample = FALSE;
    INT uiMultiSample;

    //
    // Variables used in preparing arguments for CheckDeviceMultiSampleType 
    //
    D3DMCAPS Caps;
    LPDIRECT3DMOBILESURFACE pBackBuffer = NULL;
    D3DMSURFACE_DESC SurfDesc;

    //
    // Record initial presentation parameter settings
    // 
    D3DMMULTISAMPLE_TYPE InitMultiSampleType = m_d3dpp.MultiSampleType;
    D3DMSWAPEFFECT InitSwapEffect = m_d3dpp.SwapEffect;
    DWORD InitFlags = m_d3dpp.Flags;

    //
    // All failure conditions set the result code specifically
    //
    INT Result = TPR_PASS;

    //
    // Device capabilities are needed to determine correct arguments for
    // CheckDeviceMultiSampleType
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
    {
        DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x). Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }


    //
    // Get the backbuffer, to determine the format thereof
    //
    if (FAILED(hr = m_pd3dDevice->GetBackBuffer(0,                       // UINT BackBuffer,
                                           D3DMBACKBUFFER_TYPE_MONO,// D3DMBACKBUFFER_TYPE Type,
                                           &pBackBuffer)))          // LPDIRECT3DMOBILESURFACE *ppBackBuffer
    {
        DebugOut(DO_ERROR, L"GetBackBuffer failed. (hr = 0x%08x). Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // The surface description of the backbuffer contains it's format,
    // which is needed for CheckDeviceMultiSampleType
    //
    if (FAILED(hr = pBackBuffer->GetDesc(&SurfDesc)))
    {
        DebugOut(DO_ERROR, L"GetDesc failed. (hr = 0x%08x). Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // The back-buffer interface is no longer needed
    //
    pBackBuffer->Release();
    pBackBuffer = NULL;

    //
    // Attempt to identify a multisample type that is valid
    //
    for(uiMultiSample = 0; uiMultiSample < D3DQA_NUM_MULTISAMPLE_ENUMS; uiMultiSample++)
    {
        switch(uiMultiSample)
        {    
        case 0:
            MultiSampleType = D3DMMULTISAMPLE_2_SAMPLES;
            break;
        case 1:
            MultiSampleType = D3DMMULTISAMPLE_3_SAMPLES;
            break;
        case 2:
            MultiSampleType = D3DMMULTISAMPLE_4_SAMPLES;
            break;
        case 3:
            MultiSampleType = D3DMMULTISAMPLE_5_SAMPLES;
            break;
        case 4:
            MultiSampleType = D3DMMULTISAMPLE_6_SAMPLES;
            break;
        case 5:
            MultiSampleType = D3DMMULTISAMPLE_7_SAMPLES;
            break;
        case 6:
            MultiSampleType = D3DMMULTISAMPLE_8_SAMPLES;
            break;
        case 7:
            MultiSampleType = D3DMMULTISAMPLE_9_SAMPLES;
            break;
        case 8:
            MultiSampleType = D3DMMULTISAMPLE_10_SAMPLES;
            break;
        case 9:
            MultiSampleType = D3DMMULTISAMPLE_11_SAMPLES;
            break;
        case 10:
            MultiSampleType = D3DMMULTISAMPLE_12_SAMPLES;
            break;
        case 11:
            MultiSampleType = D3DMMULTISAMPLE_13_SAMPLES;
            break;
        case 12:
            MultiSampleType = D3DMMULTISAMPLE_14_SAMPLES;
            break;
        case 13:
            MultiSampleType = D3DMMULTISAMPLE_15_SAMPLES;
            break;
        case 14:
            MultiSampleType = D3DMMULTISAMPLE_16_SAMPLES;
            break;
        }

        if (SUCCEEDED(hr = m_pD3D->CheckDeviceMultiSampleType(Caps.AdapterOrdinal,     // UINT Adapter,
                                                         Caps.DeviceType,         // D3DMDEVTYPE DeviceType,
                                                         SurfDesc.Format,         // D3DMFORMAT SurfaceFormat,
                                                         m_d3dpp.Windowed,        // BOOL Windowed,
                                                         MultiSampleType)))       // D3DMMULTISAMPLE_TYPE MultiSampleType,
        {
            //
            // Valid format found.
            //
            bValidMultiSample = TRUE;
            break;
        }
    }

    //
    // Has a valid MultiSample type been discovered?  If not, skip test.
    //
    if (FALSE == bValidMultiSample)
    {
        DebugOut(DO_ERROR, L"No MultiSample type discovered. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Attempt to reset with multisampling and lockable back-buffers.  This
    // combination should not be allowed.
    //
    m_d3dpp.MultiSampleType = MultiSampleType;
    m_d3dpp.SwapEffect = D3DMSWAPEFFECT_DISCARD;
    m_d3dpp.Flags = m_d3dpp.Flags | D3DMPRESENTFLAG_LOCKABLE_BACKBUFFER;

    //
    // Reset is expected to fail
    //
    if (SUCCEEDED(hr = m_pd3dDevice->Reset(&m_d3dpp)))
    {
        DebugOut(DO_ERROR, L"Reset succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

cleanup:

    //
    // Set up original PPARMs; reset back into initial state
    //
    m_d3dpp.MultiSampleType = InitMultiSampleType;
    m_d3dpp.SwapEffect = InitSwapEffect;
    m_d3dpp.Flags = InitFlags;
    m_pd3dDevice->Reset(&m_d3dpp);

    if (pBackBuffer)
        pBackBuffer->Release();

    return Result;
}

//
// ExecuteAttemptSwapEffectMultiSampleMismatchTest
//
//   Attempt to use multisampling when disallowed by swap effect
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSwapEffectMultiSampleMismatchTest()
{
    HRESULT hr;


    DebugOut(L"Beginning ExecuteSwapEffectMultiSampleMismatchTest.");
    
    //
    // Variables for finding a valid multisample type, if one exists for this device
    //
    D3DMMULTISAMPLE_TYPE MultiSampleType;
    BOOL bValidMultiSample = FALSE;
    INT uiMultiSample;

    //
    // Variables used in preparing arguments for CheckDeviceMultiSampleType 
    //
    D3DMCAPS Caps;
    LPDIRECT3DMOBILESURFACE pBackBuffer = NULL;
    D3DMSURFACE_DESC SurfDesc;

    //
    // Record initial presentation parameter settings
    // 
    D3DMMULTISAMPLE_TYPE InitMultiSampleType = m_d3dpp.MultiSampleType;
    D3DMSWAPEFFECT InitSwapEffect = m_d3dpp.SwapEffect;
    DWORD InitFlags = m_d3dpp.Flags;

    //
    // All failure conditions set the result code specifically
    //
    INT Result = TPR_PASS;

    //
    // Device capabilities are needed to determine correct arguments for
    // CheckDeviceMultiSampleType
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
    {
        DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x). Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }


    //
    // Get the backbuffer, to determine the format thereof
    //
    if (FAILED(hr = m_pd3dDevice->GetBackBuffer(0,                       // UINT BackBuffer,
                                           D3DMBACKBUFFER_TYPE_MONO,// D3DMBACKBUFFER_TYPE Type,
                                           &pBackBuffer)))          // LPDIRECT3DMOBILESURFACE *ppBackBuffer
    {
        DebugOut(DO_ERROR, L"GetBackBuffer failed. (hr = 0x%08x). Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // The surface description of the backbuffer contains it's format,
    // which is needed for CheckDeviceMultiSampleType
    //
    if (FAILED(hr = pBackBuffer->GetDesc(&SurfDesc)))
    {
        DebugOut(DO_ERROR, L"GetDesc failed. (hr = 0x%08x). Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // The back-buffer interface is no longer needed
    //
    pBackBuffer->Release();
    pBackBuffer = NULL;

    //
    // The multisampling feature can't be used with lockable back-buffers
    //
    m_d3dpp.Flags = m_d3dpp.Flags & ~D3DMPRESENTFLAG_LOCKABLE_BACKBUFFER;

    //
    // Attempt to identify a multisample type that is valid
    //
    for(uiMultiSample = 0; uiMultiSample < D3DQA_NUM_MULTISAMPLE_ENUMS; uiMultiSample++)
    {
        switch(uiMultiSample)
        {    
        case 0:
            MultiSampleType = D3DMMULTISAMPLE_2_SAMPLES;
            break;
        case 1:
            MultiSampleType = D3DMMULTISAMPLE_3_SAMPLES;
            break;
        case 2:
            MultiSampleType = D3DMMULTISAMPLE_4_SAMPLES;
            break;
        case 3:
            MultiSampleType = D3DMMULTISAMPLE_5_SAMPLES;
            break;
        case 4:
            MultiSampleType = D3DMMULTISAMPLE_6_SAMPLES;
            break;
        case 5:
            MultiSampleType = D3DMMULTISAMPLE_7_SAMPLES;
            break;
        case 6:
            MultiSampleType = D3DMMULTISAMPLE_8_SAMPLES;
            break;
        case 7:
            MultiSampleType = D3DMMULTISAMPLE_9_SAMPLES;
            break;
        case 8:
            MultiSampleType = D3DMMULTISAMPLE_10_SAMPLES;
            break;
        case 9:
            MultiSampleType = D3DMMULTISAMPLE_11_SAMPLES;
            break;
        case 10:
            MultiSampleType = D3DMMULTISAMPLE_12_SAMPLES;
            break;
        case 11:
            MultiSampleType = D3DMMULTISAMPLE_13_SAMPLES;
            break;
        case 12:
            MultiSampleType = D3DMMULTISAMPLE_14_SAMPLES;
            break;
        case 13:
            MultiSampleType = D3DMMULTISAMPLE_15_SAMPLES;
            break;
        case 14:
            MultiSampleType = D3DMMULTISAMPLE_16_SAMPLES;
            break;
        }

        if (SUCCEEDED(hr = m_pD3D->CheckDeviceMultiSampleType(Caps.AdapterOrdinal,     // UINT Adapter,
                                                         Caps.DeviceType,         // D3DMDEVTYPE DeviceType,
                                                         SurfDesc.Format,         // D3DMFORMAT SurfaceFormat,
                                                         m_d3dpp.Windowed,        // BOOL Windowed,
                                                         MultiSampleType)))       // D3DMMULTISAMPLE_TYPE MultiSampleType,
        {
            //
            // Valid format found.
            //
            bValidMultiSample = TRUE;
            break;
        }
    }

    //
    // Has a valid MultiSample type been discovered?  If not, skip test.
    //
    if (FALSE == bValidMultiSample)
    {
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // A valid MultiSample type exists; try to reset with a
    // multisample/swapeffect combo that is valid
    //
    m_d3dpp.MultiSampleType = MultiSampleType;
    m_d3dpp.SwapEffect = D3DMSWAPEFFECT_DISCARD;

    //
    // Reset is expected to succeed
    //
    if (FAILED(hr = m_pd3dDevice->Reset(&m_d3dpp)))
    {
        DebugOut(DO_ERROR, L"Reset failed. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // A valid MultiSample type exists; try to reset with a
    // multisample/swapeffect combo that is invalid
    //
    m_d3dpp.MultiSampleType = MultiSampleType;
    m_d3dpp.SwapEffect = D3DMSWAPEFFECT_FLIP;

    //
    // Reset is expected to fail; because multisampling can only be used
    // with a swap type of discard
    //
    if (SUCCEEDED(hr = m_pd3dDevice->Reset(&m_d3dpp)))
    {
        DebugOut(DO_ERROR, L"Reset succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // A valid MultiSample type exists; try to reset with a
    // multisample/swapeffect combo that is invalid
    //
    m_d3dpp.MultiSampleType = MultiSampleType;
    m_d3dpp.SwapEffect = D3DMSWAPEFFECT_COPY;

    //
    // Reset is expected to fail; because multisampling can only be used
    // with a swap type of discard
    //
    if (SUCCEEDED(hr = m_pd3dDevice->Reset(&m_d3dpp)))
    {
        DebugOut(DO_ERROR, L"Reset succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }


cleanup:

    //
    // Set up original PPARMs; reset back into initial state
    //
    m_d3dpp.MultiSampleType = InitMultiSampleType;
    m_d3dpp.SwapEffect = InitSwapEffect;
    m_d3dpp.Flags = InitFlags;
    m_pd3dDevice->Reset(&m_d3dpp);

    if (pBackBuffer)
        pBackBuffer->Release();

    return Result;
}


//
// ExecuteVerifyBackBufLimitSwapEffectCopyTest
//
//   Verify that multiple back-buffers are disallowed, when using the copy swap effect
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteVerifyBackBufLimitSwapEffectCopyTest()
{
    HRESULT hr;

    //
    // Device capabilities for inspecting backbuffer limit
    //
    D3DMCAPS Caps;

    //
    // All failure conditions set the result code specifically
    //
    INT Result = TPR_PASS;

    //
    // Device capabilities are needed to determine correct arguments for
    // CheckDeviceMultiSampleType
    //
    if (FAILED(hr = m_pd3dDevice->GetDeviceCaps(&Caps)))
    {
        DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x). Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // This test can only run on a device that supports multiple back-buffers.
    // A driver indicates this by either exposing a number greater than 1 in MaxBackBuffer,
    // or exposing 0 in MaxBackBuffer (which indicates no theoretical maximum backbuffer
    // count).
    //
    if (!((0 == Caps.MaxBackBuffer) || (2 <= Caps.MaxBackBuffer)))
    {
        DebugOut(DO_ERROR, L"Device does not support multiple backbuffers. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Attempt a reset with multiple backbuffers, and a swap effect of copy 
    //
    m_d3dpp.BackBufferCount = 2;
    m_d3dpp.SwapEffect = D3DMSWAPEFFECT_COPY;

    //
    // Reset is expected to fail; BackBufferCount must be 1 if SwapEffect is COPY
    //
    if (SUCCEEDED(hr = m_pd3dDevice->Reset(&m_d3dpp)))
    {
        DebugOut(DO_ERROR, L"Reset succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Attempt a reset with a single backbuffer, and a swap effect of copy 
    //
    m_d3dpp.BackBufferCount = 1;
    m_d3dpp.SwapEffect = D3DMSWAPEFFECT_COPY;

    //
    // Reset is expected to succeed
    //
    if (FAILED(hr = m_pd3dDevice->Reset(&m_d3dpp)))
    {
        DebugOut(DO_ERROR, L"Reset failed. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

cleanup:

    return Result;
}

//
// ExecuteAttemptSwapChainManipDuringResetTest
//
//   Attempt to manipulate swap chain after a failed reset
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteSwapChainManipDuringResetTest()
{
    HRESULT hr;

    //
    // All failure conditions set the result code specifically
    //
    INT Result = TPR_PASS;

    //
    // Back-buffer interface, to be queried after failed reset
    //
    LPDIRECT3DMOBILESURFACE pBackBuffer = NULL;

    //
    // Store original settings, so that device can be restored after this
    // test case executes
    //
    UINT uiInitialBackBufferCount = m_d3dpp.BackBufferCount;
    D3DMSWAPEFFECT InitialSwapEffect = m_d3dpp.SwapEffect;

    //
    // Verify debug-only check
    //
    if (!m_bDebugMiddleware)
    {
        DebugOut(DO_ERROR, L"Skipping test intended for debug middleware. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Attempt a reset with multiple backbuffers, and a swap effect of copy.
    // This should cause certain failure.  No device should allow this.
    //
    m_d3dpp.BackBufferCount = 2;
    m_d3dpp.SwapEffect = D3DMSWAPEFFECT_COPY;

    //
    // Reset is expected to fail; BackBufferCount must be 1 if SwapEffect is COPY
    //
    if (SUCCEEDED(hr = m_pd3dDevice->Reset(&m_d3dpp)))
    {
        DebugOut(DO_ERROR, L"Reset succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Reset has failed; verify that swap chain calls fail
    //
    if (SUCCEEDED(hr = m_pd3dDevice->GetBackBuffer(0,                       // UINT BackBuffer,
                                              D3DMBACKBUFFER_TYPE_MONO,// D3DMBACKBUFFER_TYPE Type,
                                              &pBackBuffer)))          // LPDIRECT3DMOBILESURFACE *ppBackBuffer
    {
        DebugOut(DO_ERROR, L"GetBackBuffer succeeded; failure expected. (hr = 0x%08x). Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

cleanup:

    //
    // Attempt to restore device to original state
    //
    m_d3dpp.BackBufferCount = uiInitialBackBufferCount;
    m_d3dpp.SwapEffect = InitialSwapEffect;
    m_pd3dDevice->Reset(&m_d3dpp);

    if (pBackBuffer)
        pBackBuffer->Release();

    return Result;
}

//
// ExecuteInterfaceCommandBufferRefTest
//
//   Cause command buffer to do significant amounts of ref counting
//
// Arguments:
//  
//  <none>
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT IDirect3DMobileDeviceTest::ExecuteInterfaceCommandBufferRefTest()
{
    HRESULT hr;

    DebugOut(DO_ERROR, L"Beginning ExecuteInterfaceCommandBufferRefTest.");

    //
    // Display mode, used to determine a format to use
    //
    D3DMDISPLAYMODE Mode;

    //
    // Number of ref counts to cause
    //
    CONST UINT uiNumRefs = 100;
    UINT uiRefIter;

    //
    // Surface format to consider for use (via CheckDeviceFormat)
    //
    D3DMFORMAT CurrentFormat;

    //
    // Color value to use for ColorFill.  Currently, the specific
    // color chosen for use in this test is arbitrary and not
    // particularly significant to the scenarios that this test
    // exercises.
    //
    CONST D3DMCOLOR ArbitraryColor = 0x7F7F7F7F;

    //
    // Interface pointer, to be used with ColorFill
    //
    IDirect3DMobileSurface* pSurface;

    if (!IsReady())
    {
        DebugOut(DO_ERROR, L"Due to failed initialization, the test must be aborted. Skipping.");
        return TPR_SKIP;
    }

    //
    // Retrieve current display mode
    //
    if (FAILED(hr = m_pd3dDevice->GetDisplayMode(&Mode)))// D3DMDISPLAYMODE* pMode
    {
        DebugOut(DO_ERROR, L"Fatal error at GetDisplayMode. (hr = 0x%08x). Failing.", hr);
        return TPR_FAIL;
    }

    //
    // Iterate through all formats in the D3DMFORMAT enumeration, seeking a format that is
    // valid for image surfaces
    //
    for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {
        if (FAILED(hr = m_pD3D->CheckDeviceFormat(m_uiAdapterOrdinal, // UINT Adapter
                                             m_DevType,          // D3DMDEVTYPE DeviceType
                                             Mode.Format,        // D3DMFORMAT AdapterFormat
                                             0,                  // ULONG Usage
                                             D3DMRTYPE_SURFACE,  // D3DMRESOURCETYPE RType
                                             CurrentFormat)))    // D3DMFORMAT CheckFormat
        {
            //
            // This is not a valid format for surfaces
            //
            continue;
        }

        if (FAILED(hr = m_pd3dDevice->CreateImageSurface(D3DQA_DEFAULT_COLORFILL_SURF_WIDTH, // UINT Width
                                                    D3DQA_DEFAULT_COLORFILL_SURF_HEIGHT,// UINT Height
                                                    CurrentFormat,                      // D3DMFORMAT Format
                                                   &pSurface)))                         // IDirect3DMobileSurface** ppSurface
        {
            DebugOut(DO_ERROR, L"CreateImageSurface failed. (hr = 0x%08x). Failing.", hr);
            return TPR_FAIL;
        }

        for (uiRefIter=0; uiRefIter < uiNumRefs; uiRefIter++)
        {
            //
            // Attempt a ColorFill operation on the image surface
            //
            if (FAILED(hr = m_pd3dDevice->ColorFill(pSurface,         // IDirect3DMobileSurface* pSurface
                                               NULL,             // CONST RECT* pRect
                                               ArbitraryColor))) // D3DMCOLOR Color
            {
                DebugOut(DO_ERROR, L"ColorFill failed. (hr = 0x%08x). Failing.", hr);
                pSurface->Release();
                return TPR_FAIL;
            }
        }

        //
        // Release interface
        //
        pSurface->Release();

        break;
    }

    //
    // All failure conditions have already returned.
    //
    return TPR_PASS;

}

INT IDirect3DMobileDeviceTest::ExecutePSLRandomizerTest()
{
    int iIterations = 10000;
    int iResult = TPR_PASS;
//    TCHAR tszIterationLabel[20];

    if (OEM_CERTIFY_TRUST == CeGetCallerTrust())
    {
        DebugOut(DO_ERROR, L"PSL Randomizer test only valid if test is untrusted. Skipping.");
        return TPR_SKIP;
    }

    AttachToAPISet();

    for (int i = 0; i < iIterations; i++)
    {
// The best way to debug a failure is to rerun the test with the appropriate
// random seed, printing the iteration number. Then run and break on that
// iteration.
//        if (47669 == i)
//        {
//            DebugBreak();
//        }
//        _stprintf(tszIterationLabel, L"Iteration %d", i);
//        DebugOut(DO_ERROR, tszIterationLabel);
        __try
        {
            CallRandomizedAPI();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            iResult = TPR_FAIL;
        }
    }
    return iResult;
}
