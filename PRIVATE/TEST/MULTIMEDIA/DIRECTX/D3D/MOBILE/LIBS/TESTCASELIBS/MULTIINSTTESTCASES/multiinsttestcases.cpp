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
#include <windows.h>
#include "ImageManagement.h"
#include "MultiInstTestHelper.h"
#include <tux.h>
#include "D3DMHelper.h"
#include "DebugOutput.h"
#include "MultiInstCases.h"
#include "MultiInstTools.h"

//
// MultiInstTest
//
//   Render and capture a single test frame; corresponding to a test case
//   for lighting.
//
// Arguments:
//   
//   LPTESTCASEARGS pTestCaseArgs:  Information pertinent to test case execution
//
// Return Value:
//
//   TPR_PASS, TPR_FAIL, TPR_SKIP, or TPR_ABORT; depending on test result.
//
INT MultiInstTest(LPTESTCASEARGS pTestCaseArgs, LPTEST_CASE_ARGS pTCArgs)

{
    //
    // API Result code
    //
    HRESULT hr;

    //
    // Texture capabilities required by this test. Specified below.
    //
    DWORD RequiredTextureOpCap;

    //
    // Test Result
    //
    INT Result = TPR_PASS;

    //
    // Target device (local variable for brevity in code)
    //
    LPDIRECT3DMOBILEDEVICE pDevice = NULL;

    //
    // The index into the test case table, based off of the test case ID
    //
    DWORD dwTableIndex;

    //
    // Device capabilities
    //
    D3DMCAPS Caps;

    //
    // The object that will run our tests on different instances.
    // Can be either MultiInstRemoteControl or MultiInstLocalControl.
    //
    MultiInstInstanceControl * pControl = NULL;
    
    //
    // Parameter validation
    //
    if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
    {
        DebugOut(DO_ERROR, _T("Invalid test case argument(s). Aborting."));
        Result = TPR_ABORT;
        goto cleanup;
    }

    if (FALSE == pTestCaseArgs->pParms->Windowed)
    {
        DebugOut(DO_ERROR, _T("MultiInstance tests must be run in windowed mode (verify that /e7 is NOT specified on command line. Aborting."));
        Result = TPR_ABORT;
        goto cleanup;
    }

    pDevice = pTestCaseArgs->pDevice;

    //
    // Choose test case subset
    //
    if ((pTestCaseArgs->dwTestIndex >= D3DMQA_MULTIINSTTEST_BASE) &&
        (pTestCaseArgs->dwTestIndex <= D3DMQA_MULTIINSTTEST_MAX))
    {
        //
        // Lighting test cases
        //

        dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_MULTIINSTTEST_BASE;
    }
    else
    {
        DebugOut(DO_ERROR, L"Invalid test index. Failing.");
        Result = TPR_FAIL;
        goto cleanup;
    }

    if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
    {
        DebugOut(DO_ERROR, L"Could not retrieve device capabilities. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Verify that required texture ops are supported
    //
    if (D3DMMULTIINSTTEST_FVF_TEX == MultiInstCases[dwTableIndex].FVF)
    {
        RequiredTextureOpCap = D3DMTEXOPCAPS_MODULATE;
    }
    else
    {
        RequiredTextureOpCap = D3DMTEXOPCAPS_SELECTARG1;
    }

    if (!(Caps.TextureOpCaps & RequiredTextureOpCap))
    {
        DebugOut(DO_ERROR, _T("Inadequate TextureOpCaps; Skipping."));
        Result = TPR_SKIP;
        goto cleanup;
    }
    
    //
    // Verify that required alpha blend ops are supported
    //
    if (MultiInstCases[dwTableIndex].VertexAlpha &&
        !(Caps.DestBlendCaps & D3DMPBLENDCAPS_INVDESTCOLOR) ||
        !(Caps.SrcBlendCaps & D3DMPBLENDCAPS_SRCCOLOR) ||
        !(Caps.BlendOpCaps & D3DMBLENDOPCAPS_ADD))
    {
        DebugOut(DO_ERROR, _T("Inadequate Alpha Blend caps; Skipping."));
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Verify that required fog blend ops are supported
    //
    DWORD RequiredFogOpCap = 0;
    if (MultiInstCases[dwTableIndex].VertexFog)
    {
        RequiredFogOpCap = D3DMPRASTERCAPS_FOGVERTEX;
    }
    else if (MultiInstCases[dwTableIndex].TableFog)
    {
        RequiredFogOpCap = D3DMPRASTERCAPS_FOGTABLE;
    }
    
    if (RequiredFogOpCap && !(Caps.RasterCaps & RequiredFogOpCap))
    {
        DebugOut(DO_ERROR, _T("Inadequate RasterCaps (fog); Skipping."));
        Result = TPR_SKIP;
        goto cleanup;
    }

    if (MultiInstCases[dwTableIndex].UseLocalInstances)
    {
        pControl = new MultiInstLocalControl();
        if (!pControl)
        {
            DebugOut(DO_ERROR, _T("Out of Memory allocating local instance controller. Aborting."));
            Result = TPR_ABORT;
            goto cleanup;
        }
    }
    else
    {
        pControl = new MultiInstRemoteControl();
        if (!pControl)
        {
            DebugOut(DO_ERROR, _T("Out of Memory allocating remote instance controller. Aborting."));
            Result = TPR_ABORT;
            goto cleanup;
        }
    }

    hr = pControl->InitializeInstances(
        MultiInstCases[dwTableIndex].InstanceCount - 1,
        pTCArgs->pwchSoftwareDeviceFilename,
        pTestCaseArgs->dwTestIndex);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, 
            L"InitializeInstances failed (Local = %d). (hr = 0x%08x) Aborting", 
            MultiInstCases[dwTableIndex].UseLocalInstances,
            hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    
    hr = pControl->RunInstances(&MultiInstCases[dwTableIndex]);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, 
            L"RunInstances failed (Local = %d). (hr = 0x%08x) Aborting.", 
            MultiInstCases[dwTableIndex].UseLocalInstances,
            hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    
    // Bring our window to the front.
    SetWindowPos(pTestCaseArgs->hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

    hr = MultiInstRenderScene(pDevice, &MultiInstCases[dwTableIndex]);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, _T("MultiInstRenderScene failed. (hr = 0x%08x) Failing."), hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

    //
    // Flush the swap chain and capture a frame
    //
    if (FAILED(hr = DumpFlushedFrame(pTestCaseArgs,                 // LPTESTCASEARGS pTestCaseArgs
                                     0,                             // UINT uiFrameNumber,
                                     NULL)))                        // RECT *pRect = NULL
    {
        DebugOut(DO_ERROR, _T("DumpFlushedFrame failed. (hr = 0x%08x) Failing."), hr);
        Result = TPR_FAIL;
        goto cleanup;
    }


cleanup:
    if (pControl)
    {
        hr = pControl->ShutdownInstances(SUCCEEDED(hr));
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, 
                L"ShutdownInstances failed (Local = %d). (hr = 0x%08x) Failing.", 
                MultiInstCases[dwTableIndex].UseLocalInstances,
                hr);
            Result = TPR_FAIL;
        }
        delete pControl;
        pControl = NULL;
    }
    return Result;
}

