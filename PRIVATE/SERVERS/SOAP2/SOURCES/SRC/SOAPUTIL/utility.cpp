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
//      Utility.cpp
//
// Contents:
//
//      Implementation of various utilities.
//
//----------------------------------------------------------------------------------

#include "Headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT QueryInterfaceRider(const InterfaceRider *pRider, void *pvThis, REFIID iid, void **ppvObject)
//
//  parameters:
//          
//  description:
//          Performs table driven query interface
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT QueryInterfaceRider(const InterfaceRider *pRider, void *pvThis, REFIID iid, void **ppvObject)
{
    if(! ppvObject)
        return E_INVALIDARG;
    *ppvObject = 0;

    const InterfaceRiderElement *pElem = pRider->elv;
    for(int iElem = 0; iElem < pRider->elc; iElem ++, pElem ++)
    {
        if(*pElem->pIID == iid)
        {
            *ppvObject = reinterpret_cast<void*>(reinterpret_cast<BYTE*>(pvThis) + pElem->offs);
            ASSERT(*ppvObject);
            reinterpret_cast<IUnknown*>(*ppvObject)->AddRef();
            return S_OK;
        }
    }
    return E_NOINTERFACE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT FindFactoryProduct(REFCLSID rclsid, const FactoryRider *pFactoryRider, const FactoryProduct **ppFactProd)
//
//  parameters:
//          
//  description:
//          Finds matching factory product in FactoryRider table
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT FindFactoryProduct(REFCLSID rclsid, const FactoryRider *pFactoryRider, const FactoryProduct **ppFactProd)
{
    ASSERT(ppFactProd);

    const FactoryProduct *pElem = pFactoryRider->elv;
    for(int iElem = 0; iElem < pFactoryRider->elc; iElem ++, pElem ++)
    {
        if(*pElem->pCLSID == rclsid)
        {
            *ppFactProd = pElem;
            return S_OK;
        }
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT FreeAndCopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc)
//
//  parameters:
//
//  description:
//      Frees bstr at pbstrDst location and stores the copy of pszSrc there
//  returns: 
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT FreeAndCopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc)
{
    BSTR bstr   = 0;
    int  length = 0;

    if(pszSrc != 0 && (length = wcslen(pszSrc)) > 0)
    {
        bstr = ::SysAllocStringLen(pszSrc, length);

        if(bstr == 0)
        {
            return E_OUTOFMEMORY;
        }
    }

    ::SysFreeString(pbstrDst);

    pbstrDst = bstr;

    return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT FreeAndCopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc, int length)
//
//  parameters:
//          
//  description:
//          Frees bstr at pbstrDst location and stores the copy of pszSrc there (length)
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT FreeAndCopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc, int length)
{
    BSTR bstr = 0;

    if(pszSrc != 0 && length > 0)
    {
        bstr = ::SysAllocStringLen(pszSrc, length);

        if(bstr == 0)
        {
            return E_OUTOFMEMORY;
        }
    }

    ::SysFreeString(pbstrDst);

    pbstrDst = bstr;

    return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc, int length)
//
//  parameters:
//          
//  description:
//          Creates copy of BSTR w/ length
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc, int length)
{
    ASSERT(pbstrDst == 0);
    BSTR bstr = 0;

    if(pszSrc != 0 && length > 0)
    {
        bstr = ::SysAllocStringLen(pszSrc, length);

        if(bstr == 0)
        {
            return E_OUTOFMEMORY;
        }
    }

    pbstrDst = bstr;
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc)
//
//  parameters:
//          
//  description:
//          Creates copy of BSTR.
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc)
{
    ASSERT(pbstrDst == 0);

    BSTR bstr   = 0;
    int  length = 0;

    if(pszSrc != 0 && (length = wcslen(pszSrc)) > 0)
    {
        bstr = ::SysAllocStringLen(pszSrc, length);

        if(bstr == 0)
        {
            return E_OUTOFMEMORY;
        }
    }

    pbstrDst = bstr;
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CatBSTRs(BSTR &pbstrDst, const WCHAR *pStr1, int len1, const WCHAR *pStr2, int len2)
//
//  parameters:
//
//  description:
//        Concatenates two BSTRs, storing the result in pbstrDst.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CatBSTRs(BSTR &pbstrDst, const WCHAR *pStr1, int len1, const WCHAR *pStr2, int len2)
{
    ASSERT(pbstrDst == 0);

    BSTR bstr   = 0;

    if ((pStr1 != 0 && len1 > 0) || (pStr2 != 0 && len2 > 0))
    {
        bstr = ::SysAllocStringLen(0, len1 + len2);

        if (bstr == 0)
        {
            return E_OUTOFMEMORY;
        }

        if (pStr1 != 0 && len1 > 0)
        {
            wcsncpy(bstr, pStr1, len1);
        }
        else
        {
            len1 = 0;
        }

        if (pStr2 != 0 && len2 > 0)
        {
            wcsncpy(bstr + len1, pStr2, len2);
        }
        else
        {
            len2 = 0;
        }

        bstr[len1 + len2] = '\0';

    }

    pbstrDst = bstr;
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT FreeAndStoreBSTR(BSTR &bstrDst, const BSTR bstrSrc)
//
//  parameters:
//          
//  description:
//          Frees BSTR stored at bstrDst and assigns bstrSrc into that location
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT FreeAndStoreBSTR(BSTR &bstrDst, const BSTR bstrSrc)
{
    BSTR bstr = reinterpret_cast<BSTR>(InterlockedExchangePointer(&bstrDst, bstrSrc));
    ::SysFreeString(bstr);
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT AtomicFreeAndCopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc)
//
//  parameters:
//
//  description:
//
//  returns: 
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT AtomicFreeAndCopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc)
{
    BSTR bstr = 0;

    bstr = reinterpret_cast<BSTR>(InterlockedExchangePointer(&pbstrDst, 0));
    SysFreeString(bstr);

    bstr = SysAllocString(pszSrc);
    bstr = reinterpret_cast<BSTR>(InterlockedExchangePointer(&pbstrDst, bstr));
    SysFreeString(bstr);

    return S_OK;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT allocateAndCopy(WCHAR ** target, const WCHAR * source)
//
//  parameters:
//
//  description:
//        generic helper function to take a source and save a copy of it
//        in target, with allocations if necessary
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT allocateAndCopy(WCHAR ** target, const WCHAR * source)
{
    HRESULT hr  = S_OK;

    if (*target)
    {
        delete[] (*target);
        *target = NULL;
    }
    
    if (source)
        {
        *target = new WCHAR[wcslen(source)+1];
        CHK_BOOL (*target != NULL, E_OUTOFMEMORY);
        
        wcscpy(*target, source);
        }
        
Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAutoFormat::swprintf(const TCHAR *pchFormatString, ...)
//
//  parameters:
//
//  description:
//      takes care of allocating a buffer
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAutoFormat::sprintf(const TCHAR *pchFormatString, const TCHAR *pchPartOne)
{
    HRESULT hr; 

    CHK_BOOL(pchPartOne, E_UNEXPECTED);
    CHK_BOOL(pchFormatString, E_UNEXPECTED);

    CHK(verifyBuffer(_tcslen(pchFormatString) + _tcslen(pchPartOne)));
    swprintf(m_pchBuffer, pchFormatString, pchPartOne); 

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAutoFormat::copy(const TCHAR *pchFormatString, )
//
//  parameters:
//
//  description:
//      takes care of allocating a buffer
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAutoFormat::copy(const TCHAR *pchSource)
{
    HRESULT hr; 

    CHK_BOOL(pchSource, E_UNEXPECTED);
    CHK(verifyBuffer(_tcslen(pchSource)));
    _tcscpy(m_pchBuffer, pchSource);

Cleanup:
    return (hr);
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAutoFormat::swprintf(const TCHAR *pchFormatString, )
//
//  parameters:
//
//  description:
//      takes care of allocating a buffer
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAutoFormat::sprintf(const TCHAR *pchFormatString, const TCHAR *pchPartOne, const TCHAR *pchPartTwo)
{
    HRESULT hr; 

    CHK_BOOL(pchPartOne, E_UNEXPECTED);
    CHK_BOOL(pchPartTwo, E_UNEXPECTED);    
    CHK_BOOL(pchFormatString, E_UNEXPECTED);
    
    CHK(verifyBuffer(_tcslen(pchFormatString) +_tcslen(pchPartOne)+_tcslen(pchPartTwo)));
    swprintf(m_pchBuffer, pchFormatString, pchPartOne, pchPartTwo); 

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAutoFormat::swprintf(const TCHAR *pchFormatString, )
//
//  parameters:
//
//  description:
//      takes care of allocating a buffer
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAutoFormat::sprintf(const TCHAR *pchFormatString, const TCHAR *pchPartOne, const TCHAR *pchPartTwo, const TCHAR *pchPartThree)
{
    HRESULT hr; 

    CHK_BOOL(pchPartOne, E_UNEXPECTED);
    CHK_BOOL(pchPartTwo, E_UNEXPECTED);    
    CHK_BOOL(pchPartThree, E_UNEXPECTED);        
    CHK_BOOL(pchFormatString, E_UNEXPECTED);
    
    CHK(verifyBuffer(_tcslen(pchFormatString) +_tcslen(pchPartOne)+_tcslen(pchPartTwo)+_tcslen(pchPartThree)));
    swprintf(m_pchBuffer, pchFormatString, pchPartOne, pchPartTwo, pchPartThree); 

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAutoFormat::swprintf(const TCHAR *pchFormatString, )
//
//  parameters:
//
//  description:
//      takes care of allocating a buffer
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAutoFormat::sprintf(const TCHAR *pchFormatString, const TCHAR *pchPartOne, const TCHAR *pchPartTwo, const TCHAR *pchPartThree,  const TCHAR *pchPartFour)
{
    HRESULT hr; 

    CHK_BOOL(pchPartOne, E_UNEXPECTED);
    CHK_BOOL(pchPartTwo, E_UNEXPECTED);    
    CHK_BOOL(pchPartThree, E_UNEXPECTED);        
    CHK_BOOL(pchPartFour, E_UNEXPECTED);        
    CHK_BOOL(pchFormatString, E_UNEXPECTED);
    
    CHK(verifyBuffer(_tcslen(pchFormatString) +_tcslen(pchPartOne)+_tcslen(pchPartTwo)+_tcslen(pchPartThree)+_tcslen(pchPartFour)));
    swprintf(m_pchBuffer, pchFormatString, pchPartOne, pchPartTwo, pchPartThree, pchPartFour); 

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAutoFormat::verifyBuffer(ULONG ulNewLen)
//
//  parameters:
//
//  description:
//      takes care of allocating a buffer
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAutoFormat::verifyBuffer(ULONG ulNewLen)
{
    HRESULT hr; 

    CHK_BOOL(ulNewLen > m_ulSize, S_OK);

    delete [] m_pchBuffer;
    m_ulSize = 0; 

    m_pchBuffer = new TCHAR[ulNewLen+1];
    CHK_BOOL(m_pchBuffer, E_OUTOFMEMORY);
    m_ulSize = ulNewLen; 
    CHK(S_OK);

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: LONG AtomicIncrement(LPLONG lpAddend)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined _M_IX86 && !defined UNDER_CE
__declspec(naked)
LONG __stdcall AtomicIncrement(LPLONG lpAddend)
{
    __asm
    {
        mov         ecx,dword ptr [esp+04h]
        mov         eax, 1
        lock xadd   dword ptr [ecx],eax
        inc         eax
        ret         04h
    }
}
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: LONG AtomicDecrement(LPLONG lpAddend)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined _M_IX86 && !defined UNDER_CE
__declspec(naked)
LONG __stdcall AtomicDecrement(LPLONG lpAddend)
{
    __asm
    {
        mov         ecx,dword ptr [esp+04h]
        mov         eax,0FFFFFFFFh
        lock xadd   dword ptr [ecx],eax
        dec         eax
        ret         04h
    }
}
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT InitSoaputil(void)
//
//  parameters:
//
//  description:
//        Initializes Global Interface Table 
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT InitSoaputil(void)
{
    HRESULT hr;

#ifndef UNDER_CE
    CHK(CoCreateInstance(CLSID_StdGlobalInterfaceTable,
                            NULL,
                            CLSCTX_INPROC_SERVER,
                            IID_IGlobalInterfaceTable,
                            (void **)&g_pGIT));
Cleanup:
#else
    hr = S_OK;
#endif 
    return hr;
}

