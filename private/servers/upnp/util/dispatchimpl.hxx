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
#ifndef __DISPATCH_IMPL__
#define __DISPATCH_IMPL__

#include "com_macros.h"

namespace ce
{

extern HINSTANCE g_hInstance;

template <typename T, const IID* piid>
class DispatchImpl : public T
{
public:
    // DispatchImpl
    DispatchImpl()
        : m_pTypeInfo(NULL),
        m_fTILoadFailed(FALSE)
    {
    }

    // ~DispatchImpl
    ~DispatchImpl()
    {
        if(m_pTypeInfo)
        {
            m_pTypeInfo->Release();
            m_pTypeInfo = NULL;
        }
    }

// IDispatch methods
public:
    // GetTypeInfoCount
    STDMETHOD(GetTypeInfoCount)(/*[out]*/ UINT *pctinfo)
    {
        CHECK_POINTER(pctinfo);

        *pctinfo = 1;

        return S_OK;
    }

    // GetTypeInfo
    STDMETHOD(GetTypeInfo)( 
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo)
    {
        CHECK_POINTER(ppTInfo);

        LoadTI();

        if(!m_pTypeInfo)
        {
            return E_FAIL;
        }
            
        m_pTypeInfo->AddRef();
        *ppTInfo = m_pTypeInfo;
        
        return S_OK;
    }

    
    // GetIDsOfNames
    STDMETHOD(GetIDsOfNames)( 
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID __RPC_FAR *rgDispId)
    {
        LoadTI();

        if(!m_pTypeInfo)
        {
            return E_FAIL;
        }

        return m_pTypeInfo->GetIDsOfNames(rgszNames, cNames, rgDispId);
    }


    // Invoke
    STDMETHOD(Invoke) ( 
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
            /* [out] */ VARIANT __RPC_FAR *pVarResult,
            /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
            /* [out] */ UINT __RPC_FAR *puArgErr)
    {
        LoadTI();

        if(!m_pTypeInfo)
        {
            return E_FAIL;
        }

        return m_pTypeInfo->Invoke((T*)this, dispIdMember, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    }

private:
    ITypeInfo *m_pTypeInfo;
    BOOL m_fTILoadFailed;

    void LoadTI()
    {
        if ( m_pTypeInfo == NULL && !m_fTILoadFailed)
        {
            ITypeLib    *ptl = NULL;
            WCHAR        szDllName[MAX_PATH];
            
            GetModuleFileNameW(g_hInstance, szDllName, MAX_PATH);
            
            if(SUCCEEDED(LoadTypeLib(szDllName, &ptl)))
            {
                ptl->GetTypeInfoOfGuid(*piid, &m_pTypeInfo);
                ptl->Release();
            }
            else
            {
                m_fTILoadFailed = TRUE;
            }
        }
    }
};

}; // namespace ce
 
#endif // __DISPATCH_IMPL__
