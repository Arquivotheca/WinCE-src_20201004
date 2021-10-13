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
#ifndef OBEX_PROPERTYBAG_H 
#define OBEX_PROPERTYBAG_H


struct VAR_LIST
{
    VARIANT var;
    VAR_LIST *pNext;
    BSTR pBSTRName;
};

class CPropertyBag : public IPropertyBag
{  
public:
    CPropertyBag();
    ~CPropertyBag();
    
    HRESULT STDMETHODCALLTYPE
    Read(
        LPCOLESTR pszPropName, 
        VARIANT *pVar, 
        IErrorLog *pErrorLog
        );
    
    
    HRESULT STDMETHODCALLTYPE
    Write(
        LPCOLESTR pszPropName, 
        VARIANT *pVar
        );
        
    ULONG STDMETHODCALLTYPE AddRef();        
    ULONG STDMETHODCALLTYPE Release();        
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);   

private:
     ULONG _refCount;
     VAR_LIST *pVar;
};

#endif
