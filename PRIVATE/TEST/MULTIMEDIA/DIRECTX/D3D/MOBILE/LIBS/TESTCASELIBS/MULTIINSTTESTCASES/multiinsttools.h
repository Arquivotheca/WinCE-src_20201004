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
//
#pragma once

#include <RemoteTester.h>
#include "MultiInstTestHelper.h"

#define MULTIINSTCONTROL_MAXINSTANCECOUNT 4

class MultiInstLocalInstance : public D3DMInitializer
{
public:
    MultiInstLocalInstance();
    HRESULT Initialize(int Instance, WCHAR * wszDeviceName, DWORD dwTestIndex);
    HRESULT SetOptions(const MultiInstOptions * pOptions);
    HRESULT RunInstTest();
    HRESULT Shutdown();
private:
    MultiInstOptions m_Options;
};

class MultiInstInstanceControl {
public:
    virtual HRESULT InitializeInstances(
        int     InstanceCount,
        WCHAR * wszDeviceName,
        DWORD   dwTestIndex) = 0;
    virtual HRESULT RunInstances(const MultiInstOptions * pOptions) = 0;
    virtual HRESULT ShutdownInstances(BOOL Successful) = 0;
    virtual ~MultiInstInstanceControl() {};
};

class MultiInstRemoteControl : public MultiInstInstanceControl {
public:
    MultiInstRemoteControl() : m_Initialized(false) {};
    virtual ~MultiInstRemoteControl() {};
    virtual HRESULT InitializeInstances(
        int     InstanceCount,
        WCHAR * wszDeviceName,
        DWORD   dwTestIndex);
    virtual HRESULT RunInstances(const MultiInstOptions * pOptions);
    virtual HRESULT ShutdownInstances(BOOL Successful);

private:
    bool m_Initialized;
    int m_RemoteInstCount;
    DWORD m_rgRemoteEvents[MULTIINSTCONTROL_MAXINSTANCECOUNT];
    RemoteMaster_t m_rgRemoteInsts[MULTIINSTCONTROL_MAXINSTANCECOUNT];
};

class MultiInstLocalControl : public MultiInstInstanceControl {
public:
    MultiInstLocalControl() : m_Initialized(false) {
        memset(m_rgThreads, 0x00, sizeof(m_rgThreads));
    };
    virtual ~MultiInstLocalControl() {};
    virtual HRESULT InitializeInstances(
        int     InstanceCount,
        WCHAR * wszDeviceName,
        DWORD   dwTestIndex);
    virtual HRESULT RunInstances(const MultiInstOptions * pOptions);
    virtual HRESULT ShutdownInstances(BOOL Successful);

    static DWORD WINAPI RunSingleInstance(LPVOID pvLocalInstance);
private:
    bool m_Initialized;
    int m_LocalInstCount;
    MultiInstLocalInstance m_rgLocalInsts[MULTIINSTCONTROL_MAXINSTANCECOUNT];
    HANDLE m_rgThreads[MULTIINSTCONTROL_MAXINSTANCECOUNT];
};
