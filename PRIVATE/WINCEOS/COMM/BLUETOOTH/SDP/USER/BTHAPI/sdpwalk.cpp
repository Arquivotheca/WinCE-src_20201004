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
// SdpWalk.cpp : Implementation of CSdpWalk
#include "stdafx.h"
#include "BthAPI.h"
#include "SdpWalk.h"

/////////////////////////////////////////////////////////////////////////////
// CSdpWalk

HRESULT STDMETHODCALLTYPE 
CSdpWalk::QueryInterface(REFIID iid, void ** ppvObject) {
    if (!ppvObject)
        return E_POINTER;

    if (iid == IID_IUnknown || iid == IID_ISdpWalk) {
        *ppvObject = (ISdpWalk*)this;
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE 
CSdpWalk::AddRef() {
    return InterlockedIncrement(&refCount);
}

ULONG STDMETHODCALLTYPE 
CSdpWalk::Release() {
    if (0 == InterlockedDecrement(&refCount)) {
        delete this;
        return 0;
    }
    return refCount;
}

STDMETHODIMP CSdpWalk::WalkNode(NodeData *pData, ULONG state)
{
    return S_OK;
}

STDMETHODIMP CSdpWalk::WalkStream(USHORT type, USHORT specificType, UCHAR *pStream, ULONG streamSize)
{
    return S_OK;
}

