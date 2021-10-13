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
#include "DebugOutput.h"
#include "D3DMHelper.h"
#include "MultiInstTestHelper.h"
#include "MultiInstTools.h"

const int SHUTDOWN_TIMEOUT = 60000;

int GetModulePath(HMODULE hMod, __out_ecount(dwCchSize) TCHAR * tszPath, DWORD dwCchSize);

MultiInstLocalInstance::MultiInstLocalInstance() : D3DMInitializer()
{
    D3DMInitializer::UseCleanD3DM(FALSE);
}

HRESULT MultiInstLocalInstance::Initialize(int Instance, WCHAR * wszDeviceName, DWORD dwTestIndex)
{
    WINDOW_ARGS WindowArgs;
    TCHAR tszName[32];
    StringCchPrintf(tszName, _countof(tszName), _T("MultiInstLocal_%d"), Instance);
    memset(&WindowArgs, 0x00, sizeof(WindowArgs));
    WindowArgs.uiWindowWidth = 100;
    WindowArgs.uiWindowHeight = 100;
    WindowArgs.uiWindowStyle = WS_OVERLAPPED;
    WindowArgs.lpClassName = tszName;
    WindowArgs.lpWindowName = tszName;
    WindowArgs.bPParmsWindowed = TRUE;

    return D3DMInitializer::Init(D3DQA_PURPOSE_RAW_TEST, &WindowArgs, wszDeviceName, dwTestIndex);
}

HRESULT MultiInstLocalInstance::SetOptions(const MultiInstOptions * pOptions)
{
    if (!pOptions)
    {
        return E_POINTER;
    }
    m_Options = *pOptions;
    return S_OK;
}

HRESULT MultiInstLocalInstance::RunInstTest()
{
    HRESULT hr;
    // Move the window to one of the corners (so that the contents aren't completely clipped)
    int xPos = 0;
    int yPos = 0;
    RECT rc;

    GetWindowRect(m_hWnd, &rc);
    if (m_Options.InstanceCount & 1)
    {
        xPos = GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left);
    }
    if (m_Options.InstanceCount & 2)
    {
        yPos = GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top);
    }
    SetWindowPos(m_hWnd, HWND_NOTOPMOST, xPos, yPos, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);


    OutputDebugString(_T("D3DMHelper: Calling MultiInstRenderScene"));
    hr = MultiInstRenderScene(m_pd3dDevice, &m_Options);
    OutputDebugString(_T("D3DMHelper: MultiInstRenderScene returned"));
    Sleep(1000);
    return hr;

}

HRESULT MultiInstLocalInstance::Shutdown()
{
    D3DMInitializer::Cleanup();
    return S_OK;
}

HRESULT MultiInstRemoteControl::InitializeInstances(
    int     InstanceCount,
    WCHAR * wszDeviceName,
    DWORD   dwTestIndex)
{
    HRESULT hr;
    HRESULT hrRemoteResult = S_OK;
    D3DMHCreateD3DMDeviceOptions_t CreateOptions;
    int i;
    
    //
    // The location of the D3DM Helper process
    //
    TCHAR tszHelperProcess[MAX_PATH + 1] = _T("");

    if (m_Initialized)
    {
        hr = E_UNEXPECTED;
        goto cleanup;
    }
    m_RemoteInstCount = InstanceCount;
    if (m_RemoteInstCount > MULTIINSTCONTROL_MAXINSTANCECOUNT || m_RemoteInstCount < 1)
    {
        return E_INVALIDARG;
    }
    
    for (i = 0; i < m_RemoteInstCount; ++i)
    {
        // Fire up the remote renderer.
        hr = m_rgRemoteInsts[i].Initialize(GetModuleHandle(NULL), _T("D3DM_DriverComp_MultipleInstance"), i);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Unable to initialize RemoteTester Master (instance %d). (hr = 0x%08x)", i, hr);
            // The RemoteTest destructor will clean up
            goto cleanup;
        }

        GetModulePath(GetModuleHandle(NULL), tszHelperProcess, _countof(tszHelperProcess));
        StringCchCat(tszHelperProcess, _countof(tszHelperProcess), D3DMHELPERPROC);
        hr = m_rgRemoteInsts[i].CreateRemoteTestProcess(tszHelperProcess);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, 
                L"Unable to create RemoteTester slave process \"%s\" (instance %d). (hr = 0x%08x)",
                tszHelperProcess,
                i,
                hr);
            goto cleanup;
        }

        // Try to create a device in the other process
        memset(&CreateOptions, 0x00, sizeof(CreateOptions));
        CreateOptions.Purpose = D3DQA_PURPOSE_RAW_TEST;
        CreateOptions.WindowArgs.uiWindowWidth = 100;
        CreateOptions.WindowArgs.uiWindowHeight = 100;
        CreateOptions.WindowArgs.uiWindowStyle = WS_OVERLAPPED;
        // RemoteTester does not handle embedded pointers.
        CreateOptions.WindowArgs.lpClassName = NULL;
        CreateOptions.WindowArgs.lpWindowName = NULL;
        CreateOptions.WindowArgs.bPParmsWindowed = TRUE;
        if (wszDeviceName)
        {
            StringCchCopy(CreateOptions.DriverName, _countof(CreateOptions.DriverName), wszDeviceName);
        }
        CreateOptions.TestCase = dwTestIndex;

        hr = m_rgRemoteInsts[i].SendTaskSynchronous(
            D3DMHELPER_CREATEDEVICE,
            sizeof(CreateOptions),
            &CreateOptions,
            &hrRemoteResult,
            5000);
            
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR,
                L"Unable to remotely process command CREATEDEVICE (instance %d). (hr = 0x%08x)",
                i,
                hr);
            goto cleanup;
        }

        if (FAILED(hrRemoteResult))
        {
            DebugOut(DO_ERROR,
                L"Unable to create IDirect3DMobileDevice object in other process (instance %d). (hr = 0x%08x)",
                i,
                hrRemoteResult);
            hr = hrRemoteResult;
            goto cleanup;
        }
    }

    m_Initialized = true;
    
cleanup:
    return hr;
}
HRESULT MultiInstRemoteControl::RunInstances(const MultiInstOptions * pOptions)
{
    HRESULT hr;
    MultiInstOptions RemoteOpts = *pOptions;
    if (!m_Initialized)
    {
        hr = E_UNEXPECTED;
        goto cleanup;
    }
    if (!pOptions)
    {
        hr = E_POINTER;
        goto cleanup;
    }
    
    for (int i = 0; i < m_RemoteInstCount; ++i)
    {
        RemoteOpts.InstanceCount = i;
        hr = m_rgRemoteInsts[i].SendTask(
            D3DMHELPER_COMPLEXRENDERPRESENT,
            sizeof(RemoteOpts),
            (void*) &RemoteOpts,
            &m_rgRemoteEvents[i]);
            
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR,
                L"Unable to remotely process command CREATEDEVICE (instance %d). (hr = 0x%08x) Aborting.",
                i,
                hr);
            goto cleanup;
        }
    }
cleanup:
    return hr;
}

HRESULT MultiInstRemoteControl::ShutdownInstances(BOOL Successful)
{
    HRESULT hr = S_OK;
    HRESULT hrRemoteResult;

    for (int i = 0; i < m_RemoteInstCount; ++i)
    {
        if (Successful)
        {
            m_rgRemoteInsts[i].WaitForCompletedEvent(m_rgRemoteEvents[i], &hrRemoteResult, SHUTDOWN_TIMEOUT);
            if (FAILED(hrRemoteResult))
            {
                DebugOut(DO_ERROR, _T("Remote Test Rendering failed (instance %d). (hr = 0x%08x) Failing."), hrRemoteResult);
                hr = hrRemoteResult;
            }
        }
        m_rgRemoteInsts[i].Shutdown();
    }    
    m_Initialized = false;
    return hr;
}

HRESULT MultiInstLocalControl::InitializeInstances(
    int     InstanceCount,
    WCHAR * wszDeviceName,
    DWORD   dwTestIndex)
{
    HRESULT hr = S_OK;
    if (m_Initialized)
    {
        hr = E_UNEXPECTED;
        goto cleanup;
    }
    m_LocalInstCount = InstanceCount;
    if (m_LocalInstCount > MULTIINSTCONTROL_MAXINSTANCECOUNT || m_LocalInstCount < 1)
    {
        hr = E_INVALIDARG;
        goto cleanup;
    }

    for (int i = 0; i < m_LocalInstCount; ++i)
    {
        hr = m_rgLocalInsts[i].Initialize(i, wszDeviceName, dwTestIndex);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Initialize LocalInst %d failed. (hr = 0x%08x)", i, hr);
            goto cleanup;
        }
        
    }
    
    m_Initialized = true;
cleanup:
    return hr;
}

HRESULT MultiInstLocalControl::RunInstances(const MultiInstOptions * pOptions)
{
    HRESULT hr;
    MultiInstOptions Options;
    if (!m_Initialized)
    {
        hr = E_UNEXPECTED;
        goto cleanup;
    }
    if (!pOptions)
    {
        hr = E_POINTER;
        goto cleanup;
    }

    Options = *pOptions;

    for (int i = 0; i < m_LocalInstCount; ++i)
    {
        Options.InstanceCount = i;
        m_rgLocalInsts[i].SetOptions(&Options);

        m_rgThreads[i] = CreateThread(
            NULL,
            0,
            RunSingleInstance,
            (void*)&m_rgLocalInsts[i],
            0,
            NULL);
        if (!m_rgThreads[i])
        {
            DebugOut(DO_ERROR, L"CreateThread for Instance %d failed (GLE = %d)", i, GetLastError());
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto cleanup;
        }
    }
    
cleanup:
    return hr;
}

HRESULT MultiInstLocalControl::ShutdownInstances(BOOL Successful)
{
    HRESULT hrLocalResult;
    HRESULT hr = S_OK;

    for (int i = 0; i < m_LocalInstCount; ++i)
    {
        WaitForSingleObject(m_rgThreads[i], SHUTDOWN_TIMEOUT);
        GetExitCodeThread(m_rgThreads[i], (DWORD*)&hrLocalResult);
        if ((HRESULT)STILL_ACTIVE == hrLocalResult)
        {
            DebugOut(DO_ERROR, L"Thread LocalInst %d did not exit.", i);
            hr = E_FAIL;
        }
        else if (FAILED(hrLocalResult))
        {
            DebugOut(DO_ERROR, L"LocalInst %d returned failure from rendering. (hr = 0x%08x)", i, hrLocalResult);
            hr = hrLocalResult;
        }
        
        hrLocalResult = m_rgLocalInsts[i].Shutdown();
        if (FAILED(hrLocalResult))
        {
            DebugOut(DO_ERROR, L"Shutdown LocalInst %d failed. (hr = 0x%08x)", i, hrLocalResult);
            hr = hrLocalResult;
        }
    }
    
    m_Initialized = false;
    return hr;
}

DWORD WINAPI MultiInstLocalControl::RunSingleInstance(LPVOID pvLocalInstance)
{
    MultiInstLocalInstance * pLocalInstance;
    pLocalInstance = (MultiInstLocalInstance *) pvLocalInstance;
    if (!pLocalInstance)
    {
        return (DWORD) E_POINTER;
    }
    
    return (DWORD) pLocalInstance->RunInstTest();
}

