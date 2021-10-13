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
#include "common.h"
#include "PropertyBag.h"

#include "ObexStrings.h"

extern CRITICAL_SECTION g_PropBagCS;

CPropertyBag::CPropertyBag() : _refCount(1), pVar(0)
{
        DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBag::CPropertyBag() -- 0x%x\n",(int)this));
}

CPropertyBag::~CPropertyBag()
{
        DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBag::~CPropertyBag() -- 0x%x\n",(int)this));

    VAR_LIST *pTemp = pVar;

    while(pTemp)
    {
        VAR_LIST *pDel = pTemp;
        VariantClear(&pTemp->var);
        SysFreeString(pTemp->pBSTRName);
        pTemp = pTemp->pNext;
        delete pDel;
    }

}


HRESULT STDMETHODCALLTYPE
CPropertyBag::Read(LPCOLESTR pszPropName,
                       VARIANT *_pVar,
                       IErrorLog *pErrorLog)
{
    CCritSection csLock(&g_PropBagCS);
    csLock.Lock();
    DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBag::Read()\n"));
    VAR_LIST *pTemp = pVar;

    while(pTemp)
    {
        if(0 == wcscmp(pszPropName, pTemp->pBSTRName))
            return VariantCopy(_pVar, &pTemp->var);

        pTemp = pTemp->pNext;
    }

    return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WINDOWS, ERROR_NOT_FOUND);
}



HRESULT STDMETHODCALLTYPE
CPropertyBag::Write(LPCOLESTR pszPropName,
                            VARIANT *_pVar)
{
    CCritSection csLock(&g_PropBagCS);
    csLock.Lock();

        DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBag::Write()\n"));

    VAR_LIST *pTemp = pVar;

    while(pTemp)
    {
        if(0 == wcscmp(pszPropName, pTemp->pBSTRName))
        {
                        VariantClear(&pTemp->var);
            return VariantCopy(&pTemp->var, _pVar);
        }
        pTemp = pTemp->pNext;
    }

    pTemp = new VAR_LIST();

        if (! pTemp)
                return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WINDOWS, ERROR_OUTOFMEMORY);

    VariantInit(&pTemp->var);

    pTemp->pBSTRName = SysAllocString(pszPropName);
    pTemp->pNext = pVar;
    pVar = pTemp;

    return VariantCopy(&pTemp->var, _pVar);
}

ULONG STDMETHODCALLTYPE
CPropertyBag::AddRef()
{
        ULONG ret = InterlockedIncrement((LONG *)&_refCount);
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CPropertyBag::AddRef(%d) -- 0x%x\n",ret, (int)this));
    return ret;
}

ULONG STDMETHODCALLTYPE
CPropertyBag::Release()
{
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CPropertyBag::Release(%d) -- 0x%x\n",ret, (int)this));
    if(!ret)
        delete this;
    return ret;
}

HRESULT STDMETHODCALLTYPE
CPropertyBag::QueryInterface(REFIID riid, void** ppv)
{
    DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBag::QueryInterface()\n"));
    if(!ppv)
        return E_POINTER;
    if(riid == IID_IPropertyBag)
        *ppv = static_cast<IPropertyBag*>(this);
    else
        return *ppv = 0, E_NOINTERFACE;

    return AddRef(), S_OK;
}

