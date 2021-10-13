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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
///////////////////////////////////////////////////////////////////////////////
//
// inttable.cpp - Copyright 1994-2001, Don Box (http://www.donbox.com)
//
// This file contains a datatype, INTERFACE_ENTRY that can be used to build
// tables that map IIDs onto vptrs. 
//
// This file contains the prototype for a routine that implements
// QueryInterface based on an interface table.
//
//     InterfaceTableQueryInterface - finds and AddRef's vptr on an object
//     

#include <windows.h>
#include "inttable.h"

#ifdef UNDER_CE

#include <guiddef.h>

#else

#ifndef _INLINEISEQUALGUID_DEFINED
#define _INLINEISEQUALGUID_DEFINED
inline BOOL  InlineIsEqualGUID(REFGUID rguid1, REFGUID rguid2)
{
   return (
      ((PLONG) &rguid1)[0] == ((PLONG) &rguid2)[0] &&
      ((PLONG) &rguid1)[1] == ((PLONG) &rguid2)[1] &&
      ((PLONG) &rguid1)[2] == ((PLONG) &rguid2)[2] &&
      ((PLONG) &rguid1)[3] == ((PLONG) &rguid2)[3]);
}
#endif

#endif // UNDER_CE

// the routine that implements QueryInterface basd on the table
EXTERN_C HRESULT STDAPICALLTYPE InterfaceTableQueryInterface(void *pThis, const INTERFACE_ENTRY *pTable, REFIID riid, void **ppv)
{
    if (InlineIsEqualGUID(riid, IID_IUnknown))
    {
        ((IUnknown*)(*ppv = (char*)pThis + pTable->dwData))->AddRef();
        return S_OK;
    }
    else
    {
        HRESULT hr = E_NOINTERFACE;
        while (pTable->pfnFinder)
        {
            if (!pTable->pIID || InlineIsEqualGUID(riid, *pTable->pIID))
            {
                if (pTable->pfnFinder == ENTRY_IS_OFFSET)
                {
                    ((IUnknown*)(*ppv = (char*)pThis + pTable->dwData))->AddRef();
                    hr = S_OK;
                    break;
                }
                else
                {
                    hr = pTable->pfnFinder(pThis, pTable->dwData, riid, ppv);
                    if (hr == S_OK)
                    {
                        break;
                    }
                }
            }
            pTable++;
        }
        if (hr != S_OK)
        {
            *ppv = 0;
        }
        return hr;
    }
}
