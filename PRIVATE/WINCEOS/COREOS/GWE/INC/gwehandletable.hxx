//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef _GWEHANDLETABLE_HXX_
#define _GWEHANDLETABLE_HXX_

#include "gwe_handle.h"


template<class T, class TC> HRESULT CreateTypeHandle(HandleTable *pHTable, T *pGweHandle, TC *pObj)
{
    HRESULT hr = E_FAIL;
    if(pHTable != NULL)
    {
        if (pHTable->CreateHandle((GWEHandle*)pGweHandle, (void*)pObj) != S_OK)
        {
            DEBUGMSG(1, (TEXT("GWES - HTABLE - Creation of Type Handle [0x%08X] failed"), pObj));
        }
        else
        {
            hr = S_OK;
        }
    }
    else
    {
        return E_INVALIDARG;
    }
    if (hr == E_OUTOFMEMORY)
    {
        RETAILMSG (TRUE, (TEXT("ERROR -- Handler - Handle Creation - Out of Memory\r\n")));
    }
    return hr;
}

template<class T> HRESULT RemoveTypeHandle(HandleTable *pHTable, T *phandle)
{
    HRESULT hr = E_FAIL;
    if(phandle != NULL && pHTable != NULL)
    {
        hr = pHTable->RemoveHandle(*(reinterpret_cast<GWEHandle*>(phandle)));
    }
    return hr;
}

template<class T> void* LookupTypeHandle(HandleTable *pHTable, T handle)
{
    void *pRet = NULL;
    if (handle != NULL && pHTable != NULL)
    {
        pRet = pHTable->LookupHandle(*(reinterpret_cast<GWEHandle*>(&handle)));
    }
    return pRet;
}

#endif /* !__GWEHANDLETABLE_HXX__ */


