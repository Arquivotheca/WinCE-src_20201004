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

#ifndef __UPNP_PROXY__
#define __UPNP_PROXY__

#include <upnp.h>
#include <atlcomcli.h>
#include <string.hxx>
#include <safe_array.hxx>
#include <variant.h>

#ifndef ZONE_UPNP_PROXY_ERROR
#   define ZONE_UPNP_PROXY_ERROR DEBUGZONE(0)
#endif

#ifndef ZONE_UPNP_PROXY_WARN
#   define ZONE_UPNP_PROXY_WARN DEBUGZONE(1)
#endif

#ifndef ZONE_UPNP_PROXY_TRACE
#   define ZONE_UPNP_PROXY_TRACE DEBUGZONE(2)
#endif

#ifndef UPNP_PROXY_TEXT
#   define UPNP_PROXY_TEXT(string) TEXT("UPNP PROXY: ") TEXT(string)
#endif

namespace ce
{

class upnp_proxy_context
{
public:
    upnp_proxy_context()
    {
    }
    
    ~upnp_proxy_context()
    {
    }

public:    
    ce::variant                             m_varOut;
    ce::safe_array<ce::variant, VT_VARIANT> m_arrayOut;
#ifdef DEBUG    
    ce::wstring                             m_strAction;
#endif    
};

class upnp_proxy
{
public:
    upnp_proxy(IUPnPService *pService = NULL)
        : m_pService(pService)
        {}
    
    void init(IUPnPService *pService)
    {
        assert(!m_pService);
        m_pService = pService;
    }
    
    // invoke
    HRESULT invoke(/* [in] */ LPCWSTR pszAction,
                   /* [in] */ DISPPARAMS* pdispparams, 
                   /* [in, out] */ VARIANT* pRetval)
    {
        HRESULT     hr;
        
#ifndef NO_INVOKE_FOR_UPNP_ACTIONS
        //
        // UPnP control point APIs on CE support invoking UPnP actions using IDispatch::Invoke
        //
        EXCEPINFO   ExcepInfo;
        UINT        uArgErr;
        DISPID      rgDispId[1];

        if(SUCCEEDED(hr = m_pService->GetIDsOfNames(IID_NULL, const_cast<LPOLESTR*>(&pszAction), 1, 0, rgDispId)))
        {
            hr =  m_pService->Invoke(rgDispId[0], IID_NULL, 0, DISPATCH_METHOD, pdispparams, pRetval, &ExcepInfo, &uArgErr);
        
            if(hr == DISP_E_EXCEPTION)
            {
                assert(FAILED(ExcepInfo.scode));
                
                hr = ExcepInfo.scode;
            }
        }
            
        return hr;
#else
        //
        // UPnP control point APIs on XP don't support invoking UPnP actions using IDispatch::Invoke
        // To use IUPnPService::InvokeAction we must copy arguments between safe arrays and DISPPARAMS structure
        //
        
        if(!pdispparams)
        {
            DEBUGMSG(ZONE_UPNP_PROXY_ERROR, (UPNP_PROXY_TEXT("pdispparams argument can't be NULL")));
            return E_POINTER;
        }
        
        unsigned i;
        long     j;
        
        // find first [in] argument
        for(i = 0; i < pdispparams->cArgs && (pdispparams->rgvarg[i].vt & VT_BYREF); ++i)
            ;
        
        ce::safe_array<ce::variant, VT_VARIANT> arrayIn(pdispparams->cArgs - i, 0);
        
        // copy [in] arguments to safe array; start from upper bound because DISPPARAMS arguments are in reverse order 
        for(j = arrayIn.ubound(); i < pdispparams->cArgs; ++i, --j)
        {
            assert(j >= arrayIn.lbound());
            
            arrayIn[j] = pdispparams->rgvarg[i];
        }
    
        // allocate BSTR action name
        ce::auto_bstr bstrAction;

        bstrAction = SysAllocString(pszAction);
        
        if(!bstrAction)
        {
            DEBUGMSG(ZONE_UPNP_PROXY_ERROR, (UPNP_PROXY_TEXT("OOM allocating BSTR action name \"%s\""), pszAction));
            return E_OUTOFMEMORY;
        }
        
        ce::variant vInArgs, vOutArgs;
        
        vInArgs.vt = VT_ARRAY | VT_VARIANT;
        vInArgs.parray = arrayIn.detach();

        // InvokeAction
        hr = m_pService->InvokeAction(bstrAction, vInArgs, &vOutArgs, pRetval);
        
        if(FAILED(hr))
        {
            DEBUGMSG(ZONE_UPNP_PROXY_WARN, (UPNP_PROXY_TEXT("Action \"%s\" failed 0x%08x"), pszAction, hr));
            return hr;
        }
        
        // transfer [out] arguemnts, if any, from safe array to DISPPARAMS
        if(vOutArgs.vt == (VT_ARRAY | VT_VARIANT))
        {
            ce::safe_array<ce::variant, VT_VARIANT> arrayOut;
            
            arrayOut.attach(vOutArgs.parray);
            
            arrayOut.lock();
            
            // copy [out] arguments starting from upper bound because arguments in DISPPARAMS are in reverse order
            for(i = 0, j = arrayOut.ubound(); j >= arrayOut.lbound() && i < pdispparams->cArgs; ++i, --j)
            {
                VARIANT* pvar = &arrayOut[j];
                
                assert(pvar);
                
                if((VT_BYREF | pvar->vt) != (pdispparams->rgvarg[i].vt))
                {
                    DEBUGMSG(ZONE_UPNP_PROXY_ERROR, (UPNP_PROXY_TEXT("Action \"%s\" returned [out] argument of type %d\n\tbut pdispparams->rgvarg[%d].vt = %d"), pszAction, pvar->vt, i, pdispparams->rgvarg[i].vt));
                    return E_INVALIDARG;
                }
                    
                switch(pvar->vt)
                {
                    case VT_I1:
                    case VT_UI1:
                        *V_UI1REF(pdispparams->rgvarg + i) = V_UI1(pvar);
                          break;

                    case VT_I2:
                    case VT_UI2:
                    case VT_BOOL:
                          *V_I2REF(pdispparams->rgvarg + i) = V_I2(pvar);
                          break;

                    case VT_I4:
                    case VT_UI4:
                    case VT_INT:
                    case VT_UINT:
                    case VT_ERROR:
                          *V_I4REF(pdispparams->rgvarg + i) = V_I4(pvar);
                          break;

                    case VT_R4:
                          *V_R4REF(pdispparams->rgvarg + i) = V_R4(pvar);
                          break;

                    case VT_R8:
                    case VT_DATE:
                          *V_R8REF(pdispparams->rgvarg + i) = V_R8(pvar);
                          break;

                    case VT_CY:
                          *V_CYREF(pdispparams->rgvarg + i) = V_CY(pvar);
                          break;

                    case VT_BSTR:
                        *V_BSTRREF(pdispparams->rgvarg + i) = SysAllocStringByteLen((char FAR *)V_BSTR(pvar), SysStringByteLen(V_BSTR(pvar)));
                          break;

                    default:
                        assert(false);
                }
            }
        }
        
        return S_OK;

#endif // NO_INVOKE_FOR_UPNP_ACTIONS
    }
    
    
    //////////////////////////////////////////////
    // make UPnP call
    //////////////////////////////////////////////
    
    // 0 [in] argument
    HRESULT call(upnp_proxy_context &context, LPCWSTR pszAction, VARIANT* pRetval = NULL)
    {
        ce::safe_array<ce::variant, VT_VARIANT> arrayIn(0, 0);
        
        return invoke_action(context, pszAction, pRetval, *&arrayIn);
    }
    
    // 1 [in] argument
    template<typename T1>
    HRESULT call(upnp_proxy_context &context, LPCWSTR pszAction, T1 x1, VARIANT* pRetval = NULL)
    {
        ce::safe_array<ce::variant, VT_VARIANT> arrayIn(1);
        
        arrayIn[0] = x1;
            
        return invoke_action(context, pszAction, pRetval, *&arrayIn);
    }
    
    // 2 [in] arguments
    template<typename T1, typename T2>
    HRESULT call(upnp_proxy_context &context, LPCWSTR pszAction, T1 x1, T2 x2, VARIANT* pRetval = NULL)
    {
        ce::safe_array<ce::variant, VT_VARIANT> arrayIn(2);
        
        arrayIn[0] = x1;
        arrayIn[1] = x2;
            
        return invoke_action(context, pszAction, pRetval, *&arrayIn);
    }
    
    // 3 [in] arguments
    template<typename T1, typename T2, typename T3>
    HRESULT call(upnp_proxy_context &context, LPCWSTR pszAction, T1 x1, T2 x2, T3 x3, VARIANT* pRetval = NULL)
    {
        ce::safe_array<ce::variant, VT_VARIANT> arrayIn(3);
        
        arrayIn[0] = x1;
        arrayIn[1] = x2;
        arrayIn[2] = x3;
            
        return invoke_action(context, pszAction, pRetval, *&arrayIn);
    }
    
    // 4 [in] arguments
    template<typename T1, typename T2, typename T3, typename T4>
    HRESULT call(upnp_proxy_context &context, LPCWSTR pszAction, T1 x1, T2 x2, T3 x3, T4 x4, VARIANT* pRetval = NULL)
    {
        ce::safe_array<ce::variant, VT_VARIANT> arrayIn(4);
        
        arrayIn[0] = x1;
        arrayIn[1] = x2;
        arrayIn[2] = x3;
        arrayIn[3] = x4;
            
        return invoke_action(context, pszAction, pRetval, *&arrayIn);
    }
    
    // 5 [in] arguments
    template<typename T1, typename T2, typename T3, typename T4, typename T5>
    HRESULT call(upnp_proxy_context &context, LPCWSTR pszAction, T1 x1, T2 x2, T3 x3, T4 x4, T5 x5, VARIANT* pRetval = NULL)
    {
        ce::safe_array<ce::variant, VT_VARIANT> arrayIn(5);
        
        arrayIn[0] = x1;
        arrayIn[1] = x2;
        arrayIn[2] = x3;
        arrayIn[3] = x4;
        arrayIn[4] = x5;
            
        return invoke_action(context, pszAction, pRetval, *&arrayIn);
    }
    
    // 6 [in] arguments
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
    HRESULT call(upnp_proxy_context &context, LPCWSTR pszAction, T1 x1, T2 x2, T3 x3, T4 x4, T5 x5, T6 x6, VARIANT* pRetval = NULL)
    {
        ce::safe_array<ce::variant, VT_VARIANT> arrayIn(6);
        
        arrayIn[0] = x1;
        arrayIn[1] = x2;
        arrayIn[2] = x3;
        arrayIn[3] = x4;
        arrayIn[4] = x5;
        arrayIn[5] = x6;
            
        return invoke_action(context, pszAction, pRetval, *&arrayIn);
    }
    
    // 7 [in] arguments
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
    HRESULT call(upnp_proxy_context &context, LPCWSTR pszAction, T1 x1, T2 x2, T3 x3, T4 x4, T5 x5, T6 x6, T7 x7, VARIANT* pRetval = NULL)
    {
        ce::safe_array<ce::variant, VT_VARIANT> arrayIn(7);
        
        arrayIn[0] = x1;
        arrayIn[1] = x2;
        arrayIn[2] = x3;
        arrayIn[3] = x4;
        arrayIn[4] = x5;
        arrayIn[5] = x6;
        arrayIn[6] = x7;
            
        return invoke_action(context, pszAction, pRetval, *&arrayIn);
    }
    
    // 8 [in] arguments
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
    HRESULT call(upnp_proxy_context &context, LPCWSTR pszAction, T1 x1, T2 x2, T3 x3, T4 x4, T5 x5, T6 x6, T7 x7, T8 x8, VARIANT* pRetval = NULL)
    {
        ce::safe_array<ce::variant, VT_VARIANT> arrayIn(8);
        
        arrayIn[0] = x1;
        arrayIn[1] = x2;
        arrayIn[2] = x3;
        arrayIn[3] = x4;
        arrayIn[4] = x5;
        arrayIn[5] = x6;
        arrayIn[6] = x7;
        arrayIn[7] = x8;
            
        return invoke_action(context, pszAction, pRetval, *&arrayIn);
    }
    
    // 9 [in] arguments
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
    HRESULT call(upnp_proxy_context &context, LPCWSTR pszAction, T1 x1, T2 x2, T3 x3, T4 x4, T5 x5, T6 x6, T7 x7, T8 x8, T9 x9, VARIANT* pRetval = NULL)
    {
        ce::safe_array<ce::variant, VT_VARIANT> arrayIn(9);
        
        arrayIn[0] = x1;
        arrayIn[1] = x2;
        arrayIn[2] = x3;
        arrayIn[3] = x4;
        arrayIn[4] = x5;
        arrayIn[5] = x6;
        arrayIn[6] = x7;
        arrayIn[7] = x8;
        arrayIn[8] = x9;
            
        return invoke_action(context, pszAction, pRetval, *&arrayIn);
    }
    
    
    //////////////////////////////////////////////
    // get [out] parameters returned by the call
    //////////////////////////////////////////////
    
    // 1 [out] argument
    template<typename T1>
    bool get_results(upnp_proxy_context &context, T1 x1)
    {
        if(!validate(context, 1))
            return false;
            
        return extract_arg(context, 1, x1);
    }
    
    // 2 [out] arguments
    template<typename T1, typename T2>
    bool get_results(upnp_proxy_context &context, T1 x1, T2 x2)
    {
        if(!validate(context, 2))
            return false;
            
        return extract_arg(context, 1, x1) &&
               extract_arg(context, 2, x2);
    }
    
    // 3 [out] arguments
    template<typename T1, typename T2, typename T3>
    bool get_results(upnp_proxy_context &context, T1 x1, T2 x2, T3 x3)
    {
        if(!validate(context, 3))
            return false;
            
        return extract_arg(context, 1, x1) &&
               extract_arg(context, 2, x2) &&
               extract_arg(context, 3, x3);
    }
    
    // 4 [out] arguments
    template<typename T1, typename T2, typename T3, typename T4>
    bool get_results(upnp_proxy_context &context, T1 x1, T2 x2, T3 x3, T4 x4)
    {
        if(!validate(context, 4))
            return false;
            
        return extract_arg(context, 1, x1) &&
               extract_arg(context, 2, x2) &&
               extract_arg(context, 3, x3) &&
               extract_arg(context, 4, x4);
    }
    
    // 5 [out] arguments
    template<typename T1, typename T2, typename T3, typename T4, typename T5>
    bool get_results(upnp_proxy_context &context, T1 x1, T2 x2, T3 x3, T4 x4, T5 x5)
    {
        if(!validate(context, 5))
            return false;
            
        return extract_arg(context, 1, x1) &&
               extract_arg(context, 2, x2) &&
               extract_arg(context, 3, x3) &&
               extract_arg(context, 4, x4) &&
               extract_arg(context, 5, x5);
    }
    
    // 6 [out] arguments
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
    bool get_results(upnp_proxy_context &context, T1 x1, T2 x2, T3 x3, T4 x4, T5 x5, T6 x6)
    {
        if(!validate(context, 6))
            return false;
            
        return extract_arg(context, 1, x1) &&
               extract_arg(context, 2, x2) &&
               extract_arg(context, 3, x3) &&
               extract_arg(context, 4, x4) &&
               extract_arg(context, 5, x5) &&
               extract_arg(context, 6, x6);
    }
    
    // 7 [out] arguments
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
    bool get_results(upnp_proxy_context &context, T1 x1, T2 x2, T3 x3, T4 x4, T5 x5, T6 x6, T7 x7)
    {
        if(!validate(context, 7))
            return false;
            
        return extract_arg(context, 1, x1) &&
               extract_arg(context, 2, x2) &&
               extract_arg(context, 3, x3) &&
               extract_arg(context, 4, x4) &&
               extract_arg(context, 5, x5) &&
               extract_arg(context, 6, x6) &&
               extract_arg(context, 7, x7);
    }
    
    // 8 [out] arguments
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
    bool get_results(upnp_proxy_context &context, T1 x1, T2 x2, T3 x3, T4 x4, T5 x5, T6 x6, T7 x7, T8 x8)
    {
        if(!validate(context, 8))
            return false;
            
        return extract_arg(context, 1, x1) &&
               extract_arg(context, 2, x2) &&
               extract_arg(context, 3, x3) &&
               extract_arg(context, 4, x4) &&
               extract_arg(context, 5, x5) &&
               extract_arg(context, 6, x6) &&
               extract_arg(context, 7, x7) &&
               extract_arg(context, 8, x8);
    }
    
    // 9 [out] arguments
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
    bool get_results(upnp_proxy_context &context, T1 x1, T2 x2, T3 x3, T4 x4, T5 x5, T6 x6, T7 x7, T8 x8, T9 x9)
    {
        if(!validate(context, 9))
            return false;
            
        return extract_arg(context, 1, x1) &&
               extract_arg(context, 2, x2) &&
               extract_arg(context, 3, x3) &&
               extract_arg(context, 4, x4) &&
               extract_arg(context, 5, x5) &&
               extract_arg(context, 6, x6) &&
               extract_arg(context, 7, x7) &&
               extract_arg(context, 8, x8) &&
               extract_arg(context, 9, x9);
    }

protected:
    //
    // invoke UPnP action
    //
    HRESULT invoke_action(upnp_proxy_context &context, LPCWSTR pszAction, VARIANT* pRetval, SAFEARRAY* pInArgs)
    {
        // this unlocks the safe array so that it can be destroyed by VariantClear (see below)
        context.m_arrayOut.destroy();
        
        HRESULT hr;
        
        hr = context.m_varOut.Clear();
        
        // if this failed we would leak memory!
        assert(SUCCEEDED(hr));
        
        if(!m_pService)
        {
            DEBUGMSG(ZONE_UPNP_PROXY_ERROR, (UPNP_PROXY_TEXT("Can't invoke action \"%s\", m_pService is NULL"), pszAction));
            return E_ABORT;
        }
        
        DEBUGMSG(ZONE_UPNP_PROXY_TRACE, (UPNP_PROXY_TEXT("Invoking action \"%s\""), pszAction));
        
        ce::auto_bstr   bstrAction;
        VARIANT         vInArgs; // don't clear vInArgs

#ifdef DEBUG
        context.m_strAction = pszAction;
#endif

        bstrAction = SysAllocString(pszAction);
        
        vInArgs.vt = VT_ARRAY | VT_VARIANT;
        vInArgs.parray = pInArgs;

        hr = m_pService->InvokeAction(bstrAction, vInArgs, &context.m_varOut, pRetval);
        
        if(FAILED(hr))
        {
            DEBUGMSG(ZONE_UPNP_PROXY_WARN, (UPNP_PROXY_TEXT("Action \"%s\" failed 0x%08x"), pszAction, hr));
        }
        
        return hr;
    }
    
    //
    // When caller passes NULL to get_results for unwanted [out] argument
    //
    bool extract_arg(upnp_proxy_context &context, int nArg, int pValue)
    {
        assert(pValue == NULL);
        return true;
    }
    
    
    //
    // get VT_BSTR [out] argument
    //
    template <unsigned _BUF_SIZE, class _Tr, class _Al>
    bool extract_arg(upnp_proxy_context &context, int nArg, ce::_string_t<wchar_t, _BUF_SIZE, _Tr, _Al>* pValue)
    {
        if(VARIANT* pvar = validate_arg(context, nArg, VT_BSTR))
        {        
            if(pValue)
                *pValue = pvar->bstrVal;
            return true;
        }
        
        return false;
    }
    
    //
    // get VT_I4 [out] argument
    //
    bool extract_arg(upnp_proxy_context &context, int nArg, long* pValue)
    {
        if(VARIANT* pvar = validate_arg(context, nArg, VT_I4))
        {        
            if(pValue)
                *pValue = pvar->lVal;
            return true;
        }
        
        return false;
    }
    
    //
    // get VT_UI4 [out] argument
    //
    bool extract_arg(upnp_proxy_context &context, int nArg, unsigned long* pValue)
    {
        if(VARIANT* pvar = validate_arg(context, nArg, VT_UI4))
        {        
            if(pValue)
                *pValue = pvar->ulVal;
            return true;
        }
        
        return false;
    }
    
    
    //
    // get VT_I2 [out] argument
    //
    bool extract_arg(upnp_proxy_context &context, int nArg, short* pValue)
    {
        if(VARIANT* pvar = validate_arg(context, nArg, VT_I2))
        {        
            if(pValue)
                *pValue = pvar->iVal;
            return true;
        }
        
        return false;
    }
    
    
    //
    // get VT_UI2 [out] argument
    //
    bool extract_arg(upnp_proxy_context &context, int nArg, unsigned short* pValue)
    {
        if(VARIANT* pvar = validate_arg(context, nArg, VT_UI2))
        {        
            if(pValue)
                *pValue = pvar->uiVal;
            return true;
        }
        
        return false;
    }
    
    
    //
    // get VT_BOOL [out] argument
    //
    bool extract_arg(upnp_proxy_context &context, int nArg, bool* pValue)
    {
        if(VARIANT* pvar = validate_arg(context, nArg, VT_BOOL))
        {        
            if(pValue)
                *pValue = (pvar->boolVal != VARIANT_FALSE);
            return true;
        }
        
        return false;
    }
    
    
    //
    // validate argument number nArg
    //
    VARIANT* validate_arg(upnp_proxy_context &context, int nArg, VARTYPE vt)
    {
        assert(context.m_arrayOut.size() >= nArg);
        
        VARIANT* pvar = &context.m_arrayOut[context.m_arrayOut.lbound() + nArg - 1];
        
        if(pvar->vt != vt)
        {
            DEBUGMSG(ZONE_UPNP_PROXY_ERROR, (UPNP_PROXY_TEXT("Action \"%s\": expected [out] argument %d to be of type %d but received type %d"), static_cast<LPCWSTR>(context.m_strAction), nArg, vt, pvar->vt));
            return NULL;
        }
        
        return pvar;
    }
    
    //
    // validate that call returned nArgs [out] arguments
    //
    bool validate(upnp_proxy_context &context, int nArgs)
    {
        // calling get_results after action failed or didn't return any out arguments
        assert(context.m_varOut.vt != VT_EMPTY);
        
        if(context.m_varOut.vt != (VT_ARRAY | VT_VARIANT))
        {
            DEBUGMSG(ZONE_UPNP_PROXY_ERROR, (UPNP_PROXY_TEXT("Action \"%s\": [out] arguments should be VT_ARRAY | VT_VARIANT"), static_cast<LPCWSTR>(context.m_strAction)));
            return false;
        }
        
        context.m_arrayOut.attach(context.m_varOut.parray);
        
        // lock the safe array so that validate_arg can return pointers to elements
        context.m_arrayOut.lock();
        
        if(context.m_arrayOut.size() != nArgs)
        {
            DEBUGMSG(ZONE_UPNP_PROXY_ERROR, (UPNP_PROXY_TEXT("Action \"%s\": expected %d [out] arguments but received %d"), static_cast<LPCWSTR>(context.m_strAction), nArgs, context.m_arrayOut.size()));
            return false;
        }
        
        return true;
    }

private:
    CComPtr<IUPnPService> m_pService;
};

}


#endif // __UPNP_PROXY__
