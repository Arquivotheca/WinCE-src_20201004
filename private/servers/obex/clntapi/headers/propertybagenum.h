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
#ifndef OBEX_PROPERTYBAGENUM_H 
#define OBEX_PROPERTYBAGENUM_H

struct PROP_HOLDER 
{
    LPPROPERTYBAG2 pItem;
    PROP_HOLDER *pNext;
};

class CPropertyBagEnum : public IPropertyBagEnum 
{
public:
    CPropertyBagEnum();
    ~CPropertyBagEnum();

	HRESULT STDMETHODCALLTYPE Next (ULONG celt,
		LPPROPERTYBAG2 * rgelt,
		ULONG* pceltFetched);

	HRESULT STDMETHODCALLTYPE Skip (ULONG celt);

	HRESULT STDMETHODCALLTYPE Reset();

	HRESULT STDMETHODCALLTYPE Clone (IPropertyBagEnum** ppenum);
        
    HRESULT Insert(LPPROPERTYBAG2 pPropBag);

    ULONG STDMETHODCALLTYPE AddRef();        
    ULONG STDMETHODCALLTYPE Release();        
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);   

private:
     ULONG _refCount;  
     PROP_HOLDER *pPropList;
     PROP_HOLDER *pPropCurrent;    
};

#endif