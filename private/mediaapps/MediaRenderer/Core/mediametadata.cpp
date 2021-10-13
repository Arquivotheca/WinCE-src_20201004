//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//


#include <stdafx.h>

static const long lMaxString = 0x8080;


//
//  Utilities
//
static HRESULT inline DupString(const BSTR S, BSTR *D)
{
    *D = SysAllocString(S);
    return (*D == NULL) ? E_OUTOFMEMORY : S_OK;
}

static void inline ReleaseString(BSTR &S)
{
    SysFreeString(S);
    S = NULL;
}

static bool CompStrings(BSTR S, BSTR T)
{
    if (SysStringByteLen(S) == SysStringByteLen(T))
    {
        return memcmp(S, T, SysStringByteLen(S)) ?  false : true;
    }
    return false;
}

static KeyVal* ResizeArray(KeyVal *oldArray, ULONG &cnt)
{
    if (cnt+10 < cnt)
    {
        // overflow/underflow on the addition
        return oldArray;
    }
    if ( (unsigned __int64)(cnt+10) * sizeof(KeyVal) > UINT_MAX )
    {
        // overflow/underflow on the multiplication
        return oldArray;
    }
    KeyVal *newArray = new KeyVal[cnt+10];
    if (newArray == NULL)
    {
        return oldArray;
    }
    memset(newArray, 0, sizeof(KeyVal) * (cnt+10));
    memcpy(newArray, oldArray, sizeof(KeyVal) * cnt);
    cnt += 10;
    delete [] oldArray;
    return newArray;
}








//
//  CMediaMetadata.  It implements IMediaMetadata
//

CMediaMetadata::CMediaMetadata(void)
    : m_dwRefCount(0)
    , m_nKeysAlloc(0)
    , m_nKeysInuse(0)
    , m_pKeys(NULL)
{
}

CMediaMetadata::~CMediaMetadata(void)
{
    for (ULONG i = 0; i < m_nKeysInuse; i++)
    {
        ReleaseString(m_pKeys[i].KeyName);
        ReleaseString(m_pKeys[i].Value);
    }
    delete [] m_pKeys;
    m_pKeys = NULL;
    m_nKeysInuse = 0;
    m_nKeysAlloc = 0;
}

ULONG CMediaMetadata::AddRef(void)
{
    return InterlockedIncrement((LONG*)&m_dwRefCount);
}

ULONG CMediaMetadata::Release(void)
{
    long x = InterlockedDecrement((LONG*)&m_dwRefCount);
    if (x == 0)
    {
        delete this;
    }
    return x;
}

HRESULT CMediaMetadata::QueryInterface(REFIID iidInterface, void **ppInterface)
{
    HRESULT hr = S_OK;
    
    ChkBool(ppInterface != NULL, E_POINTER);
    
    if (iidInterface == __uuidof(IMediaMetadata))
    {
        *ppInterface = (void *)static_cast<IMediaMetadata*>(this);
    }
    else if (iidInterface == __uuidof(IUnknown))
    {
        *ppInterface = (void *)static_cast<IUnknown*>(this);
    }
    else
    {
        Chk(E_NOINTERFACE);
    }
    
    AddRef();
    hr = S_OK;
    
Cleanup:    
    return hr;
}


HRESULT CMediaMetadata::Get(BSTR KeyIn, BSTR *pValOut)
{
    HRESULT hr = S_OK;
    ChkBool(KeyIn != NULL && pValOut != NULL, E_POINTER);

    // Find the item, copy out its length
    for (int i = 0; i < m_nKeysInuse; i++)
    {
        if (CompStrings(m_pKeys[i].KeyName, KeyIn))
        {
            hr = DupString(m_pKeys[i].Value, pValOut);
            goto Cleanup;
        }
    }
    hr = E_FAIL;

Cleanup:
    return hr;
}

HRESULT CMediaMetadata::Add(LPCWSTR pKeyIn, LPCWSTR pValIn)
{
    HRESULT hr = S_OK;
    BSTR KeyIn = NULL;
    BSTR ValIn = NULL;
    ULONG inx;

    ChkBool(pKeyIn != NULL && pValIn != NULL, E_POINTER);

    KeyIn = SysAllocString(pKeyIn);
    ChkBool(KeyIn != NULL, E_FAIL);
    ValIn = SysAllocString(pValIn);
    ChkBool(ValIn != NULL, E_FAIL);

    // get bigger table if necessary
    if (m_nKeysInuse == m_nKeysAlloc)
    {
        m_pKeys = ResizeArray(m_pKeys, m_nKeysAlloc);
        ChkBool(m_nKeysInuse != m_nKeysAlloc, E_OUTOFMEMORY);
    }

    // Search for item - Replace it if it already exists
    for (inx = 0; inx < m_nKeysInuse; inx++)
    {
        if (CompStrings(m_pKeys[inx].KeyName, KeyIn))
        {
            ReleaseString(m_pKeys[inx].KeyName);
            ReleaseString(m_pKeys[inx].Value);
            m_pKeys[inx].KeyName = KeyIn;
            m_pKeys[inx].Value = ValIn;
            break;
        }
    }

    // add new item if not just replaced
    if (inx == m_nKeysInuse)
    {
        m_pKeys[m_nKeysInuse].KeyName = KeyIn;
        m_pKeys[m_nKeysInuse].Value = ValIn;
        m_nKeysInuse++;
    }

Cleanup:
    if (FAILED(hr))
    {
        ReleaseString(KeyIn);
        ReleaseString(ValIn);
    }
    return hr;
}
