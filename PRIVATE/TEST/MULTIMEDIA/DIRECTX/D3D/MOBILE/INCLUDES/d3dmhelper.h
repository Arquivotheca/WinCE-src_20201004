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
#pragma once

#include <windows.h>
#include <RemoteTester.h>
#include "initializer.h"
#include <ddraw.h>

#define D3DMHELPERPROC _T("D3DMHelper.exe")
#define D3DMHELPER_TRYCREATEOBJECT      0
#define D3DMHELPER_CREATEDEVICE         1
#define D3DMHELPER_CLEARPRESENT         2
#define D3DMHELPER_COMPLEXRENDERPRESENT 3
#define D3DMHELPER_DIRECTDRAWCREATE     4
#define D3DMHELPER_DIRECTDRAWSURFACE    5
#define D3DMHELPER_CLEANUP              6
#define D3DMHELPER_RELEASEDEVICE        7
#define D3DMHELPER_TESTCOOPLEVEL        8

struct D3DMHCreateD3DMDeviceOptions_t
{
    D3DQA_PURPOSE Purpose;
    // WindowArgs' embedded pointers are null, need to be assigned
    WINDOW_ARGS   WindowArgs;
    WCHAR         DriverName[MAX_PATH];
    UINT          TestCase;
};

struct D3DMHClearOptions_t
{
    DWORD Flags;
    D3DMCOLOR Color;
    float Z;
    DWORD Stencil;
};

struct D3DMHPresentOptions_t
{
    RECT SourceRect;
    RECT DestRect;

    // Unsupported currently
    bool UseOverride;
};

struct D3DMHClearAndPresentOptions_t
{
    // Options for the Clear call
    D3DMHClearOptions_t ClearOptions;
    D3DMHPresentOptions_t PresentOptions;
};

struct D3DMHDirectDrawCreationOptions_t
{
    bool UseFullscreen;
};

class D3DMHelper_t : public D3DMInitializer
{
public:
    static HRESULT Callback(Task_t * pTask, void * pvContext);

    D3DMHelper_t();
    virtual ~D3DMHelper_t();

    // Sets the name of the helper (used when creating the window)
    HRESULT SetName(const TCHAR * tszName);

    // Just creates a D3DM Object and returns the result. The object is released
    // before returning.
    HRESULT TryCreatingD3DMObject();

    // The CreateD3DMDevice command accepts the D3DMHCreateD3DMDevice_t structure,
    // and uses that structure's members to call CreateDevice, saving the
    // device pointer for later.
    HRESULT CreateD3DMDevice(Task_t * pTask);

    // If we have a device, just go ahead and release it, without cleaning
    // up everything else.
    HRESULT ReleaseD3DMDevice();

    HRESULT TestCoopLevel();

    HRESULT ClearAndPresent(Task_t * pTask);

    HRESULT ComplexRenderAndPresent(Task_t * pTask);

    HRESULT DirectDrawCreation(Task_t * pTask);

    // This is a good way to determine if the DirectDraw object is lost or not.
    HRESULT DirectDrawTryCreatePrimary(Task_t * pTask);

    // Frees up the objects we hold
    HRESULT Cleanup();

private:
    TCHAR m_tszName[200];
    IDirectDraw * m_pDD;
};
