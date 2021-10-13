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
#include <d3dm.h>

#include <RemoteTester.h>
#include <DebugOutput.h>

#include "D3DMHelper.h"

#include "MultiInstTestHelper.h"

#define D3DMDLL _T("D3DM.DLL")
#define D3DMCREATE _T("Direct3DMobileCreate")

#define countof(x) (sizeof(x)/sizeof(*(x)))

HANDLE g_hInstance = NULL;

//static HRESULT Callback(Task_t * pTask, void * pvContext);
HRESULT D3DMHelper_t::Callback(Task_t * pTask, void * pvContext)
{
    D3DMHelper_t * This = (D3DMHelper_t*)pvContext;
    switch(pTask->GetTaskID())
    {
    case D3DMHELPER_TRYCREATEOBJECT:
        return This->TryCreatingD3DMObject();
    case D3DMHELPER_CREATEDEVICE:
        return This->CreateD3DMDevice(pTask);
    case D3DMHELPER_CLEARPRESENT:
        return This->ClearAndPresent(pTask);
    case D3DMHELPER_COMPLEXRENDERPRESENT:
        return This->ComplexRenderAndPresent(pTask);
    case D3DMHELPER_DIRECTDRAWCREATE:
        return This->DirectDrawCreation(pTask);
    case D3DMHELPER_DIRECTDRAWSURFACE:
        return This->DirectDrawTryCreatePrimary(pTask);
    case D3DMHELPER_CLEANUP:
        return This->Cleanup();
    case D3DMHELPER_RELEASEDEVICE:
        return This->ReleaseD3DMDevice();
    case D3DMHELPER_TESTCOOPLEVEL:
        return This->TestCoopLevel();
    default:
        return E_INVALIDARG;
    }
}

//D3DMHelper_t();
D3DMHelper_t::D3DMHelper_t() :
    D3DMInitializer(),
    m_pDD(NULL)
{
}

//virtual ~D3DMHelper_t();
D3DMHelper_t::~D3DMHelper_t()
{
    Cleanup();
}

HRESULT D3DMHelper_t::SetName(const TCHAR * tszName)
{
    return StringCchPrintf(m_tszName, countof(m_tszName), _T("D3DMHelper:%s"), tszName);
}

HRESULT D3DMHelper_t::TryCreatingD3DMObject()
{
    HRESULT hr;
    OutputDebugString(_T("D3DMHelper_t::TryCreatingD3DMObject called"));
    // Since we already have a D3DM object, this should fail
    IDirect3DMobile * pMobile = NULL;
    LPDIRECT3DMOBILE (WINAPI* pfnDirect3DMobileCreate)(UINT SDKVersion);
    HMODULE hD3DMDLL = NULL;
    FARPROC pSWDeviceEntry = NULL;
    hD3DMDLL = LoadLibrary(D3DMDLL);
    if (NULL == hD3DMDLL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        DebugOut(DO_ERROR, _T("GetModuleHandle failed for \"%s\". Error: 0x%08x. Failing."), D3DMDLL, hr);
        goto Cleanup;
    }
    
    //
    // The address of the DLL's exported function indicates success. 
    // NULL indicates failure. 
    //
    (FARPROC&)pfnDirect3DMobileCreate = GetProcAddress(hD3DMDLL,    // HMODULE hModule, 
                                                       D3DMCREATE); // LPCWSTR lpProcName
    if (NULL == pfnDirect3DMobileCreate)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        DebugOut(DO_ERROR, _T("GetProcAddress failed for Direct3DMobileCreate. Error: 0x%08x. Failing."), hr);
        goto Cleanup;
    }

    pMobile = pfnDirect3DMobileCreate( D3DM_SDK_VERSION );
    if (NULL == pMobile)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        DebugOut(DO_ERROR, L"Failed to create Direct3DMobile object in helper process. (hr = 0x%08x) Failing.", hr);
        hr = E_FAIL;
        goto Cleanup;
    }
Cleanup:
    if (pMobile)
    {
        pMobile->Release();
        pMobile = NULL;
    }
    FreeLibrary(hD3DMDLL);
    return hr;
}


// The CreateD3DMDevice command accepts the D3DMHCreateD3DMDevice_t structure,
// and uses that structure's members to call CreateDevice, saving the
// device pointer for later.
//HRESULT CreateD3DMDevice(Task_t * pTask);
HRESULT D3DMHelper_t::CreateD3DMDevice(Task_t * pTask)
{
    HRESULT hr;
    DebugOut(_T("D3DMHelper_t::CreateD3DMDevice called"));
    D3DMHCreateD3DMDeviceOptions_t CreateOptions;
    WCHAR * wszDriverName = NULL;
    
    hr = pTask->GetTaskData(sizeof(CreateOptions), &CreateOptions);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"GetTaskData failed. (hr = 0x%08x) Failing.", hr);
        return hr;
    }

    if (NULL == m_pD3D)
    {
        CreateOptions.WindowArgs.lpClassName = CreateOptions.WindowArgs.lpWindowName = m_tszName;
        if (CreateOptions.DriverName[0])
        {
            wszDriverName = CreateOptions.DriverName;
        }

        return D3DMInitializer::Init(CreateOptions.Purpose, &CreateOptions.WindowArgs, wszDriverName, CreateOptions.TestCase);
    }

    // Make sure we have a fresh start.
    ReleaseD3DMDevice();

    switch (CreateOptions.Purpose)
    {
    case D3DQA_PURPOSE_WORLD_VIEW_TEST:
        return D3DInitForWorldViewTest(&CreateOptions.WindowArgs);
    case D3DQA_PURPOSE_SMALL_TOPMOST:
    case D3DQA_PURPOSE_RAW_TEST:
        return D3DInitRawTest(&CreateOptions.WindowArgs);
    case D3DQA_PURPOSE_DEPTH_STENCIL_TEST:
        return D3DDepthStencilSupportedTest(&CreateOptions.WindowArgs);
    case D3DQA_PURPOSE_TEXTURE_TEST:
    case D3DQA_PURPOSE_CLIP_STATUS_TEST:
    case D3DQA_PURPOSE_FULLPIPE_TEST:
    default:
        DebugOut(DO_ERROR, _T("D3DMInitializer: Not yet implemented.\n"));
        return E_NOTIMPL;
    }
}

HRESULT D3DMHelper_t::ReleaseD3DMDevice()
{
    if (m_pd3dDevice)
        m_pd3dDevice->Release();
    m_pd3dDevice = NULL;
    return S_OK;
}

HRESULT D3DMHelper_t::TestCoopLevel()
{
    if (m_pd3dDevice)
    {
        return m_pd3dDevice->TestCooperativeLevel();
    }
    return E_UNEXPECTED;
}

//
//HRESULT ClearAndPresent(Task_t * pTask);
HRESULT D3DMHelper_t::ClearAndPresent(Task_t * pTask)
{
    DebugOut(_T("D3DMHelper_t::ClearAndPresent called"));
    return E_NOTIMPL;
}

//
//HRESULT ComplexRenderAndPresent(Task_t * pTask);
HRESULT D3DMHelper_t::ComplexRenderAndPresent(Task_t * pTask)
{
    DebugOut(_T("D3DMHelper_t::ComplexRenderAndPresent called"));

    HRESULT hr;
    MultiInstOptions Options;
    // Move the window to one of the corners (so that the contents aren't completely clipped)
    int xPos = 0;
    int yPos = 0;
    RECT rc;
    hr = pTask->GetTaskData(sizeof(Options), &Options);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, _T("GetTaskData failed for ComplexRenderAndPresent"));
        return hr;
    }

    GetWindowRect(m_hWnd, &rc);
    if (Options.InstanceCount & 1)
    {
        xPos = GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left);
    }
    if (Options.InstanceCount & 2)
    {
        yPos = GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top);
    }
    SetWindowPos(m_hWnd, HWND_NOTOPMOST, xPos, yPos, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);


    DebugOut(_T("D3DMHelper: Calling MultiInstRenderScene"));
    hr = MultiInstRenderScene(m_pd3dDevice, &Options);
    DebugOut(_T("D3DMHelper: MultiInstRenderScene returned"));
    return hr;
}

//
//HRESULT DirectDrawCreation(Task_t * pTask);
HRESULT D3DMHelper_t::DirectDrawCreation(Task_t * pTask)
{
    return E_NOTIMPL;
}

//
//// This is a good way to determine if the DirectDraw object is lost or not.
//HRESULT DirectDrawTryCreatePrimary(Task_t * pTask);
HRESULT D3DMHelper_t::DirectDrawTryCreatePrimary(Task_t * pTask)
{
    return E_NOTIMPL;
}

//
//// Frees up the objects we hold
//HRESULT Cleanup();
HRESULT D3DMHelper_t::Cleanup()
{
    if (NULL != m_pDD)
    {
        m_pDD->Release();
        m_pDD = NULL;
    }
    D3DMInitializer::Cleanup();
    return S_OK;
}

int WINAPI WinMain
(
    HINSTANCE hInstance,
    HINSTANCE hInstPrev,
#ifdef  UNDER_CE
    LPWSTR    pszCmdLine,
#else
    LPSTR     pszCmdLine,
#endif
    int       nCmdShow
)
{
    // Our plan here is to just hang loose until we get something to do.
    HRESULT hr;
    RemoteSlave_t CommandProvider;
    D3DMHelper_t Helper;

    g_hInstance = hInstance;

    hr = CommandProvider.InitializeFromCommandLine(hInstance, pszCmdLine, (PFNRemoteSlaveCallback_t)D3DMHelper_t::Callback);
    if (FAILED(hr))
    {
        return (int)hr;
    }

    Helper.SetName(CommandProvider.GetSlaveName());
    
    hr = CommandProvider.Run(&Helper);

    CommandProvider.Shutdown();

    return hr;
}
