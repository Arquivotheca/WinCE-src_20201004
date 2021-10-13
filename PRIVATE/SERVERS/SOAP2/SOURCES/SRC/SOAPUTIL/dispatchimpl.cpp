//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      dispatchimpl.cpp
//
// Contents:
//
//      DispatchImplementation cpp file
//
//----------------------------------------------------------------------------------

#include "Headers.h"
#include "DispatchImpl.h"

//
// CTypeInfo
//

CTypeInfo * CTypeInfo::sm_pRoot = NULL;


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CTypeInfo::CTypeInfo()
//
//  parameters:
//
//  description:
//        Construct a CTypeInfo object
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CTypeInfo::CTypeInfo()
{
    m_pInfo = NULL;

    // chain CTypeInfo objects together
    m_pNext = sm_pRoot;
    sm_pRoot = this;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CTypeInfo::ReleaseAll()
//
//  parameters:
//
//  description:
//        Release all typeinfo's from the chain.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTypeInfo::ReleaseAll()
{
    CTypeInfo *pcti = sm_pRoot;

    // release all type info's in the chain
    while( pcti )
    {
        if( pcti->m_pInfo )
        {
            pcti->m_pInfo->Release();
            pcti->m_pInfo = NULL;
        }
        pcti = pcti->m_pNext;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CTypeInfo::LoadTypeInfo( LCID lcid, const GUID *pguid )
//
//  parameters:
//
//  description:
//        Load the TypeInfo object corresponding to the guid & lcid
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CTypeInfo::LoadTypeInfo( LCID lcid, const GUID *pguid )
{
    HRESULT hRes = NOERROR;
    
    // Even though this function is parameterized the implementation is optimized
    // to only work for a constant guid; lcid is actually ignored.
    
    // Only load the typeinfo, if it has not already been loaded.     
    if( !m_pInfo )
    {
        ITypeInfo *pTypeInfo = NULL;
        ITypeLib *pTypeLib=NULL;
        // the typelib was loaded at DLL initialization
        // Load your typelib here
        hRes = LoadRegTypeLib(*sm_plibid, 1, 0, lcid, &pTypeLib);
        if (SUCCEEDED(hRes))
        {
            //look for our GUID
            hRes = pTypeLib->GetTypeInfoOfGuid(*pguid, &pTypeInfo);
            pTypeLib->Release();
        }
        
        if (SUCCEEDED(hRes))
        {
            if (InterlockedCompareExchange((LPLONG) &m_pInfo, (LONG)pTypeInfo, NULL))
            {
                // Already created by another thread
                pTypeInfo->Release();
            }
        }
    }

    return hRes;
}


#ifdef _DEBUG

//////////////////////////////////////////////////////////////////////////////
//
// DebugCheckInvokeMethodOrder
// 
// @mfunc    Validates the sorted order of the method table.
// 
// @side    None
//
// @rdesc    None
//
//////////////////////////////////////////////////////////////////////////////

DebugCheckInvokeMethodOrder::DebugCheckInvokeMethodOrder( 
    WCHAR *pwszTable, INVOKE_METHOD *rgNames, int cNames )
{
    if( cNames > 1 )
    {
        for( int iName = 1; iName < cNames; iName++ )
        {
            if( wcsicmp( rgNames[ iName - 1 ].pwszMethod, rgNames[ iName ].pwszMethod ) >= 0 )
            {
                OutputDebugStringW(L"Bad Invoke Method Table Order: " );
                if( pwszTable )
                    OutputDebugStringW( pwszTable );
                OutputDebugStringW( L"\n" );
                break;
            }
        }
    }
}
#endif


//////////////////////////////////////////////////////////////////////////////
//
// FindIdsOfNames
// 
// @mfunc    Functions similar to IDispatch::GetIdsOfNames, however it works 
//            off of a sorted string table instead of a type library.
// 
// @side    None
//
// @rdesc    None
//
//////////////////////////////////////////////////////////////////////////////

HRESULT FindIdsOfNames( OLECHAR **rgNames, UINT cNames, INVOKE_METHOD *rgKnownNames, int cKnownNames, LCID , DISPID *rgdispid )
{
    ::SetErrorInfo(0,NULL);

    if( !rgNames || !cNames || !rgKnownNames || !cKnownNames || !rgdispid )
        return E_INVALIDARG;

    // cannot handle the case were parameter also need translating
    if( cNames != 1 )
        return DISP_E_UNKNOWNNAME;

    int iBot = 0;
    int iTop = cKnownNames - 1;

    #pragma warning (push)
    #pragma warning (disable : 4127)

    while( true )
    {
        int iMid = (iTop + iBot) / 2;
        int iCmp = wcsicmp( *rgNames, rgKnownNames[iMid].pwszMethod );
    #pragma warning (pop)

        if( iCmp < 0 )
        {
            iTop = iMid - 1;
        }
        else if( iCmp > 0 )
        {
            iBot = iMid + 1;
        }
        else
        {
            *rgdispid = rgKnownNames[iMid].dispid;
            break;
        }

        if( iBot > iTop )
        {
            return DISP_E_UNKNOWNNAME;
        }
    }

    return NOERROR;
}




//////////////////////////////////////////////////////////////////////////////
//
// PrepareInvokeArgs
// 
// @mfunc    Prepares the arguements passed in via a dispparams structure to be
//            passed directly to the intended method call.  Variants are coerced
//            to the appropriate types if possible.
// 
// @side    None
//
// @rdesc    None
//
//////////////////////////////////////////////////////////////////////////////

HRESULT PrepareInvokeArgs( 
    DISPPARAMS *pdispparams, INVOKE_ARG *rgArgs, const VARTYPE *rgTypes, UINT cArgs )
{
    HRESULT hr = NOERROR;

    ASSERT( pdispparams );
    ASSERT( rgArgs );
    ASSERT( rgTypes );
    ASSERT( cArgs > 0 );

    memset( &rgArgs[0], 0, sizeof(*rgArgs)*cArgs );

    if( pdispparams->cArgs > cArgs )
        return DISP_E_BADPARAMCOUNT;

    for( UINT iArg = 0; iArg < cArgs; iArg++ )
    {
        INVOKE_ARG *pArg = &rgArgs[iArg];
        VARTYPE vt = rgTypes[iArg] & (~VT_OPTIONAL);
        bool fOptional = (rgTypes[iArg] & VT_OPTIONAL) != 0;

        if( iArg >= pdispparams->cArgs || FAILED(hr) )
        {
            // We only use NULL/empty string values as default. We should actually look   
            // up the TypeLibrary and get the real default value here.
            if (fOptional)
            {
                pArg->vArg.vt = vt;
                if (vt == VT_BSTR)
                {
                    pArg->vArg.bstrVal = ::SysAllocString(L"");
                    pArg->fClear = true;
                }
                else    
                    pArg->vArg.lVal = 0;
            }
            else
            {
                pArg->vArg.vt = VT_ERROR;
                pArg->vArg.scode = DISP_E_PARAMNOTFOUND;
                pArg->fMissing = true;
            }    
        }
        else
        {
            // parameters are recorded in right to left order
            VARIANT *pParam = &pdispparams->rgvarg[pdispparams->cArgs - iArg - 1];

            if( vt & VT_BYREF ) // special case for output args
            {
                // dereference indirect variants
                while( pParam->vt == (VT_BYREF|VT_VARIANT) && pParam->pvarVal )
                {
                    pParam = pParam->pvarVal;
                }

                if( vt == (VT_VARIANT|VT_BYREF) )
                {
                    if( !(pParam->vt & VT_BYREF) )
                        VariantClear(pParam);

                    pArg->vArg.vt = VT_VARIANT|VT_BYREF;
                    pArg->vArg.pvarVal = pParam;
                }
                else if( VARTYPEMAP(vt) == VARTYPEMAP(pParam->vt) )
                {
                    pArg->vArg = *pParam;
                }
                else
                {
                    hr = E_INVALIDARG;
                }
            }
            else
            {
                // dereference indirect variants
                while( pParam->vt == (VT_BYREF|VT_VARIANT) && pParam->pvarVal )
                {
                    pParam = pParam->pvarVal;
                }

                // if this is the type we are expecting, or missing use it w/o coercing
                if( VARTYPEMAP( pParam->vt ) == VARTYPEMAP(vt) || VT_VARIANT == vt ||
                     (pParam->vt == VT_ERROR && pParam->scode == DISP_E_PARAMNOTFOUND) )
                {
                    pArg->vArg = *pParam;
                }
                else // otherwise, coerce this into the correct type if possible
                {
                    pArg->fClear = true;
                    hr = VariantChangeType( &pArg->vArg, pParam, 0, vt );
                }
            }

            if( SUCCEEDED(hr) && pArg->vArg.vt == VT_ERROR )
            {
                pArg->fMissing = true;
            }
        }

        if( pArg->fMissing && !fOptional )
        {
            hr = DISP_E_BADPARAMCOUNT;
        }
    }

    return hr;
}


//////////////////////////////////////////////////////////////////////////////
//
// PrepareInvokeArgsAndResult
// 
// @mfunc    Calls PrepareInvokeArgs if cArgs > 0, also may use part of rgArgs
//            if pvarResult is NULL.  In this case pvarResult is redirected to
//            point at one of variants in rgArgs, and its clear flag is set so
//            ClearInvokeArgs will free it.
// 
// @side    None
//
// @rdesc    None
//
//////////////////////////////////////////////////////////////////////////////

HRESULT PrepareInvokeArgsAndResult( 
    DISPPARAMS *pdispparams, INVOKE_ARG *rgArgs, VARTYPE *rgTypes, UINT &cArgs,
    VARIANT *&pvarResult, VARTYPE resType )
{
    HRESULT hr = NOERROR;

    if( cArgs > 0 )
    {
        hr = PrepareInvokeArgs( pdispparams, rgArgs, rgTypes, cArgs );
#ifndef UNDER_CE
		JMP_ERR( hr, errExit );
#else
        JMP_FAIL(hr, errExit);
#endif

    }
    else if( pdispparams->cArgs != 0 )
    {
        hr = DISP_E_BADPARAMCOUNT;
        goto errExit;
    }

    // if no result is supplied, point to an invoke_arg
    if( !pvarResult )
    {
        pvarResult = &rgArgs[cArgs].vArg;
        rgArgs[cArgs].fClear = true;
        cArgs++;
    }

    memset( pvarResult, 0, sizeof(*pvarResult) );
    pvarResult->vt = resType;

errExit:
    return hr;
}



//////////////////////////////////////////////////////////////////////////////
//
// ClearInvokeArgs
// 
// @mfunc    Releases any memory allocated by coersion during the 
//            PrepareInvokeArgs call.
// 
// @side    None
//
// @rdesc    None
//
//////////////////////////////////////////////////////////////////////////////

void ClearInvokeArgs( INVOKE_ARG *rgArgs, UINT cArgs )
{
    for( UINT iArg = 0; iArg < cArgs; iArg++ )
    {
        if( rgArgs[iArg].fClear )
            VariantClear( &rgArgs[iArg].vArg );
    }
}



