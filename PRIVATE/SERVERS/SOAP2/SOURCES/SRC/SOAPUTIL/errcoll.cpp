//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      errcoll.cpp
//
// Contents:
//
//      Errorcollection  class implementation
//
//-----------------------------------------------------------------------------

#include "Headers.h"
#include "mssoap.h"


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorDescription::Init(TCHAR *pchDescription, TCHAR *pchActor, HRESULT hrErrorCode)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorDescription::Init(TCHAR *pchDescription, TCHAR *pchComponent, TCHAR *pchActor, TCHAR *pchFaultString, HRESULT hrErrorCode)
{
    HRESULT hr = E_FAIL;

    if (!pchDescription)
    {
        goto Cleanup;
    }
    m_pchDescription = new TCHAR[wcslen(pchDescription)+1];
    if (!m_pchDescription)
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }
    wcscpy(m_pchDescription, pchDescription);
    if (pchActor)
    {
        m_pchActor = new TCHAR[wcslen(pchActor)+1];
        if (!m_pchActor)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }
        wcscpy(m_pchActor, pchActor);
    }
    if (pchComponent)
    {
        m_pchComponent = new TCHAR[wcslen(pchComponent)+1];
        if (!m_pchComponent)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }
        wcscpy(m_pchComponent, pchComponent);
        
    }
    
    if (pchFaultString)
    {
        m_bFaultEntry = true;;
        m_pchFaultString = new TCHAR[wcslen(pchFaultString)+1];
        if (!m_pchFaultString)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }
        wcscpy(m_pchFaultString, pchFaultString);
    }
    
    m_hrErrorCode = hrErrorCode;
    hr = S_OK;


Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




BOOL CErrorDescription::IsFaultInfo(void)
{
    return(m_bFaultEntry);
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: TCHAR * CErrorDescription::GetDescription(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
TCHAR * CErrorDescription::GetDescription(void)
{
    return(m_pchDescription ? m_pchDescription : L"No Description");
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: TCHAR * CErrorDescription::GetActor(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
TCHAR * CErrorDescription::GetActor(void)
{
    return(m_pchActor);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: TCHAR * CErrorDescription::GetComponent(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
TCHAR * CErrorDescription::GetComponent(void)
{
    return(m_pchComponent ? m_pchComponent : L"UnknownComponent");
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorDescription::GetErrorCode(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorDescription::GetErrorCode(void)
{
    return(m_hrErrorCode);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorDescription::GetFaultString(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
TCHAR * CErrorDescription::GetFaultString(void)
{
    return(m_pchFaultString);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorListEntry::getFaultCode(BSTR *pbstrFaultCode)
//
//  parameters:
//
//  description:
//          here we look at the FIRST entry in our stack and take the correct guy out of it
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorListEntry::getFaultCode(BSTR *pbstrFaultCode)
{
    HRESULT hr = S_FALSE;
    CErrorDescription *pError;

    if (m_fGotSoapError)
    {
        CHK(m_bstrFaultCode.CopyTo(pbstrFaultCode));
    }
    else
    {
        pError = GetAt(0);
        if (pError && pError->IsFaultInfo())
        {
            *pbstrFaultCode = ::SysAllocString(pError->GetComponent());
            if (!*pbstrFaultCode)
            {
                hr = E_OUTOFMEMORY;
                goto Cleanup;
            }
            hr = S_OK;
        }
    }    
    
Cleanup:
    ASSERT(SUCCEEDED(hr));
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorListEntry::getFaultActor(BSTR *pbstrActor)
//
//  parameters:
//
//  description:
//      return the actor for the running thread
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorListEntry::getFaultActor(BSTR *pbstrActor)
{
    if (m_fGotSoapError && !m_bstrFaultActor.isEmpty())
    {
        return(m_bstrFaultActor.CopyTo(pbstrActor));
    }
    return m_bstrActor.CopyTo(pbstrActor);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorListEntry::setFaultActor(BSTR bstrActor)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorListEntry::setFaultActor(BSTR bstrActor)
{
    return (m_bstrActor.Assign(bstrActor));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorListEntry::getFaultNamespace(BSTR *pbstrNamespace)
//
//  parameters:
//
//  description:
//      returns faultnamespace if set by ISoapError
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorListEntry::getFaultNamespace(BSTR *pbstrNamespace)
{
    HRESULT hr = S_OK;

    *pbstrNamespace = 0; 
    if (m_fGotSoapError)
    {
        CHK(m_bstrFaultNamespace.CopyTo(pbstrNamespace));
    }
Cleanup:
    return hr; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT void CErrorListEntry::Reset(void)
//
//  parameters:
//
//  description:
//      resets current state
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CErrorListEntry::Reset(void)
{
    m_bstrActor.Clear();
    m_bstrDescription.Clear();
    m_bstrHelpFile.Clear();
    m_bstrSource.Clear();

	m_bstrFaultString.Clear();
    m_bstrFaultActor.Clear();
    m_bstrFaultDetail.Clear();
    m_bstrFaultCode.Clear();
    m_bstrFaultNamespace.Clear(); 
    
    m_dwHelpContext = 0;
    m_hrFromException = S_OK;
    m_fGotErrorInfo = false;
    m_fGotSoapError = false; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorListEntry::getFaultDetailBSTR(BSTR *pbstrDetail)
//
//  parameters:
//
//  description:
//      here we walk over the current stack, starting at the top and add all those strings together...
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorListEntry::getFaultDetailBSTR(BSTR *pbstrDetail)
{
    HRESULT             hr = S_OK;
    UINT                uiSize;
    UINT                uiDetailSize=0;
    CErrorDescription   *pError;   
    bool                fstart=true; 
    TCHAR               achBuffer[ONEK+1];

    
    if (m_fGotSoapError)
    {
        return(m_bstrFaultDetail.CopyTo(pbstrDetail));
    }

    // otherwise do the complicated stuff
                                                       
    uiSize = m_parrErrors->Size();
    
    // first figure out the size of this
    for (UINT i=0;i<uiSize ;i++ )
    {
        pError = GetAt(i);
        uiDetailSize += wcslen(pError->GetDescription());
        uiDetailSize += wcslen(pError->GetComponent());
        // put some spaces + errorcode around them
        uiDetailSize += 25; 
    }
    if (uiDetailSize)
    {
        uiDetailSize++;
        hr = S_OK;
        
        *pbstrDetail = ::SysAllocStringLen(0, uiDetailSize);
        if (!*pbstrDetail)
        {
            hr = E_OUTOFMEMORY;        
            goto Cleanup;
        }
        // now cut/copy them all togheter
        for (int i=(int)uiSize-1;i>=0 ;i-- )
        {
            pError = GetAt(i);
            if (wcslen(pError->GetDescription())>0)
            {
                if (fstart)
                {
                    wcscpy(*pbstrDetail, pError->GetComponent());
                    wcscat(*pbstrDetail, L":");                    
                    wcscat(*pbstrDetail, pError->GetDescription());
                    fstart=false;
                }
                else
                {
                    wcscat(*pbstrDetail, L" - ");
                    wcscat(*pbstrDetail, pError->GetComponent());
                    wcscat(*pbstrDetail, L":");                    
                    wcscat(*pbstrDetail, pError->GetDescription());
                }
                // now insert the HR
                swprintf(achBuffer, L" HRESULT=0x%lX", pError->GetErrorCode());
                wcscat(*pbstrDetail, achBuffer);
                
            }
        }
    }


Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorListEntry::getFaultString(BSTR *pbstrFaultString)
//
//  parameters:
//
//  description:
//      picks the latest entry in the error collection as the faultstring...
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorListEntry::getFaultString(BSTR *pbstrFaultString)
{
    CErrorDescription   *pError;   
    HRESULT             hr = E_FAIL;
    UINT                uiSize;
    bool                fFaultString=true;

    
    if (m_fGotSoapError)
    {
        return(m_bstrFaultString.CopyTo(pbstrFaultString));
    }
    if (m_fGotErrorInfo)
    {
        return (m_bstrDescription.CopyTo(pbstrFaultString));
    }


    // get the first guy                                                   
    pError = GetAt(0);
    if (pError)
    {
    
        if (pError->GetFaultString()==0)
        {
            uiSize = wcslen(pError->GetDescription());
            uiSize += wcslen(pError->GetComponent());
            // put some spaces around them
            uiSize += 3;
            fFaultString = false;
                
        }
        else
        {
            uiSize = wcslen(pError->GetFaultString());            
        }
    
        
        *pbstrFaultString = ::SysAllocStringLen(0, uiSize);
        if (!*pbstrFaultString)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }
        
        if (fFaultString)
        {
            wcscpy(*pbstrFaultString, pError->GetFaultString());
        }
        else
        {
            wcscpy(*pbstrFaultString, pError->GetComponent());
            wcscat(*pbstrFaultString, L": ");
            wcscat(*pbstrFaultString, pError->GetDescription());
        }
        hr = S_OK;
    }
Cleanup:
    return(hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:         HRESULT CErrorListEntry::SetErrorInfo(, 
//
//  parameters:
//
//  description:
//      set's the ERRORINFO information
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorListEntry::SetErrorInfo(BSTR bstrDescription, 
                BSTR bstrSource, BSTR bstrHelpFile, 
                DWORD dwHelpContext, HRESULT hrFromException)
{
    HRESULT hr;

    CHK(m_bstrDescription.Assign(bstrDescription));
    CHK(m_bstrSource.Assign(bstrSource));
    CHK(m_bstrHelpFile.Assign(bstrHelpFile));
    m_dwHelpContext = dwHelpContext;
    m_hrFromException = hrFromException;

    m_fGotErrorInfo = true; 

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:         HRESULT CErrorListEntry::GetErrorInfo(, 
//
//  parameters:
//
//  description:
//      surprise: get's the ERRORINFO information
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorListEntry::GetErrorInfo(BSTR *pbstrDescription, 
                        BSTR *pbstrSource, BSTR *pbstrHelpFile, 
                        DWORD *pdwHelpContext, HRESULT *phrFromException)
{
    HRESULT hr=S_FALSE;

    if (m_fGotErrorInfo)
    {
        CHK(m_bstrDescription.CopyTo(pbstrDescription));
        CHK(m_bstrSource.CopyTo(pbstrSource));
        CHK(m_bstrHelpFile.CopyTo(pbstrHelpFile));    

        *pdwHelpContext = m_dwHelpContext;
        *phrFromException = m_hrFromException;
    }    

Cleanup:
    return (hr);
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:         HRESULT CErrorListEntry::SetSoapError
//
//  parameters:
//
//  description:
//      sets the soaperror for this list
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorListEntry::SetSoapError(BSTR bstrFaultString, BSTR bstrFaultActor, BSTR bstrDetail, BSTR bstrFaultCode, BSTR bstrNameSpace)
{
    HRESULT hr; 

    CHK(m_bstrFaultString.Assign(bstrFaultString, true));
    CHK(m_bstrFaultActor.Assign(bstrFaultActor, true));
    CHK(m_bstrFaultDetail.Assign(bstrDetail, true));
    CHK(m_bstrFaultCode.Assign(bstrFaultCode, true));
    CHK(m_bstrFaultNamespace.Assign(bstrNameSpace, true));

    m_fGotSoapError = true; 
Cleanup:
    return hr; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:         HRESULT CErrorListEntry::GetSoapError
//
//  parameters:
//
//  description:
//      gets the soaperror for this list, S_FALSE if none there...
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorListEntry::GetSoapError(BSTR *pbstrFaultString, BSTR *pbstrFaultActor, BSTR *pbstrDetail, BSTR *pbstrFaultCode, BSTR *pbstrNameSpace)
{
    HRESULT hr = S_FALSE;

    if (m_fGotSoapError)
    {
        CHK(m_bstrFaultString.CopyTo(pbstrFaultString));
        CHK(m_bstrFaultActor.CopyTo(pbstrFaultActor));
        CHK(m_bstrFaultDetail.CopyTo(pbstrDetail));
        CHK(m_bstrFaultCode.CopyTo(pbstrFaultCode));
        CHK(m_bstrFaultNamespace.CopyTo(pbstrNameSpace));
    }

Cleanup:
    return hr; 
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:         HRESULT CErrorListEntry::GetReturnCode(HRESULT *phrReturnCode)
//
//  parameters:
//
//  description:
//      returns the returncode : either from errorinfo, ot the first entry in the stack
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorListEntry::GetReturnCode(HRESULT *phrReturnCode)
{
    if (m_fGotErrorInfo)
    {
        *phrReturnCode = m_hrFromException;
        return S_OK; 
    }
    else
    {
        CErrorDescription *pError = GetAt(0);
        if (pError)
        {
            *phrReturnCode = pError->GetErrorCode();
            return S_OK;
        }

    }
    return E_FAIL;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:         void    CErrorCollection::setActor(BSTR *pbstrActor)
//
//  parameters:
//
//  description:
//      sets the actor attribute for the errorlist...
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorCollection::setActor(BSTR bstrActor)
{
    HRESULT             hr = E_FAIL;
    CErrorListEntry     *pList = 0; 

    pList = getCurrentList();
    CHK_BOOL(pList, E_OUTOFMEMORY);
    CHK(pList->setFaultActor(bstrActor));

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorCollection::AddErrorEntry(TCHAR *pchDescription, TCHAR *pchActor, HRESULT hrErrorCode)
//
//  parameters:
//      2 strings and the HR
//  description:
//      adds this to the bucket list for a threadid
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorCollection::AddErrorEntry(TCHAR *pchDescription, TCHAR *pchComponent, 
                                        TCHAR *pchActor, TCHAR *pchFaultString, HRESULT hrErrorCode)
{
    HRESULT             hr = E_FAIL;
    CErrorDescription   *pEntry=0;
    CErrorListEntry     *pList = 0; 

    
    pEntry = new CErrorDescription();
    CHK_BOOL(pEntry, E_OUTOFMEMORY);
    
    CHK(pEntry->Init(pchDescription, pchComponent, pchActor, pchFaultString, hrErrorCode));

    pList = getCurrentList();
    CHK_BOOL(pList, E_OUTOFMEMORY);

    CHK(pList->AddEntry(pEntry))

    // ownership for pEntry successfully passed to the errorcollection    
    pEntry = 0;
    


Cleanup:
    delete pEntry;
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorCollection::AddErrorFromSoapError(ISOAPError * pSoapError)
//
//  parameters:
//
//  description:
//      is used to add the ISoapErrorInfo after a call to a server object failed
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorCollection::AddErrorFromSoapError(IDispatch * pDispatch)
{
    CAutoRefc<ISOAPError>   pISoapError=0;
    HRESULT                 hr; 
    CAutoBSTR               bstrFaultString;
    CAutoBSTR               bstrFaultActor;
    CAutoBSTR               bstrDetail;
    CAutoBSTR               bstrFaultCode;
    CAutoBSTR               bstrNamespace; 
    
	hr = pDispatch->QueryInterface(IID_ISOAPError, (void **)&pISoapError); 
    if (SUCCEEDED(hr) && pISoapError)    
    {
        CErrorListEntry     *pList = 0; 
        pList = getCurrentList();
        CHK_BOOL(pList, E_OUTOFMEMORY);
        
        CHK(pISoapError->get_faultstring(&bstrFaultString));
        CHK(pISoapError->get_faultactor(&bstrFaultActor));
        CHK(pISoapError->get_detail(&bstrDetail));
        CHK(pISoapError->get_faultcode(&bstrFaultCode));
        CHK(pISoapError->get_faultcodeNS(&bstrNamespace));
        
        if (!bstrDetail.isEmpty() && !bstrFaultString.isEmpty())
        {
            CHK(pList->SetSoapError(bstrFaultString, 
                             bstrFaultActor, 
                             bstrDetail, 
                             bstrFaultCode, 
                             bstrNamespace));
        }
    }

Cleanup:
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorCollection::AddErrorFromErrorInfo(LPEXCEPINFO pExecpInfo, HRESULT hrErrorCode)
//
//  parameters:
//
//  description:
//      is used to add the exepinfo/errorinfo after a call to a server object failed
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorCollection::AddErrorFromErrorInfo(LPEXCEPINFO pexepInfo, HRESULT hrErrorCode)
{
    HRESULT                 hr=S_OK;
    CAutoRefc<IErrorInfo>      pIErrInfo;
    CAutoBSTR               bstrSource;
    CAutoBSTR               bstrDescription;
    CAutoBSTR               bstrHelpFile;
    DWORD                  dwHelpContext; 

    CErrorListEntry     *pList = 0; 

    pList = getCurrentList();
    CHK_BOOL(pList, E_OUTOFMEMORY);

    
    if (pexepInfo)
    {
        // get the information out of the exepInfo structure
        CHK(pList->SetErrorInfo(pexepInfo->bstrDescription , 
                             pexepInfo->bstrSource , 
                             pexepInfo->bstrHelpFile, 
                             pexepInfo->dwHelpContext, 
                             pexepInfo->scode ? pexepInfo->scode : pexepInfo->wCode));
    }
    else
    {
        if (GetErrorInfo(0, &pIErrInfo)==S_OK)
        {
        
            CHK(pIErrInfo->GetSource(&bstrSource));
            CHK(pIErrInfo->GetDescription(&bstrDescription))
            CHK(pIErrInfo->GetHelpFile(&bstrHelpFile));
            CHK(pIErrInfo->GetHelpContext(&dwHelpContext));
            CHK(pList->SetErrorInfo(bstrDescription , 
                             bstrSource , 
                             bstrHelpFile, 
                             dwHelpContext, 
                             hrErrorCode));
            
        

        }
    }

Cleanup:
    return (hr);
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CErrorCollection::GetErrorEntry(CErrorListEntry **ppEntry)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CErrorCollection::GetErrorEntry(CErrorListEntry **ppEntry)
{
    HRESULT             hr = S_OK;

    *ppEntry =  m_errorList.FindEntry(GetCurrentThreadId());    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CErrorListEntry * CErrorCollection::getCurrentList(void)
//
//  parameters:
//
//  description:
//      helper to find the current list
//  returns: 
//      0 in case of outofmemory. But there is not a lot we can do, we are the errorhandler anyway.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CErrorListEntry * CErrorCollection::getCurrentList(void)
{
    CErrorListEntry                     *pList = 0; 
    DWORD                           dwCookie; 
    CTypedDynArray<CErrorDescription>   *pArr;

    // first find the right list

    dwCookie = GetCurrentThreadId(); 
    
    pList =  m_errorList.FindEntry(dwCookie);    
    if (!pList)
    {
        pArr = new CTypedDynArray<CErrorDescription>();
        if (pArr)
        {
            m_errorList.Add(pArr, dwCookie); 
            pList = m_errorList.FindEntry(dwCookie); 
        }    
    }

    return pList;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CErrorCollection::Shutdown(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CErrorCollection::Shutdown(void)
{
    m_errorList.Shutdown();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CErrorCollection::Reset(void)
//
//  parameters:
//
//  description:
//      deletes the current list for the thread
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CErrorCollection::Reset(void)
{
    CTypedDynArray<CErrorDescription> *pArr;
    CErrorListEntry *pList;

    pArr = new CTypedDynArray<CErrorDescription>();
    m_errorList.Replace(pArr, GetCurrentThreadId());

    pList = getCurrentList();
    if (pList)
        pList->Reset();
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void globalAddError(UINT idsResource, HRESULT hr)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void globalAddError(UINT idsResource, UINT idsComponent, HRESULT hr)
{
#ifndef UNDER_CE
    TCHAR   achBuffer[ONEK+1]; 
    TCHAR   achComponent[ONEK+1];    
#else
    TCHAR   *achBuffer = new TCHAR[ONEK+1]; 
    TCHAR   *achComponent = new TCHAR[ONEK+1];   
    
    CHK_MEM(achBuffer);
    CHK_MEM(achComponent);
#endif 
    UINT    uiLen;

    uiLen = GetResourceString(idsResource, (WCHAR *)achBuffer, ONEK, 0);
    if (!uiLen)
#ifndef UNDER_CE
        return;        // Error not found
#else
        goto Cleanup;
#endif 
        
    uiLen = GetResourceString(idsComponent, (WCHAR *)achComponent, ONEK, 0);
    if (!uiLen)
#ifndef UNDER_CE
        return;        // Error not found
#else
        goto Cleanup;
#endif 
        
        
    g_cErrorCollection.AddErrorEntry(achBuffer, achComponent, 0, 0, hr);
        
#ifdef UNDER_CE
Cleanup:
    if(achBuffer)
        delete [] achBuffer;
    if(achComponent)
        delete [] achComponent;       
#endif 

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void globalAddError(UINT idsResource, HRESULT hr, TCHAR *pchMsgPart)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void globalAddError(UINT idsResource, UINT idsComponent, HRESULT hr, ...)
{
#ifndef UNDER_CE
    TCHAR   achBuffer[ONEK+1]; 
    TCHAR   achComponent[ONEK+1];    
#else
    TCHAR   *achBuffer = new TCHAR[ONEK+1]; 
    TCHAR   *achComponent = new TCHAR[ONEK+1];   
    
    CHK_MEM(achBuffer);
    CHK_MEM(achComponent);
#endif 
    
    
    UINT    uiLen;
    va_list args;
    
    
    va_start(args, hr);


    uiLen = GetResourceStringHelper(idsResource, (WCHAR *)achBuffer, ONEK, &args);
    if (!uiLen)
#ifndef UNDER_CE
        return;        // Error not found
#else
        goto Cleanup;
#endif 

    va_end(args);
        
    uiLen = GetResourceString(idsComponent, (WCHAR *)achComponent, ONEK);
    if (!uiLen)
#ifndef UNDER_CE
        return;        // Error not found
#else
        goto Cleanup;
#endif 
        
        
    g_cErrorCollection.AddErrorEntry(achBuffer, achComponent, 0,0, hr);
        
#ifdef UNDER_CE
Cleanup:
    if(achBuffer)
        delete [] achBuffer;
    if(achComponent)
        delete [] achComponent;       
#endif
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void globalDeleteErrorList(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void globalDeleteErrorList(void)
{
     g_cErrorCollection.Shutdown();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void globalResetErrors(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void globalResetErrors(void)
{
    g_cErrorCollection.Reset();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////void globalResetErrors(void)




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void globalGetError(CErrorListEntry **ppErrorList)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void globalGetError(CErrorListEntry **ppErrorList)
{
    g_cErrorCollection.GetErrorEntry(ppErrorList);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void         globalAddErrorFromErrorInfo();
//
//  parameters:
//
//  description:
//      adds a global error entry based on IErrorInfo
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void globalAddErrorFromErrorInfo(EXCEPINFO * pexepInfo, HRESULT hrErrorCode)
{
    g_cErrorCollection.AddErrorFromErrorInfo(pexepInfo, hrErrorCode); 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void globalAddErrorFromISoapError(IDispatch *pDispatch)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void globalAddErrorFromISoapError(IDispatch *pDispatch, HRESULT hrErrorCode)
{
    g_cErrorCollection.AddErrorFromSoapError(pDispatch);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT globalSetActor(BSTR bstrCurActor)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT globalSetActor(WCHAR * pcCurActor)
{
    if (pcCurActor)
    {   
        g_cErrorCollection.setActor(pcCurActor);        
    }
    return S_OK;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

