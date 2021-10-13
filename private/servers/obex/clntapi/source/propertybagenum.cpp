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
#include "PropertyBagEnum.h"


/***********************************/
/* Property Bag ENUM stuff
/***********************************/
CPropertyBagEnum:: CPropertyBagEnum() : _refCount(1), pPropList(0), pPropCurrent(0)
{
    DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBagEnum::CPropertyBagEnum()\n"));
}

CPropertyBagEnum::~CPropertyBagEnum()
{
    DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBagEnum::~CPropertyBagEnum()\n"));

    PROP_HOLDER *pTemp = pPropList;  
    while(pTemp)
    {
         PROP_HOLDER *pDel = pTemp;
         pDel->pItem->Release();
         pTemp = pTemp->pNext;
         delete pDel;
    }

}

HRESULT STDMETHODCALLTYPE
CPropertyBagEnum::Next (ULONG celt,
                            LPPROPERTYBAG2 * rgelt,
                            ULONG* pceltFetched)
{
    DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBagEnum::Next()\n"));

    ULONG ulCopied = 0;

    while(celt && pPropCurrent)
    {
        rgelt[ulCopied] = pPropCurrent->pItem;
        rgelt[ulCopied]->AddRef();
        pPropCurrent = pPropCurrent->pNext;
        celt --;
        ulCopied ++;
    }
    *pceltFetched = ulCopied;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CPropertyBagEnum::Skip (ULONG celt)
{
    DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBagEnum::Skip()\n"));

	while(celt && pPropCurrent)
	{
		pPropCurrent = pPropCurrent->pNext;
	}
    if(celt) 
    	return S_FALSE;
    else
    	return S_OK;
}

HRESULT STDMETHODCALLTYPE 
CPropertyBagEnum::Reset()
{
    DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBagEnum::Reset()\n"));
    pPropCurrent = pPropList;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE
CPropertyBagEnum::Clone (IPropertyBagEnum** ppenum)
{
    DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBagEnum::Clone()\n"));
    return E_NOTIMPL;
}   

HRESULT 
CPropertyBagEnum::Insert(LPPROPERTYBAG2 pPropBag)
{
    if(!pPropList)
    {
        pPropList = new PROP_HOLDER();
        
        if(!pPropList)
            return E_OUTOFMEMORY;
            
        pPropList->pItem = pPropBag;
        pPropList->pNext = 0;
    }
    else
    {
        PROP_HOLDER *pHolderTemp = new PROP_HOLDER();
        
        if(!pHolderTemp)
            return E_OUTOFMEMORY;
            
        pHolderTemp->pNext = pPropList;
        pHolderTemp->pItem = pPropBag;
        pPropList = pHolderTemp;
    }
       
    //reset
    pPropBag->AddRef();
    pPropCurrent = pPropList;
    return S_OK;
}

ULONG STDMETHODCALLTYPE 
CPropertyBagEnum::AddRef() 
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CPropertyBagEnum::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE 
CPropertyBagEnum::Release() 
{
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);    
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CPropertyBagEnum::Release() = %d\n", _refCount));
    if(!ret) 
        delete this; 
    return ret;
}

HRESULT STDMETHODCALLTYPE 
CPropertyBagEnum::QueryInterface(REFIID riid, void** ppv) 
{
    DEBUGMSG(OBEX_PROPERTYBAG_ZONE,(L"CPropertyBagEnum::QueryInterface()\n"));
    if(!ppv) 
        return E_POINTER;
    if(riid == IID_IPropertyBag) 
        *ppv = static_cast<IPropertyBagEnum*>(this);	
    else 
        return *ppv = 0, E_NOINTERFACE;
    
    return AddRef(), S_OK;	
}




