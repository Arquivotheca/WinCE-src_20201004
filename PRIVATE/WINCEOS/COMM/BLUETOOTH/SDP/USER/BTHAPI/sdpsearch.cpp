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
// SdpSearch.cpp : Implementation of CSdpSearch
#include "stdafx.h"
#include "BthAPI.h"
#include "SdpSearch.h"

/////////////////////////////////////////////////////////////////////////////
// CSdpSearch

// BUGBUG:  define an error code of not initialized
#define PRELOG()    if (hRadio == INVALID_HANDLE_VALUE || hConnection == 0) return E_ACCESSDENIED;


STDMETHODIMP CSdpSearch::Begin(ULONGLONG *pAddress, ULONG fConnect) { return E_NOTIMPL; }
STDMETHODIMP CSdpSearch::End() { return E_NOTIMPL; }

STDMETHODIMP
CSdpSearch::ServiceSearch(
    SdpQueryUuid *pUuidList,
    ULONG listSize,
    ULONG *pHandles,
    USHORT *pNumHandles
    )
{ return E_NOTIMPL; }


STDMETHODIMP
CSdpSearch::AttributeSearch(
    ULONG handle,
    SdpAttributeRange *pRangeList,
    ULONG numRanges,
    ISdpRecord **ppSdpRecord
    )
{ return E_NOTIMPL; }

STDMETHODIMP
CSdpSearch::ServiceAndAttributeSearch(
    SdpQueryUuid *pUuidList,
    ULONG listSize,
    SdpAttributeRange *pRangeList,
    ULONG numRanges,
    ISdpRecord ***pppSdpRecords,
    ULONG *pNumRecords
    )
{ return E_NOTIMPL; }


HRESULT STDMETHODCALLTYPE 
CSdpSearch::QueryInterface(REFIID iid, void ** ppvObject)  {
    if (!ppvObject)
        return E_POINTER;

    if (iid == IID_IUnknown || iid == IID_ISdpSearch) {
        *ppvObject = (ISdpSearch*)this;
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE 
CSdpSearch::AddRef() {
    return InterlockedIncrement(&refCount);
}


ULONG STDMETHODCALLTYPE 
CSdpSearch::Release() {
    if (0 == InterlockedDecrement(&refCount)) {
        delete this;
        return 0;
    }
    return refCount;
}



